"""Build the vitaGL GLSL translator for a Linux host from a local source tree."""

import argparse
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    source = args.source.resolve() / "source"
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    host = Path(__file__).resolve().parent / "shader_translator"

    # Replace target-only includes in generated build copies. Translation and
    # preprocessing remain the implementations from the supplied vitaGL tree.
    utils = (source / "utils/glsl_utils.c").read_text()
    utils = utils.replace('#include "../shared.h"', '#include "host.h"')
    (out / "glsl_utils.c").write_text(utils)
    preprocessor = (source / "utils/preprocessor/preprocessor.cpp").read_text()
    preprocessor = preprocessor.replace("#include <vitasdk.h>", "")
    preprocessor = preprocessor.replace('#include "../debug_utils.h"', "")
    (out / "preprocessor.cpp").write_text(preprocessor)
    shutil.copy2(host / "host.h", out / "host.h")
    (out / "vitaGL.h").write_text('#include "host.h"\n')

    includes = ["-I", str(out), "-I", str(source / "utils"),
                "-I", str(source / "utils/preprocessor"), "-include", str(out / "host.h")]
    objects = []
    for name, path in (("glsl_utils", out / "glsl_utils.c"), ("main", host / "main.c")):
        target = out / (name + ".o")
        subprocess.run(["cc", "-std=gnu11", "-O2", *includes, "-c", str(path), "-o", str(target)], check=True)
        objects.append(str(target))

    subprocess.run(["c++", "-O2", *includes, *objects, str(out / "preprocessor.cpp"),
                    str(source / "utils/preprocessor/expression.cpp"), "-o", str(out / "translate")], check=True)
    print(out / "translate")


if __name__ == "__main__":
    main()
