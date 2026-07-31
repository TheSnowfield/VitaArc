"""Run the linked production ARM JNI VM and native dispatch with platform stubs.

Only allocation, logging, pthread/TLS and Vita memory endpoints are replaced.
The runtime, reference collector, JNI table and native trampoline execute from
build/VitaArc. This is single-threaded instruction validation, not a Vita boot.
"""

import argparse
from pathlib import Path
import subprocess
import sys

from check_arm_bridges import ArmCalls, ArmImage
from arm_fixture_build import compile_source, link_fixture, root
from unicorn import arm_const as registers


class Platform:
    HEAP = 0x30000000
    CAPACITY = 16 * 1024 * 1024

    def __init__(self, calls, image):
        self.calls = calls
        self.machine = calls.machine
        self.machine.mem_map(self.HEAP, self.CAPACITY)
        self.cursor = self.HEAP
        self.allocations = {}
        self.keys = {}
        self.next_key = 1
        self.once = set()
        self.locks = set()
        self.image = image
        for name, callback in (
                ("malloc", self.malloc), ("calloc", self.calloc), ("free", self.free),
                ("realloc", self.realloc), ("pthread_once", self.initialize_once),
                ("pthread_key_create", self.create_key), ("pthread_getspecific", self.get_key),
                ("pthread_setspecific", self.set_key), ("pthread_mutex_lock", self.lock),
                ("pthread_mutex_unlock", self.unlock), ("pthread_cond_broadcast", self.success),
                ("sceClibMemcpy", self.copy), ("sceClibMemmove", self.copy), ("sceClibMemset", self.fill)):
            calls.register(image.symbols[name], callback)
        for name in ("log_v", "log_i", "log_w", "log_e", "log_f", "log_printf"):
            calls.register(image.symbols[name], self.success)
        # Newlib requests its per-thread errno/locale state through Vita TLS.
        for name in ("__getreent", "__getreent_for_thread"):
            if name in image.symbols:
                calls.register(image.symbols[name], self.get_reent)

    def return_value(self, value):
        self.machine.reg_write(registers.UC_ARM_REG_R0, value)

    def allocate(self, size):
        size = max(size, 1)
        address = self.cursor
        self.cursor += (size + 15) & ~15
        assert self.cursor <= self.HEAP + self.CAPACITY, "fixture heap exhausted"
        self.allocations[address] = size
        return address

    def malloc(self):
        self.return_value(self.allocate(self.calls.reg("R0")))

    def calloc(self):
        size = self.calls.reg("R0") * self.calls.reg("R1")
        address = self.allocate(size)
        self.machine.mem_write(address, bytes(size))
        self.return_value(address)

    def release(self, address):
        if address:
            size = self.allocations.pop(address)
            self.machine.mem_write(address, b"\xdd" * size)

    def free(self):
        self.release(self.calls.reg("R0"))

    def realloc(self):
        previous, size = self.calls.reg("R0"), self.calls.reg("R1")
        address = self.allocate(size)
        if previous:
            self.machine.mem_write(address, bytes(self.machine.mem_read(previous, min(size, self.allocations[previous]))))
            self.release(previous)
        self.return_value(address)

    def initialize_once(self):
        control = self.calls.reg("R0")
        if control not in self.once:
            self.once.add(control)
            # The initializer returns straight to pthread_once's caller.
            self.machine.reg_write(registers.UC_ARM_REG_PC, self.calls.reg("R1"))
            return False
        self.success()

    def create_key(self):
        self.calls.word(self.calls.reg("R0"), self.next_key)
        self.next_key += 1
        self.success()

    def get_key(self):
        self.return_value(self.keys.get(self.calls.reg("R0"), 0))

    def set_key(self):
        self.keys[self.calls.reg("R0")] = self.calls.reg("R1")
        self.success()

    def lock(self):
        address = self.calls.reg("R0")
        assert address not in self.locks, "runtime lock was acquired recursively in native reentry"
        self.locks.add(address)
        self.success()

    def unlock(self):
        self.locks.remove(self.calls.reg("R0"))
        self.success()

    def copy(self):
        destination, source, size = (self.calls.reg(f"R{index}") for index in range(3))
        if size:
            self.machine.mem_write(destination, bytes(self.machine.mem_read(source, size)))
        self.return_value(destination)

    def fill(self):
        destination, value, size = (self.calls.reg(f"R{index}") for index in range(3))
        self.machine.mem_write(destination, bytes([value & 255]) * size)
        self.return_value(destination)

    def success(self):
        self.return_value(0)

    def get_reent(self):
        self.return_value(self.calls.SCRATCH + 0x5000)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, default=root / "build/VitaArc")
    args = parser.parse_args()
    output = root / "build/arm-native-vm"
    output.mkdir(parents=True, exist_ok=True)
    fixture = output / "native-fixture.c"
    subprocess.run([sys.executable, str(root / "tests/generate_native_fixture.py"), str(fixture)], check=True)
    sources = [root / "tests/test_arm_native_vm.c", fixture,
               root / "src/bridges/jni/descriptor.c", root / "tests/test_arm_memory.c"]
    objects = [compile_source(source, output / (source.stem + ".o")) for source in sources]
    executable = output / "native-vm.elf"
    link_fixture(objects, executable, "test_arm_native_vm_entry")
    calls = ArmCalls()
    target = ArmImage(calls.machine, args.elf)
    fixture_image = ArmImage(calls.machine, executable)
    platform = Platform(calls, target)

    def assertion_failed():
        expression = bytearray()
        cursor = calls.reg("R3")
        while cursor and len(expression) < 2048:
            value = calls.machine.mem_read(cursor, 1)[0]
            if not value:
                break
            expression.append(value)
            cursor += 1
        raise AssertionError(f"ARM C assertion line {calls.reg('R1')}: {expression.decode()}")

    calls.register(fixture_image.symbols["__assert_func"], assertion_failed)
    env = calls.call(target.symbols["jni_runtime_get_env"])
    assert env
    try:
        result = calls.call(fixture_image.symbols["test_arm_native_vm_entry"], env,
                            target.symbols["jni_runtime_get_stats"], instruction_limit=10000000)
    except Exception:
        state = {name: hex(calls.reg(name)) for name in ("PC", "LR", "SP", "R0", "R1", "R2", "R3")}
        print("ARM JNI stop:", state, flush=True)
        raise
    assert result == 0, f"production ARM JNI native fixture failed case {result}"
    assert not platform.allocations, f"ARM VM teardown leaked {len(platform.allocations)} mock allocations"
    assert not platform.locks and not any(platform.keys.values())
    print("Production ARM JNI VM passed: all scalar returns, mixed A/V/variadic calls, object frames/GC, exceptions, 255-slot calls and complete VM allocation cleanup (platform APIs mocked).")


if __name__ == "__main__":
    main()
