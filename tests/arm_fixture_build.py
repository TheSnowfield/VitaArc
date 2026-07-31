"""Small shared VitaSDK build helpers for standalone ARM fixtures."""

import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parent.parent
sdk = Path(os.environ.get("VITASDK", "/usr/local/vitasdk"))
compiler = str(sdk / "bin/arm-vita-eabi-gcc")


def compile_source(source, output, mode="-mthumb", defines=(), includes=()):
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([compiler, "-O2", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
                    mode, *["-D" + value for value in defines],
                    "-ffunction-sections", "-fdata-sections",
                    "-fno-unwind-tables", "-fno-asynchronous-unwind-tables",
                    "-I", str(root / "src/bridges"), "-I", str(output.parent),
                    *[argument for path in includes for argument in ("-I", str(path))],
                    "-c", str(source), "-o", str(output)], check=True)
    return str(output)


def link_fixture(objects, output, entry, exports=()):
    subprocess.run([compiler, "-nostdlib", "-static", "-Wl,--gc-sections", "-Wl,-Ttext=0x10000",
                    "-Wl,-e," + entry, *["-Wl,--undefined=" + name for name in exports],
                    *objects, "-lc", "-lgcc", "-o", str(output)], check=True)
