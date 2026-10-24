from pathlib import Path
import json
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'tools'))
from hash_shader import hash_shader

out = root / 'build/arcaea-7.0.260c/effects'
out.mkdir(parents=True, exist_ok=True)
runtime = root / 'build/arcaea-7.0.260c/runtime'
app = runtime / 'portable/fs/ux0/app/VARC61616'
sample = (runtime / 'portable/fs/ux0/data/vitaarc/shaders/missing/8d83aeb33ec7b220.frag.glsl').read_bytes()
prefix = sample[:sample.index(b'#ifdef GL_ES')]
manifest = {'format': 1, 'shaders': []}
for path in (app / 'package/assets/img').iterdir():
    if path.suffix not in ('.fsh', '.vsh'):
        continue
    if path.stem == 'texture_external_oes':
        continue
    stage = 'vertex' if path.suffix == '.vsh' else 'fragment'
    head = prefix.replace(b'precision mediump', b'precision highp') if stage == 'vertex' else prefix
    body = path.read_bytes()
    source = head + body
    digest = f'{hash_shader(source):016x}'
    glsl = out / (path.stem + '.glsl')
    cg = out / (path.stem + '.' + stage + '.cg')
    glsl.write_bytes(source)
    subprocess.run([str(root / 'build/arcaea-7.0.260c/translator/translate'), str(glsl), str(cg), stage], check=True)
    manifest['shaders'].append({'hash': digest, 'stage': stage, 'source': cg.name,
        'matrices': ['CC_MVPMatrix'] if stage == 'vertex' else []})
    print(path.name, digest, flush=True)
(out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
