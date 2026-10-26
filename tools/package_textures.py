#!/usr/bin/env python3
"""Resample game textures and embed source dimensions in the image itself."""

from __future__ import annotations

import io
import struct
import zlib
from pathlib import PurePosixPath

IMAGE_SUFFIXES = {".png", ".jpg", ".jpeg"}
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
SIZE_RECORD = struct.Struct(">4s4H")


def small_size(width: int, height: int) -> tuple[int, int]:
    """Keep dots/small art; halve large art, then fit a 960px long edge.

    Keep in sync with vita_texture_small_size in texture_size.c. A narrow
    texture is only reduced if it exceeds the GPU's 4096px dimension limit.
    """
    if not 0 < width <= 16384 or not 0 < height <= 16384:
        raise ValueError(f"unsupported texture dimensions: {width}x{height}")
    limit = 4096 if min(width, height) < 64 else 960
    if min(width, height) >= 64 and max(width, height) >= 256:
        width, height = (width + 1) // 2, (height + 1) // 2
    while max(width, height) > limit:
        width, height = (width + 1) // 2, (height + 1) // 2
    return width, height


def image_api():
    try:
        from PIL import Image
    except ImportError as error:
        raise ValueError("Texture resampling requires: python -m pip install -r tools/requirements.txt") from error
    return Image


def resample_texture(content: bytes, name: str) -> bytes:
    """Return unchanged bytes below the threshold; otherwise encode a new image.

    PNG uses a private ancillary vaRc chunk immediately after IHDR. JPEG uses
    APP15 immediately after SOI. Both hold VTX1 and four big-endian uint16s:
    source width/height and packed width/height. No sidecar file is needed.
    """
    suffix = PurePosixPath(name).suffix.lower()
    if suffix not in IMAGE_SUFFIXES:
        return content
    Image = image_api()
    with Image.open(io.BytesIO(content)) as source:
        width, height = source.size
        size = small_size(width, height)
        if size == source.size:
            return content
        if getattr(source, "n_frames", 1) != 1:
            raise ValueError(f"animated texture cannot be resampled: {name}")
        # Some official APKs contain PNG data under a .jpg name. Cocos detects
        # images by their bytes, so preserve the decoded format as well.
        expected = source.format
        if expected not in ("PNG", "JPEG"):
            raise ValueError(f"unsupported texture format {expected}: {name}")
        alpha = "A" in source.getbands() or "transparency" in source.info
        image = source.convert("RGBA" if alpha else "RGB")
        if alpha:
            image = image.convert("RGBa")
        image = image.resize(size, Image.Resampling.LANCZOS)
        if alpha:
            image = image.convert("RGBA")
        output = io.BytesIO()
        if expected == "PNG":
            image.save(output, format="PNG", compress_level=6)
        else:
            image.convert("RGB").save(output, format="JPEG", quality=92, subsampling=0)
    encoded = output.getvalue()
    record = SIZE_RECORD.pack(b"VTX1", width, height, *size)
    if expected == "PNG":
        chunk = b"vaRc" + record
        chunk = struct.pack(">I", len(record)) + chunk + struct.pack(">I", zlib.crc32(chunk))
        return encoded[:33] + chunk + encoded[33:]
    return encoded[:2] + b"\xff\xef" + struct.pack(">H", len(record) + 2) + record + encoded[2:]
