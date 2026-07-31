"""Execute production ARM poll/errno adapters and SDK descriptor pins.

Network epoll, monotonic time and delay endpoints are mocked. Tests exercise
event masks, duplicates, deadlines, interruption, failures and real grab/drop
lifetime. They do not establish physical networking or Vita scheduling.
"""

import struct

from arm_fixture_build import root
from check_arm_bridges import ArmCalls, ArmImage
from check_arm_native_vm import Platform
from unicorn import arm_const as registers


class PollPlatform(Platform):
    def __init__(self, calls, target):
        super().__init__(calls, target)
        self.clock = 0
        self.live = {}
        self.next_epoll = 1000
        self.ready = {}
        self.waits = []
        self.delays = []
        self.controls = []
        self.destroyed = []
        self.failure = ""
        self.early = False
        self.interrupt_sleep = False
        self.close_during_wait = False
        self.closed_sockets = []
        self.expected = {}
        self.error_address = 0
        for name, callback in (("sceNetEpollCreate", self.create), ("sceNetEpollControl", self.control),
                               ("sceNetEpollWait", self.wait), ("sceNetEpollDestroy", self.destroy),
                               ("sceKernelGetProcessTimeWide", self.time), ("sceKernelDelayThread", self.delay),
                               ("sceNetSocketClose", self.socket_close), ("calloc", self.calloc)):
            calls.register(target.symbols[name], callback)
        for name in ("sceKernelLockLwMutex", "sceKernelUnlockLwMutex"):
            calls.register(target.symbols[name], self.success)

    def calloc(self):
        if self.failure == "allocation":
            self.calls.word(self.error_address, 12)
            self.return_value(0)
        else:
            super().calloc()

    def time(self):
        self.return_value(self.clock & 0xffffffff)
        self.machine.reg_write(registers.UC_ARM_REG_R1, self.clock >> 32)

    def delay(self):
        duration = self.calls.reg("R0")
        self.delays.append(duration)
        if self.interrupt_sleep:
            self.return_value(0x80010004)
        else:
            self.clock += duration
            self.return_value(0)

    def create(self):
        if self.failure == "create":
            self.return_value(0x8041010c)
            return
        eid = self.next_epoll
        self.next_epoll += 1
        self.live[eid] = {}
        self.return_value(eid)

    def control(self):
        eid, operation, uid, address = (self.calls.reg("R" + str(index)) for index in range(4))
        assert operation == 1
        raw = bytes(self.machine.mem_read(address, 32))
        interest, slot = struct.unpack_from("<I", raw)[0], struct.unpack_from("<i", raw, 24)[0]
        assert raw[4:24] == bytes(20) and raw[28:] == bytes(4)
        assert uid not in self.live[eid], "duplicate native socket registration"
        if self.failure == "control" and self.live[eid]:
            self.return_value(0x80410116)
            return
        self.live[eid][uid] = (slot, interest)
        self.controls.append((uid, slot, interest))
        self.return_value(0)

    def wait(self):
        eid, output, maximum, timeout = (self.calls.reg("R" + str(index)) for index in range(4))
        timeout = timeout if timeout < 0x80000000 else timeout - 0x100000000
        self.waits.append(timeout)
        assert maximum == len(self.live[eid])
        for pointer, expected in self.expected.items():
            actual = struct.unpack("<i", self.machine.mem_read(pointer + 8, 4))[0]
            assert actual == expected, (hex(pointer), actual, expected)
        if self.close_during_wait:
            self.close_during_wait = False
            # Model the other thread dropping its fd 9 ownership, while poll
            # holds the remaining pin. Real SDK drop must close after destroy.
            table = self.image.symbols["__vita_fdmap"]
            descriptor = struct.unpack("<I", self.machine.mem_read(table + 9 * 4, 4))[0]
            self.calls.word(table + 9 * 4, 0)
            current = struct.unpack("<i", self.machine.mem_read(descriptor + 8, 4))[0]
            self.calls.word(descriptor + 8, current - 1)
        if self.failure in ("wait", "wait_destroy") or (self.failure == "infinite" and len(self.waits) == 2):
            self.return_value(0x80410104)
            return
        if self.failure == "bad_count":
            self.return_value(maximum + 1)
            return
        if self.failure == "bad_slot":
            self.machine.mem_write(output, struct.pack("<6IiI", 1, 0, 0, 0, 0, 0, maximum, 0))
            self.return_value(1)
            return
        if self.early:
            self.early = False
            self.clock += 10000
            self.return_value(0)
            return
        count = 0
        for uid, (slot, interest) in self.live[eid].items():
            returned = self.ready.get(uid, 0) & interest
            if returned:
                self.machine.mem_write(output + count * 32, struct.pack("<6IiI", returned, 0, 0, 0, 0, 0, slot, 0))
                count += 1
        if not count and timeout > 0:
            self.clock += timeout
        self.return_value(count)

    def destroy(self):
        eid = self.calls.reg("R0")
        self.destroyed.append(eid)
        if self.failure in ("destroy", "wait_destroy"):
            self.return_value(0x80410105)
        else:
            self.live.pop(eid)
            self.return_value(0)

    def socket_close(self):
        assert not self.live, "socket was closed before its poll pin was released"
        self.closed_sockets.append(self.calls.reg("R0"))
        self.return_value(0)


calls = ArmCalls()
target = ArmImage(calls.machine, root / "build/VitaArc")
platform = PollPlatform(calls, target)
platform.error_address = calls.call(target.symbols["__errno"])
calls.machine.mem_write(calls.SCRATCH, b"poll\0")
entry = calls.call(target.symbols["loader_get_bridge_proc_address"], calls.SCRATCH)
assert entry == target.symbols["bridge_guest_poll"]
table, pool = target.symbols["__vita_fdmap"], target.symbols["__vita_fdmap_pool"]
initial = {}
for fd, uid, kind in ((7, 100, 2), (9, 101, 2), (10, 200, 0), (11, 0, 127), (12, 201, 1), (13, 300, 4)):
    pointer = pool + fd * 20
    references = 2 if fd == 7 else 1
    calls.machine.mem_write(pointer, struct.pack("<iB3xiII", uid, kind, references, 0, 0))
    calls.word(table + fd * 4, pointer)
    initial[pointer] = references
calls.word(table + 8 * 4, pool + 7 * 20)  # A real dup shares its SDK descriptor.


def poll(entries, timeout, expected, returned, error=123456):
    platform.expected = dict(initial)
    for fd, _ in entries:
        if 0 <= fd < 256:
            pointer = struct.unpack("<I", calls.machine.mem_read(table + fd * 4, 4))[0]
            if pointer:
                platform.expected[pointer] += 1
    platform.waits.clear()
    platform.delays.clear()
    platform.controls.clear()
    platform.destroyed.clear()
    output = calls.SCRATCH + 0x1000
    calls.machine.mem_write(output - 8, b"q" * (len(entries) * 8 + 16))
    for index, (fd, events) in enumerate(entries):
        calls.machine.mem_write(output + index * 8, struct.pack("<ihh", fd, events, 0x7777))
    calls.word(platform.error_address, 123456)
    result = calls.call(entry, output, len(entries), timeout & 0xffffffff, instruction_limit=200000000)
    assert result == expected & 0xffffffff, (hex(result), expected)
    actual = [struct.unpack("<h", calls.machine.mem_read(output + index * 8 + 6, 2))[0] for index in range(len(entries))]
    assert actual == returned, (actual, returned)
    assert struct.unpack("<I", calls.machine.mem_read(platform.error_address, 4))[0] == error
    assert bytes(calls.machine.mem_read(output - 8, 8)) == b"q" * 8
    assert bytes(calls.machine.mem_read(output + len(entries) * 8, 8)) == b"q" * 8
    assert not platform.allocations and not platform.locks
    for pointer, references in initial.items():
        actual = struct.unpack("<i", calls.machine.mem_read(pointer + 8, 4))[0]
        # fd 9 may have been closed by the modeled other thread.
        if calls.machine.mem_read(table + 9 * 4, 4) == bytes(4) and pointer == pool + 9 * 20:
            assert actual == 0
        else:
            assert actual == references, (hex(pointer), actual, references)


platform.ready = {100: 3, 101: 16}
poll([(7, 1), (8, 4), (7, 64), (9, 0)], 5000, 4, [1, 4, 64, 16])
assert platform.controls == [(100, 0, 27), (101, 1, 24)] and platform.waits == [5000000]
platform.ready = {}
poll([(10, 5), (7, 1), (99, 0), (-1, 1)], 5000, 2, [5, 0, 32, 0])
assert platform.waits == [0]
poll([(11, 1), (12, 4)], -1, 2, [1, 4])
assert not platform.controls and not platform.waits
poll([(11, 4)], 10, 0, [0])
assert sum(platform.delays) == 10000
platform.ready = {100: 2}
poll([(7, 256)], 0, 1, [256])
platform.ready = {100: 8 | 16}
poll([(7, 0)], 0, 1, [24])
platform.ready = {100: 1}
poll([(7, 1)], -2, 1, [1])
assert platform.waits == [-1]
platform.ready = {}
platform.early = True
poll([(7, 1)], 50, 0, [0])
assert platform.waits == [50000, 40000]
poll([(7, 1)], 0x7fffffff, 0, [0])
assert len(platform.waits) == 1000 and set(platform.waits) == {0x7fffffff}
poll([], 0x7fffffff, 0, [])
assert sum(platform.delays) == 0x7fffffff * 1000 and all(0 < delay <= 0xffffffff for delay in platform.delays)
platform.interrupt_sleep = True
poll([], -1, -1, [], 4)
platform.interrupt_sleep = False
poll([(-1, 1)] * 256, 0, 0, [0] * 256)
for events in (2, 128, 512, 8192):
    poll([(7, events)], 0, -1, [0], 95)
poll([(13, 1)], 0, -1, [0], 95)
for failure, code in (("allocation", 12), ("create", 12), ("control", 22), ("wait", 4),
                      ("bad_count", 5), ("bad_slot", 5), ("destroy", 5), ("wait_destroy", 4), ("infinite", 4)):
    platform.failure = failure
    poll([(7, 1), (9, 4)], -1 if failure == "infinite" else 10, -1, [0, 0], code)
    if platform.live:
        # Failed destruction leaves the kernel object's state unknown. Report
        # the failure, then release the test backend explicitly for isolation.
        assert failure in ("destroy", "wait_destroy")
        platform.failure = ""
        for eid in list(platform.live):
            calls.call(target.symbols["sceNetEpollDestroy"], eid)
platform.failure = ""
platform.ready = {101: 1}
platform.close_during_wait = True
poll([(9, 1)], 0, 1, [1])
assert platform.closed_sockets == [101] and not platform.live
assert calls.call(entry, 0, 1, 0) == 0xffffffff
assert struct.unpack("<I", calls.machine.mem_read(platform.error_address, 4))[0] == 14
assert calls.call(entry, 0, 257, 0) == 0xffffffff
assert struct.unpack("<I", calls.machine.mem_read(platform.error_address, 4))[0] == 22
assert not platform.allocations
print("Production ARM poll passed: event/alias/error/HUP masks, duplicate fds, immediate files/RNG/NVAL, deadline/chunk/infinite waits, failures/canaries, Android errno and real SDK pin cleanup (platform endpoints mocked).")
