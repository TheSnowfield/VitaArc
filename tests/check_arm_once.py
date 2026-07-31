"""Check the 4-byte guest once ABI against independent ARM/Thumb callers.

The production bridge and SDK cleanup-stack push/pop execute. Allocation,
mutex/TLS endpoints are modeled; actual concurrent scheduling/exit/cancel is
covered by the host test, not this single-threaded ARM instruction fixture.
"""

import struct

from arm_fixture_build import root, compile_source, link_fixture
from check_arm_bridges import ArmCalls, ArmImage
from check_arm_native_vm import Platform


class OncePlatform(Platform):
    def __init__(self, calls, target):
        super().__init__(calls, target)
        self.fail_allocation = False
        self.error_address = 0
        calls.register(target.symbols["calloc"], self.calloc)
        calls.register(target.symbols["pthread_once"], self.native_once)

    def calloc(self):
        if self.fail_allocation:
            self.fail_allocation = False
            self.calls.word(self.error_address, 12)
            self.return_value(0)
        else:
            super().calloc()

    def native_once(self):
        raise AssertionError("guest once was forwarded to the incompatible native control layout")


output = root / "build/arm-once"
output.mkdir(parents=True, exist_ok=True)
for mode in ("arm", "thumb"):
    source = compile_source(root / "tests/test_arm_once.c", output / (mode + ".o"), mode="-m" + mode)
    fixture_path = output / (mode + ".elf")
    link_fixture([source], fixture_path, "test_arm_once_entry",
                 exports=["test_native_once_size", "test_guest_once_size", "once_guard"])
    calls = ArmCalls()
    target = ArmImage(calls.machine, root / "build/VitaArc")
    fixture = ArmImage(calls.machine, fixture_path)
    platform = OncePlatform(calls, target)
    platform.error_address = calls.call(target.symbols["__errno"])
    assert calls.call(fixture.symbols["test_native_once_size"]) == 16
    assert calls.call(fixture.symbols["test_guest_once_size"]) == 4
    calls.machine.mem_write(calls.SCRATCH, b"pthread_once\0")
    entry = calls.call(target.symbols["loader_get_bridge_proc_address"], calls.SCRATCH)
    assert entry == target.symbols["bridge_guest_pthread_once"]
    calls.machine.mem_write(calls.SCRATCH, struct.pack("<II", entry, target.symbols["__errno"]))
    platform.fail_allocation = True
    assert calls.call(fixture.symbols["test_arm_once_entry"], calls.SCRATCH) == 12
    assert struct.unpack("<4I", calls.machine.mem_read(fixture.symbols["once_guard"], 16)) == (0x11223344, 0, 0x55667788, 0x99aabbcc)
    assert not platform.allocations and not platform.locks
    assert calls.call(fixture.symbols["test_arm_once_entry"], calls.SCRATCH) == 0
    assert struct.unpack("<4I", calls.machine.mem_read(fixture.symbols["once_guard"], 16)) == (0x11223344, 2, 0x55667788, 0x99aabbcc)
    assert not platform.allocations and not platform.locks
    assert not any(platform.keys.values()), "native cleanup-stack node left installed"
    assert struct.unpack("<I", calls.machine.mem_read(target.symbols["__once_entries"], 4))[0] == 0
    assert struct.unpack("<I", calls.machine.mem_read(target.symbols["__once_waiters"], 4))[0] == 0
print("Production ARM/Thumb once ABI passed: 4-byte controls vs native 16 bytes, OOM/retry, adjacent canaries, nested initializers, errno, done/reuse and actual SDK cleanup-stack restoration (platform endpoints mocked).")
