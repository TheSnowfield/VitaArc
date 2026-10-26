"""Synthetic APKs exercise resource filtering and lossless layout metadata."""

import contextlib
import hashlib
import io
from pathlib import Path
import stat
import struct
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from PIL import Image
from apk_manifest import version_name
from package_apk import LIBRARY, prepare_apk
from package_textures import SIZE_RECORD, resample_texture, small_size


def manifest(version="1.6.1c"):
    strings = ["manifest", "versionName", version]
    data = bytearray()
    offsets = []
    for text in strings:
        encoded = text.encode("utf-8")
        offsets.append(len(data))
        data += bytes([len(text), len(encoded)]) + encoded + b"\0"
    data += b"\0" * (-len(data) % 4)
    pool = (struct.pack("<HHI5I", 1, 28, 40 + len(data), 3, 0, 256, 40, 0)
            + struct.pack("<3I", *offsets) + data)
    tag = (struct.pack("<HHIII", 0x102, 16, 56, 1, 0xffffffff)
           + struct.pack("<II6H", 0xffffffff, 0, 20, 20, 1, 0, 0, 0)
           + struct.pack("<IIIHBBI", 0xffffffff, 1, 2, 8, 0, 3, 2))
    return struct.pack("<HHI", 3, 8, 8 + len(pool) + len(tag)) + pool + tag


def image_bytes(size, mode="RGBA", fmt="PNG"):
    output = io.BytesIO()
    Image.new(mode, size, (20, 80, 140, 100) if mode == "RGBA" else (20, 80, 140)).save(output, format=fmt)
    return output.getvalue()


def elf():
    data = bytearray(52)
    data[:7] = b"\x7fELF\x01\x01\x01"
    struct.pack_into("<HHI", data, 16, 3, 40, 1)
    return bytes(data)


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.apk = self.root / "arcaea_1.6.1c.apk"
        self.output = self.root / "ready"

    def apk_with(self, extra=(), library=True):
        with zipfile.ZipFile(self.apk, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            archive.writestr("AndroidManifest.xml", manifest())
            archive.writestr("assets/dot.png", image_bytes((1, 1)))
            if library:
                archive.writestr(LIBRARY, elf())
            for name, content in extra:
                info = zipfile.ZipInfo(name)
                # ZipInfo normally converts the platform separator on Windows.
                info.filename = name
                archive.writestr(info, content)

    def prepare(self, *args):
        with contextlib.redirect_stdout(io.StringIO()):
            return prepare_apk(self.apk, self.output, *args)

    def test_manifest_fixture(self):
        self.assertEqual(version_name(manifest()), "1.6.1c")

    def test_filter_resources_resample_and_preserve_obb(self):
        large = image_bytes((1001, 1801))
        dot = image_bytes((1, 1))
        jpeg = image_bytes((1024, 768), "RGB", "JPEG")
        self.apk_with([
            ("assets/char/large.png", large), ("assets/atlas.png", large),
            ("assets/atlas.plist", b"sprite coordinates stay unchanged"),
            ("assets/bg.jpg", jpeg), ("assets/audio/song.ogg", b"audio"),
            ("obb/main.obb", b"opaque expansion"),
            ("res/layout/main.xml", b"layout"), ("META-INF/CERT.RSA", b"signature"),
            ("classes.dex", b"dex"), ("resources.arsc", b"android resources"),
            ("lib/arm64-v8a/libcocos2dcpp.so", b"arm64"),
            ("lib/armeabi-v7a/libfmod.so", b"unused library")])
        digest = hashlib.sha256(self.apk.read_bytes()).digest()
        obb = self.root / "patch.obb"
        obb.write_bytes(b"external expansion")
        target = self.prepare([obb])
        self.assertEqual(target, self.output / "version/1.6.1c")
        self.assertEqual((target / "assets/dot.png").read_bytes(), dot)
        self.assertEqual((target / LIBRARY).read_bytes(), elf())
        self.assertEqual((target / "obb/main.obb").read_bytes(), b"opaque expansion")
        self.assertEqual((target / "obb/patch.obb").read_bytes(), b"external expansion")
        self.assertEqual((target / "vitaarc-apk-version.txt").read_text().strip(), "1.6.1c")
        for name in ("res", "META-INF", "classes.dex", "resources.arsc", "AndroidManifest.xml",
                     "lib/arm64-v8a", "lib/armeabi-v7a/libfmod.so", "vitaarc-textures.bin", "vitaarc-textures.json"):
            self.assertFalse((target / name).exists(), name)
        png = (target / "assets/char/large.png").read_bytes()
        self.assertEqual(png[37:41], b"vaRc")
        self.assertEqual(SIZE_RECORD.unpack(png[41:53]), (b"VTX1", 1001, 1801, 501, 901))
        with Image.open(io.BytesIO(png)) as image:
            image.load()
            self.assertEqual(image.size, (501, 901))
            self.assertEqual(image.mode, "RGBA")
        packed_jpeg = (target / "assets/bg.jpg").read_bytes()
        self.assertEqual(SIZE_RECORD.unpack(packed_jpeg[6:18]), (b"VTX1", 1024, 768, 512, 384))
        with Image.open(io.BytesIO(packed_jpeg)) as image:
            image.load()
            self.assertEqual(image.size, (512, 384))
        self.assertEqual(hashlib.sha256(self.apk.read_bytes()).digest(), digest)

    def test_threshold_boundaries(self):
        cases = [((1, 1), (1, 1)), ((63, 1024), (63, 1024)),
                 ((64, 255), (64, 255)), ((64, 256), (32, 128)),
                 ((1001, 1801), (501, 901)), ((4096, 2048), (512, 256)),
                 ((16384, 1), (4096, 1))]
        for source, expected in cases:
            with self.subTest(source=source):
                self.assertEqual(small_size(*source), expected)
                data = image_bytes(source)
                encoded = resample_texture(data, "test.png")
                with Image.open(io.BytesIO(encoded)) as image:
                    self.assertEqual(image.size, expected)
                if source == expected:
                    self.assertIs(encoded, data)
        for source in ((0, 1), (1, 16385)):
            with self.assertRaises(ValueError):
                small_size(*source)

    def test_existing_version_is_preserved(self):
        self.apk_with()
        target = self.prepare()
        (target / "keep.txt").write_text("keep")
        with self.assertRaises(FileExistsError):
            self.prepare()
        self.assertEqual((target / "keep.txt").read_text(), "keep")

    def test_png_with_jpg_extension_keeps_its_decoded_format(self):
        # Arcaea 7 includes onelastdrive/1080_base.jpg with a PNG payload.
        self.apk_with([("assets/base.jpg", image_bytes((768, 768)))])
        target = self.prepare()
        data = (target / "assets/base.jpg").read_bytes()
        self.assertEqual(SIZE_RECORD.unpack(data[41:53]), (b"VTX1", 768, 768, 384, 384))
        with Image.open(io.BytesIO(data)) as image:
            image.load()
            self.assertEqual(image.format, "PNG")
            self.assertEqual(image.size, (384, 384))

    def test_invalid_and_missing_arm_library(self):
        self.apk_with(library=False)
        with self.assertRaisesRegex(ValueError, "missing"):
            self.prepare()
        self.apk_with([(LIBRARY, b"not an ARM ELF")], library=False)
        with self.assertRaisesRegex(ValueError, "ARM32"):
            self.prepare()

    def test_path_traversal_and_case_collisions_rejected(self):
        for name in ("assets/../../outside", "/absolute", "assets/C:/bad", "assets/a\\b", "assets/NUL.bin"):
            with self.subTest(name=name):
                self.apk_with([(name, b"data")])
                with self.assertRaises(ValueError):
                    self.prepare()
        self.apk_with([("assets/DOT.png", image_bytes((1, 1)))])
        with self.assertRaisesRegex(ValueError, "duplicate"):
            self.prepare()

    def test_symlink_rejected(self):
        self.apk_with()
        with zipfile.ZipFile(self.apk, "a") as archive:
            info = zipfile.ZipInfo("assets/link")
            info.create_system = 3
            info.external_attr = (stat.S_IFLNK | 0o777) << 16
            archive.writestr(info, "../../outside")
        with self.assertRaisesRegex(ValueError, "symlink"):
            self.prepare()

    def test_failed_conversion_does_not_publish_partial_directory(self):
        self.apk_with([("assets/broken.png", b"not a PNG")])
        with self.assertRaises(OSError):
            self.prepare()
        self.assertEqual(list((self.output / "version").iterdir()), [])

    def test_duplicate_obb_rejected_before_output(self):
        self.apk_with([("obb/main.obb", b"embedded")])
        obb = self.root / "main.obb"
        obb.write_bytes(b"external")
        with self.assertRaisesRegex(ValueError, "duplicate OBB"):
            self.prepare([obb])
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main()
