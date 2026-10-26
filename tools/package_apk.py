#!/usr/bin/env python3
"""Prepare an Arcaea APK as version/<version>/ for ux0:data/vitaarc/."""

from __future__ import annotations

import argparse
import re
import shutil
import stat
import struct
import sys
import tempfile
import zipfile
from dataclasses import dataclass
from pathlib import Path

from apk_manifest import version_name
from package_textures import IMAGE_SUFFIXES, resample_texture

LIBRARY = "lib/armeabi-v7a/libcocos2dcpp.so"
COPY_BUFFER = 1024 * 1024


@dataclass(frozen=True)
class ApkCandidate:
    version: str
    path: Path


def version_from_filename(path: Path) -> str:
    match = re.fullmatch(r"arcaea[_-](.+)", path.stem, re.IGNORECASE)
    return match.group(1) if match else path.stem


def natural_sort_key(value: str):
    return tuple((1, int(part)) if part.isdigit() else (0, part.casefold())
                 for part in re.split(r"(\d+)", value))


def enumerate_apks(apk_dir: Path) -> list[ApkCandidate]:
    if not apk_dir.is_dir():
        raise ValueError(f"APK directory does not exist: {apk_dir}")
    candidates = sorted((ApkCandidate(version_from_filename(path), path)
                         for path in apk_dir.iterdir()
                         if path.is_file() and path.suffix.lower() == ".apk"),
                        key=lambda item: natural_sort_key(item.version))
    if not candidates:
        raise ValueError(f"no APK files found under: {apk_dir}")
    if len({item.version.casefold() for item in candidates}) != len(candidates):
        raise ValueError(f"duplicate APK version filenames under: {apk_dir}")
    return candidates


def print_candidates(candidates: list[ApkCandidate]) -> None:
    for index, item in enumerate(candidates, 1):
        print(f"  {index}. {item.version} ({item.path.name})")


def select_apk(candidates: list[ApkCandidate], requested: str | None) -> ApkCandidate:
    if requested:
        for item in candidates:
            if requested.casefold() in {item.version.casefold(), item.path.stem.casefold(), item.path.name.casefold()}:
                return item
        raise ValueError(f"APK version not found: {requested}")
    if len(candidates) == 1:
        return candidates[0]
    if not sys.stdin.isatty():
        raise ValueError("multiple APKs found; pass --version or --all")
    print_candidates(candidates)
    while True:
        answer = input(f"Select APK [1-{len(candidates)}]: ").strip()
        if answer.isdigit() and 1 <= int(answer) <= len(candidates):
            return candidates[int(answer) - 1]
        print("Invalid selection.", file=sys.stderr)


def safe_path(name: str) -> str:
    if not name or "\\" in name or ":" in name or "\0" in name or any(ord(c) < 32 for c in name):
        raise ValueError(f"unsafe archive path: {name!r}")
    if any(part in ("", ".", "..") or part.endswith((" ", ".")) for part in name.split("/")):
        raise ValueError(f"unsafe archive path: {name!r}")
    for part in name.split("/"):
        stem = part.split(".")[0].upper()
        if re.fullmatch(r"CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9]", stem) or any(c in '<>"|?*' for c in part):
            raise ValueError(f"unsupported archive path: {name!r}")
    return name


def retained_entry(name: str) -> bool:
    return name.startswith(("assets/", "obb/")) or name == LIBRARY


def game_entries(apk: zipfile.ZipFile) -> list[zipfile.ZipInfo]:
    entries = []
    names: set[str] = set()
    for info in apk.infolist():
        if info.is_dir():
            continue
        safe_path(info.orig_filename)
        if not retained_entry(info.filename):
            continue
        if info.compress_type not in (zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED):
            raise ValueError(f"unsupported ZIP compression: {info.filename}")
        if stat.S_ISLNK(info.external_attr >> 16):
            raise ValueError(f"symlink in APK: {info.filename}")
        key = info.filename.casefold()
        if key in names:
            raise ValueError(f"duplicate APK resource: {info.filename}")
        names.add(key)
        entries.append(info)
    if LIBRARY.casefold() not in names:
        raise ValueError(f"APK is missing {LIBRARY}")
    if not any(info.filename.startswith("assets/") for info in entries):
        raise ValueError("APK has no game assets")
    return entries


def check_library(apk: zipfile.ZipFile) -> None:
    with apk.open(LIBRARY) as stream:
        header = stream.read(52)
    if (len(header) != 52 or header[:7] != b"\x7fELF\x01\x01\x01" or
            struct.unpack_from("<HH", header, 16) != (3, 40)):
        raise ValueError("libcocos2dcpp.so must be a little-endian ARM32 ELF shared library")


def prepare_apk(apk_path: Path, output: Path, obb_paths=()) -> Path:
    """Publish a complete directory atomically; preserve all existing versions."""
    with zipfile.ZipFile(apk_path) as apk:
        version = version_name(apk.read("AndroidManifest.xml"))
        safe_path(version)
        entries = game_entries(apk)
        check_library(apk)
        external_obbs = []
        obb_names = {i.filename.casefold() for i in entries if i.filename.startswith("obb/")}
        for path in map(Path, obb_paths):
            if not path.is_file() or path.suffix.lower() != ".obb":
                raise ValueError(f"OBB file not found or not .obb: {path}")
            name = "obb/" + safe_path(path.name)
            if name.casefold() in obb_names:
                raise ValueError(f"duplicate OBB filename: {path.name}")
            obb_names.add(name.casefold())
            external_obbs.append((path, name))
        versions = output / "version"
        versions.mkdir(parents=True, exist_ok=True)
        destination = versions / version
        if destination.exists() or any(p.name.casefold() == version.casefold() for p in versions.iterdir()):
            raise FileExistsError(f"preserve existing version: {destination}")
        partial = Path(tempfile.mkdtemp(prefix=f".{version}-", dir=versions))
        if partial.resolve().parent != versions.resolve():
            raise ValueError(f"temporary directory escaped the output root: {partial}")
        resized = 0
        before = after = 0
        try:
            (partial / "assets").mkdir()
            (partial / "obb").mkdir()
            for index, info in enumerate(entries, 1):
                target = partial / info.filename
                target.parent.mkdir(parents=True, exist_ok=True)
                if info.filename.startswith("assets/") and target.suffix.lower() in IMAGE_SUFFIXES:
                    original = apk.read(info)
                    content = resample_texture(original, info.filename)
                    resized += content is not original
                    before += len(original)
                    after += len(content)
                    target.write_bytes(content)
                else:
                    with apk.open(info) as source, target.open("xb") as stream:
                        shutil.copyfileobj(source, stream, COPY_BUFFER)
                if index % 250 == 0 or index == len(entries):
                    print(f"  {version}: {index}/{len(entries)} files", flush=True)
            for path, name in external_obbs:
                shutil.copyfile(path, partial / name)
            (partial / "vitaarc-apk-version.txt").write_text(version + "\n", encoding="ascii")
            partial.rename(destination)
        except BaseException:
            if partial.resolve().parent != versions.resolve():
                raise RuntimeError(f"refusing cleanup outside the output root: {partial}") from None
            shutil.rmtree(partial)
            raise
    print(f"Prepared {destination}\n  {resized} textures resized; encoded images {before:,} -> {after:,} bytes")
    return destination


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--apk", type=Path)
    source.add_argument("--apk-dir", type=Path)
    parser.add_argument("--output", type=Path, help="destination root; creates version/<manifest-version>/ inside it")
    parser.add_argument("--version", help="select an APK filename/version from --apk-dir")
    parser.add_argument("--all", action="store_true", help="prepare every APK in --apk-dir")
    parser.add_argument("--list", action="store_true", help="list APK candidates and exit")
    parser.add_argument("--obb", type=Path, action="append", default=[], help="external OBB to copy unchanged (repeatable)")
    args = parser.parse_args()
    if (args.version or args.all or args.list) and not args.apk_dir:
        parser.error("--version, --all and --list require --apk-dir")
    if args.all and (args.version or args.obb):
        parser.error("--all cannot be combined with --version or --obb; prepare OBB versions separately")
    if not args.output and not args.list:
        parser.error("--output is required unless --list is used")
    try:
        candidates = enumerate_apks(args.apk_dir.resolve()) if args.apk_dir else []
        if args.list:
            print_candidates(candidates)
            return 0
        selected = ([item.path for item in candidates] if args.all else
                    [select_apk(candidates, args.version).path] if args.apk_dir else [args.apk])
        for apk in selected:
            prepare_apk(apk.resolve(), args.output.resolve(), args.obb)
    except (OSError, ValueError, KeyError, zipfile.BadZipFile, RuntimeError) as error:
        print(f"package_apk.py: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
