"""Execute linked VitaArc main with intercepted platform/subsystem endpoints.

Checks both vitaGL resolution results, compiler prerequisites, and application
failures through shutdown. This does not initialize real graphics
or execute the loader/JNI subsystems, which have their own ARM checks.
"""

import argparse
from pathlib import Path

from arm_fixture_build import root
from check_arm_bridges import ArmCalls, ArmImage
from unicorn import arm_const as registers


def check_startup(elf, fallback, failure, compiler="primary", pacing_delay=0):
    calls = ArmCalls()
    image = ArmImage(calls.machine, elf)
    events = []
    messages = []
    handle = calls.SCRATCH
    frames = iter((0, 1, 2))  # Ready, paused, then a callback failure.

    def string(address):
        data = bytearray()
        while len(data) < 512:
            value = calls.machine.mem_read(address + len(data), 1)[0]
            if not value:
                return data.decode()
            data.append(value)
        raise AssertionError("unterminated startup string")

    def endpoint(name, result=0):
        def invoke():
            args = tuple(calls.reg(f"R{index}") for index in range(4))
            events.append((name, args))
            if name in ("log_i", "log_w", "log_e", "log_f"):
                messages.append((name, string(args[1])))
            if name == "vglInitExtended":
                assert args == (0, 960, 544, 24 * 1024 * 1024)
                assert int.from_bytes(calls.machine.mem_read(calls.reg("SP"), 4), "little") == 0
            if name == "loader_load_library":
                assert string(args[0]) == "app0:package/lib/armeabi-v7a/libcocos2dcpp.so"
            if name == "shark_init":
                if args[0]:
                    assert string(args[0]) == "ur0:data/external/libshacccg.suprx"
            if name in ("bridge_patch_gl", "bridge_patch_cocos2dx", "loader_flush_caches",
                        "loader_init_library", "bridge_cocos_configure_storage",
                        "bridge_cocos_configure_textures", "loader_free_library"):
                assert args[0] == handle
            if name == "bridge_jni_cocos_start":
                assert args[:3] == (handle, 960, 544)
            if name == "sceKernelDelayThread":
                assert args[0] in (16000, pacing_delay)
            if name == "sceKernelGetProcessTimeWide":
                calls.machine.reg_write(registers.UC_ARM_REG_R1, 0)
            if name == "vglSwapBuffers":
                assert args[0] == 1
            if name == "sceKernelExitProcess":
                assert args[0] == 1
                calls.machine.reg_write(registers.UC_ARM_REG_R0, 1)
                calls.machine.reg_write(registers.UC_ARM_REG_PC, calls.RETURN)
                return False
            value = next(frames) if name == "vita_frame_step" else result
            if name == "shark_init":
                # In the offline case a runtime compiler would be available;
                # selecting the pack must prevent even attempting to init it.
                value = 0 if (compiler in ("primary", "offline") or (args[0] and compiler == "fallback")) else 0xffffffff
            if name == "bridge_shader_enable_offline":
                value = int(compiler == "offline")
            calls.machine.reg_write(registers.UC_ARM_REG_R0, value)
        calls.register(image.symbols[name], invoke)

    for name in (
            "log_begin", "log_end", "log_i", "log_w", "log_e", "log_f", "log_flush",
            "log_screen_begin", "log_screen_ready", "log_screen_end", "sceIoMkdir",
            "scePowerSetArmClockFrequency", "scePowerSetBusClockFrequency",
            "scePowerSetGpuClockFrequency", "scePowerSetGpuXbarClockFrequency",
            "vglSetupRuntimeShaderCompiler", "vglSetVDMBufferSize",
            "vglSetVertexBufferSize", "vglSetFragmentBufferSize", "vglSetUSSEBufferSize",
            "bridge_compat_initialize", "bridge_patch_gl", "bridge_patch_cocos2dx",
            "loader_flush_caches", "bridge_jni_cocos_stop", "glFinish",
            "loader_free_library", "bridge_cocos_release_textures", "bridge_gl_shutdown_buffers", "vita_frame_step",
            "vglSwapBuffers", "sceKernelDelayThread", "sceKernelExitProcess", "shark_init",
            "bridge_shader_enable_offline", "log_startup_heap"):
        endpoint(name)
    endpoint("sceKernelGetProcessTimeWide")
    endpoint("vita_frame_delay", pacing_delay)
    endpoint("vglInitExtended", fallback)
    endpoint("loader_load_library", 0 if failure == "load" else handle)
    endpoint("loader_init_library", int(failure != "constructors"))
    endpoint("bridge_cocos_configure_storage", int(failure != "storage"))
    endpoint("bridge_cocos_configure_textures", 1)
    endpoint("bridge_jni_cocos_start", int(failure != "jni"))
    endpoint("vita_input_initialize", int(failure != "input"))

    assert calls.call(image.symbols["main"]) == 1
    names = [event[0] for event in events]
    assert names.count("vglInitExtended") == 1
    assert names.index("log_screen_begin") < names.index("log_begin")
    assert names.index("vglInitExtended") < names.index("log_screen_ready")
    assert names.index("log_screen_ready") < names.index("bridge_shader_enable_offline")
    assert names.count("log_screen_end") == 1
    assert names.count("shark_init") == (0 if compiler == "offline" else 1 if compiler == "primary" else 2)
    assert names.count("bridge_shader_enable_offline") == 1
    assert names.index("vglInitExtended") < names.index("bridge_shader_enable_offline")
    if compiler != "offline":
        assert names.index("bridge_shader_enable_offline") < names.index("shark_init")
    fallback_messages = [message for level, message in messages
                         if "fallback display resolution" in message and level == "log_w"]
    assert len(fallback_messages) == fallback
    assert not any("VitaGL initialization failed" in message for _, message in messages)
    assert names[-2:] == ["log_end", "sceKernelExitProcess"]
    if compiler == "missing":
        assert any("Shader compiler unavailable" in message for _, message in messages)
        assert "bridge_compat_initialize" not in names and "loader_load_library" not in names
        assert "bridge_jni_cocos_stop" not in names and "glFinish" not in names
        return
    assert names.count("loader_load_library") == 1
    selected = "bridge_shader_enable_offline" if compiler == "offline" else "shark_init"
    assert names.index(selected) < names.index("bridge_compat_initialize") < names.index("loader_load_library")
    if failure == "load":
        assert "bridge_patch_gl" not in names and "loader_free_library" not in names
        assert "bridge_jni_cocos_stop" not in names and "glFinish" not in names
    else:
        shutdown = ["bridge_jni_cocos_stop", "glFinish", "loader_free_library", "bridge_cocos_release_textures",
                    "glFinish", "bridge_gl_shutdown_buffers", "log_end", "sceKernelExitProcess"]
        assert names[-len(shutdown):] == shutdown
    if failure == "constructors":
        assert "bridge_cocos_configure_storage" not in names
    if failure in ("constructors", "storage"):
        assert "bridge_jni_cocos_start" not in names
    if failure in ("constructors", "storage", "jni"):
        assert "vita_input_initialize" not in names
    if failure == "frame":
        assert names.index("log_screen_end") < names.index("vita_frame_step")
        assert names.count("vita_frame_step") == 3
        assert names.count("vglSwapBuffers") == names.count("vita_frame_delay") == 1
        delays = [args[0] for name, args in events if name == "sceKernelDelayThread"]
        assert delays == ([pacing_delay, 16000] if pacing_delay else [16000])
        assert names.index("vglSwapBuffers") < names.index("vita_frame_delay")
    else:
        assert "vita_frame_step" not in names and "vglSwapBuffers" not in names


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, default=root / "build/VitaArc")
    args = parser.parse_args()
    for fallback in (0, 1):
        for compiler in ("primary", "fallback", "offline"):
            for failure in ("load", "constructors", "storage", "jni", "input", "frame"):
                check_startup(args.elf, fallback, failure, compiler)
        check_startup(args.elf, fallback, "load", "missing")
        check_startup(args.elf, fallback, "frame", pacing_delay=3000)
    print("Linked ARM main passed 40 cases: both vitaGL resolution results, offline pack preferred even with available runtime compiler, compiler primary/fallback/missing paths; "
          "load/constructor/storage/JNI/input failures and ready/paused/failed frames reach ordered shutdown; "
          "requested frame delay is applied once after swap.")


if __name__ == "__main__":
    main()
