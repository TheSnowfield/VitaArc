"""Execute linked ARM directory ABI conversion and real SDK fd ownership.

Vita filesystem, kernel mutex and allocator endpoints are mocked. The target
open/path conversion, directory converter, seek rollback and SDK descriptors
execute as linked. This does not establish physical storage behavior.
"""

import struct

from arm_fixture_build import root
from check_arm_bridges import ArmCalls, ArmImage
from check_arm_loader import LoaderCase, LoaderPlatform

ROOT = "ux0:/vitaarc/persistent/data/list"
ENTRIES = [("file", 0x2000), ("folder", 0x1000), ("link", 0x4000), ("x" * 255, 0x2000), (".", 0x1000), ("..", 0x1000)]


class DirectoryPlatform(LoaderPlatform):
    def __init__(self, calls, target):
        super().__init__(calls, target, {ROOT + "/file": b"contents"})
        self.directories = {}
        self.fail_dopen = False
        self.fail_dread = False
        self.fail_dclose = 0
        self.interrupt = False
        self.malformed = False
        self.fail_calloc = False
        self.native_reads = 0
        self.error_address = 0
        self.stat_paths = []
        for name, callback in (("sceIoDopen", self.dopen), ("sceIoDread", self.dread),
                               ("sceIoDclose", self.dclose), ("calloc", self.calloc)):
            calls.register(target.symbols[name], callback)
        for name in ("sceKernelLockLwMutex", "sceKernelUnlockLwMutex"):
            calls.register(target.symbols[name], self.success)

    def calloc(self):
        if self.fail_calloc:
            self.fail_calloc = False
            self.calls.word(self.error_address, 12)
            self.return_value(0)
            return
        super().calloc()

    def stat(self):
        path = self.string(self.calls.reg("R0"))
        self.stat_paths.append(path)
        mode = 0x1000 if path in (ROOT, "ux0:/vitaarc/persistent/data") else next((kind for name, kind in ENTRIES if path == ROOT + "/" + name), None)
        if mode is None:
            self.return_value(0x80010002)
            return
        output = bytearray(88)
        struct.pack_into("<I", output, 0, mode | 0o700)
        self.machine.mem_write(self.calls.reg("R1"), bytes(output))
        self.return_value(0)

    def dopen(self):
        path = self.string(self.calls.reg("R0"))
        assert path == ROOT, path
        if self.fail_dopen:
            self.return_value(0x80010005)
            return
        handle = self.next_handle
        self.next_handle += 1
        self.directories[handle] = 0
        self.return_value(handle)

    def dread(self):
        self.native_reads += 1
        handle, output = self.calls.reg("R0"), self.calls.reg("R1")
        assert bytes(self.machine.mem_read(output, 352)) == bytes(352), "SceIoDirent must be initialized"
        if self.fail_dread or self.interrupt:
            self.return_value(0x80010004 if self.interrupt else 0x80010005)
            self.interrupt = False
            return
        position = self.directories[handle]
        if position == len(ENTRIES):
            self.return_value(0)
            return
        name, mode = ENTRIES[position]
        self.calls.word(output, mode)
        self.machine.mem_write(output + 88, b"z" * 256 if self.malformed else name.encode() + b"\0")
        self.directories[handle] += 1
        self.return_value(1)

    def dclose(self):
        handle = self.calls.reg("R0")
        if self.fail_dclose == handle:
            self.return_value(0x80010005)
            return
        self.directories.pop(handle)
        self.return_value(0)


class DirectoryCase(LoaderCase):
    def __init__(self):
        self.calls = ArmCalls()
        self.target = ArmImage(self.calls.machine, root / "build/VitaArc")
        self.platform = DirectoryPlatform(self.calls, self.target)
        self.platform.error_address = self.call("__errno")

    def clean(self):
        super().clean()
        assert not self.platform.directories
        table = struct.unpack("<256I", self.calls.machine.mem_read(self.target.symbols["__vita_fdmap"], 1024))
        assert not any(table), "directory SDK descriptor leak"

    def error(self):
        return self.word(self.platform.error_address)

    def check_entry(self, address, index):
        raw = bytes(self.calls.machine.mem_read(address, 280))
        inode, offset, size, kind = struct.unpack_from("<QqHB", raw)
        name = raw[19:].split(b"\0", 1)[0].decode()
        expected, mode = ENTRIES[index]
        assert name == expected and offset == index + 1
        assert kind == {0x2000: 8, 0x1000: 4, 0x4000: 10}[mode]
        assert size == (19 + len(name) + 1 + 7) & ~7
        path = "/data/data/moe.low.arc/list/" + name
        status = self.calls.SCRATCH + 0x3000
        assert self.call("bridge_bionic_lstat", self.string(path), status) == 0
        actual_inode = struct.unpack("<Q", self.calls.machine.mem_read(status + 96, 8))[0]
        assert inode == actual_inode and inode


def main():
    case = DirectoryCase()
    call, platform = case.call, case.platform
    for name, callback in (("opendir", "bridge_opendir"), ("closedir", "bridge_closedir"),
                           ("fdopendir", "bridge_fdopendir"), ("dirfd", "bridge_dirfd"),
                           ("readdir", "bridge_readdir"), ("readdir64", "bridge_readdir"),
                           ("readdir_r", "bridge_readdir_r"), ("readdir64_r", "bridge_readdir_r"),
                           ("rewinddir", "bridge_rewinddir"), ("seekdir", "bridge_seekdir"), ("telldir", "bridge_telldir")):
        assert call("loader_get_bridge_proc_address", case.string(name)) == case.target.symbols["bridge_guest_" + callback.removeprefix("bridge_")]

    directory = call("bridge_opendir", case.string("/data/user/0/moe.low.arc/list"))
    fd = call("bridge_dirfd", directory) if directory else -1
    assert directory and fd >= 0, (hex(directory), fd, case.error(), platform.stat_paths)
    assert call("bridge_fcntl", fd, 1) == 1
    platform.interrupt = True
    for index in range(len(ENTRIES)):
        case.calls.word(platform.error_address, 35)
        entry = call("bridge_readdir", directory)
        assert entry and case.error() == 35
        case.check_entry(entry, index)
    assert call("bridge_readdir", directory) == 0 and case.error() == 35
    assert call("bridge_telldir", directory) == len(ENTRIES)
    call("bridge_seekdir", directory, 1)
    assert call("bridge_telldir", directory) == 1
    case.check_entry(call("bridge_readdir", directory), 1)
    for failure in ("open", "read", "eof", "close"):
        before = dict(platform.directories)
        platform.fail_dopen = failure == "open"
        platform.fail_dread = failure == "read"
        platform.fail_dclose = next(iter(before)) if failure == "close" else 0
        call("bridge_seekdir", directory, 100 if failure == "eof" else 1)
        assert case.error() == (22 if failure == "eof" else 5)
        assert platform.directories == before and call("bridge_telldir", directory) == 2
        platform.fail_dopen = platform.fail_dread = False
        platform.fail_dclose = 0
    case.check_entry(call("bridge_readdir", directory), 2)

    call("bridge_rewinddir", directory)
    output = case.calls.SCRATCH + 0x1000
    result = case.calls.SCRATCH + 0x2000
    case.calls.machine.mem_write(output - 8, b"q" * (280 + 16))
    case.calls.word(platform.error_address, 35)
    assert call("bridge_readdir_r", directory, output, result) == 0 and case.error() == 35
    assert case.word(result) == output
    case.check_entry(output, 0)
    assert bytes(case.calls.machine.mem_read(output - 8, 8)) == b"q" * 8
    assert bytes(case.calls.machine.mem_read(output + 280, 8)) == b"q" * 8
    platform.fail_dread = True
    assert call("bridge_readdir_r", directory, output, result) == 5 and case.error() == 35 and case.word(result) == 0
    platform.fail_dread = False
    platform.malformed = True
    assert call("bridge_readdir", directory) == 0 and case.error() == 91  # SDK ENAMETOOLONG, errno translation remains open.
    assert call("bridge_telldir", directory) == 2  # A malformed consumed record still advances its cookie.
    platform.malformed = False
    reads = platform.native_reads
    case.calls.word(directory + 4, 0x7fffffff)
    assert call("bridge_readdir", directory) == 0 and case.error() == 139 and platform.native_reads == reads
    assert call("bridge_closedir", directory) == 0
    case.clean()

    platform.fail_calloc = True
    assert call("bridge_opendir", case.string("/data/data/moe.low.arc/list")) == 0 and case.error() == 12
    case.clean()
    fd = call("bridge_open", case.string("/data/data/moe.low.arc/list/file"), 0)
    assert fd != 0xffffffff and call("bridge_fdopendir", fd) == 0 and case.error() == 20
    assert platform.handles
    assert call("bridge_close", fd) == 0
    case.clean()
    fd = call("bridge_open", case.string("/data/data/moe.low.arc/list"), 0x4000)
    assert fd != 0xffffffff
    platform.fail_calloc = True
    failed = call("bridge_fdopendir", fd)
    assert failed == 0 and case.error() == 12 and platform.directories, (hex(failed), case.error(), platform.directories)
    directory = call("bridge_fdopendir", fd)
    assert directory and call("bridge_closedir", directory) == 0
    case.clean()
    print("Production ARM directory ABI passed: aliases, 280-byte layout/types/path inodes, descriptor ownership/CLOEXEC, short/long names, EINTR/EOF/error, cookies/rollback, reentrant canaries, OOM and SDK cleanup (platform mocked).")


if __name__ == "__main__":
    main()
