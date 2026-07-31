"""Build the real vitaGL/GXM binary shader probe, without a runtime compiler.

Use the native Windows psp2cgc tool to compile resources/shaders/offline first. This script
only links a separate SELF, reusing the configured game's SDK library list.
"""

import shlex
import subprocess

from arm_fixture_build import root, sdk, compile_source


def main():
    output = root / "build/vita-shader-probe"
    output.mkdir(parents=True, exist_ok=True)
    obj = compile_source(root / "tests/vita_shader_probe.c", output / "probe.o")
    objects = [obj]
    for name in ("pixel_store", "buffers"):
        objects.append(compile_source(root / f"src/bridges/opengl/{name}.c", output / (name + ".o")))
    command = shlex.split((root / "build/CMakeFiles/VitaArc.dir/link.txt").read_text())
    libraries = command[command.index("-Wl,--whole-archive"):]
    executable = output / "shader-probe.elf"
    subprocess.run([str(sdk / "bin/arm-vita-eabi-g++"), "-Wl,-q", *objects,
                    *libraries, "-o", str(executable)], check=True)
    velf = output / "shader-probe.velf"
    subprocess.run([str(sdk / "bin/vita-elf-create"), str(executable), str(velf)], check=True)
    subprocess.run([str(sdk / "bin/vita-make-fself"), "-c", str(velf),
                    str(output / "shader-probe.self")], check=True)
    print(output / "shader-probe.self")


if __name__ == "__main__":
    main()
