from pathlib import Path
import sys
import zipfile

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'tools'))
from package_textures import TexturePlan

app = root / 'build/arcaea-7.0.260c/runtime/portable/fs/ux0/app/VARC61616'
with zipfile.ZipFile(root / 'third-party/arcaea/arcaea_7.0.260c.apk') as apk:
    plan = TexturePlan(apk, 'vita')
    plan.describe()
    for index, name in enumerate(plan.entries):
        (app / 'package' / name).write_bytes(plan.rewrite(name))
        if index % 100 == 0:
            print(index, name, flush=True)
    (app / 'package/vitaarc-textures.bin').write_bytes(plan.manifest())
    (root / 'build/arcaea-7.0.260c/textures.json').write_bytes(plan.report())

(app / 'vitaarc-apk-version.txt').unlink(missing_ok=True)
(app / 'vitaarc-apk-version.txt').write_text('7.0.260c\n')
