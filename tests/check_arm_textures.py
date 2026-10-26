"""Execute the linked texture hook with real packaging headers and modeled I/O.

The production __from_image_with_format, image-path ABI decoding and source-size validation
execute as ARM instructions. Original Cocos image initialization and native FILE
I/O are intercepted; this does not decode images on Vita or exercise the GPU.
Optional local game libraries check the production ABI fingerprints and patch
targets without executing game constructors or including game files in CI.
"""

import argparse
from functools import lru_cache
import io
import math
from pathlib import Path
import struct
import sys
import zlib

from arm_fixture_build import root
from check_arm_bridges import ArmCalls, ArmImage
from unicorn import arm_const as registers

sys.path.insert(0, str(root / "tools"))
from package_textures import SIZE_RECORD, resample_texture
from PIL import Image


PACKAGE = "ux0:data/vitaarc/version/7.0.260c/"
ASSETS = PACKAGE + "assets"


@lru_cache(maxsize=None)
def encoded_image(size, extension="png", resample=True):
    source = io.BytesIO()
    Image.new("RGB", size, (20, 80, 140)).save(source, format="PNG" if extension == "png" else "JPEG")
    original = source.getvalue()
    data = resample_texture(original, "a." + extension) if resample else original
    with Image.open(io.BytesIO(data)) as image:
        packed = image.size
    return data, packed


def png_record(data, record):
    chunk = b"vaRc" + record
    chunk = struct.pack(">I", len(record)) + chunk + struct.pack(">I", zlib.crc32(chunk))
    end = 57 if data[37:41] == b"vaRc" else 33
    return data[:33] + chunk + data[end:]


class TextureCase:
    MODES = ("gnu-cow", "ndk-sso", "ndk-long", "7.0-sso", "7.0-long")

    def __init__(self, elf):
        self.calls = ArmCalls()
        self.target = ArmImage(self.calls.machine, elf)
        self.texture = self.calls.SCRATCH + 0x1000
        self.image = self.calls.SCRATCH + 0x2000
        self.path = self.calls.SCRATCH + 0x3000
        self.file = self.calls.SCRATCH + 0x5000
        self.init = self.calls.RETURN + 0x100
        self.count = 0
        for symbol, value in (("PATH_TO_PACKAGE", PACKAGE), ("PATH_TO_ASSETS", ASSETS)):
            self.calls.machine.mem_write(self.target.symbols[symbol], value.encode() + b"\0")
        self.calls.register(self.init, self.initialize)
        for symbol, callback in (("fopen", self.open), ("fread", self.read), ("fclose", self.close),
                                 ("snprintf", self.format_path)):
            self.calls.register(self.target.symbols[symbol], callback)

    def result(self, value):
        self.calls.machine.reg_write(registers.UC_ARM_REG_R0, value)

    def string(self, address):
        output = bytearray()
        while len(output) < 2048:
            value = self.calls.machine.mem_read(address + len(output), 1)[0]
            if not value:
                return output.decode()
            output.append(value)
        raise AssertionError("unterminated texture path")

    def initialize(self):
        assert tuple(self.calls.reg("R" + str(index)) for index in range(3)) == (
            self.texture, self.image, self.pixel_format)
        self.events.append("init")
        if self.init_ok:
            self.calls.machine.mem_write(self.texture, bytes(self.initial))
        self.result(int(self.init_ok))

    def format_path(self):
        destination, capacity, fmt, first = (self.calls.reg("R" + str(index)) for index in range(4))
        second = struct.unpack("<I", self.calls.machine.mem_read(self.calls.reg("SP"), 4))[0]
        fmt = self.string(fmt)
        assert fmt in ("%s%s", "%s/%s")
        value = (fmt % (self.string(first), self.string(second))).encode()
        assert capacity == 1024
        self.calls.machine.mem_write(destination, value[:capacity - 1] + b"\0")
        self.result(len(value))

    def open(self):
        path, mode = self.calls.reg("R0"), self.calls.reg("R1")
        assert self.string(mode) == "rb"
        assert self.opened is not None and self.string(path) == self.opened, self.string(path)
        self.events.append("open")
        self.result(self.file if self.data is not None else 0)

    def read(self):
        destination, size, count, file = (self.calls.reg("R" + str(index)) for index in range(4))
        assert (size, count, file) == (1, 57, self.file)
        self.events.append("read")
        data = self.data[:57]
        self.calls.machine.mem_write(destination, data)
        self.result(len(data))

    def close(self):
        assert self.calls.reg("R0") == self.file
        self.events.append("close")
        self.result(0)

    def run(self, mode, name, data, packed, restored=None, opened=ASSETS + "/a.png", init_ok=True, repeat=1,
            pixel_format=27, configured=False, entry=None, default_wrapper=False):
        assert mode in self.MODES
        self.modern = mode.startswith("7.0")
        self.init_ok, self.data, self.opened = init_ok, data, opened
        self.pixel_format = pixel_format
        pixels, size, logical = (20, 40, 12) if self.modern else (12, 32, 0)
        for symbol, value in (("__cow", int(mode == "gnu-cow")), ("__pixels", pixels),
                              ("__size", size), ("__logical", logical)):
            if configured:
                assert self.calls.machine.mem_read(self.target.symbols[symbol], 1)[0] == value
            else:
                self.calls.machine.mem_write(self.target.symbols[symbol], bytes([value]))
        if not configured:
            self.calls.word(self.target.symbols["__init"], self.init | 1)
        self.calls.machine.mem_write(self.image, bytes(256))
        if mode == "gnu-cow":
            self.calls.word(self.image + 172, self.path if name is not None else 0)
        elif mode.endswith("sso"):
            assert name is not None and len(name.encode()) <= 10
            self.calls.machine.mem_write(self.image + 172, bytes([len(name.encode()) * 2]) + name.encode() + b"\0")
        else:
            self.calls.word(self.image + 172, 1)
            self.calls.word(self.image + 176, len(name.encode()))
            self.calls.word(self.image + 180, self.path)
        if name is not None:
            self.calls.machine.mem_write(self.path, name.encode() + b"\0")
        image_before = bytes(self.calls.machine.mem_read(self.image, 256))

        self.initial = bytearray(b"\xa5" * 64)
        struct.pack_into("<II", self.initial, pixels, *packed)
        struct.pack_into("<ff", self.initial, size, *(value / (1080 if self.modern else 2) for value in packed))
        if logical:
            struct.pack_into("<ff", self.initial, logical, *(value / 2 for value in packed))
        self.calls.machine.mem_write(self.texture, b"\x99" * 64)
        expected = bytearray(self.initial if init_ok else b"\x99" * 64)
        if restored and init_ok:
            struct.pack_into("<II", expected, pixels, *restored)
            struct.pack_into("<ff", expected, size, *(value / (1080 if self.modern else 1) for value in restored))
            if logical:
                struct.pack_into("<ff", expected, logical, *(value / 2 for value in restored))

        for _ in range(repeat):
            self.events = []
            arguments = (self.texture, self.image) if default_wrapper else (self.texture, self.image, pixel_format)
            actual = self.calls.call(entry or self.target.symbols["__from_image_with_format"], *arguments)
            assert actual == int(init_ok), (mode, name, actual)
            actual = bytearray(self.calls.machine.mem_read(self.texture, 64))
            # Content-scale products round as float on ARM; compare those fields
            # numerically, then require every other byte (including UV/storage
            # fields outside the reported dimensions) to remain exact.
            if init_ok:
                for offset in (size, logical) if logical else (size,):
                    values = struct.unpack_from("<ff", actual, offset)
                    targets = struct.unpack_from("<ff", expected, offset)
                    assert all(math.isclose(a, b, rel_tol=2e-6) for a, b in zip(values, targets)), (
                        mode, name, packed, restored, offset, values, targets)
                    actual[offset:offset + 8] = expected[offset:offset + 8]
            assert actual == expected, (mode, name, packed, restored)
            assert bytes(self.calls.machine.mem_read(self.image, 256)) == image_before
            expected_events = ["init"]
            if init_ok and opened is not None:
                expected_events += ["open"] + (["read", "close"] if data is not None else [])
            assert self.events == expected_events, (mode, name, self.events, expected_events)
            self.count += 1


def check_hook(elf):
    case = TextureCase(elf)
    for mode in case.MODES:
        for source in ((1, 1), (64, 255), (64, 256), (1001, 1801), (1921, 1081), (4096, 2048), (16384, 1)):
            data, packed = encoded_image(source)
            restored = source if packed != source else None
            case.run(mode, "a.png", data, packed, restored, repeat=2 if source == (1921, 1081) else 1)
        data, packed = encoded_image((1001, 1801), "jpg")
        case.run(mode, "a.jpg", data, packed, (1001, 1801), opened=ASSETS + "/a.jpg")
        data, packed = encoded_image((1001, 1801), resample=False)
        case.run(mode, "a.png", data, packed)  # An untagged image is never inferred to be resampled.
        case.run(mode, "", b"", (401, 61), opened=None)  # Dynamic text/image has no immutable file path.
        data, packed = encoded_image((1001, 1801))
        case.run(mode, "a.png", data, packed, init_ok=False, opened=None)
        case.run(mode, "a.png", None, packed)  # File disappeared after image decoding.

    data, packed = encoded_image((1001, 1801))
    for name, opened in (
            ("assets/char/a.png", ASSETS + "/char/a.png"),
            ("char/a.png", ASSETS + "/char/a.png"),
            (PACKAGE + "assets/char/a.png", ASSETS + "/char/a.png"),
            ("file:///android_asset/char/a.png", ASSETS + "/char/a.png"),
            ("file:///android_asset/assets/char/a.png", ASSETS + "/char/a.png")):
        for mode in ("gnu-cow", "ndk-long", "7.0-long"):
            case.run(mode, name, data, packed, (1001, 1801), opened=opened)
    for name in ("../a.png", "assets/../a.png", "char//a.png", "./a.png", "/a.png", "char\\a.png",
                 "ux0:data/vitaarc/persistent/text.png", "ux0:data/vitaarc/version/other/assets/a.png",
                 "a" * 1024 + ".png"):
        case.run("ndk-long", name, data, packed, opened=None)
    case.run("gnu-cow", None, data, packed, opened=None)

    bad_crc = bytearray(data)
    bad_crc[56] ^= 1
    bad_ihdr = bytearray(data)
    struct.pack_into(">I", bad_ihdr, 16, packed[0] + 1)
    wrong_magic = png_record(data, SIZE_RECORD.pack(b"VTX2", 1001, 1801, *packed))
    wrong_source = png_record(data, SIZE_RECORD.pack(b"VTX1", 1000, 1800, *packed))
    wrong_packed = png_record(data, SIZE_RECORD.pack(b"VTX1", 1001, 1801, packed[0] - 1, packed[1]))
    for invalid in (bytes(bad_crc), bytes(bad_ihdr), wrong_magic, wrong_source, wrong_packed, data[:56]):
        case.run("ndk-long", "a.png", invalid, packed)
    case.run("ndk-long", "a.png", data, (packed[0] - 1, packed[1]))
    dot, _ = encoded_image((1, 1))
    tagged_dot = png_record(dot, SIZE_RECORD.pack(b"VTX1", 1, 1, 1, 1))
    case.run("gnu-cow", "a.png", tagged_dot, (1, 1))
    narrow, packed = encoded_image((32, 512), resample=False)
    invalid_narrow = png_record(narrow, SIZE_RECORD.pack(b"VTX1", 63, 1024, *packed))
    case.run("ndk-long", "a.png", invalid_narrow, packed)
    jpeg, packed = encoded_image((1001, 1801), "jpg")
    for invalid in (jpeg[:17], jpeg[:6] + b"VTX2" + jpeg[10:]):
        case.run("7.0-long", "a.jpg", invalid, packed, opened=ASSETS + "/a.jpg")
    data, packed = encoded_image((1001, 1801))
    for mode in case.MODES:
        for pixel_format in (0, 1, 2, 4, 12):
            case.run(mode, "a.png", data, packed, (1001, 1801), pixel_format=pixel_format)
    print(f"Linked ARM texture hook passed {case.count} cases: COW/NDK short/long paths, selected assets, "
          "PNG/JPEG embedded sizes, odd/repeated halving, 7.0 logical/content scale, unchanged small/untagged/text images, "
          "explicit PixelFormat forwarding, metadata/path/init/I/O rejection and unchanged image/UV/storage fields.", flush=True)


def prologue_state(case, entry, stop):
    """Compare complete saved-register/stack state at the original continuation."""
    calls, machine = case.calls, case.calls.machine
    machine.mem_write(calls.STACK, b"\xa5" * 0x10000)
    for index in range(13):
        value = (case.texture, case.image, 27)[index] if index < 3 else 0x12340000 + index
        machine.reg_write(getattr(registers, "UC_ARM_REG_R" + str(index)), value)
    machine.reg_write(registers.UC_ARM_REG_SP, calls.STACK + 0x8000)
    machine.reg_write(registers.UC_ARM_REG_LR, calls.RETURN | 1)
    machine.reg_write(registers.UC_ARM_REG_CPSR, 0x400001f3)
    machine.emu_start(entry | 1, stop, count=64)
    assert calls.reg("PC") == stop, "texture prologue did not reach its original continuation"
    names = ["R" + str(index) for index in range(13)] + ["SP", "LR", "CPSR"]
    return tuple(calls.reg(name) for name in names), bytes(machine.mem_read(calls.STACK, 0x10000))


def check_library(elf, path):
    """Optional real-library recognition/configuration; no game file enters CI."""
    case = TextureCase(elf)
    calls, target, machine = case.calls, case.target, case.calls.machine
    data = path.read_bytes()
    bias, metadata = 0x98000000, 0x50000000
    game = ArmImage(machine, path, bias)
    header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
    sections = [struct.unpack_from("<IIIIIIIIII", data, header[6] + index * header[11])
                for index in range(header[12])]
    programs = [struct.unpack_from("<IIIIIIII", data, header[5] + index * header[9])
                for index in range(header[10])]
    symbols = next(section for section in sections if section[1] == 11)
    strings = sections[symbols[6]]
    machine.mem_map(metadata, (len(data) + 4095) & ~4095)
    machine.mem_write(metadata, data)

    # Native ARM dynalib_t metadata only. The game segments and all symbol/code
    # bytes are the unmodified local ELF; loader relocation/constructors are out
    # of scope. These offsets describe the public 32-bit src/dynalib.h layout.
    library = calls.SCRATCH + 0x6000
    machine.mem_write(library, bytes(344))
    span = max(program[2] + program[5] for program in programs if program[0] == 1)
    for offset, value in ((208, bias), (212, span), (288, metadata), (292, metadata + header[5]),
                          (316, metadata + symbols[4]), (320, metadata + strings[4]), (324, symbols[5] // symbols[9])):
        calls.word(library + offset, value)
    patches, flushes, messages = [], [], []

    def get_proc():
        assert calls.reg("R0") == library
        case.result(game.symbols.get(case.string(calls.reg("R1")), 0))

    def copy():
        destination, source, size = (calls.reg("R" + str(index)) for index in range(3))
        assert size in (8, 20)
        patches.append((destination, bytes(machine.mem_read(source, size))))
        machine.mem_write(destination, patches[-1][1])
        # Discard Unicorn's translated blocks just as the target invalidates
        # instruction caches before entering the patched Cocos module.
        machine.ctl_remove_cache(destination, destination + size)
        case.result(0)

    def flush():
        flushes.append((calls.reg("R0"), calls.reg("R1")))
        case.result(0)

    def log():
        messages.append(case.string(calls.reg("R1")))
        case.result(0)

    for symbol, callback in (("loader_get_proc_address", get_proc), ("kuKernelCpuUnrestrictedMemcpy", copy),
                             ("kuKernelFlushCaches", flush), ("log_i", log), ("log_e", log)):
        calls.register(target.symbols[symbol], callback)
    explicit = game.symbols.get("_ZN7cocos2d9Texture2D13initWithImageEPNS_5ImageENS0_11PixelFormatE")
    wrapper = game.symbols.get("_ZN7cocos2d9Texture2D13initWithImageEPNS_5ImageE")
    if not explicit:
        explicit, wrapper = bias + 0xf4bb04 + 1, bias + 0xa38598 + 1
    # NDK default wrappers can branch through an internal PLT thunk. Supply its
    # single named relocation as the loader would; no external import or game
    # constructor is executed by this fixture.
    names = data[strings[4]:strings[4] + strings[5]]
    for section in sections:
        if section[1] != 9:  # SHT_REL
            continue
        for offset in range(section[4], section[4] + section[5], section[9]):
            destination, info = struct.unpack_from("<II", data, offset)
            if info & 255 not in (21, 22):  # R_ARM_GLOB_DAT / R_ARM_JUMP_SLOT
                continue
            name = struct.unpack_from("<I", data, symbols[4] + (info >> 8) * symbols[9])[0]
            name = names[name:names.index(b"\0", name)]
            if name == b"_ZN7cocos2d9Texture2D13initWithImageEPNS_5ImageENS0_11PixelFormatE":
                calls.word(bias + destination, explicit)
    original = explicit & ~1
    original_bytes = bytes(machine.mem_read(original, 16))
    baseline = prologue_state(case, original, original + 16)
    assert calls.call(target.symbols["bridge_cocos_configure_textures"], library, instruction_limit=20000000) == 1, (
        path, messages)
    trampoline = target.symbols["__texture_trampoline"] & ~1
    assert [address for address, _ in patches] == [trampoline, original], (path, patches)
    assert len(patches[0][1]) == 20
    assert patches[1][1] == struct.pack("<II", 0xf000f8df, target.symbols["__from_image_with_format"])
    assert flushes == [(trampoline, 20)]
    assert struct.unpack("<I", machine.mem_read(target.symbols["__init"], 4))[0] == trampoline | 1
    assert prologue_state(case, trampoline, original + 16) == baseline, path
    cow = machine.mem_read(target.symbols["__cow"], 1)[0]
    logical = machine.mem_read(target.symbols["__logical"], 1)[0]
    mode = "gnu-cow" if cow else "7.0-long" if logical else "ndk-long"

    # Run the actual patched explicit overload and the game's default wrapper.
    # The modeled original body restores its native epilogue after executing the
    # real generated prologue; image/GPU initialization itself stays modeled.
    saved = {}

    def enter_trampoline():
        saved.update((name, calls.reg(name)) for name in ["R" + str(index) for index in range(4, 12)] + ["SP", "LR"])
        return False

    def original_body():
        case.initialize()
        for name, value in saved.items():
            machine.reg_write(getattr(registers, "UC_ARM_REG_" + name), value)

    calls.register(trampoline, enter_trampoline)
    calls.register(original + 8, original_body)
    image, packed = encoded_image((1001, 1801))
    case.run(mode, "a.png", image, packed, (1001, 1801), configured=True, entry=explicit, pixel_format=27)
    wrapper_address = wrapper & ~1
    wrapper_code = bytes(machine.mem_read(wrapper_address, 16))
    assert wrapper_code[:6] in (b"\x02\x4b\x7b\x44\x1a\x68", b"\x02\x4a\x7a\x44\x12\x68"), path
    displacement = struct.unpack_from("<I", wrapper_code, 12)[0]
    default_format = (wrapper_address + 6 + displacement) & 0xffffffff
    calls.word(default_format, 23)
    case.run(mode, "a.png", image, packed, (1001, 1801), configured=True, entry=wrapper,
             default_wrapper=True, pixel_format=23)
    case.run(mode, "a.png", image, packed, configured=True, entry=explicit, init_ok=False)

    # A modified real image-init body must fail the fingerprint gate before any
    # trampoline or entry patch is written, and must clear the prior callback.
    machine.mem_write(original, original_bytes[:8] + bytes([original_bytes[8] ^ 1]))
    patches.clear()
    assert calls.call(target.symbols["bridge_cocos_configure_textures"], library, instruction_limit=20000000) == 0
    assert not patches
    assert struct.unpack("<I", machine.mem_read(target.symbols["__init"], 4))[0] == 0
    print(f"Local {path.parent.name}: real ABI fingerprint/configuration, generated trampoline register/stack equivalence, "
          "explicit/default entry paths, PixelFormat preservation and modified-code rejection passed.", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, default=root / "build/VitaArc")
    parser.add_argument("--game-dir", type=Path, help="Optional local directory containing arcaea_*/libcocos2dcpp.so")
    parser.add_argument("--game", type=Path, action="append", default=[], help="Optional local game SO; may be repeated")
    args = parser.parse_args()
    check_hook(args.elf)
    games = args.game + (sorted(args.game_dir.glob("*/libcocos2dcpp.so")) if args.game_dir else [])
    for game in games:
        check_library(args.elf, game)


if __name__ == "__main__":
    main()
