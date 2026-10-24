from pathlib import Path
import os
import shutil
import sys
import zipfile

root = Path(__file__).resolve().parents[2]
old = root / 'build/song-switch-20261025'
out = root / 'build/arcaea-7.0.260c/runtime'
out.mkdir(parents=True, exist_ok=True)

def link(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists():
        os.link(source, target)

for entry in old.iterdir():
    if entry.is_file() and entry.suffix.lower() in ('.exe', '.dll'):
        link(entry, out / entry.name)

for name in ('data', 'plugins', 'shaders-builtin', 'translations'):
    for source in (old / name).rglob('*'):
        if source.is_file():
            link(source, out / source.relative_to(old))

fs = out / 'portable/fs'
for name in ('vs0', 'sa0', 'ur0/tai'):
    for source in (old / 'portable/fs' / name).rglob('*'):
        if source.is_file():
            link(source, fs / source.relative_to(old / 'portable/fs'))

app = fs / 'ux0/app/VARC61616'
for source in (old / 'portable/fs/ux0/app/VARC61616').rglob('*'):
    if not source.is_file():
        continue
    relative = source.relative_to(old / 'portable/fs/ux0/app/VARC61616')
    if relative.parts[0] == 'package':
        continue
    link(source, app / relative)

pref = 'M:\\Projects\\PSV Projects\\VitaArc\\build\\arcaea-7.0.260c\\runtime\\portable\\fs'
config = (old / 'config.yml').read_text().replace('M:\\Projects\\PSV Projects\\VitaArc\\build\\song-switch-20261025\\portable\\fs', pref)
(out / 'config.yml').write_text(config)
(out / 'portable/config.yml').write_text(config)

package = app / 'package'
with zipfile.ZipFile(root / 'third-party/arcaea/arcaea_7.0.260c.apk') as apk:
    for info in apk.infolist():
        if info.is_dir():
            continue
        if not info.filename.startswith(('assets/', 'lib/armeabi-v7a/')):
            continue
        target = package / info.filename
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.exists() and target.stat().st_size == info.file_size:
            continue
        with apk.open(info) as source, target.open('wb') as destination:
            shutil.copyfileobj(source, destination, 1024 * 1024)

(app / 'vitaarc-apk-version.txt').unlink(missing_ok=True)
(app / 'vitaarc-apk-version.txt').write_text('7.0.260c\n')
print(package, flush=True)
