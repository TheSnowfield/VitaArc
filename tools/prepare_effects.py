"""Prepare Arcaea's packaged GLSL effects for the offline Vita shader compiler."""

import argparse
import json
from pathlib import Path, PurePosixPath
import subprocess
import shutil
import zipfile

from hash_shader import hash_shader


def shader_prefix(stage):
    precision = "highp" if stage == "vertex" else "mediump"
    header = f"precision {precision} float;\n precision {precision} int;\n"
    for kind, name in (("mat4", "PMatrix"), ("mat4", "MVMatrix"), ("mat4", "MVPMatrix"),
                       ("mat3", "NormalMatrix"), ("vec4", "Time"), ("vec4", "SinTime"),
                       ("vec4", "CosTime"), ("vec4", "Random01")):
        header += f"uniform {kind} CC_{name};\n"

    for index in range(4):
        header += f"uniform sampler2D CC_Texture{index};\n"

    return (header + "//CC INCLUDES END\n\n\n#define guy_empty\n").encode()


def prepare(apk_path, translator, output):
    output.mkdir(parents=True, exist_ok=True)
    root = Path(__file__).resolve().parent.parent
    shutil.copy2(root / "resources/shaders/offline/basic.frag.cg", output / "cocos-alpha.frag.cg")
    manifest = {"format": 1, "shaders": [{
        "hash": "b294b2f0f14aa455", "stage": "fragment", "source": "cocos-alpha.frag.cg", "matrices": [],
        "defines": {"VITAARC_FRAGMENT_MODE": "4", "CC_alpha_value": "CCAlphaValue"},
    }]}
    with zipfile.ZipFile(apk_path) as apk:
        for name in sorted(apk.namelist()):
            path = PurePosixPath(name)
            if path.parent != PurePosixPath("assets/img") or path.suffix not in (".fsh", ".vsh"):
                continue

            # This effect needs an Android Surface/MediaCodec texture. The Vita
            # backend reports unsupported video formats before it can be used.
            if path.stem == "texture_external_oes":
                continue

            stage = "vertex" if path.suffix == ".vsh" else "fragment"
            source = shader_prefix(stage) + apk.read(name)
            digest = f"{hash_shader(source):016x}"
            glsl = output / f"{path.stem}.{stage}.glsl"
            cg = output / f"{path.stem}.{stage}.cg"
            glsl.write_bytes(source)
            subprocess.run([str(translator), str(glsl), str(cg), stage], check=True)
            manifest["shaders"].append({
                "hash": digest, "stage": stage, "source": cg.name,
                "matrices": ["CC_MVPMatrix"] if stage == "vertex" else [],
            })
            print(f"{name}: {digest}", flush=True)

    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apk", required=True, type=Path)
    parser.add_argument("--translator", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    prepare(args.apk.resolve(), args.translator.resolve(), args.output.resolve())


if __name__ == "__main__":
    main()
