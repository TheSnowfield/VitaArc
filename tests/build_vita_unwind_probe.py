"""Build a real throw/catch/RAII probe using production loader and game runtime.

No normal eboot or VPK is changed. Run unwind-probe.self in the private installed
VitaArc app after copying unwind-fixture.so beside it. Neither game constructors
nor Cocos/native renderer initialization is needed for this runtime fixture.
"""

import shlex
import subprocess
import struct

from arm_fixture_build import root, sdk, compile_source


def personality_profile(output):
    raw = (root / "build/audit-so/lib/armeabi-v7a/libcocos2dcpp.so").read_bytes()
    elf = struct.unpack_from("<16sHHIIIIIHHHHHH", raw)
    programs = [struct.unpack_from("<8I", raw, elf[5] + index * elf[9]) for index in range(elf[10])]

    def offset(address, size):
        for p in programs:
            if p[0] == 1 and address >= p[2] and address + size <= p[2] + p[4]:
                return p[1] + address - p[2]
        raise ValueError("EHABI pointer outside file-backed load segments")

    def prel31(value, address):
        value &= 0x7fffffff
        return address + value - (0x80000000 if value & 0x40000000 else 0)

    index = next(p for p in programs if p[0] == 0x70000001)
    personalities = set()
    for position in range(0, index[4], 8):
        descriptor = struct.unpack_from("<I", raw, index[1] + position + 4)[0]
        if descriptor == 1 or descriptor & 0x80000000:
            continue
        table = prel31(descriptor, index[2] + position + 4)
        header = struct.unpack_from("<I", raw, offset(table, 4))[0]
        if not header & 0x80000000:
            personalities.add(prel31(header, table))
    if len(personalities) != 1:
        raise ValueError("Expected one generic game C++ personality in EHABI metadata")
    personality = personalities.pop()
    if not any(p[0] == 1 and p[6] & 1 and p[2] <= (personality & ~1) < p[2] + p[4] for p in programs):
        raise ValueError("Personality is not executable")
    start = offset(personality & ~1, 64)
    profile = ",".join(f"0x{value:02x}" for value in raw[start:start + 64])
    (output / "game_unwind_profile.h").write_text(
        f"#define GAME_PERSONALITY_RVA 0x{personality:x}u\n"
        f"static const unsigned char game_personality_profile[] = {{{profile}}};\n")


def main():
    output = root / "build/vita-unwind-probe"
    output.mkdir(parents=True, exist_ok=True)
    personality_profile(output)
    cxx = str(sdk / "bin/arm-vita-eabi-g++")
    subprocess.run([cxx, "-O1", "-Wall", "-Wextra", "-Werror", "-mthumb", "-fPIC",
                    "-fexceptions", "-funwind-tables", "-nostdlib", "-shared",
                    "-Wl,--hash-style=sysv,-z,max-page-size=4096",
                    str(root / "tests/test_arm_exception_fixture.cpp"),
                    "-o", str(output / "unwind-fixture.so")], check=True)
    obj = compile_source(root / "tests/vita_unwind_probe.c", output / "probe.o",
                         includes=(root / "src",))
    configured = shlex.split((root / "build/CMakeFiles/VitaArc.dir/link.txt").read_text())
    objects = [str(root / "build" / arg) for arg in configured
               if arg.endswith(".obj") and not arg.endswith("/src/init.c.obj")]
    libraries = configured[configured.index("-Wl,--whole-archive"):]
    executable = output / "unwind-probe.elf"
    subprocess.run([cxx, "-Wl,-q", obj, *objects, *libraries, "-o", str(executable)], check=True)
    velf = output / "unwind-probe.velf"
    subprocess.run([str(sdk / "bin/vita-elf-create"), str(executable), str(velf)], check=True)
    subprocess.run([str(sdk / "bin/vita-make-fself"), "-c", str(velf),
                    str(output / "unwind-probe.self")], check=True)
    print(output / "unwind-probe.self")


if __name__ == "__main__":
    main()
