"""Link a bounded real-game lifecycle SELF without replacing normal eboot/VPK."""

import argparse
import shlex
import subprocess

from arm_fixture_build import root, sdk, compile_source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--soak", action="store_true", help="6600 frames and ten pause/resume cycles")
    args = parser.parse_args()
    name = "lifecycle-soak" if args.soak else "lifecycle-probe"
    output = root / f"build/vita-{name}"
    output.mkdir(parents=True, exist_ok=True)
    obj = compile_source(root / "tests/vita_lifecycle_probe.c", output / "probe.o",
                         includes=(root / "src",),
                         defines=("VITAARC_LIFECYCLE_FRAMES=6600",) if args.soak else ())
    configured = shlex.split((root / "build/CMakeFiles/VitaArc.dir/link.txt").read_text())
    objects = [str(root / "build" / arg) for arg in configured
               if arg.endswith(".obj") and not arg.endswith("/src/init.c.obj")]
    libraries = configured[configured.index("-Wl,--whole-archive"):]
    executable = output / f"{name}.elf"
    subprocess.run([str(sdk / "bin/arm-vita-eabi-g++"), "-Wl,-q", obj, *objects,
                    *libraries, "-o", str(executable)], check=True)
    velf = output / f"{name}.velf"
    subprocess.run([str(sdk / "bin/vita-elf-create"), str(executable), str(velf)], check=True)
    subprocess.run([str(sdk / "bin/vita-make-fself"), "-c", str(velf),
                    str(output / f"{name}.self")], check=True)
    print(output / f"{name}.self")


if __name__ == "__main__":
    main()
