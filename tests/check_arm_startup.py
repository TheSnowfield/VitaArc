"""Execute linked VitaArc main with intercepted platform/subsystem endpoints.

Checks version selection, both vitaGL resolution results, compiler prerequisites,
and application failures through shutdown. This does not initialize real graphics
or execute the loader/JNI subsystems, which have their own ARM checks.
"""

import argparse
from pathlib import Path

from arm_fixture_build import root
from check_arm_bridges import ArmCalls, ArmImage
from unicorn import arm_const as registers


def check_startup(elf, fallback, failure, compiler="primary", pacing_delay=0, version="selected"):
    calls = ArmCalls()
    image = ArmImage(calls.machine, elf)
    events = []
    messages = []
    version_logs = []
    handle = calls.SCRATCH
    version_names = ("1.6.1c", "7.0.260c")
    selected_name = version_names[1]
    version_vector = calls.SCRATCH + 0x1000
    version_strings = (calls.SCRATCH + 0x1100, calls.SCRATCH + 0x1200)
    version_root = f"ux0:data/vitaarc/version/{selected_name}/"
    selected_paths = {
        "PATH_TO_PACKAGE": version_root,
        "PATH_TO_LIBRARY": version_root + "lib/armeabi-v7a/",
        "PATH_TO_ASSETS": version_root + "assets",
        "PATH_TO_OBB": version_root + "obb/",
        "LIBRARY_LIBCOCOS2DCPP": version_root + "lib/armeabi-v7a/libcocos2dcpp.so",
    }
    for index, (name, address) in enumerate(zip(version_names, version_strings)):
        calls.machine.mem_write(address, name.encode() + b"\0")
        calls.word(version_vector + index * 4, address)
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
                message = string(args[1])
                messages.append((name, message))
                if message in ("Selected version: %s", "Game resources: %s"):
                    version_logs.append((len(events) - 1, message, string(args[2])))
            if name == "vita_versions_scan":
                assert calls.machine.mem_read(args[0], 8) == bytes(8)
                empty = version in ("empty", "scan-failure")
                calls.word(args[0], 0 if empty else version_vector)
                calls.word(args[0] + 4, 0 if empty else len(version_names))
            if name == "log_screen_select":
                assert args[0:2] == ((0, 0) if version == "empty" else (version_vector, len(version_names)))
                assert int.from_bytes(calls.machine.mem_read(args[2], 4), "little") == 0
                if version != "empty":
                    calls.word(args[2], len(version_names) if version == "invalid-index" else 1)
            if name == "vita_version_select":
                assert string(args[0]) == selected_name
                if version != "select-failure":
                    for symbol, path in selected_paths.items():
                        calls.machine.mem_write(image.symbols[symbol], path.encode() + b"\0")
            if name == "vita_versions_free":
                expected = (bytes(8) if version == "empty" else
                            version_vector.to_bytes(4, "little") + len(version_names).to_bytes(4, "little"))
                assert calls.machine.mem_read(args[0], 8) == expected
                calls.machine.mem_write(args[0], bytes(8))
            if name == "sceKernelGetFreeMemorySize":
                values = (16, 160 * 1024 * 1024, 112 * 1024 * 1024, 26 * 1024 * 1024)
                calls.machine.mem_write(args[0], b"".join(value.to_bytes(4, "little") for value in values))
            if name == "vglSetCircularPoolSize":
                assert args[0] == 6 * 1024 * 1024
            if name == "vglInitWithCustomThreshold":
                assert args == (0, 960, 544, 128 * 1024 * 1024)
                stack = calls.machine.mem_read(calls.reg("SP"), 16)
                assert [int.from_bytes(stack[n:n+4], "little") for n in range(0, 16, 4)] == [0, 0x7fffffff, 0x7fffffff, 0]
            if name == "loader_load_library":
                assert string(args[0]) == selected_paths["LIBRARY_LIBCOCOS2DCPP"]
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
            if name == "vita_versions_scan":
                value = int(version != "scan-failure")
            if name == "log_screen_select":
                value = int(version not in ("empty", "cancel"))
            if name == "vita_version_select":
                value = int(version != "select-failure")
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
            "log_screen_begin", "log_screen_ready", "log_screen_select", "log_screen_wait", "log_screen_end",
            "vita_versions_scan", "vita_versions_free", "vita_version_select", "sceIoMkdir",
            "scePowerSetArmClockFrequency", "scePowerSetBusClockFrequency",
            "scePowerSetGpuClockFrequency", "scePowerSetGpuXbarClockFrequency",
            "vglSetupRuntimeShaderCompiler", "vglSetVDMBufferSize",
            "vglSetVertexBufferSize", "vglSetFragmentBufferSize", "vglSetUSSEBufferSize",
            "vglSetCircularPoolSize", "sceKernelGetFreeMemorySize", "vita_memory_log", "vita_memory_tick", "vita_perf_frame",
            "bridge_compat_initialize", "bridge_patch_gl", "bridge_patch_cocos2dx",
            "loader_flush_caches", "bridge_jni_cocos_stop", "glFinish",
            "loader_free_library", "bridge_cocos_release_textures", "bridge_gl_shutdown_buffers", "vita_frame_step",
            "vglSwapBuffers", "sceKernelDelayThread", "sceKernelExitProcess", "shark_init",
            "bridge_shader_enable_offline"):
        endpoint(name)
    endpoint("sceKernelGetProcessTimeWide")
    endpoint("vita_frame_delay", pacing_delay)
    endpoint("vglInitWithCustomThreshold", fallback)
    endpoint("loader_load_library", 0 if failure == "load" else handle)
    endpoint("loader_init_library", int(failure != "constructors"))
    endpoint("bridge_cocos_configure_storage", int(failure != "storage"))
    endpoint("bridge_cocos_configure_textures", 1)
    endpoint("bridge_jni_cocos_start", int(failure != "jni"))
    endpoint("vita_input_initialize", int(failure != "input"))

    assert calls.call(image.symbols["main"]) == 1
    names = [event[0] for event in events]
    assert names.count("vglInitWithCustomThreshold") == 1
    assert names.index("log_screen_begin") < names.index("log_begin")
    assert names.index("vglInitWithCustomThreshold") < names.index("log_screen_ready")
    assert names.index("log_screen_ready") < names.index("vita_versions_scan")
    assert names.count("vita_versions_scan") == 1
    assert names.count("log_screen_end") == 1
    assert names[-2:] == ["log_end", "sceKernelExitProcess"]
    assert names.count("vita_versions_free") == int(version != "scan-failure")
    if version != "selected":
        assert "bridge_shader_enable_offline" not in names and "shark_init" not in names
        assert "bridge_compat_initialize" not in names and "loader_load_library" not in names
        assert "bridge_jni_cocos_start" not in names and "vita_frame_step" not in names
        assert "bridge_jni_cocos_stop" not in names and "loader_free_library" not in names
        assert not version_logs
        assert names.count("log_screen_select") == int(version != "scan-failure")
        assert names.count("vita_version_select") == int(version == "select-failure")
        assert names.count("log_screen_wait") == int(version in ("scan-failure", "select-failure", "invalid-index"))
        if version == "scan-failure":
            assert any("Cannot scan game versions" in message for _, message in messages)
        if version in ("select-failure", "invalid-index"):
            assert any("Selected game directory is unavailable" in message for _, message in messages)
        if "log_screen_select" in names:
            assert names.index("vita_versions_scan") < names.index("log_screen_select") < names.index("vita_versions_free")
        if "log_screen_wait" in names:
            assert names.index("log_screen_wait") < names.index("log_screen_end")
        return

    assert names.count("log_screen_select") == names.count("vita_version_select") == 1
    assert names.index("vita_versions_scan") < names.index("log_screen_select") < names.index("vita_version_select")
    assert [(message, value) for _, message, value in version_logs] == [
        ("Selected version: %s", selected_name), ("Game resources: %s", version_root)]
    assert names.index("vita_version_select") < version_logs[0][0] < version_logs[1][0]
    assert version_logs[1][0] < names.index("vita_versions_free") < names.index("bridge_shader_enable_offline")
    assert names.count("log_screen_wait") == int(compiler == "missing" or failure != "frame")
    for symbol, expected in selected_paths.items():
        assert string(image.symbols[symbol]) == expected
    assert names.index("log_screen_ready") < names.index("bridge_shader_enable_offline")
    assert names.count("shark_init") == (0 if compiler == "offline" else 1 if compiler == "primary" else 2)
    assert names.count("bridge_shader_enable_offline") == 1
    assert names.index("vglInitWithCustomThreshold") < names.index("bridge_shader_enable_offline")
    if compiler != "offline":
        assert names.index("bridge_shader_enable_offline") < names.index("shark_init")
    fallback_messages = [message for level, message in messages
                         if "fallback display resolution" in message and level == "log_w"]
    assert len(fallback_messages) == fallback
    assert not any("VitaGL initialization failed" in message for _, message in messages)
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
        for version in ("empty", "cancel", "scan-failure", "select-failure", "invalid-index"):
            check_startup(args.elf, fallback, "frame", version=version)
    print("Linked ARM main passed 50 cases: version selection updates resource paths and logs the selected directory before boot; "
          "empty/cancel/scan/select/invalid-index failures exit without shader or loader startup; "
          "both vitaGL resolution results, offline pack preferred even with available runtime compiler, compiler primary/fallback/missing paths; "
          "load/constructor/storage/JNI/input failures and ready/paused/failed frames reach ordered shutdown; "
          "requested frame delay is applied once after swap.")


if __name__ == "__main__":
    main()
