"""Execute the production loader, not Python ELF relocation, in its target ELF.

Only Vita I/O, memory allocation, cache and kubridge endpoints are replaced.
Fixture segments are mapped by calls from loader_load_library, with RX/RW
permissions, poisoned initial memory and tracked handles. This is instruction
validation; it does not establish kubridge installation, GPU/audio or game boot.
"""

import argparse
from pathlib import Path
import struct
import subprocess

from arm_fixture_build import compiler, root
from check_arm_bridges import ArmCalls, ArmImage
from check_arm_native_vm import Platform
from unicorn import UC_PROT_READ, UC_PROT_WRITE, UC_PROT_EXEC
from unicorn import arm_const as registers


class LoaderPlatform(Platform):
    def __init__(self, calls, target, files):
        super().__init__(calls, target)
        self.files = files
        self.handles = {}
        self.blocks = {}
        self.next_handle = 10
        self.next_block = 100
        self.image_cursor = 0x40000000
        self.flushes = []
        self.messages = []
        self.allocation_calls = 0
        self.fail_allocation = 0
        self.fail_base = False
        self.base_calls = 0
        self.fail_base_at = 0
        self.wrong_base = False
        self.short_read = False
        self.open_calls = 0
        self.fail_open = 0
        self.fail_seek = False
        self.size_override = None
        self.copy_calls = 0
        self.fail_copy = 0
        for name, callback in (
                ("sceIoGetstat", self.stat), ("sceIoOpen", self.open),
                ("sceIoClose", self.close), ("sceIoRead", self.read), ("sceIoLseek", self.seek),
                ("sceKernelAllocMemBlock", self.allocate_image), ("kuKernelAllocMemBlock", self.allocate_segment),
                ("sceKernelGetMemBlockBase", self.block_base), ("sceKernelFreeMemBlock", self.release_block),
                ("kuKernelCpuUnrestrictedMemcpy", self.kernel_copy), ("kuKernelFlushCaches", self.flush)):
            calls.register(target.symbols[name], callback)
        for name in ("log_v", "log_i", "log_w", "log_e", "log_f"):
            calls.register(target.symbols[name], self.log)

    def string(self, address):
        if not address:
            return "(null)"
        data = bytearray()
        while True:
            value = bytes(self.machine.mem_read(address + len(data), 1))
            if value == b"\0":
                return data.decode(errors="replace")
            data.extend(value)
            assert len(data) < 4096

    def log(self):
        fmt = self.string(self.calls.reg("R1"))
        if "Unresolved required import" in fmt:
            fmt += " " + self.string(self.calls.reg("R2"))
        self.messages.append(fmt)
        self.return_value(0)

    def stat(self):
        data = self.files.get(self.string(self.calls.reg("R0")))
        if data is None:
            self.return_value(0x80010002)
            return
        output = bytearray(88)
        struct.pack_into("<I", output, 0, 0o20000 | 0o644)
        struct.pack_into("<q", output, 8, len(data))
        self.machine.mem_write(self.calls.reg("R1"), bytes(output))
        self.return_value(0)

    def open(self):
        self.open_calls += 1
        path = self.string(self.calls.reg("R0"))
        if path not in self.files or self.open_calls == self.fail_open:
            self.return_value(0x80010002)
            return
        handle = self.next_handle
        self.next_handle += 1
        self.handles[handle] = [self.files[path], 0]
        self.return_value(handle)

    def close(self):
        self.handles.pop(self.calls.reg("R0"))
        self.return_value(0)

    def seek(self):
        if self.fail_seek:
            self.return_value(0x80010005)
            self.machine.reg_write(registers.UC_ARM_REG_R1, 0xffffffff)
            return
        handle = self.handles[self.calls.reg("R0")]
        offset = self.calls.reg("R2") | self.calls.reg("R3") << 32
        if offset & (1 << 63):
            offset -= 1 << 64
        whence = struct.unpack("<I", self.machine.mem_read(self.calls.reg("SP"), 4))[0]
        handle[1] = offset + (len(handle[0]) if whence == 2 else handle[1] if whence == 1 else 0)
        if whence == 2 and self.size_override is not None:
            handle[1] = self.size_override
        self.return_value(handle[1] & 0xffffffff)
        self.machine.reg_write(registers.UC_ARM_REG_R1, handle[1] >> 32)

    def read(self):
        handle = self.handles[self.calls.reg("R0")]
        count = self.calls.reg("R2")
        data = handle[0][handle[1]:handle[1] + count]
        if self.short_read and data:
            data = data[:-1]
        self.machine.mem_write(self.calls.reg("R1"), data)
        handle[1] += len(data)
        self.return_value(len(data))

    def allocate_block(self, segment):
        self.allocation_calls += 1
        if self.allocation_calls == self.fail_allocation:
            self.return_value(0x8001000c)
            return
        size = self.calls.reg("R2")
        assert size and not size % 4096 and size < 0x10000000
        if segment:
            options = self.calls.reg("R3")
            base = struct.unpack("<I", self.machine.mem_read(options + 12, 4))[0]
        else:
            base = self.image_cursor
            self.image_cursor += size + 4096
        name = self.string(self.calls.reg("R0"))
        permissions = UC_PROT_READ | (UC_PROT_EXEC if name == "elf_rx" else UC_PROT_WRITE)
        self.machine.mem_map(base, size, permissions)
        self.machine.mem_write(base, b"\xa5" * size)
        handle = self.next_block
        self.next_block += 1
        self.blocks[handle] = (base, size, name)
        self.return_value(handle)

    def allocate_image(self):
        self.allocate_block(False)

    def allocate_segment(self):
        self.allocate_block(True)

    def block_base(self):
        self.base_calls += 1
        if self.fail_base or self.base_calls == self.fail_base_at:
            self.return_value(0x8001000c)
            return
        base = self.blocks[self.calls.reg("R0")][0]
        if self.wrong_base and self.base_calls > 1:
            base += 4096
        self.calls.word(self.calls.reg("R1"), base)
        self.return_value(0)

    def release_block(self):
        base, size, _ = self.blocks.pop(self.calls.reg("R0"))
        self.machine.mem_unmap(base, size)
        self.return_value(0)

    def kernel_copy(self):
        self.copy_calls += 1
        if self.copy_calls == self.fail_copy:
            self.return_value(0x80010005)
            return
        self.copy()
        self.return_value(0)

    def flush(self):
        self.flushes.append((self.calls.reg("R0"), self.calls.reg("R1")))
        self.return_value(0)


def build_fixtures():
    output = root / "build/arm-loader"
    output.mkdir(parents=True, exist_ok=True)
    result = {}
    for name, define in (("provider", "LOADER_PROVIDER"), ("consumer", None), ("missing", "LOADER_MISSING_IMPORT"), ("cocos-cleanup", None)):
        path = output / (name + ".so")
        command = [compiler, "-O2", "-Wall", "-Wextra", "-Werror", "-mthumb", "-fPIC", "-fno-builtin",
                   "-nostdlib", "-shared", "-Wl,--hash-style=sysv,-z,max-page-size=4096",
                   "-Wl,-soname," + path.name,
                   str(root / ("tests/test_cocos_cleanup_fixture.c" if name == "cocos-cleanup" else "tests/test_loader_fixture.c")),
                   "-o", str(path)]
        if define:
            command.insert(1, "-D" + define)
        if name == "provider":
            command.insert(1, "-Wl,--defsym,fixture_absolute=0x1234")
        subprocess.run(command, check=True)
        result["app0:/" + path.name] = path.read_bytes()
    return result


class LoaderCase:
    def __init__(self, files):
        self.calls = ArmCalls()
        self.target = ArmImage(self.calls.machine, root / "build/VitaArc")
        self.platform = LoaderPlatform(self.calls, self.target, files)

    def call(self, name, *args):
        return self.calls.call(self.target.symbols[name], *args, instruction_limit=200000000)

    def string(self, value):
        self.calls.machine.mem_write(self.calls.SCRATCH, value.encode() + b"\0")
        return self.calls.SCRATCH

    def load(self, name):
        return self.call("loader_load_library", self.string(name))

    def export(self, library, name):
        return self.call("loader_get_proc_address", library, self.string(name))

    def word(self, address):
        return struct.unpack("<I", self.calls.machine.mem_read(address, 4))[0]

    def clean(self):
        assert not self.platform.blocks, f"{len(self.platform.blocks)} native memblocks remain"
        assert not self.platform.handles, f"{len(self.platform.handles)} file handles remain"
        assert not self.platform.allocations, f"{len(self.platform.allocations)} heap allocations remain"
        assert not self.platform.locks
        assert self.call("loader_find_empty_slot") == 0


def check_fixture(files):
    case = LoaderCase(files)
    provider = case.load("app0:/provider.so")
    assert provider, case.platform.messages
    absolute = case.export(provider, "fixture_absolute")
    assert absolute == 0x1234, f"SHN_ABS export incorrectly biased: {absolute:#x}"
    assert case.calls.call(case.export(provider, "provider_absolute_address")) == 0x1234
    consumer = case.load("app0:/consumer.so")
    assert consumer, case.platform.messages
    assert case.calls.call(case.export(consumer, "fixture_absolute_address")) == 0x1234
    assert case.calls.call(case.export(consumer, "fixture_run"), 5) == 90
    trace = case.export(provider, "provider_trace")
    assert case.word(trace) == 0
    assert case.call("loader_init_library", consumer) == 1
    assert case.call("loader_init_library", consumer) == 1
    assert case.word(trace) == 12
    destroyed = case.export(provider, "provider_destroyed")
    assert case.load("app0:/consumer.so") == consumer
    case.call("loader_free_library", consumer)
    assert case.word(trace) == 12
    assert case.word(destroyed) == 0
    case.call("loader_free_library", consumer)
    assert case.word(trace) == 1243
    assert case.word(destroyed) == 128
    assert case.word(case.export(provider, "provider_bad_order")) == 0
    assert case.call("loader_find_library", case.string("consumer.so")) == 0
    case.call("loader_free_library", provider)
    assert case.platform.flushes
    case.clean()
    bare = dict(files)
    bare["bare.so"] = files["app0:/provider.so"]
    case = LoaderCase(bare)
    library = case.load("bare.so")
    assert library and case.load("bare.so") == library
    assert case.call("loader_find_library", case.string("bare.so")) == library
    case.call("loader_free_library", library)
    case.call("loader_free_library", library)
    case.clean()
    print("Production ARM loader passed: mapped RX/RW segments, BSS, relative/absolute/import/weak relocations, linked code, constructors/finalizers, handle reuse and cleanup.", flush=True)


def check_cocos_cleanup(files):
    for foreign in ("none", "application", "provider"):
        case = LoaderCase(files)
        provider = case.load("app0:/provider.so")
        library = case.load("app0:/cocos-cleanup.so")
        case.call("bridge_patch_cocos2dx", library)
        provider_deletes = []

        def observe_provider_delete():
            provider_deletes.append(case.calls.reg("R0"))
            return False

        case.calls.register(case.target.symbols["audio_provider_delete"], observe_provider_delete)
        assert case.call("loader_init_library", library) == 1
        trace = case.export(provider, "provider_trace")
        assert case.word(trace) == 1
        if foreign == "application":
            application = case.word(case.export(library, "_ZN7cocos2d11Application21sm_pSharedApplicationE"))
            case.calls.word(application, case.export(library, "_ZTV11AppDelegate"))
        elif foreign == "provider":
            case.calls.call(case.export(library, "set_foreign_provider"))
        assert case.load("app0:/cocos-cleanup.so") == library
        case.call("loader_free_library", library)
        assert case.word(trace) == 1
        case.call("loader_free_library", library)
        assert case.word(trace) == {"none": 1345, "application": 1425, "provider": 13465}[foreign]
        assert len(provider_deletes) == (1 if foreign == "none" else 0)
        case.call("loader_free_library", provider)
        case.clean()
    case = LoaderCase(files)
    library = case.load("app0:/cocos-cleanup.so")
    # Missing required provider imports reject the library before any ctor.
    assert library == 0
    case.clean()
    print("Production ARM Cocos cleanup passed: final handle, pre-fini ordering, owned deleting destructor/factory API and foreign-vtable preservation.", flush=True)


def check_failures(files):
    for path in ("app0:/", "app0:/" + "a" * 64, "app0:/" + "a/" * 64 + "provider.so"):
        case = LoaderCase({path: files["app0:/provider.so"]})
        assert case.load(path) == 0
        assert case.platform.allocation_calls == 0
        case.clean()
    case = LoaderCase(files)
    assert case.load("app0:/absent.so") == 0
    case.clean()
    for open_call in (1, 2):
        case = LoaderCase(files)
        case.platform.fail_open = open_call
        assert case.load("app0:/provider.so") == 0
        case.clean()
    for large_size in (-1, 5 * 1024**3):
        case = LoaderCase(files)
        if large_size == -1:
            case.platform.fail_seek = True
        else:
            case.platform.size_override = large_size
        assert case.load("app0:/provider.so") == 0
        assert case.platform.allocation_calls == 0
        case.clean()
    case.platform.short_read = True
    assert case.load("app0:/provider.so") == 0
    case.clean()
    for allocation in (1, 2, 3):
        case = LoaderCase(files)
        case.platform.fail_allocation = allocation
        assert case.load("app0:/provider.so") == 0
        case.clean()
    case = LoaderCase(files)
    case.platform.fail_base = True
    assert case.load("app0:/provider.so") == 0
    case.clean()
    for wrong in (False, True):
        case = LoaderCase(files)
        case.platform.fail_base_at = 0 if wrong else 2
        case.platform.wrong_base = wrong
        assert case.load("app0:/provider.so") == 0
        case.clean()
    baseline = LoaderCase(files)
    provider = baseline.load("app0:/provider.so")
    internal_copies = baseline.platform.copy_calls
    consumer = baseline.load("app0:/consumer.so")
    all_copies = baseline.platform.copy_calls
    baseline.call("loader_free_library", consumer)
    baseline.call("loader_free_library", provider)
    baseline.clean()
    for copy_call in (1, 5, internal_copies):
        case = LoaderCase(files)
        case.platform.fail_copy = copy_call
        assert case.load("app0:/provider.so") == 0
        case.clean()
    case = LoaderCase(files)
    provider = case.load("app0:/provider.so")
    case.platform.fail_copy = all_copies
    assert case.load("app0:/consumer.so") == 0
    case.call("loader_free_library", provider)
    case.clean()
    case = LoaderCase(files)
    provider = case.load("app0:/provider.so")
    assert case.load("app0:/missing.so") == 0
    assert any("required_missing_function" in line for line in case.platform.messages)
    case.call("loader_free_library", provider)
    case.clean()
    print("Production ARM loader failure cleanup passed: open/seek/large-size/read/allocation/base/memset/copy/relocation/import failures.", flush=True)


def check_malformed(files):
    data = files["app0:/consumer.so"]
    header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
    sections = [struct.unpack_from("<IIIIIIIIII", data, header[6] + index * header[11])
                for index in range(header[12])]
    names = sections[header[13]]
    strings = data[names[4]:names[4] + names[5]]
    lookup = {strings[section[0]:].split(b"\0", 1)[0].decode(): index for index, section in enumerate(sections)}
    programs = [(header[5] + index * header[9], struct.unpack_from("<IIIIIIII", data, header[5] + index * header[9]))
                for index in range(header[10])]
    load = next(offset for offset, program in programs if program[0] == 1)
    dynsym = sections[lookup[".dynsym"]]
    dynstr = sections[lookup[".dynstr"]]
    rel = sections[lookup[".rel.dyn"]]

    def section_offset(name, field):
        return header[6] + lookup[name] * header[11] + field

    mutations = [
        ("endian", 5, "B", 2), ("version", 20, "I", 0),
        ("program-table-overflow", 28, "I", 0xfffffff0),
        ("section-table-overflow", 32, "I", 0xfffffff0),
        ("program-entry-size", 42, "H", 0), ("section-entry-size", 46, "H", 0),
        ("section-name-range", header[6] + header[11], "I", 0xffffffff),
        ("section-string-range", header[6] + header[13] * header[11] + 16, "I", len(data) - 1),
        ("segment-file-range", load + 4, "I", len(data)),
        ("segment-filesz-memsz", load + 16, "I", 0xffff0000),
        ("segment-address-overflow", load + 8, "I", 0xfffff000),
        ("symbol-entry-size", section_offset(".dynsym", 36), "I", 4),
        ("symbol-name-range", dynsym[4] + 16, "I", dynstr[5]),
        ("symbol-string-link", section_offset(".dynsym", 24), "I", 0),
        ("relocation-entry-size", section_offset(".rel.dyn", 36), "I", 4),
        ("relocation-target-range", rel[4], "I", 0x02000000),
        ("relocation-symbol-range", rel[4] + 4, "I", 0xffffff02),
        ("init-array-size", section_offset(".init_array", 20), "I", 3),
    ]
    for name, offset, fmt, value in mutations:
        changed = bytearray(data)
        struct.pack_into("<" + fmt, changed, offset, value)
        case = LoaderCase({"app0:/bad.so": bytes(changed)})
        assert case.load("app0:/bad.so") == 0, name
        assert case.platform.allocation_calls == 1, (name, "mapped before validation")
        assert any("Invalid ARM ELF" in line for line in case.platform.messages), (name, case.platform.messages)
        case.clean()
    case = LoaderCase({"app0:/bad.so": data[:20]})
    assert case.load("app0:/bad.so") == 0
    case.clean()
    print(f"Production ARM loader rejected {len(mutations) + 1} malformed ELF cases before segment mapping.", flush=True)


def check_game_relocations(case, library, data):
    """Independent scalar relocation oracle using the original file addends."""
    header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
    sections = [struct.unpack_from("<IIIIIIIIII", data, header[6] + index * header[11])
                for index in range(header[12])]
    programs = [struct.unpack_from("<IIIIIIII", data, header[5] + index * header[9])
                for index in range(header[10])]
    symbols = next(section for section in sections if section[1] == 11)
    strings = sections[symbols[6]]
    names = data[strings[4]:strings[4] + strings[5]]
    table = [struct.unpack_from("<IIIBBH", data, offset)
             for offset in range(symbols[4], symbols[4] + symbols[5], symbols[9])]

    def name(symbol):
        return names[symbol[0]:].split(b"\0", 1)[0].decode()

    entry = next(symbol for symbol in table if name(symbol) == "JNI_OnLoad")
    bias = case.export(library, "JNI_OnLoad") - entry[1]
    imports = {}
    checked = 0
    for section in sections:
        if section[1] != 9:  # SHT_REL
            continue
        for position in range(section[4], section[4] + section[5], section[9]):
            offset, info = struct.unpack_from("<II", data, position)
            kind, index = info & 255, info >> 8
            if kind == 0:
                continue
            program = next(program for program in programs
                           if program[0] == 1 and program[2] <= offset < program[2] + program[5])
            within = offset - program[2]
            addend = struct.unpack_from("<I", data, program[1] + within)[0] if within + 4 <= program[4] else 0
            symbol = table[index]
            if kind == 23:  # R_ARM_RELATIVE
                expected = bias + addend
            else:
                if symbol[5]:
                    address = symbol[1] if symbol[5] == 0xfff1 else bias + symbol[1]
                else:
                    imported = name(symbol)
                    if imported not in imports:
                        imports[imported] = case.call("loader_get_import_proc_address", library, case.string(imported))
                    address = imports[imported]
                    assert address or symbol[3] >> 4 == 2, ("unresolved strong import", imported)
                assert kind in (2, 21, 22), ("unsupported oracle relocation", kind)
                expected = address + addend if kind == 2 else address
            assert case.word(bias + offset) == expected & 0xffffffff, ("relocation mismatch", hex(offset), kind)
            checked += 1
    print(f"Independent real-game oracle matched all {checked} applied relocations and {len(imports)} distinct imports.", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, help="Optional local game SO; load and relocate without constructors")
    args = parser.parse_args()
    files = build_fixtures()
    check_fixture(files)
    check_cocos_cleanup(files)
    check_failures(files)
    check_malformed(files)
    if args.game:
        files = {"app0:/libcocos2dcpp.so": args.game.read_bytes()}
        case = LoaderCase(files)
        library = case.load("app0:/libcocos2dcpp.so")
        assert library, case.platform.messages
        assert case.export(library, "JNI_OnLoad")
        check_game_relocations(case, library, files["app0:/libcocos2dcpp.so"])
        case.call("loader_free_library", library)
        case.clean()
        print("Production loader mapped/relocated the actual game SO and resolved JNI_OnLoad; constructors and game boot were not executed.", flush=True)


if __name__ == "__main__":
    main()
