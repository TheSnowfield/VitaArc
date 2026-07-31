"""Build separate native UserDefault writer/reader using production bridge code."""

import shlex
import subprocess

from arm_fixture_build import root, sdk, compile_source


def main():
    output = root / "build/vita-preferences-probe"
    output.mkdir(parents=True, exist_ok=True)
    configured = shlex.split((root / "build/CMakeFiles/VitaArc.dir/link.txt").read_text())
    objects = [str(root / "build" / arg) for arg in configured
               if arg.endswith(".obj") and not arg.endswith("/src/init.c.obj")]
    libraries = configured[configured.index("-Wl,--whole-archive"):]
    for mode in ("write", "read"):
        name = f"preferences-{mode}"
        obj = compile_source(root / "tests/vita_preferences_probe.c", output / f"{name}.o",
                             includes=(root / "src",),
                             defines=("VITAARC_PREF_WRITE",) if mode == "write" else ())
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
