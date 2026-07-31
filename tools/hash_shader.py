#!/usr/bin/env python3
"""Print the runtime VitaArc shader hash for an exact source file."""

from __future__ import annotations

import argparse
from pathlib import Path


FNV_OFFSET = 14695981039346656037
FNV_PRIME = 1099511628211
MASK_64 = (1 << 64) - 1


def hash_shader(source: bytes) -> int:
    value = FNV_OFFSET
    for byte in source:
        value ^= byte
        value = (value * FNV_PRIME) & MASK_64
    return value


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    args = parser.parse_args()
    digest = hash_shader(args.source.read_bytes())
    print(f"{digest:016x}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
