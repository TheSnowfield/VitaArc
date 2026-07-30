#!/usr/bin/env python3
"""Import every APK file into an existing VPK under a fixed prefix."""

from __future__ import annotations

import argparse
import re
import shutil
import sys
import zipfile
from dataclasses import dataclass
from pathlib import Path, PurePosixPath


copy_buffer_size = 1024 * 1024
supported_compression = {
    zipfile.ZIP_STORED,
    zipfile.ZIP_DEFLATED,
}


@dataclass(frozen=True)
class ApkCandidate:
    version: str
    path: Path


def version_from_filename(path: Path) -> str:
    match = re.fullmatch(r"arcaea[_-](.+)", path.stem, re.IGNORECASE)
    return match.group(1) if match else path.stem


def natural_sort_key(value: str) -> tuple[object, ...]:
    return tuple(
        int(part) if part.isdigit() else part.casefold()
        for part in re.split(r"(\d+)", value)
    )


def enumerate_apks(apk_dir: Path) -> list[ApkCandidate]:
    if not apk_dir.is_dir():
        raise ValueError(f"APK directory does not exist: {apk_dir}")

    candidates = [
        ApkCandidate(version_from_filename(path), path)
        for path in apk_dir.glob("*.apk")
        if path.is_file()
    ]
    candidates.sort(key=lambda item: natural_sort_key(item.version))
    if not candidates:
        raise ValueError(f"no APK files found under: {apk_dir}")

    versions: set[str] = set()
    for candidate in candidates:
        version_key = candidate.version.casefold()
        if version_key in versions:
            raise ValueError(
                f"duplicate APK version {candidate.version!r} under: "
                f"{apk_dir}"
            )
        versions.add(version_key)
    return candidates


def print_candidates(candidates: list[ApkCandidate]) -> None:
    print("Available Arcaea APK versions:")
    for index, candidate in enumerate(candidates, start=1):
        print(
            f"  {index}. {candidate.version} "
            f"({candidate.path.name}, {candidate.path.stat().st_size} bytes)"
        )


def select_apk(
    candidates: list[ApkCandidate], requested_version: str | None
) -> ApkCandidate:
    if requested_version:
        requested_key = requested_version.casefold()
        matches = [
            candidate
            for candidate in candidates
            if requested_key
            in {
                candidate.version.casefold(),
                candidate.path.stem.casefold(),
                candidate.path.name.casefold(),
            }
        ]
        if not matches:
            available = ", ".join(
                candidate.version for candidate in candidates
            )
            raise ValueError(
                f"APK version {requested_version!r} was not found; "
                f"available versions: {available}"
            )
        return matches[0]

    if len(candidates) == 1:
        return candidates[0]
    if not sys.stdin.isatty():
        available = ", ".join(
            candidate.version for candidate in candidates
        )
        raise ValueError(
            "multiple APK versions are available in a non-interactive "
            f"terminal; pass --version. Available versions: {available}"
        )

    print_candidates(candidates)
    while True:
        selection = input(
            f"Select APK version [1-{len(candidates)}]: "
        ).strip()
        if selection.isdigit():
            index = int(selection)
            if 1 <= index <= len(candidates):
                return candidates[index - 1]
        print("Invalid selection.", file=sys.stderr)


def normalize_prefix(value: str) -> str:
    prefix = value.strip("/")
    if not prefix or "\\" in prefix:
        raise ValueError("prefix must be a non-empty ZIP path")

    parts = PurePosixPath(prefix).parts
    if any(part in ("", ".", "..") for part in parts):
        raise ValueError("prefix contains an unsafe path component")
    return "/".join(parts)


def make_target_name(prefix: str, source_name: str) -> str:
    if (
        not source_name
        or source_name.startswith("/")
        or "\\" in source_name
        or "\0" in source_name
    ):
        raise ValueError(f"unsafe APK entry path: {source_name!r}")

    parts = source_name.split("/")
    if any(part in ("", ".", "..") for part in parts):
        raise ValueError(f"unsafe APK entry path: {source_name!r}")
    return f"{prefix}/{source_name}"


def make_target_info(
    source_info: zipfile.ZipInfo, target_name: str
) -> zipfile.ZipInfo:
    target_info = zipfile.ZipInfo(target_name, source_info.date_time)
    target_info.compress_type = source_info.compress_type
    target_info.comment = source_info.comment
    target_info.internal_attr = source_info.internal_attr
    target_info.external_attr = source_info.external_attr
    target_info.create_system = source_info.create_system
    return target_info


def import_apk(vpk_path: Path, apk_path: Path, prefix: str) -> None:
    if not zipfile.is_zipfile(vpk_path):
        raise ValueError(f"not a valid VPK/ZIP archive: {vpk_path}")
    if not zipfile.is_zipfile(apk_path):
        raise ValueError(f"not a valid APK/ZIP archive: {apk_path}")

    with zipfile.ZipFile(apk_path, "r") as apk:
        source_entries = [
            entry for entry in apk.infolist() if not entry.is_dir()
        ]
        target_entries: list[tuple[zipfile.ZipInfo, str]] = []
        target_names: set[str] = set()

        for source_info in source_entries:
            if source_info.compress_type not in supported_compression:
                raise ValueError(
                    f"unsupported compression method "
                    f"{source_info.compress_type}: {source_info.filename}"
                )

            target_name = make_target_name(prefix, source_info.filename)
            if target_name in target_names:
                raise ValueError(f"duplicate APK entry: {source_info.filename}")
            target_names.add(target_name)
            target_entries.append((source_info, target_name))

        with zipfile.ZipFile(vpk_path, "a", allowZip64=True) as vpk:
            existing_names = set(vpk.namelist())
            collisions = sorted(existing_names.intersection(target_names))
            if collisions:
                raise ValueError(
                    f"VPK entry already exists: {collisions[0]}"
                )

            total_size = sum(
                source_info.file_size
                for source_info, _ in target_entries
            )
            print(
                f"Importing {len(target_entries)} APK files "
                f"({total_size} bytes) into {vpk_path} under {prefix}/"
            )

            for index, (source_info, target_name) in enumerate(
                target_entries, start=1
            ):
                target_info = make_target_info(source_info, target_name)
                with apk.open(source_info, "r") as source:
                    with vpk.open(target_info, "w") as target:
                        shutil.copyfileobj(
                            source, target, length=copy_buffer_size
                        )

                if index % 100 == 0 or index == len(target_entries):
                    print(
                        f"  imported {index}/{len(target_entries)} files",
                        flush=True,
                    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Import APK contents into an existing VPK archive."
    )
    parser.add_argument("--vpk", type=Path)
    parser.add_argument("--apk-dir", required=True, type=Path)
    parser.add_argument("--version")
    parser.add_argument("--prefix", default="package")
    parser.add_argument(
        "--list", action="store_true",
        help="list available APK versions and exit",
    )
    args = parser.parse_args()

    try:
        candidates = enumerate_apks(args.apk_dir.resolve())
        if args.list:
            print_candidates(candidates)
            return 0
        if not args.vpk:
            parser.error("--vpk is required unless --list is used")

        selected_apk = select_apk(candidates, args.version)
        print(
            f"Selected Arcaea APK {selected_apk.version}: "
            f"{selected_apk.path}"
        )
        import_apk(
            args.vpk.resolve(),
            selected_apk.path.resolve(),
            normalize_prefix(args.prefix),
        )
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        print(f"package_apk.py: {error}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
