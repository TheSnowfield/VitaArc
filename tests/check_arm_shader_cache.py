"""Exercise linked shader bridge/cache with modeled GL and Vita filesystem.

Checks exact source assembly/hash matching, Cg handle creation, binary loading,
rejection before native GL, handle reuse and allocation/file cleanup. Actual
GPU execution is covered separately by vita_shader_probe.c and game runs.
"""

import struct
import sys

from check_arm_loader import LoaderCase
from arm_fixture_build import root
sys.path.insert(0, str(root / "tools"))
from hash_shader import hash_shader
from unicorn import arm_const as registers


def binary():
    gxp = bytearray(216)
    for offset, value in ((0, 0x00505847), (8, 216), (0x24, 1),
                          (0x28, 156 - 0x28), (156, 16), (160, 1)):
        struct.pack_into("<I", gxp, offset, value)
    gxp[172:185] = b"CC_MVPMatrix\0"
    return struct.pack("<II", 1, 0) + gxp


def check(offline, failure=None):
    source = b"void main() {}"
    digest = hash_shader(source)
    files = {"app0:/shaders/offline.enabled": b"VitaArc offline shaders 1\n"}
    payload = binary()
    cache = f"app0:/shaders/{digest:016x}.vgxp"
    if failure != "missing":
        files[cache] = b"bad" if failure == "invalid" else payload
    case = LoaderCase(files)
    calls, image, platform = case.calls, case.target, case.platform
    assert case.call("bridge_shader_enable_offline") == 1
    if not offline:
        files.clear()
        assert case.call("bridge_shader_enable_offline") == 0
    native_types, sources, compiled, deleted, exits, dumps = [], [], [], [], [], []
    states = {}
    next_handle = 1

    def create():
        nonlocal next_handle
        native_types.append(calls.reg("R0"))
        states[next_handle] = False
        platform.return_value(next_handle)
        next_handle += 1

    def get_iv():
        shader, query, output = calls.reg("R0"), calls.reg("R1"), calls.reg("R2")
        value = {0x8b4f: 0x8b31, 0x8b81: int(states[shader]), 0x8b84: 0}[query]
        calls.word(output, value)

    def set_source():
        shader, count, strings, lengths = (calls.reg(f"R{i}") for i in range(4))
        assert count == 1
        pointer = struct.unpack("<I", calls.machine.mem_read(strings, 4))[0]
        length = struct.unpack("<I", calls.machine.mem_read(lengths, 4))[0]
        sources.append(bytes(calls.machine.mem_read(pointer, length)))
        states[shader] = False

    def load_binary():
        assert offline and calls.reg("R0") == 1 and calls.reg("R2") == 0
        shader = struct.unpack("<I", calls.machine.mem_read(calls.reg("R1"), 4))[0]
        length = struct.unpack("<I", calls.machine.mem_read(calls.reg("SP"), 4))[0]
        assert bytes(calls.machine.mem_read(calls.reg("R3"), length)) == payload
        states[shader] = True
        compiled.append("binary")

    def compile_source():
        assert not offline
        states[calls.reg("R0")] = True
        compiled.append("source")

    def exit_process():
        exits.append(calls.reg("R0"))
        calls.machine.reg_write(registers.UC_ARM_REG_PC, calls.RETURN)
        return False

    for name, callback in (("glCreateShader", create), ("glGetShaderiv", get_iv),
                           ("glShaderSource", set_source), ("glShaderBinary", load_binary),
                           ("glCompileShader", compile_source),
                           ("glDeleteShader", lambda: deleted.append(calls.reg("R0"))),
                           ("bridge_shader_dump_missing", lambda: dumps.append(True)),
                           ("sceKernelExitProcess", exit_process)):
        calls.register(image.symbols[name], callback)
    # Supply two source segments, including an explicit length that omits junk.
    first, second, pointers, lengths = (calls.SCRATCH + offset for offset in (0, 64, 128, 192))
    calls.machine.mem_write(first, b"void JUNK\0")
    calls.machine.mem_write(second, b"main() {}\0")
    calls.machine.mem_write(pointers, struct.pack("<II", first, second))
    calls.machine.mem_write(lengths, struct.pack("<ii", 5, -1))
    iterations = 300 if not failure else 1  # Exceeds the 256 tracking slots.
    for index in range(iterations):
        shader = case.call("bridge_gl_create_shader", 0x8b31)
        assert native_types[-1] == (0x890e if offline else 0x8b31)
        case.call("_gl_shader_source", shader, 2, pointers, lengths)
        assert sources[-1] == source
        case.call("_gl_compile_shader", shader)
        if failure:
            assert exits == [1] and not compiled
        else:
            assert not exits and compiled[-1] == ("binary" if offline else "source")
        case.call("bridge_gl_delete_shader", shader)
        assert deleted[-1] == shader
        assert not platform.allocations and not platform.handles
    assert len(dumps) == iterations


def check_link(offline, linked):
    files = {"app0:/shaders/offline.enabled": b"VitaArc offline shaders 1\n"} if offline else {}
    case = LoaderCase(files)
    assert case.call("bridge_shader_enable_offline") == int(offline)
    calls, target, platform = case.calls, case.target, case.platform
    events = []

    def link():
        assert calls.reg("R0") == 42
        events.append("link")

    def status():
        assert calls.reg("R0") == 42 and calls.reg("R1") == 0x8b82
        calls.word(calls.reg("R2"), int(linked))
        events.append("status")

    def stop():
        assert calls.reg("R0") == 1
        events.append("stop")
        calls.machine.reg_write(registers.UC_ARM_REG_PC, calls.RETURN)
        return False

    for name, callback in (("glLinkProgram", link), ("glGetProgramiv", status),
                           ("sceKernelExitProcess", stop)):
        calls.register(target.symbols[name], callback)
    case.call("bridge_gl_link_program", 42)
    assert events == (["link"] if not offline else
                       ["link", "status"] if linked else ["link", "status", "stop"])
    assert not platform.allocations and not platform.handles


if __name__ == "__main__":
    for offline in (False, True):
        check(offline)
    for failure in ("missing", "invalid"):
        check(True, failure)
    for offline, linked in ((False, False), (True, True), (True, False)):
        check_link(offline, linked)
    print("Linked ARM shader cache passed: exact assembled source, online/offline dispatch, "
          "600 handle lifetimes, missing/invalid/link rejection, zero allocations and file handles.")
