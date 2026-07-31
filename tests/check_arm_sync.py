"""Execute production guest sync/errno/loader code with native pthread endpoints modeled.

SDK attribute helpers and cleanup-stack operations execute unchanged. Native
mutex/condition scheduling is modeled here and tested concurrently on the host.
"""

import struct
import subprocess

from arm_fixture_build import root, compiler, compile_source, link_fixture
from check_arm_bridges import ArmCalls, ArmImage
from check_arm_native_vm import Platform
from check_arm_loader import LoaderCase
from unicorn import arm_const as registers


class NativeSync:
    def __init__(self, platform, constants):
        self.platform = platform
        self.calls, self.image = platform.calls, platform.image
        (self.normal, self.recursive, self.errorcheck, self.busy, self.deadlock,
         self.timeout, self.io, *sizes) = constants
        assert sizes == [4, 4, 4, 4]
        self.mutexes, self.conditions = {}, set()
        self.fail_destroy = False
        self.fail_init = False
        self.clock = 10_000_000
        self.realtime = 1_700_000_000_000_000
        self.waits = []
        for name, callback in (("pthread_mutex_init", self.mutex_init),
                               ("pthread_mutex_destroy", self.mutex_destroy),
                               ("pthread_mutex_lock", self.lock), ("pthread_mutex_trylock", self.trylock),
                               ("pthread_mutex_unlock", self.unlock),
                               ("pthread_cond_init", self.cond_init), ("pthread_cond_destroy", self.cond_destroy),
                               ("pthread_cond_signal", self.pulse), ("pthread_cond_broadcast", self.pulse),
                               ("pthread_cond_timedwait", self.timedwait),
                               ("sceKernelGetProcessTimeWide", self.time), ("gettimeofday", self.walltime)):
            self.calls.register(self.image.symbols[name], callback)

    def word(self, address):
        return struct.unpack("<I", self.calls.machine.mem_read(address, 4))[0]

    def result(self, value):
        self.platform.return_value(value)

    def mutex_init(self):
        address, attributes = self.calls.reg("R0"), self.calls.reg("R1")
        assert address not in self.mutexes
        if self.fail_init:
            self.fail_init = False
            self.result(self.io)
            return
        # Execute actual SDK attr init/settype/destroy, observing its pointer ABI.
        kind = self.word(self.word(attributes) + 4) if attributes else self.normal
        self.mutexes[address] = [kind, 0]
        self.calls.word(address, 0xabc00000)
        self.result(0)

    def mutex_destroy(self):
        address = self.calls.reg("R0")
        assert self.mutexes[address][1] == 0
        if self.fail_destroy:
            self.fail_destroy = False
            self.result(self.io)
        else:
            del self.mutexes[address]
            self.result(0)

    def lock(self):
        address = self.calls.reg("R0")
        if address not in self.mutexes:
            self.platform.lock()
            return
        state = self.mutexes[address]
        if state[1] and state[0] != self.recursive:
            assert state[0] == self.errorcheck
            self.result(self.deadlock)
        else:
            state[1] += 1
            self.result(0)

    def trylock(self):
        state = self.mutexes[self.calls.reg("R0")]
        if state[1] and state[0] != self.recursive:
            self.result(self.busy)
        else:
            state[1] += 1
            self.result(0)

    def unlock(self):
        address = self.calls.reg("R0")
        if address not in self.mutexes:
            self.platform.unlock()
            return
        assert self.mutexes[address][1]
        self.mutexes[address][1] -= 1
        self.result(0)

    def cond_init(self):
        assert self.calls.reg("R1") == 0, "Vita native cond clocks are ignored; bridge must convert deadlines"
        self.conditions.add(self.calls.reg("R0"))
        self.calls.word(self.calls.reg("R0"), 0xdef00000)
        self.result(0)

    def cond_destroy(self):
        self.conditions.remove(self.calls.reg("R0"))
        self.result(0)

    def pulse(self):
        if self.calls.reg("R0") in self.conditions:
            self.result(0)
        else:
            self.platform.success()

    def time(self):
        self.result(self.clock & 0xffffffff)
        self.calls.machine.reg_write(registers.UC_ARM_REG_R1, self.clock >> 32)

    def walltime(self):
        self.calls.machine.mem_write(self.calls.reg("R0"), struct.pack("<ii", self.realtime // 1_000_000, self.realtime % 1_000_000))
        self.result(0)

    def timedwait(self):
        condition, mutex, deadline = (self.calls.reg("R" + str(index)) for index in range(3))
        assert condition in self.conditions and self.mutexes[mutex][1] == 1
        seconds, nanoseconds = struct.unpack("<ii", self.calls.machine.mem_read(deadline, 8))
        duration = seconds * 1_000_000 + nanoseconds // 1000 - self.realtime
        assert 0 <= duration <= 100_000
        self.waits.append(duration)
        self.clock += duration
        # Native timedwait returns after reacquiring the mutex. Realtime can
        # change between waits; the next slice must rebase from monotonic time.
        self.realtime += duration + (7_000_000 if len(self.waits) % 2 else -3_000_000)
        self.result(self.timeout)

    def clean(self):
        assert not self.mutexes and not self.conditions
        assert self.word(self.image.symbols["__sync_entries"]) == 0
        assert not self.platform.locks and not any(self.platform.keys.values())


def main():
    output = root / "build/arm-sync"
    output.mkdir(parents=True, exist_ok=True)
    constants = None
    for mode in ("arm", "thumb"):
        source = compile_source(root / "tests/test_arm_sync.c", output / (mode + ".o"), mode="-m" + mode)
        executable = output / (mode + ".elf")
        link_fixture([source], executable, "test_arm_sync_entry", exports=["sync_native_constants", "sync_guard"])
        calls = ArmCalls()
        target = ArmImage(calls.machine, root / "build/VitaArc")
        fixture = ArmImage(calls.machine, executable)
        constants = struct.unpack("<11I", calls.machine.mem_read(fixture.symbols["sync_native_constants"], 44))
        platform = Platform(calls, target)
        backend = NativeSync(platform, constants)
        result = calls.call(fixture.symbols["test_arm_sync_entry"], target.symbols["loader_get_bridge_proc_address"], instruction_limit=2000000)
        assert result == 0, f"{mode} guest sync assertion line {result}"
        assert backend.waits == [100000, 100000, 50000, 0], backend.waits
        backend.clean()
        assert not platform.allocations
        # Native initialization/destruction failures must retain a reusable word.
        word = calls.SCRATCH
        calls.word(word, 0)
        allocations = [0]

        def fail_native_attribute_allocation():
            allocations[0] += 1
            if allocations[0] == 2:
                platform.return_value(0)
            else:
                platform.calloc()

        calls.register(target.symbols["calloc"], fail_native_attribute_allocation)
        assert calls.call(target.symbols["bridge_guest_pthread_mutex_lock"], word) == 12
        assert backend.word(word) == 0 and not platform.allocations
        calls.register(target.symbols["calloc"], platform.calloc)
        backend.fail_init = True
        assert calls.call(target.symbols["bridge_guest_pthread_mutex_lock"], word) == 5
        assert backend.word(word) == 0 and not platform.allocations
        assert calls.call(target.symbols["bridge_guest_pthread_mutex_lock"], word) == 0
        assert calls.call(target.symbols["bridge_guest_pthread_mutex_unlock"], word) == 0
        marker = backend.word(word)
        backend.fail_destroy = True
        assert calls.call(target.symbols["bridge_guest_pthread_mutex_destroy"], word) == 5
        assert backend.word(word) == marker
        assert calls.call(target.symbols["bridge_guest_pthread_mutex_destroy"], word) == 0
        backend.clean()
        assert not platform.allocations
    # A real mapped DSO owns the guest controls. Busy unload must precede fini;
    # cleanup failure keeps mappings and must not execute fini twice on retry.
    path = output / "sync.so"
    subprocess.run([compiler, "-O2", "-Wall", "-Wextra", "-Werror", "-mthumb", "-fPIC", "-nostdlib", "-shared", "-I", str(root / "src"),
                    "-Wl,--hash-style=sysv,-z,max-page-size=4096", str(root / "tests/test_sync_fixture.c"), "-o", str(path)], check=True)
    case = LoaderCase({"app0:/sync.so": path.read_bytes()})
    backend = NativeSync(case.platform, constants)
    library = case.load("app0:/sync.so")
    assert library and case.call("loader_init_library", library) == 1
    mutex = case.export(library, "sync_mutex")
    condition = case.export(library, "sync_condition")
    finalized = case.export(library, "sync_finalized")
    prepare_fails = case.export(library, "sync_prepare_fails")
    prepare_calls = case.export(library, "sync_prepare_calls")
    after_calls = case.export(library, "sync_after_calls")
    case.calls.call(case.export(library, "sync_set_hooks"), library)
    case.calls.word(prepare_fails, 1)
    finalizers = []
    cleanups = []

    def observe_finalizer():
        finalizers.append(True)
        return False

    case.calls.register(case.export(library, "sync_finish"), observe_finalizer)

    def observe_cleanup():
        cleanups.append(True)
        return False

    case.calls.register(case.export(library, "sync_after_finalize"), observe_cleanup)
    assert case.call("bridge_guest_pthread_mutex_lock", mutex) == 0
    assert case.call("bridge_guest_pthread_cond_init", condition, 0) == 0
    assert case.call("loader_clone_handle", library) == library
    case.call("loader_free_library", library)
    case.call("loader_free_library", library)
    assert case.word(finalized) == 0 and case.platform.blocks
    assert not finalizers
    assert case.word(prepare_calls) == 0 and case.word(after_calls) == 0
    assert len(backend.conditions) == 1
    assert case.call("loader_find_library", case.string("sync.so")) == library
    assert case.call("bridge_guest_pthread_mutex_unlock", mutex) == 0
    case.call("loader_free_library", library)
    assert case.word(prepare_calls) == 1 and case.word(after_calls) == 0
    assert case.word(finalized) == 0 and not finalizers and not cleanups
    assert len(backend.conditions) == 1 and case.export(library, "sync_finish")
    assert case.call("loader_clone_handle", library) == library
    case.call("loader_free_library", library)
    assert case.word(prepare_calls) == 1
    case.calls.word(prepare_fails, 0)
    backend.fail_destroy = True
    case.call("loader_free_library", library)
    assert case.word(finalized) == 1 and case.platform.blocks
    assert not backend.conditions
    assert case.word(prepare_calls) == 2 and case.word(after_calls) == 1
    assert case.call("loader_init_library", library) == 0
    assert case.call("loader_clone_handle", library) == 0
    assert case.load("app0:/sync.so") == 0
    assert case.export(library, "sync_finish") == 0
    assert case.call("loader_get_global_proc_address", case.string("sync_finish")) == 0
    case.call("loader_free_library", library)
    assert len(finalizers) == 1
    assert len(cleanups) == 1
    backend.clean()
    case.clean()
    print("Production ARM/Thumb sync passed: 4-byte controls/attributes, static types, SDK attr/cleanup execution, canaries/errno, monotonic deadline rebasing, native failure retry and loader busy/prepare-failure/fini/after-cleanup protection (native scheduling modeled).")


if __name__ == "__main__":
    main()
