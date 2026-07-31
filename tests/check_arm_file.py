"""Execute linked production file/stat/RNG bridges and VitaSDK fd management.

The filesystem, RNG, RTC and kernel mutex endpoints are mocked. The actual
Android flag converter, Sony flag converter, newlib descriptor allocator,
reference/dup/close management, 64-bit I/O and stat packing execute on ARM.
This does not prove physical filesystem, permissions, entropy or game boot.
"""

from pathlib import Path
import struct

from arm_fixture_build import compile_source, link_fixture, root
from check_arm_bridges import ArmCalls, ArmImage
from check_arm_native_vm import Platform
from unicorn import arm_const as registers

output = root / "build/arm-file"
output.mkdir(parents=True, exist_ok=True)
endpoints = (
    ("sceIoOpen", "mock_file_open"), ("sceIoClose", "mock_file_close"),
    ("sceIoGetstat", "mock_file_getstat"), ("sceIoGetstatByFd", "mock_file_fstat"),
    ("sceIoChstatByFd", "mock_file_chstat"), ("sceIoLseek", "mock_file_seek"),
    ("sceIoRead", "mock_file_read"), ("sceIoWrite", "mock_file_write"),
    ("sceIoSyncByFd", "mock_file_sync"), ("sceRtcGetTime64_t", "mock_file_time"),
    ("sceIoMkdir", "mock_path_mkdir"), ("sceIoRename", "mock_path_rename"),
    ("sceIoRemove", "mock_path_remove"), ("sceIoRmdir", "mock_path_rmdir"),
    ("sceIoDopen", "mock_path_dopen"), ("sceIoDread", "mock_path_dread"), ("sceIoDclose", "mock_path_dclose"),
    ("sceKernelGetRandomNumber", "mock_file_random"))
objects = [compile_source(source, output / (source.stem + ".o")) for source in
           (root / "tests/test_arm_file.c", root / "tests/test_arm_memory.c")]
executable = output / "file.elf"
link_fixture(objects, executable, "test_arm_file_entry", exports=[mock for _, mock in endpoints])
calls = ArmCalls()
target = ArmImage(calls.machine, root / "build/VitaArc")
fixture = ArmImage(calls.machine, executable)
platform = Platform(calls, target)


def forward(address):
    def endpoint():
        calls.machine.reg_write(registers.UC_ARM_REG_PC, address)
        return False
    return endpoint


for native, mock in endpoints:
    calls.register(target.symbols[native], forward(fixture.symbols[mock]))
for name in ("sceKernelLockLwMutex", "sceKernelUnlockLwMutex"):
    calls.register(target.symbols[name], platform.success)

# Model CRT's already-open stdout in its real newlib descriptor pool. RNG
# opens/dups/closes must neither use fd 1 nor release its native resource.
stdout = target.symbols["__vita_fdmap_pool"] + 20
calls.machine.mem_write(stdout, struct.pack("<iiIII", 777, 3, 1, 0, 1))
calls.word(target.symbols["__vita_fdmap"] + 4, stdout)
for name, callback in (
        ("open", "bridge_open"), ("__open_2", "bridge_open_2"),
        ("read", "bridge_read"), ("write", "bridge_write"), ("close", "bridge_close"),
        ("stat", "bridge_bionic_stat"), ("fstat", "bridge_bionic_fstat"), ("lstat", "bridge_bionic_lstat"),
        ("lseek", "bridge_lseek"), ("lseek64", "bridge_lseek64"),
        ("ftruncate", "bridge_ftruncate"), ("fcntl", "bridge_fcntl"), ("fdopen", "bridge_fdopen"),
        ("rename", "bridge_rename"), ("mkdir", "bridge_mkdir")):
    calls.machine.mem_write(calls.SCRATCH, name.encode() + b"\0")
    guest_callback = "bridge_guest_" + callback.removeprefix("bridge_")
    assert calls.call(target.symbols["loader_get_bridge_proc_address"], calls.SCRATCH) == target.symbols[guest_callback], name
api = ["bridge_open", "bridge_read", "bridge_write", "bridge_close", "bridge_bionic_stat",
       "bridge_bionic_fstat", "bridge_lseek64", "bridge_ftruncate64", "bridge_fcntl", "bridge_fsync", "__errno", "bridge_fdopen",
       "bridge_rename", "bridge_mkdir"]
calls.machine.mem_write(calls.SCRATCH, struct.pack("<" + "I" * len(api), *[target.symbols[name] for name in api]))
try:
    result = calls.call(fixture.symbols["test_arm_file_entry"], calls.SCRATCH, instruction_limit=3000000)
except Exception:
    print("ARM file stop:", {name: hex(calls.reg(name)) for name in ("PC", "LR", "SP", "R0", "R1", "R2", "R3")}, flush=True)
    raise
assert result == 0, f"ARM file fixture failed case {result}"
assert bytes(calls.machine.mem_read(stdout, 20)) == struct.pack("<iiIII", 777, 3, 1, 0, 1)
table = struct.unpack("<256I", calls.machine.mem_read(target.symbols["__vita_fdmap"], 256 * 4))
assert table[1] == stdout and not any(table[3:]), "file/RNG descriptor leak"
assert not platform.allocations, f"file/RNG heap leak: {len(platform.allocations)}"
assert not platform.locks
print("Production ARM file ABI passed: flags/modes, stat/64-bit offsets, SDK fd lifetime, CLOEXEC, RNG, mkdir, replacement/rollback/recovery and directory checks (platform mocked).")
