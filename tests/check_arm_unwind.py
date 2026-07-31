"""Check actual linked EHABI index lookup, not a modeled lookup implementation.

The assembly DSO supplies a real linker-generated ARM exidx table. Vita memory,
I/O and allocation endpoints are modeled by the loader fixture. Optional game
validation compares every returned table byte against an independent ELF parse;
neither that comparison nor this cantunwind fixture proves C++ catch behavior.
"""

import argparse
from pathlib import Path
import struct
import subprocess

from arm_fixture_build import root, compiler
from check_arm_loader import LoaderCase


def headers(raw):
    elf = struct.unpack_from("<16sHHIIIIIHHHHHH", raw)
    programs = [(elf[5] + index * elf[9], struct.unpack_from("<8I", raw, elf[5] + index * elf[9]))
                for index in range(elf[10])]
    sections = [(elf[6] + index * elf[11], struct.unpack_from("<10I", raw, elf[6] + index * elf[11]))
                for index in range(elf[12])]
    return programs, sections


def lookup(case, pc, expected, count):
    output = case.calls.SCRATCH + 0x800
    case.calls.word(output, 0xdeadbeef)
    assert case.call("bridge_gnu_unwind_find_exidx", pc, output) == expected
    assert case.word(output) == count


def check_fixture(raw):
    case = LoaderCase({"app0:/unwind.so": raw})
    library = case.load("app0:/unwind.so")
    assert library, case.platform.messages
    function = case.export(library, "fixture_unwind_function")
    data = case.export(library, "fixture_unwind_data")
    bias = 0x98000000
    programs, sections = headers(raw)
    ph_offset, exidx = next((offset, p) for offset, p in programs if p[0] == 0x70000001)
    sh_offset, sh = next((offset, s) for offset, s in sections if s[1] == 0x70000001)
    pointer, count = bias + exidx[2], exidx[4] // 8
    assert count and sh[3] == exidx[2] and sh[5] == exidx[4]
    lookup(case, function, pointer, count)
    lookup(case, function & ~1, pointer, count)
    assert bytes(case.calls.machine.mem_read(pointer, exidx[4])) == raw[exidx[1]:exidx[1] + exidx[4]]
    lookup(case, data, 0, 0)
    lookup(case, 0xffffffff, 0, 0)
    assert case.call("bridge_gnu_unwind_find_exidx", function, 0) == 0
    # The metadata image is writable; exercise invalid tables without remapping
    # actual code or tricking the native index lookup endpoint.
    image = next(base for base, size, name in case.platform.blocks.values() if name == "elf_image")
    for field, value in ((8, exidx[2] + 1), (16, exidx[4] - 1), (20, exidx[5] + 8),
                          (8, 0x12340000), (16, 0)):
        case.calls.word(image + ph_offset + field, value)
        lookup(case, function, 0, 0)
        case.calls.machine.mem_write(image + ph_offset, raw[ph_offset:ph_offset + 32])
    # Section-header fallback when no PT_ARM_EXIDX is advertised.
    case.calls.word(image + ph_offset, 0)
    lookup(case, function, bias + sh[3], sh[5] // 8)
    case.calls.word(image + sh_offset + 20, sh[5] - 1)
    lookup(case, function, 0, 0)
    case.calls.machine.mem_write(image + sh_offset, raw[sh_offset:sh_offset + 40])
    case.calls.machine.mem_write(image + ph_offset, raw[ph_offset:ph_offset + 32])
    # Native C bridge frames use the executable's own table.
    native_start = case.target.symbols["__exidx_start"]
    native_count = (case.target.symbols["__exidx_end"] - native_start) // 8
    lookup(case, case.target.symbols["bridge_gnu_unwind_find_exidx"], native_start, native_count)
    clone = case.call("loader_clone_handle", library)
    case.call("loader_free_library", library)
    lookup(case, function, pointer, count)
    case.call("loader_free_library", clone)
    lookup(case, function, 0, 0)
    case.clean()


def check_game(path):
    raw = path.read_bytes()
    case = LoaderCase({"app0:/game.so": raw})
    library = case.load("app0:/game.so")
    assert library, case.platform.messages
    programs, _ = headers(raw)
    _, exidx = next((offset, p) for offset, p in programs if p[0] == 0x70000001)
    pointer, count = 0x98000000 + exidx[2], exidx[4] // 8
    lookup(case, case.export(library, "JNI_OnLoad"), pointer, count)
    lookup(case, 0x98000000 + 0x5f25ad, pointer, count)
    assert bytes(case.calls.machine.mem_read(pointer, exidx[4])) == raw[exidx[1]:exidx[1] + exidx[4]]
    case.call("loader_free_library", library)
    case.clean()
    print(f"Actual-game lookup matched {count} EHABI entries ({exidx[4]} bytes), then unloaded cleanly.", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path)
    args = parser.parse_args()
    output = root / "build/arm-unwind"
    output.mkdir(parents=True, exist_ok=True)
    path = output / "unwind.so"
    subprocess.run([compiler, "-nostdlib", "-shared", "-Wl,--hash-style=sysv,-z,max-page-size=4096",
                    str(root / "tests/test_arm_unwind_fixture.S"), "-o", str(path)], check=True)
    check_fixture(path.read_bytes())
    print("Linked ARM exidx passed: real PT/section tables, Thumb PCs, native fallback, malformed bounds, "
          "null/count reset, cloned lifetime and post-unload miss.", flush=True)
    if args.game:
        check_game(args.game)


if __name__ == "__main__":
    main()
