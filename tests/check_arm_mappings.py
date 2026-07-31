"""Execute production ARM mappings, SDK descriptors and positional file reads.

Only the Vita memory/file/mutex/kubridge endpoints are mocked. This checks the
linked bridge and native descriptor ownership; it does not prove kernel page
protection or physical storage behavior. Partial holes retain their memblock.
"""

import struct

from check_arm_loader import LoaderCase, LoaderPlatform
from check_arm_bridges import ArmCalls, ArmImage
from arm_fixture_build import root
from unicorn import UC_PROT_READ


class MappingPlatform(LoaderPlatform):
    def __init__(self, calls, target, files):
        super().__init__(calls, target, files)
        self.types = []
        self.reads = []
        self.fail_read = False
        self.interrupt = False
        calls.register(target.symbols["sceIoPread"], self.pread)
        calls.register(target.symbols["sceIoGetstatByFd"], self.fstat)
        for name in ("sceKernelLockLwMutex", "sceKernelUnlockLwMutex"):
            calls.register(target.symbols[name], self.success)

    def allocate_image(self):
        kind = self.calls.reg("R1")
        self.types.append(kind)
        super().allocate_image()
        if kind == 0x0c20d040 and self.calls.reg("R0") in self.blocks:
            base, size, _ = self.blocks[self.calls.reg("R0")]
            self.machine.mem_protect(base, size, UC_PROT_READ)

    def fstat(self):
        data = self.handles[self.calls.reg("R0")][0]
        output = bytearray(88)
        struct.pack_into("<I", output, 0, 0o20000 | 0o644)
        struct.pack_into("<q", output, 8, len(data))
        self.machine.mem_write(self.calls.reg("R1"), bytes(output))
        self.return_value(0)

    def pread(self):
        offset = struct.unpack("<q", self.machine.mem_read(self.calls.reg("SP"), 8))[0]
        self.reads.append(offset)
        if self.fail_read or self.interrupt:
            self.return_value(0x80010004 if self.interrupt else 0x80010005)
            self.interrupt = False
            return
        data = self.handles[self.calls.reg("R0")][0]
        if offset >= 5 * 1024**3:
            data = b"64-bit-tail"[offset - 5 * 1024**3:]
        else:
            data = data[offset:]
        data = data[:min(self.calls.reg("R2"), 97)]  # Short positional reads.
        self.machine.mem_write(self.calls.reg("R1"), data)
        self.return_value(len(data))


data = bytes((index * 17 + index // 4096) & 255 for index in range(3 * 4096))


class MappingCase(LoaderCase):
    def __init__(self):
        self.calls = ArmCalls()
        self.target = ArmImage(self.calls.machine, root / "build/VitaArc")
        self.platform = MappingPlatform(self.calls, self.target, {"app0:/mapped.bin": data})


case = MappingCase()
platform = case.platform
call = case.call
PAGE, FAILED, RW, PRIVATE, ANON = 4096, 0xffffffff, 3, 2, 0x22


def error():
    return case.word(call("__errno"))


def anonymous(length):
    return call("bridge_mmap", 0, length, RW, ANON, FAILED, 0)


for name, bridge in (("mmap", "bridge_mmap"), ("__mmap2", "bridge_mmap2"),
                     ("munmap", "bridge_munmap"), ("mremap", "bridge_mremap")):
    assert call("loader_get_bridge_proc_address", case.string(name)) == case.target.symbols["bridge_guest_" + bridge.removeprefix("bridge_")]

assert anonymous(0) == FAILED and error() == 22
assert anonymous(0xffffffff) == FAILED and error() == 12
base = anonymous(PAGE * 3 - 1)
assert base != FAILED and base % PAGE == 0
assert bytes(case.calls.machine.mem_read(base, PAGE * 3)) == bytes(PAGE * 3)
case.calls.machine.mem_write(base, b"a" * PAGE + b"b" * PAGE + b"c" * PAGE)
assert call("bridge_munmap", base + PAGE, 1) == 0
assert call("bridge_mremap", base, PAGE * 3, PAGE * 4, 1) == FAILED and error() == 14
assert call("bridge_mremap", base, PAGE, PAGE * 2, 0) == base
assert bytes(case.calls.machine.mem_read(base + PAGE, PAGE)) == bytes(PAGE)
assert call("bridge_mremap", base, PAGE * 2, PAGE * 4, 0) == FAILED and error() == 12
moved = call("bridge_mremap", base, PAGE * 2, PAGE * 4, 1)
assert moved != FAILED and moved != base
assert bytes(case.calls.machine.mem_read(moved, PAGE * 4)) == b"a" * PAGE + bytes(PAGE * 3)
assert bytes(case.calls.machine.mem_read(base + PAGE * 2, PAGE)) == b"c" * PAGE
assert call("bridge_munmap", base, PAGE * 3) == 0
assert call("bridge_munmap", moved, PAGE * 4) == 0
assert call("bridge_munmap", moved, PAGE * 4) == 0
case.clean()

for failure in ("allocation", "base", "copy"):
    if failure == "allocation":
        platform.fail_allocation = platform.allocation_calls + 1
    elif failure == "base":
        platform.fail_base_at = platform.base_calls + 1
    else:
        platform.fail_copy = platform.copy_calls + 1
    assert anonymous(PAGE) == FAILED
    case.clean()

fd = call("bridge_open", case.string("app0:/mapped.bin"), 0, 0)
assert fd != FAILED
native = next(iter(platform.handles))
platform.handles[native][1] = 23
platform.interrupt = True
base = call("bridge_mmap", 0, PAGE, RW, PRIVATE, fd, PAGE)
assert base != FAILED and bytes(case.calls.machine.mem_read(base, PAGE)) == data[PAGE:PAGE * 2]
assert platform.handles[native][1] == 23
case.calls.machine.mem_write(base, b"edited")
assert platform.handles[native][0] == data
platform.fail_read = True
assert call("bridge_mremap", base, PAGE, PAGE * 2, 1) == FAILED and error() == 5
assert bytes(case.calls.machine.mem_read(base, 6)) == b"edited"
platform.fail_read = False
assert call("bridge_close", fd) == 0
assert platform.handles  # Mapping duplicate still owns the file.
moved = call("bridge_mremap", base, PAGE, PAGE * 2, 1)
assert moved != FAILED and bytes(case.calls.machine.mem_read(moved, 6)) == b"edited"
assert bytes(case.calls.machine.mem_read(moved + PAGE, PAGE)) == data[PAGE * 2:]
assert call("bridge_munmap", moved, PAGE * 2) == 0
case.clean()

fd = call("bridge_open", case.string("app0:/mapped.bin"), 0, 0)
base = call("bridge_mmap2", 0, PAGE, 1, PRIVATE, fd, (5 * 1024**3) // PAGE)
assert base != FAILED and platform.types[-1] == 0x0c20d040
assert 5 * 1024**3 in platform.reads
assert bytes(case.calls.machine.mem_read(base, PAGE)) == b"64-bit-tail" + bytes(PAGE - 11)
assert call("bridge_close", fd) == 0
assert call("bridge_munmap", base, PAGE) == 0
case.clean()
table = struct.unpack("<256I", case.calls.machine.mem_read(case.target.symbols["__vita_fdmap"], 256 * 4))
assert not any(table), "mapping duplicate descriptor leak"
assert case.word(case.target.symbols["__mappings"]) == 0
print("Production ARM mappings passed: pages/partial ranges, moved growth, rollback, kernel failures, private files/offsets, SDK duplicate lifetime, read-only type and >4 GiB mmap2 (platform mocked).")
