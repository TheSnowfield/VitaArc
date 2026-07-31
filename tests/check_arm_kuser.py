"""Check linked ARM Linux helper ABI and optional real-game literal repair.

Exclusive-store contention is injected for the retry cases. This does not
establish actual Vita scheduling or cache coherence between hardware cores.
"""

import argparse
from pathlib import Path
import re
import struct
import subprocess

from arm_fixture_build import root, sdk
from check_arm_bridges import ArmCalls, ArmImage
from unicorn import arm_const as registers


def check_abi():
    calls = ArmCalls()
    image = ArmImage(calls.machine, root / "build/VitaArc")
    compare = image.symbols["bridge_arm_kuser_cmpxchg"]
    barrier = image.symbols["bridge_arm_kuser_memory_barrier"]
    destination = calls.SCRATCH
    preserved = {f"R{i}": 0x12340000 + i for i in range(4, 12)}

    def run(initial, old, new, success):
        calls.word(destination, initial)
        for name, value in preserved.items():
            calls.machine.reg_write(getattr(registers, "UC_ARM_REG_" + name), value)
        result = calls.call(compare, old, new, destination)
        assert (result == 0) == success
        assert (calls.reg("CPSR") >> 29 & 1) == success
        assert calls.reg("R1") == new and calls.reg("R2") == destination
        assert all(calls.reg(name) == value for name, value in preserved.items())
        return struct.unpack("<I", calls.machine.mem_read(destination, 4))[0]

    assert run(0xffffffff, 0xffffffff, 0, True) == 0
    assert run(0, 1, 0xdeadbeef, False) == 0
    assert run(0x80000000, 0x80000000, 0x80000000, True) == 0x80000000

    # Independent GNU disassembly locates the actual exclusive store. Force
    # one store-exclusive failure, then execute the target's real retry loop.
    disassembly = subprocess.check_output([
        str(sdk / "bin/arm-vita-eabi-objdump"), "-d",
        "--disassemble=bridge_arm_kuser_cmpxchg", str(root / "build/VitaArc")], text=True)
    matches = re.findall(r"^\s*([0-9a-f]+):.*\bstrex\s+ip, r1, \[r2\]", disassembly, re.M)
    assert len(matches) == 1, disassembly
    store = int(matches[0], 16)
    attempts = 0
    interfere = False

    def exclusive_store():
        nonlocal attempts
        attempts += 1
        if attempts == 1:
            if interfere:
                calls.word(destination, 99)
            calls.machine.reg_write(registers.UC_ARM_REG_R12, 1)
            calls.machine.reg_write(registers.UC_ARM_REG_PC, (store + 4) | 1)
        return False

    calls.register(store, exclusive_store)
    assert run(17, 17, 23, True) == 23 and attempts == 2
    attempts = 0
    interfere = True
    assert run(17, 17, 23, False) == 99 and attempts == 1
    del calls.endpoints[store]

    saved = {f"R{i}": 0x98760000 + i for i in range(4, 13)}
    for name, value in saved.items():
        calls.machine.reg_write(getattr(registers, "UC_ARM_REG_" + name), value)
    flags = calls.reg("CPSR") & 0xf0000000
    args = (0x12345678, 0x23456789, 0x3456789a, 0x456789ab)
    calls.call(barrier, *args)
    assert all(calls.reg(f"R{i}") == value for i, value in enumerate(args))
    assert all(calls.reg(name) == value for name, value in saved.items())
    assert calls.reg("CPSR") & 0xf0000000 == flags
    assert "dmb\tish" in disassembly or re.search(r"\bdmb\s+ish", disassembly)
    print("ARM kuser ABI passed: success/failure carry and results, preserved registers, "
          "real exclusive instructions, forced retries/interference and register-preserving barrier.", flush=True)


def check_game(path):
    from check_arm_loader import LoaderCase
    data = Path(path).read_bytes()
    case = LoaderCase({"app0:/game.so": data})
    library = case.load("app0:/game.so")
    assert library, case.platform.messages
    anchor = case.export(library, "__gnu_uldivmod_helper")
    group = anchor + 60
    header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
    programs = [struct.unpack_from("<IIIIIIII", data, header[5] + i * header[9])
                for i in range(header[10])]
    relative = group - 0x98000000
    segment = next(p for p in programs if p[0] == 1 and p[2] <= relative < p[2] + p[4])
    offset = segment[1] + relative - segment[2]
    # Both audited libgcc profiles terminate after the last helper literal.
    literal_addresses = [p[2] + i for p in programs if p[0] == 1 and p[6] & 1
                         for i in range((-p[2]) & 3, p[4] - 3, 4)
                         if struct.unpack_from('<I', data, p[1] + i)[0] in (0xffff0fc0, 0xffff0fa0)]
    group_size = max(literal_addresses) + 4 - relative
    assert group_size in (0xef0, 0xf68)
    words = struct.unpack_from("<" + "I" * (group_size // 4), data, offset)
    compare_slots = [i * 4 for i, value in enumerate(words) if value == 0xffff0fc0]
    barrier_slots = [i * 4 for i, value in enumerate(words) if value == 0xffff0fa0]
    assert len(compare_slots) == 43 and len(barrier_slots) == 5
    for slots, name in ((compare_slots, "bridge_arm_kuser_cmpxchg"),
                        (barrier_slots, "bridge_arm_kuser_memory_barrier")):
        assert all(case.word(group + i) == case.target.symbols[name] for i in slots)
    case.calls.word(case.calls.SCRATCH, 17)
    assert case.calls.call(group, case.calls.SCRATCH, 0xfffffffd) == 17
    assert case.word(case.calls.SCRATCH) == 14
    assert case.call("bridge_patch_arm_kuser", library) == 1  # Idempotent.
    case.call("loader_free_library", library)
    case.clean()
    print("Real-game helper literals/fetch-add/idempotence passed.", flush=True)

    changed = bytearray(data)
    changed[offset + 8] ^= 1
    rejected = LoaderCase({"app0:/changed.so": bytes(changed)})
    assert rejected.load("app0:/changed.so") == 0
    assert any("Unsupported ARM Linux kuser helper layout" in m for m in rejected.platform.messages)
    rejected.clean()
    print("Unknown libgcc profile rejected with no retained loader resources.", flush=True)

    failed = LoaderCase({"app0:/game.so": data})
    original_copy = failed.platform.kernel_copy

    def fail_second_patch():
        if failed.calls.reg("R0") == group + compare_slots[1]:
            failed.platform.return_value(0xffffffff)
        else:
            original_copy()

    failed.calls.register(failed.target.symbols["kuKernelCpuUnrestrictedMemcpy"], fail_second_patch)
    assert failed.load("app0:/game.so") == 0
    assert any("Could not patch ARM kuser helper" in m for m in failed.platform.messages), failed.platform.messages
    failed.clean()
    print("Real-game helper repair passed: all 43 compare/5 barrier literals, actual "
          "libgcc fetch-add, idempotence, profile rejection and partial-write failure cleanup.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, help="Optional local 1.6.1c or 1.9.0c game SO")
    args = parser.parse_args()
    check_abi()
    if args.game:
        check_game(args.game)


if __name__ == "__main__":
    main()
