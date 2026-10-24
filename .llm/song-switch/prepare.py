from pathlib import Path
import os
import re
import shutil

root = Path(__file__).resolve().parents[2]
out = root / "build/song-switch-20261025"
source = root / "build/cpu-profile-20261025"
target = out / "portable/fs"
if target.exists():
    raise SystemExit("Preserve existing diagnostic filesystem")

for name in ("os0", "vs0", "ux0/app/VARC61616"):
    for path in (source / "portable/fs" / name).rglob("*"):
        relative = path.relative_to(source / "portable/fs")
        dest = target / relative
        if path.is_dir():
            dest.mkdir(parents=True, exist_ok=True)
        elif path.is_file() and path.name != "profile.self":
            dest.parent.mkdir(parents=True, exist_ok=True)
            os.link(path, dest)

shutil.copytree(source / "portable/fs/ur0", target / "ur0")
shutil.copytree(source / "portable/fs/ux0/data/vitaarc/persistent", target / "ux0/data/vitaarc/persistent")
(target / "ux0/data/vitaarc/profile").mkdir(parents=True)
shutil.copy2(out / "profile.self", target / "ux0/app/VARC61616/profile.self")
for path in source.iterdir():
    if path.suffix.lower() not in (".exe", ".dll"):
        continue
    os.link(path, out / path.name)

for name in ("data", "plugins", "shaders-builtin", "translations"):
    for path in (source / name).rglob("*"):
        dest = out / path.relative_to(source)
        if path.is_dir():
            dest.mkdir(parents=True, exist_ok=True)
        elif path.is_file():
            dest.parent.mkdir(parents=True, exist_ok=True)
            os.link(path, dest)

config = (source / "config.yml").read_text()
config = re.sub(r"(?m)^pref-path:.*$", "pref-path: 'M:/Projects/PSV Projects/VitaArc/build/song-switch-20261025/portable/fs'", config)
config = re.sub(r"(?m)^gdbstub:.*$", "gdbstub: false", config)
(out / "config.yml").write_text(config)
(out / "portable/config.yml").write_text(config)
print("Prepared private song-switch runtime")
