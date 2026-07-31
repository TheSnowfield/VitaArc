"""Validate guest thread attributes using real SDK attribute code and modeled scheduling.

Independent ARM/Thumb callers replace only native create/join/TLS endpoints.
The target conversion/errno/resolver and native attribute allocations/getters/
setters/destruction execute, with actual host threads checked separately.
"""

import struct

from arm_fixture_build import root, compile_source, link_fixture
from check_arm_bridges import ArmCalls, ArmImage
from check_arm_native_vm import Platform
from unicorn import arm_const as registers


def main():
    output = root / "build/arm-thread-attributes"
    output.mkdir(parents=True, exist_ok=True)
    for mode in ("arm", "thumb"):
        source = compile_source(root / "tests/test_arm_thread_attributes.c", output / (mode + ".o"), mode="-m" + mode)
        path = output / (mode + ".elf")
        link_fixture([source], path, "test_arm_thread_attributes_entry",
                     exports=["test_native_create", "test_native_join", "thread_native_sizes", "thread_attributes_guard", "thread_id_guard"])
        calls = ArmCalls()
        target = ArmImage(calls.machine, root / "build/VitaArc")
        fixture = ArmImage(calls.machine, path)
        platform = Platform(calls, target)
        assert struct.unpack("<4I", calls.machine.mem_read(fixture.symbols["thread_native_sizes"], 16)) == (4, 4, 4, 24)
        for suffix in ("init", "destroy", "getdetachstate", "setdetachstate", "getstacksize", "setstacksize",
                       "getguardsize", "setguardsize", "getstack", "setstack", "getschedpolicy", "setschedpolicy",
                       "getschedparam", "setschedparam", "getscope", "setscope"):
            name = "pthread_attr_" + suffix
            calls.machine.mem_write(calls.SCRATCH + 0x800, name.encode() + b"\0")
            assert calls.call(target.symbols["loader_get_bridge_proc_address"], calls.SCRATCH + 0x800) == target.symbols["bridge_guest_" + name]

        def create():
            calls.machine.reg_write(registers.UC_ARM_REG_PC, fixture.symbols["test_native_create"])
            return False

        def join():
            calls.machine.reg_write(registers.UC_ARM_REG_PC, fixture.symbols["test_native_join"])
            return False

        def delete_key():
            platform.keys.pop(calls.reg("R0"))
            platform.success()

        calls.register(target.symbols["pthread_create"], create)
        calls.register(target.symbols["pthread_join"], join)
        # Guest equality must compare ID values without dereferencing them,
        # including zero and unmapped equal sentinels in the independent caller.
        self_address = calls.SCRATCH + 0x1000
        calls.register(target.symbols["pthread_self"], lambda: platform.return_value(self_address))
        calls.register(target.symbols["pthread_key_delete"], delete_key)
        functions = [target.symbols["pthread_attr_" + name] for name in ("getdetachstate", "getstacksize", "getguardsize", "getscope")]
        calls.machine.mem_write(calls.SCRATCH, struct.pack("<4I", *functions))
        try:
            result = calls.call(fixture.symbols["test_arm_thread_attributes_entry"], target.symbols["loader_get_bridge_proc_address"], calls.SCRATCH, instruction_limit=2000000)
        except Exception:
            pc = calls.reg("PC")
            symbols = {**target.symbols, **fixture.symbols}
            print("ARM thread stop:", {name: hex(calls.reg(name)) for name in ("PC", "LR", "R0", "R1", "R2", "R3")},
                  max((address, name) for name, address in symbols.items() if (address & ~1) <= pc))
            raise
        assert result == 0, f"{mode} thread attributes failed at C line {result}"
        assert not platform.allocations and not platform.locks and not platform.keys
        # Native pthread_attr_init's malloc failure leaves the guest ID intact.
        attributes = calls.SCRATCH + 128
        tid = calls.SCRATCH + 192
        calls.machine.mem_write(attributes, struct.pack("<6I", 1, 0, 1024 * 1024, 0, 0, 0))
        calls.word(tid, 0xaabbccdd)
        calls.register(target.symbols["malloc"], lambda: platform.return_value(0))
        result = calls.call(target.symbols["bridge_guest_pthread_create"], tid, attributes, 0, 0)
        assert result == 22
        result = calls.call(target.symbols["bridge_guest_pthread_create"], tid, attributes, fixture.symbols["test_native_join"], 0)
        assert result == 12
        assert struct.unpack("<I", calls.machine.mem_read(tid, 4))[0] == 0xaabbccdd
        assert not platform.allocations and not platform.locks
    print("Production ARM/Thumb thread attributes passed: 24-byte guest vs native pointer ABI, real SDK attr helpers/allocations, opposite detach/scope constants, native minimum-stack failure, unsupported guard/custom-stack/realtime requests, errno/canaries/OOM/create rollback and 4-byte ID/key calls (scheduling/TLS endpoints modeled).")


if __name__ == "__main__":
    main()
