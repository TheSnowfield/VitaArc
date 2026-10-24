#!/usr/bin/env python3
"""Prepare smaller Vita textures with their original Cocos layout dimensions."""

from __future__ import annotations

import argparse
import io
import json
import plistlib
import struct
import zipfile
import zlib
from dataclasses import dataclass
from pathlib import Path, PurePosixPath


texture_map = "vitaarc-textures.bin"
texture_report = "vitaarc-textures.json"
map_magic = b"VARCTX1\0"
map_header = struct.Struct("<8sII")
map_row = struct.Struct("<256s4H")
image_suffixes = {".png", ".jpg", ".jpeg"}
protected_roots = ("assets/Default/", "assets/particle/")


@dataclass
class Texture:
    path: str
    width: int
    height: int
    small_width: int
    small_height: int
    source_crc: int
    packed_crc: int = 0

    def row(self) -> bytes:
        name = self.path.encode("utf-8")
        if len(name) >= 256:
            raise ValueError(f"texture path is too long: {self.path}")

        return map_row.pack(
            name, self.width, self.height, self.small_width, self.small_height
        )


def image_api():
    try:
        from PIL import Image
    except ImportError as error:
        raise ValueError(
            "Vita textures require Pillow; install tools/requirements.txt"
        ) from error

    return Image


def atlas_paths(apk: zipfile.ZipFile) -> set[str]:
    paths: set[str] = set()
    for name in apk.namelist():
        if not name.startswith("assets/") or not name.endswith(".plist"):
            continue

        data = plistlib.loads(apk.read(name))
        if not isinstance(data, dict) or "frames" not in data:
            continue

        meta = data.get("metadata", {})
        paths.add(str(PurePosixPath(name).with_suffix(".png")))
        for key in ("textureFileName", "realTextureFileName"):
            value = meta.get(key)
            if not isinstance(value, str) or not value:
                continue

            path = PurePosixPath(name).parent / value
            paths.add(str(path))

    return paths


def small_size(width: int, height: int) -> tuple[int, int]:
    if min(width, height) < 64 and max(width, height) > 4096:
        while max(width, height) > 4096:
            width = (width + 1) // 2
            height = (height + 1) // 2

        return width, height

    if min(width, height) < 64 or max(width, height) < 256:
        return width, height

    width = (width + 1) // 2
    height = (height + 1) // 2
    while max(width, height) > 960:
        width = (width + 1) // 2
        height = (height + 1) // 2

    return width, height


class TexturePlan:
    def __init__(self, apk: zipfile.ZipFile, profile: str = "original"):
        if profile not in ("original", "vita"):
            raise ValueError(f"unknown texture profile: {profile}")

        self.apk = apk
        self.profile = profile
        self.entries: dict[str, Texture] = {}
        self.rgba_before = 0
        self.rgba_after = 0
        if profile == "original":
            return

        Image = image_api()
        atlases = atlas_paths(apk)
        for info in apk.infolist():
            name = info.filename
            path = PurePosixPath(name)
            if not name.startswith("assets/") or path.suffix.lower() not in image_suffixes:
                continue

            with apk.open(info) as stream, Image.open(stream) as image:
                width, height = image.size

            if not 0 < width <= 16384 or not 0 < height <= 16384:
                raise ValueError(f"unsupported texture dimensions: {name}")

            self.rgba_before += width * height * 4
            size = small_size(width, height)
            if name in atlases or name.startswith(protected_roots) or name.endswith(".9.png"):
                size = width, height
                while max(size) > 4096:
                    size = (size[0] + 1) // 2, (size[1] + 1) // 2

            self.rgba_after += size[0] * size[1] * 4
            if size == (width, height):
                continue

            entry = Texture(name, width, height, *size, info.CRC)
            if "\\" in name or any(part in ("", ".", "..") for part in name.split("/")):
                raise ValueError(f"unsafe texture path: {name}")

            if name in self.entries:
                raise ValueError(f"duplicate texture path: {name}")

            entry.row()
            self.entries[name] = entry

        if len(self.entries) > 4096:
            raise ValueError("too many texture records")

    def rewrite(self, name: str) -> bytes | None:
        entry = self.entries.get(name)
        if not entry:
            return None

        Image = image_api()
        size = entry.small_width, entry.small_height
        with self.apk.open(name) as stream, Image.open(stream) as image:
            if getattr(image, "n_frames", 1) != 1:
                raise ValueError(f"animated texture cannot be resized: {name}")

            alpha = "A" in image.getbands() or "transparency" in image.info
            image = image.convert("RGBA" if alpha else "RGB")
            if alpha:
                image = image.convert("RGBa")

            image = image.resize(size, Image.Resampling.LANCZOS)
            if alpha:
                image = image.convert("RGBA")

            output = io.BytesIO()
            if PurePosixPath(name).suffix.lower() == ".png":
                image.save(output, format="PNG", compress_level=6)
            else:
                image.convert("RGB").save(output, format="JPEG", quality=92, subsampling=0)

        content = output.getvalue()
        entry.packed_crc = zlib.crc32(content)
        return content

    def manifest(self) -> bytes:
        rows = [self.entries[name].row() for name in sorted(self.entries)]
        return map_header.pack(map_magic, len(rows), map_row.size) + b"".join(rows)

    def report(self) -> bytes:
        rows = []
        for name in sorted(self.entries):
            entry = self.entries[name]
            rows.append({
                "path": name,
                "layout": [entry.width, entry.height],
                "pixels": [entry.small_width, entry.small_height],
                "source_crc": entry.source_crc,
                "packed_crc": entry.packed_crc,
            })

        report = {
            "format": 1,
            "profile": self.profile,
            "rgba_bytes_before": self.rgba_before,
            "rgba_bytes_after": self.rgba_after,
            "textures": rows,
        }
        return (json.dumps(report, indent=2, ensure_ascii=False) + "\n").encode("utf-8")

    def write_metadata(self, output: zipfile.ZipFile, prefix: str) -> None:
        if self.profile == "original":
            return

        output.writestr(f"{prefix}/{texture_map}", self.manifest())
        output.writestr(texture_report, self.report())

    def describe(self) -> None:
        if self.profile == "original":
            return

        mib = 1024 * 1024
        print(
            f"Vita textures: {len(self.entries)} resized; "
            f"RGBA estimate {self.rgba_before / mib:.1f} -> "
            f"{self.rgba_after / mib:.1f} MiB (all image assets, not live VRAM)",
            flush=True,
        )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apk", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--self", dest="self_path", required=True, type=Path)
    args = parser.parse_args()

    output = args.output.resolve()
    partial = output.with_suffix(output.suffix + ".partial")
    if output.exists() or partial.exists():
        parser.error(f"preserve existing output: {output}")

    output.parent.mkdir(parents=True, exist_ok=True)
    try:
        with zipfile.ZipFile(args.apk) as apk:
            plan = TexturePlan(apk, "vita")
            plan.describe()
            with zipfile.ZipFile(partial, "w", compression=zipfile.ZIP_DEFLATED) as patch:
                patch.write(args.self_path, "eboot.bin")
                for index, name in enumerate(sorted(plan.entries), 1):
                    patch.writestr(f"package/{name}", plan.rewrite(name))
                    if index % 25 == 0 or index == len(plan.entries):
                        print(f"  resized {index}/{len(plan.entries)} textures", flush=True)

                plan.write_metadata(patch, "package")

        partial.replace(output)
    except Exception:
        partial.unlink(missing_ok=True)
        raise

    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
