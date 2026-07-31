"""Execute selected production ARM bridge paths from the actual linked ELF.

Vita platform endpoints are intercepted before execution. This checks lookup,
ABI adaptation and optional real-game vtable call sites; it is not game boot,
execution of the production loader, or graphics/audio hardware validation.
"""

import argparse
from pathlib import Path
import struct
import sys

root = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(root / "build/arm-tools"))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn import arm_const as registers


class ArmImage:
    def __init__(self, machine, path, bias=0):
        data = Path(path).read_bytes()
        header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
        assert header[0][:6] == b"\x7fELF\x01\x01" and header[2] == 40
        segments = []
        for index in range(header[10]):
            segment = struct.unpack_from("<IIIIIIII", data, header[5] + index * header[9])
            if segment[0] == 1:
                segments.append(segment)
        start = min(segment[2] + bias for segment in segments) & ~4095
        end = (max(segment[2] + bias + segment[5] for segment in segments) + 4095) & ~4095
        machine.mem_map(start, end - start)
        for _, offset, address, _, size, _, _, _ in segments:
            machine.mem_write(address + bias, data[offset:offset + size])
        sections = [struct.unpack_from("<IIIIIIIIII", data, header[6] + index * header[11])
                    for index in range(header[12])]
        self.symbols = {}
        for section in sections:
            if section[1] not in (2, 11):  # SYMTAB or DYNSYM
                continue
            strings = sections[section[6]]
            names = data[strings[4]:strings[4] + strings[5]]
            for offset in range(section[4], section[4] + section[5], section[9]):
                name, value, _, _, _, owner = struct.unpack_from("<IIIBBH", data, offset)
                if owner and name:
                    end = names.index(b"\0", name)
                    self.symbols[names[name:end].decode()] = value + bias


class ArmCalls:
    STACK = 0x200000
    SCRATCH = 0x220000
    RETURN = 0x230000

    def __init__(self):
        self.machine = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        self.machine.mem_map(self.STACK, 0x10000)
        self.machine.mem_map(self.SCRATCH, 0x10000)
        self.machine.mem_map(self.RETURN, 0x1000)
        self.machine.reg_write(registers.UC_ARM_REG_C1_C0_2, 0xF00000)
        self.machine.reg_write(registers.UC_ARM_REG_FPEXC, 0x40000000)
        self.endpoints = {}
        self.machine.hook_add(UC_HOOK_CODE, self._endpoint)

    def _endpoint(self, machine, address, size, context):
        callback = self.endpoints.get(address)
        if callback:
            if callback() is not False:
                machine.reg_write(registers.UC_ARM_REG_PC, machine.reg_read(registers.UC_ARM_REG_LR))

    def register(self, address, callback):
        self.endpoints[address & ~1] = callback

    def reg(self, name):
        return self.machine.reg_read(getattr(registers, "UC_ARM_REG_" + name))

    def word(self, address, value):
        self.machine.mem_write(address, struct.pack("<I", value))

    def call(self, address, *arguments, instruction_limit=200000):
        stack = self.STACK + 0x10000 - 64
        self.machine.reg_write(registers.UC_ARM_REG_SP, stack)
        self.machine.reg_write(registers.UC_ARM_REG_LR, self.RETURN | 1)
        for index in range(4):
            self.machine.reg_write(getattr(registers, f"UC_ARM_REG_R{index}"),
                                   arguments[index] if index < len(arguments) else 0)
        for index, value in enumerate(arguments[4:]):
            self.word(stack + index * 4, value)
        for index in range(16):
            self.machine.reg_write(getattr(registers, f"UC_ARM_REG_S{index}"), 0xdeadbeef)
        self.machine.emu_start(address, self.RETURN, count=instruction_limit)
        assert self.reg("PC") == self.RETURN, "ARM function did not return"
        return self.reg("R0")


def float_bits(value):
    return struct.unpack("<I", struct.pack("<f", value))[0]


def check_graphics(calls, image):
    def lookup(name):
        calls.machine.mem_write(calls.SCRATCH, name.encode() + b"\0")
        return calls.call(image.symbols["bridge_egl_get_proc_address"], calls.SCRATCH)

    count = 0
    for line in (root / "src/bridges/symbol.def").read_text().splitlines():
        if not line.startswith(("gl", "egl")):
            continue
        name, callback = line.split("|")
        assert lookup(name) == image.symbols[callback], name
        count += 1
    for alias, canonical in (
            ("glGenVertexArraysOES", "glGenVertexArrays"),
            ("glBindVertexArrayOES", "glBindVertexArray"),
            ("glDeleteVertexArraysOES", "glDeleteVertexArrays"),
            ("glMapBuffer", "glMapBufferOES"),
            ("glBindFramebufferOES", "glBindFramebuffer")):
        assert lookup(alias) == lookup(canonical), alias
    assert lookup("glDoesNotExist") == 0
    assert lookup("malloc") == 0
    assert calls.call(image.symbols["bridge_egl_get_proc_address"], 0) == 0

    for name, floats, location in (
            ("glClearColor", (0.125, 0.25, 0.5, 1.0), None),
            ("glClearDepthf", (0.75,), None),
            ("glLineWidth", (2.5,), None),
            ("glUniform2f", (1.25, -2.5), 7),
            ("glUniform4f", (1.25, -2.5, 3.75, 0.5), 11)):
        seen = []
        expected = [float_bits(value) for value in floats]

        def endpoint():
            assert [calls.reg(f"S{index}") for index in range(len(expected))] == expected, name
            if location is not None:
                assert calls.reg("R0") == location, name
            seen.append(True)

        calls.register(image.symbols[name], endpoint)
        arguments = expected if location is None else [location, *expected]
        calls.call(lookup(name), *arguments)
        assert len(seen) == 1, name
        calls.endpoints.clear()
    print(f"Production ARM GL passed: {count} canonical lookups, extension aliases and five base-PCS float adapters.")


def check_game_audio(calls, target, path):
    game = ArmImage(calls.machine, path, 0x98000000)
    manager, provider, state = calls.SCRATCH + 0x1000, calls.SCRATCH + 0x2000, calls.SCRATCH + 0x3000
    calls.word(manager + 0x1e4, provider)  # AudioManager 1.6.1c provider member
    calls.word(provider, target.symbols["vita_audio_provider_vtable"] + 8)
    calls.word(provider + 4, state)
    seen = []

    def get_position():
        assert calls.reg("R0") == state and calls.reg("R1") == 0
        calls.machine.reg_write(registers.UC_ARM_REG_R0, 12345)
        seen.append("get")

    def seek_position():
        assert (calls.reg("R0"), calls.reg("R1"), calls.reg("R2")) == (state, 56789, 0)
        calls.machine.reg_write(registers.UC_ARM_REG_R0, 1)
        seen.append("seek")

    calls.register(target.symbols["vita_audio_get_bgm_position"], get_position)
    calls.register(target.symbols["vita_audio_seek_bgm"], seek_position)
    assert calls.call(game.symbols["_ZN12AudioManager14getBGMPositionEv"], manager) == 12345
    calls.call(game.symbols["_ZN12AudioManager19debugSetBGMPositionEi"], manager, 56789)
    assert seen == ["get", "seek"]
    calls.endpoints.clear()

    def channel_volume():
        assert (calls.reg("R0"), calls.reg("R1"), calls.reg("S0")) == (state, 2, float_bits(0.25))
        seen.append("volume")

    calls.register(target.symbols["vita_audio_set_channel_bgm_volume"], channel_volume)
    vtable_slot = target.symbols["vita_audio_provider_vtable"] + 8 + 9 * 4
    volume_entry = struct.unpack("<I", calls.machine.mem_read(vtable_slot, 4))[0]
    calls.call(volume_entry, provider, float_bits(0.25), 2)
    assert seen == ["get", "seek", "volume"]
    calls.endpoints.clear()

    # This 1.6.1c path also calls unrelated std::string/Node scheduling code.
    # Intercept those calls, and supply its external stack guard via the GOT;
    # the manager's volume, seconds-to-ms conversion and playback code execute.
    internal = game.symbols["_ZN12AudioManager26internal_playBGMForChannelEbif"] & ~1
    guard_delta = struct.unpack("<I", calls.machine.mem_read(internal + 0x90, 4))[0]
    guard_slot = internal + 0x16 + guard_delta
    calls.word(guard_slot, calls.SCRATCH + 0x4000)
    calls.word(calls.SCRATCH + 0x4000, 0x12345678)
    for address in (0x98000000 + 0x62642c, 0x98000000 + 0x623f34,
                    game.symbols["_ZN7cocos2d4Node10unscheduleERKSs"]):
        calls.register(address, lambda: None)
    playback = []

    def volume_before_play():
        playback.append(("volume", calls.reg("R0"), calls.reg("R1"), calls.reg("S0")))

    def play():
        playback.append(("play", calls.reg("R0"), calls.reg("R1"), calls.reg("R2"), calls.reg("R3")))

    calls.register(target.symbols["vita_audio_set_channel_bgm_volume"], volume_before_play)
    calls.register(target.symbols["vita_audio_play_bgm"], play)
    calls.call(game.symbols["_ZN12AudioManager26internal_playBGMForChannelEbif"],
               manager, 1, 1, float_bits(0.375))
    calls.call(game.symbols["_ZN12AudioManager14playSpecialBGMEv"], manager)
    assert playback == [
        ("volume", state, 1, float_bits(1.0)), ("play", state, 1, 1, 375),
        ("volume", state, 2, float_bits(1.0)), ("play", state, 0, 2, 0),
    ], playback
    calls.endpoints.clear()
    print("Real-game ARM audio passed: get/debug-set, actual provider vtable, per-channel volume ABI, seconds-to-ms delay and special BGM channel 2.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, default=root / "build/VitaArc")
    parser.add_argument("--game", type=Path, help="Optional local 1.6.1c libcocos2dcpp.so")
    args = parser.parse_args()
    calls = ArmCalls()
    image = ArmImage(calls.machine, args.elf)
    check_graphics(calls, image)
    if args.game:
        check_game_audio(calls, image, args.game)


if __name__ == "__main__":
    main()
