"""Compile explicit Cg translations to the installed vitaGL binary format.

Run on the compiler's host (Windows for psp2cgc.exe). No SDK is downloaded or
redistributed. Only hashes in the checked-in manifest are supported; this is
not a general GLSL translator. The original-source directory is optional, and
when supplied must contain a matching exact dump for each manifest hash.
"""

import argparse
import json
from pathlib import Path
import re
import struct
import subprocess

from hash_shader import hash_shader

ROOT = Path(__file__).resolve().parent.parent


def serialize_shader(raw, stage, matrices):
    if len(raw) < 156 or len(raw) > 1024 * 1024 or raw[:4] != b"GXP\0":
        raise ValueError("invalid GXP header")
    length = struct.unpack_from("<I", raw, 8)[0]
    flags = struct.unpack_from("<I", raw, 20)[0]
    if length < 156 or not 0 <= len(raw) - length <= 3:
        raise ValueError("invalid GXP length")
    if stage not in ("vertex", "fragment") or flags & 1 != (stage == "fragment"):
        raise ValueError("wrong GXP stage")
    count, relative = struct.unpack_from("<II", raw, 0x24)
    table = 0x28 + relative
    if table > length or count > (length - table) // 16:
        raise ValueError("invalid GXP parameter table")
    parameters = {}
    for index in range(count):
        entry = table + index * 16
        offset = struct.unpack_from("<I", raw, entry)[0]
        start = entry + offset
        end = raw.find(b"\0", start, length)
        if start >= length or end < 0:
            raise ValueError("invalid GXP parameter name")
        name = raw[start:end].decode("ascii")
        if name in parameters:
            raise ValueError("duplicate GXP parameter name")
        parameters[name] = (index, raw[entry + 4] & 15)
    if len(matrices) > 64 or len(set(matrices)) != len(matrices):
        raise ValueError("invalid matrix list")
    indices = []
    for name in matrices:
        if name not in parameters:
            raise ValueError(f"matrix absent from compiled program: {name}")
        index, category = parameters[name]
        if category != 1:
            raise ValueError("matrix is not a uniform")
        indices.append(index)
    return struct.pack("<I", len(indices)) + b"".join(
        struct.pack("<I", index) for index in indices) + raw


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "build/offline-shaders")
    parser.add_argument("--manifest", type=Path,
                        default=ROOT / "resources/shaders/offline/manifest.json")
    parser.add_argument("--originals", type=Path)
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text())
    if manifest["format"] != 1:
        raise ValueError("unsupported offline shader manifest")
    args.output.mkdir(parents=True, exist_ok=True)
    seen = set()
    for shader in manifest["shaders"]:
        digest = shader["hash"]
        stage = shader["stage"]
        if not re.fullmatch(r"[0-9a-f]{16}", digest) or digest in seen:
            raise ValueError("invalid/duplicate shader hash")
        seen.add(digest)
        suffix = {"vertex": "vert", "fragment": "frag"}[stage]
        if args.originals:
            original = (args.originals / f"{digest}.{suffix}.glsl").read_bytes()
            if hash_shader(original) != int(digest, 16):
                raise ValueError(f"original shader hash mismatch: {digest}")
        raw_path = args.output / f"{digest}.raw.gxp"
        profile = "sce_vp_psp2" if stage == "vertex" else "sce_fp_psp2"
        defines = [f"-D{key}={value}" for key, value in shader.get("defines", {}).items()]
        subprocess.run([str(args.compiler), "-profile", profile, *defines,
                        "-I" + str(args.manifest.parent),
                        str(args.manifest.parent / shader["source"]),
                        "-o", str(raw_path)], check=True)
        binary = serialize_shader(raw_path.read_bytes(), stage, shader["matrices"])
        (args.output / f"{digest}.vgxp").write_bytes(binary)
        print(f"{digest} {stage}: {len(binary)} bytes")
    (args.output / "offline.enabled").write_bytes(b"VitaArc offline shaders 1\n")


if __name__ == "__main__":
    main()
