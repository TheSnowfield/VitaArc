from pathlib import Path
import os
import re
import shutil
import zipfile

root = Path(__file__).resolve().parents[2]
out = root / "build/cpu-profile-20261025"
runtime = root / "build/vita3k/runtime"
original = runtime / "portable/fs"
target = out / "portable/fs"
if target.exists():
    raise SystemExit("Preserve existing profile filesystem")

for name in ("os0", "vs0", "ux0/app/VARC61616"):
    source = original / name
    for path in source.rglob("*"):
        relative = path.relative_to(original)
        dest = target / relative
        if path.is_dir():
            dest.mkdir(parents=True, exist_ok=True)
        elif path.is_file() and path.name != "bootimage.skprx":
            dest.parent.mkdir(parents=True, exist_ok=True)
            os.link(path, dest)

shutil.copytree(original / "ur0", target / "ur0")

seed = original / "ux0/vitaarc/persistent"
if seed.exists():
    shutil.copytree(seed, target / "ux0/data/vitaarc/persistent")

with zipfile.ZipFile(root / "build/texture-profile/VitaArc-1.6.1c-textures-vita.zip") as archive:
    for info in archive.infolist():
        if not info.filename.startswith("package/") or info.is_dir():
            continue
        dest = target / "ux0/app/VARC61616" / info.filename
        dest.parent.mkdir(parents=True, exist_ok=True)
        if dest.exists():
            dest.unlink()
        dest.write_bytes(archive.read(info))

shutil.copy2(out / "profile.self", target / "ux0/app/VARC61616/profile.self")
(target / "ux0/data/vitaarc/profile").mkdir(parents=True)
config = (runtime / "shader-probe-config.yml").read_text()
winpath = "M:/Projects/PSV Projects/VitaArc/build/cpu-profile-20261025/portable/fs"
config = re.sub(r"(?m)^pref-path:.*$", "pref-path: '" + winpath + "'", config)
config = re.sub(r"(?m)^gdbstub:.*$", "gdbstub: true", config)
(out / "config.yml").write_text(config)
(out / "portable/config.yml").write_text(config)
for path in runtime.rglob("*"):
    relative = path.relative_to(runtime)
    if relative.parts[0] == "portable" or path.suffix.lower() in (".log", ".yml"):
        continue
    dest = out / relative
    if path.is_dir():
        dest.mkdir(parents=True, exist_ok=True)
    elif path.is_file():
        dest.parent.mkdir(parents=True, exist_ok=True)
        os.link(path, dest)

print("Isolated filesystem:", target)
