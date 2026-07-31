"""Execute the actual ARM guest nanosleep export with timed kernel endpoints."""
import struct
from arm_fixture_build import root
from check_arm_bridges import ArmCalls, ArmImage
from unicorn import arm_const as regs


def check(request, elapsed, expected, *, fail_at=1, extra=0, regress=False, alias=False):
    calls = ArmCalls()
    image = ArmImage(calls.machine, root / "build/VitaArc")
    address = calls.SCRATCH + 0x100
    remaining = address if alias else calls.SCRATCH + 0x200
    errno_address = calls.SCRATCH + 0x300
    canary = b"guard!!!"
    calls.machine.mem_write(address, struct.pack("<ii", *request) + canary)
    if not alias:
        calls.machine.mem_write(remaining, b"\x55" * 8 + canary)
    clock = (1 << 40) + 12345
    delays = []

    def current_time():
        calls.machine.reg_write(regs.UC_ARM_REG_R0, clock & 0xffffffff)
        calls.machine.reg_write(regs.UC_ARM_REG_R1, clock >> 32)

    def delay():
        nonlocal clock
        duration = calls.reg("R0")
        delays.append(duration)
        assert duration <= 3601000000
        failed = len(delays) == fail_at
        clock += (-elapsed if regress else elapsed) if failed else duration + extra
        calls.machine.reg_write(regs.UC_ARM_REG_R0, 0xffffff85 if failed else 0)

    def native_error():
        assert calls.reg("R0") == 0xffffff85 and calls.reg("R1") == 0
        calls.machine.reg_write(regs.UC_ARM_REG_R0, 4)  # native EINTR

    calls.register(image.symbols["sceKernelGetProcessTimeWide"], current_time)
    calls.register(image.symbols["sceKernelDelayThread"], delay)
    calls.register(image.symbols["__vita_sce_errno_to_errno"], native_error)
    calls.register(image.symbols["__errno"], lambda: calls.machine.reg_write(regs.UC_ARM_REG_R0, errno_address))
    result = calls.call(image.symbols["bridge_guest_nanosleep"], address, remaining)
    assert result == 0xffffffff
    assert struct.unpack("<ii", calls.machine.mem_read(remaining, 8)) == expected
    assert bytes(calls.machine.mem_read(remaining + 8, 8)) == canary
    assert bytes(calls.machine.mem_read(address + 8, 8)) == canary
    assert struct.unpack("<i", calls.machine.mem_read(errno_address, 4))[0] == 4


def main():
    check((2, 100000000), 750000, (1, 350000000))
    check((0, 500000000), 500000, (0, 0))
    check((0, 500000000), 700000, (0, 0))
    check((3601, 250000000), 500000, (0, 750000000), fail_at=2)
    check((3601, 250000000), 500000, (0, 500000000), fail_at=2, extra=250000)
    check((2, 100000000), 750000, (2, 100000000), regress=True)
    check((0, 90000), 50, (0, 40000), alias=True)
    check((2147483647, 1), 1250, (2147483646, 998750001))
    print("Linked ARM nanosleep passed 8 cases: actual guest export, 64-bit clock, elapsed remainder, chunk oversleep, clamp, regression, alias, time_t limit and canaries.")


if __name__ == "__main__":
    main()
