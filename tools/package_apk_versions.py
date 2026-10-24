"""Build and verify separate APK variants from an existing VitaArc VPK.

The SELF, title ID and resources come from the base VPK. Each variant replaces
the complete package/ tree; filenames do not imply runtime compatibility.
"""

import argparse
import hashlib
import json
from pathlib import Path
import zipfile

from package_apk import enumerate_apks, import_apk
from package_textures import texture_map, texture_report


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-vpk", required=True, type=Path)
    parser.add_argument("--apk-dir", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--textures", choices=("original", "vita"), default="vita")
    parser.add_argument("--self", dest="self_path", type=Path,
                        help="Use this new bridge SELF while keeping the base VPK untouched")
    args = parser.parse_args()
    candidates = enumerate_apks(args.apk_dir.resolve())
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    destinations = [(item, output / f"VitaArc-{item.version}.vpk") for item in candidates]
    report_path = output / "packages.json"
    for path in [report_path, *[path for _, path in destinations]]:
        if path.exists() or path.with_suffix(path.suffix + ".partial").exists():
            raise FileExistsError(f"Preserve existing output: {path}")
    report = {"textures": args.textures, "base_vpk": str(args.base_vpk.resolve()),
              "base_sha256": sha256(args.base_vpk), "variants": []}
    if args.self_path:
        report["self_source"] = str(args.self_path.resolve())
    with zipfile.ZipFile(args.base_vpk) as base:
        resources = [(info, base.read(info)) for info in base.infolist()
                     if not info.filename.startswith("package/")
                     and info.filename not in ("vitaarc-apk-version.txt", texture_report)]
        if args.self_path:
            replacement = args.self_path.read_bytes()
            resources = [(info, replacement if info.filename == "eboot.bin" else content)
                         for info, content in resources]
        if len({info.filename for info, _ in resources}) != len(resources):
            raise ValueError("Duplicate base resource entries")
        for item, destination in destinations:
            partial = destination.with_suffix(destination.suffix + ".partial")
            with zipfile.ZipFile(partial, "w") as vpk:
                for info, content in resources:
                    vpk.writestr(info, content)
            plan = import_apk(partial, item.path, "package", args.textures)
            with zipfile.ZipFile(partial) as vpk, zipfile.ZipFile(item.path) as apk:
                if vpk.testzip() is not None:
                    raise ValueError(f"CRC verification failed: {partial}")
                expected = {"package/" + info.filename: info for info in apk.infolist()
                            if not info.is_dir()}
                actual = {info.filename: info for info in vpk.infolist()
                          if info.filename.startswith("package/")}
                if args.textures == "vita":
                    actual.pop(f"package/{texture_map}")
                    if vpk.read(f"package/{texture_map}") != plan.manifest():
                        raise ValueError("Texture manifest differs from converted assets")

                if actual.keys() != expected.keys():
                    raise ValueError("APK file list differs from VPK package tree")
                for name, info in actual.items():
                    original = expected[name]
                    converted = plan.entries.get(original.filename)
                    if converted:
                        if info.CRC != converted.packed_crc:
                            raise ValueError(f"Converted texture differs: {name}")

                        continue

                    if (info.CRC, info.file_size) != (original.CRC, original.file_size):
                        raise ValueError(f"APK entry differs: {name}")
                for info, content in resources:
                    if vpk.read(info.filename) != content:
                        raise ValueError(f"Base resource differs: {info.filename}")
                entry_count = len(vpk.infolist())
                self_hash = hashlib.sha256(vpk.read("eboot.bin")).hexdigest()
                manifest_version = vpk.read('vitaarc-apk-version.txt').decode('ascii').strip()
            partial.rename(destination)
            report["variants"].append({
                "filename_version": item.version, "apk": str(item.path),
                "manifest_version": manifest_version,
                "apk_sha256": sha256(item.path), "vpk": str(destination),
                "vpk_sha256": sha256(destination), "bytes": destination.stat().st_size,
                "entries": entry_count, "apk_entries": len(expected),
                "self_sha256": self_hash, "crc_verified": True,
                "apk_entry_metadata_verified": True, "base_resources_verified": True,
                "runtime_compatibility": "Not tested by this packaging tool"})
            report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
            print(f"Verified {destination}", flush=True)


if __name__ == "__main__":
    main()
