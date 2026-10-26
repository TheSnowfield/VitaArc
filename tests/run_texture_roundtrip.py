"""Verify the C decoder accepts PNG and JPEG produced by the Python tool."""

import io
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from package_textures import image_api, resample_texture

with tempfile.TemporaryDirectory(prefix="vitaarc-texture-") as directory:
    Image = image_api()
    for fmt, suffix in (("PNG", ".png"), ("JPEG", ".jpg")):
        source = io.BytesIO()
        Image.new("RGB", (1001, 1801), (50, 100, 200)).save(source, format=fmt)
        target = Path(directory) / ("odd" + suffix)
        target.write_bytes(resample_texture(source.getvalue(), target.name))
        subprocess.run([sys.argv[1], str(target)], check=True)
