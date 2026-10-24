from pathlib import Path
import argparse
import json
import os
import shutil
import sys
import zipfile

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'tools'))
from package_textures import TexturePlan

base = root / 'build/arcaea-7.0.260c/runtime'
out = root / 'build/compat-current'
profiles = {
    '1.6.1c': 'run-1.6.1c-20261024-143038',
    '2.0.0c': 'run-2.0.0c-20261024-142932',
    '3.0.0c': 'run-3.0.0c-20261024-140830',
}


def link(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists():
        os.link(source, target)


def prepare(version):
    dest = out / version
    marker = dest / 'prepared.json'
    if marker.exists():
        print(version, 'already prepared', flush=True)
        return

    dest.mkdir(parents=True, exist_ok=True)
    for source in base.iterdir():
        if source.is_file() and source.suffix.lower() in ('.exe', '.dll'):
            link(source, dest / source.name)

    for name in ('data', 'plugins', 'shaders-builtin', 'translations',
                 'portable/fs/vs0', 'portable/fs/sa0', 'portable/fs/ur0/tai'):
        for source in (base / name).rglob('*'):
            if source.is_file():
                link(source, dest / source.relative_to(base))

    app = dest / 'portable/fs/ux0/app/VARC61616'
    app.mkdir(parents=True, exist_ok=True)
    source_app = base / 'portable/fs/ux0/app/VARC61616'
    for name in ('sce_sys', 'resources/licenses'):
        for source in (source_app / name).rglob('*'):
            if source.is_file():
                link(source, app / source.relative_to(source_app))

    shaders = root / 'build/arcaea-7.0.260c/offline-shaders'
    for source in shaders.iterdir():
        if source.suffix == '.vgxp' or source.name == 'offline.enabled':
            link(source, app / 'shaders' / source.name)

    apk_path = root / f'third-party/arcaea/arcaea_{version}.apk'
    with zipfile.ZipFile(apk_path) as apk:
        for info in apk.infolist():
            if info.is_dir() or not info.filename.startswith(('assets/', 'lib/armeabi-v7a/')):
                continue

            target = app / 'package' / info.filename
            target.parent.mkdir(parents=True, exist_ok=True)
            with apk.open(info) as source, target.open('wb') as stream:
                shutil.copyfileobj(source, stream)

        plan = TexturePlan(apk, 'vita')
        plan.describe()
        old = root / 'build/texture-profile/VitaArc-1.6.1c-textures-vita/package'
        for index, name in enumerate(sorted(plan.entries), 1):
            target = app / 'package' / name
            if version == '1.6.1c' and (old / name).is_file():
                shutil.copy2(old / name, target)
            else:
                target.write_bytes(plan.rewrite(name))

            if index % 100 == 0:
                print(version, index, '/', len(plan.entries), flush=True)

        (app / 'package/vitaarc-textures.bin').write_bytes(plan.manifest())
        (dest / 'textures.json').write_bytes(plan.report())

    (app / 'vitaarc-apk-version.txt').write_text(version + '\n')
    shutil.copy2(root / 'build/release-7.0.260c/VitaArc.self', app / 'eboot.bin')
    shutil.copy2(base / 'probe.self', app / 'probe.self')
    user = dest / 'portable/fs/ux0/data/vitaarc'
    user.mkdir(parents=True, exist_ok=True)
    source_user = root / 'build/apk-compat-r3' / profiles[version] / 'tested-user/persistent'
    shutil.copytree(source_user, user / 'persistent')
    (user / 'profile').mkdir()
    (user / 'profile/0001.cmd').write_text('capture 0 0 1800\n')
    old_path = str(base / 'portable/fs').replace('/mnt/m/', 'M:/').replace('/', '\\')
    new_path = str(dest / 'portable/fs').replace('/mnt/m/', 'M:/').replace('/', '\\')
    config = (base / 'config.yml').read_text()
    assert old_path in config
    (dest / 'config.yml').write_text(config.replace(old_path, new_path))
    (dest / 'portable/config.yml').write_text(config.replace(old_path, new_path))
    marker.write_text(json.dumps({'version': version, 'resized': len(plan.entries)}, indent=2) + '\n')
    print(version, 'ready', flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('versions', nargs='+', choices=profiles)
    for version in parser.parse_args().versions:
        prepare(version)
