#!/usr/bin/env python3
"""Validate the distributable VPK and reject bundled game resources."""

import argparse
import re
import zipfile
from pathlib import Path


_ALLOWED_FILES = {
    "eboot.bin",
    "sce_sys/param.sfo",
    "sce_sys/icon0.png",
    "sce_sys/livearea/contents/background.png",
    "sce_sys/livearea/contents/startup.png",
    "sce_sys/livearea/contents/template.xml",
    "shaders/offline.enabled",
}


def check_vpk(path):
    """Raise ValueError for malformed archives or unexpected payloads."""
    with zipfile.ZipFile(path) as archive:
        names = set()
        for entry in archive.infolist():
            name = entry.orig_filename
            if "\\" in name or "\0" in name or name.startswith("/") or ".." in name.split("/"):
                raise ValueError(f"unsafe VPK path: {name}")
            if name in names:
                raise ValueError(f"duplicate VPK path: {name}")
            names.add(name)
            if entry.is_dir():
                if name not in {
                    "sce_sys/", "sce_sys/livearea/", "sce_sys/livearea/contents/",
                    "licenses/", "shaders/",
                }:
                    raise ValueError(f"unexpected VPK directory: {name}")
                continue
            allowed = name in _ALLOWED_FILES or re.fullmatch(
                r"(?:licenses/[^/]+\.txt|shaders/[^/]+\.(?:cg|vgxp))", name
            )
            if not allowed:
                raise ValueError(f"unexpected VPK content: {name}")
        for required in ("eboot.bin", "sce_sys/param.sfo"):
            if required not in names or archive.getinfo(required).file_size == 0:
                raise ValueError(f"missing or empty VPK file: {required}")
        corrupt = archive.testzip()
        if corrupt:
            raise ValueError(f"corrupt VPK file: {corrupt}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("vpk", type=Path)
    args = parser.parse_args()
    try:
        check_vpk(args.vpk)
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        parser.exit(1, f"VPK verification failed: {error}\n")
    print(f"Verified {args.vpk}: application files only")


if __name__ == "__main__":
    main()
