"""Link a separate synthetic-panel real-game touch probe, preserving normal eboot."""
import argparse
import shlex
import subprocess
from arm_fixture_build import root, sdk, compile_source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sqlite", action="store_true")
    args = parser.parse_args()
    name = "sqlite-probe" if args.sqlite else "touch-probe"
    output = root / f"build/vita-{name}"
    output.mkdir(parents=True, exist_ok=True)
    obj = compile_source(root / ("tests/vita_sqlite_probe.c" if args.sqlite else "tests/vita_touch_probe.c"), output / "probe.o",
                         includes=(root / "src",))
    configured = shlex.split((root / "build/CMakeFiles/VitaArc.dir/link.txt").read_text())
    objects = [str(root / "build" / arg) for arg in configured
               if arg.endswith(".obj") and not arg.endswith("/src/init.c.obj")]
    libraries = configured[configured.index("-Wl,--whole-archive"):]
    executable = output / f"{name}.elf"
    wrappers = [] if args.sqlite else ["-Wl,--wrap=sceTouchPeek"]
    subprocess.run([str(sdk / "bin/arm-vita-eabi-g++"), "-Wl,-q",
                    *wrappers, obj, *objects, *libraries,
                    "-o", str(executable)], check=True)
    velf = output / f"{name}.velf"
    subprocess.run([str(sdk / "bin/vita-elf-create"), str(executable), str(velf)], check=True)
    subprocess.run([str(sdk / "bin/vita-make-fself"), "-c", str(velf),
                    str(output / f"{name}.self")], check=True)
    print(output / f"{name}.self")


if __name__ == "__main__":
    main()
