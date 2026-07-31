"""Verify Linux error numbers through resolved production ARM guest adapters.

Conversion oracles use Linux UAPI and the installed SDK errno headers, not the
bridge's generated table. Filesystem/kernel endpoints remain mocked. Socket
and pthread bridge endpoints inject native failures to test adapter boundaries,
not network or scheduling behavior.
"""

from pathlib import Path
import re
import struct

from arm_fixture_build import sdk, root, compile_source, link_fixture
from check_arm_bridges import ArmImage
from check_arm_directory import DirectoryCase, ENTRIES

case = DirectoryCase()
call, platform = case.call, case.platform
error_address = platform.error_address

# Native formatted I/O requires the CRT's initialized per-thread FILE pointers.
output_directory = root / "build/arm-errno"
output_directory.mkdir(parents=True, exist_ok=True)
objects = [compile_source(root / source, output_directory / (Path(source).stem + ".o"))
           for source in ("tests/test_arm_game_bootstrap.c", "tests/test_arm_memory.c")]
executable = output_directory / "errno-crt.elf"
link_fixture(objects, executable, "test_initialize_reent", exports=["test_reent_size"])
fixture = ArmImage(case.calls.machine, executable)
assert case.calls.call(fixture.symbols["test_reent_size"]) < 0x4000
case.calls.call(fixture.symbols["test_initialize_reent"], case.calls.SCRATCH + 0x5000)


def assertion_failed():
    raise AssertionError((platform.string(case.calls.reg("R0")), case.calls.reg("R1"), platform.string(case.calls.reg("R3"))))


case.calls.register(case.target.symbols["__assert_func"], assertion_failed)


def set_error(value):
    case.calls.word(error_address, value)


def error():
    return case.word(error_address)


def resolve(name):
    address = call("loader_get_bridge_proc_address", case.string(name))
    assert address, name
    return address


def invoke(name, *arguments):
    return case.calls.call(resolve(name), *[value & 0xffffffff for value in arguments], instruction_limit=20000000)


# Resolve before preparing argument strings: LoaderCase.string shares scratch.
def entry(name):
    return resolve(name)


native_header = (sdk / "arm-vita-eabi/include/sys/errno.h").read_text()
native = {name: int(number) for name, number in re.findall(r"^#define\s+(E[A-Z0-9_]+)\s+(\d+)", native_header, re.M)}
for name, alias in re.findall(r"^#define\s+(E[A-Z0-9_]+)\s+(E[A-Z0-9_]+)", native_header, re.M):
    if alias in native:
        native[name] = native[alias]
linux = {}
for header in ("errno-base.h", "errno.h"):
    source = (Path("/usr/include/asm-generic") / header).read_text()
    linux.update({name: int(number) for name, number in re.findall(r"^#define\s+(E[A-Z0-9_]+)\s+(\d+)", source, re.M)})
verified = 0
for name, expected in linux.items():
    if name not in native:
        continue
    actual = call("bridge_errno_from_native", native[name])
    assert actual == expected, (name, native[name], expected, actual)
    back = call("bridge_errno_to_native", expected)
    assert call("bridge_errno_from_native", back) == expected, (name, back)
    verified += 1
for name, expected in (("ENOTSUP", 95), ("EDEADLOCK", 35)):
    if name in native:
        assert call("bridge_errno_from_native", native[name]) == expected
assert call("bridge_errno_from_native", 99999) == 5

assert call("bridge_syscall", -1) == 0xffffffff and error() == native["ENOSYS"]
for original in (0, 95, 123456):
    set_error(original)
    assert invoke("syscall", -1) == 0xffffffff and error() == 38
    set_error(original)
    assert invoke("fcntl", -1, 5) == 0xffffffff and error() == 95
    set_error(original)
    assert invoke("mmap", 0, 4096, 3, 0x21, -1, 0) == 0xffffffff and error() == 95
    set_error(original)
    assert invoke("munmap", 0x1000, 4096) == 0 and error() == original

# A native number with no Linux equivalent must not collide with an unchanged
# guest errno value (newlib EFTYPE=79, Linux ELIBACC=79 on this SDK).
def native_failure(value, result=0xffffffff):
    def fail():
        set_error(value)
        platform.return_value(result)
    return fail


platform.calls.register(case.target.symbols["socket"], native_failure(native["EAFNOSUPPORT"]))
set_error(0)
assert invoke("socket", 2, 1, 0) == 0xffffffff and error() == 97
if "EFTYPE" in native:
    platform.calls.register(case.target.symbols["socket"], native_failure(native["EFTYPE"]))
    set_error(native["EFTYPE"])
    assert invoke("socket", 2, 1, 0) == 0xffffffff and error() == 5
platform.calls.register(case.target.symbols["_pthread_cond_timedwait"], native_failure(native["EILSEQ"], native["ETIMEDOUT"]))
set_error(95)
assert invoke("pthread_cond_timedwait", 0, 0, 0) == 110 and error() == 95
platform.calls.register(case.target.symbols["_pthread_join"], native_failure(native["EILSEQ"], native["EDEADLK"]))
set_error(36)
assert invoke("pthread_join", 0, 0) == 35 and error() == 36

open_directory = entry("opendir")
directory = case.calls.call(open_directory, case.string("/data/user/0/moe.low.arc/list"))
assert directory
set_error(123456)
for _ in ENTRIES:
    assert invoke("readdir", directory) and error() == 123456
assert invoke("readdir", directory) == 0 and error() == 123456
platform.fail_dread = True
assert invoke("readdir", directory) == 0 and error() == 5
output = case.calls.SCRATCH + 0x1000
result = case.calls.SCRATCH + 0x2000
set_error(95)
assert invoke("readdir_r", directory, output, result) == 5 and error() == 95
platform.fail_dread = False
invoke("rewinddir", directory)
platform.malformed = True
set_error(0)
assert invoke("readdir", directory) == 0 and error() == 36
platform.malformed = False
assert invoke("closedir", directory) == 0
case.clean()

open_file = entry("open")
fd = case.calls.call(open_file, case.string("/data/data/moe.low.arc/list/file"), 0)
assert fd != 0xffffffff
offset = 5 * 1024**3
low = invoke("lseek64", fd, 0, offset & 0xffffffff, offset >> 32, 0)
assert low | case.calls.reg("R1") << 32 == offset, "errno adapter corrupted a 64-bit return"
assert invoke("close", fd) == 0
case.clean()

buffer = case.calls.SCRATCH + 0x1000
set_error(95)
assert invoke("strerror_r", 95, buffer, 128) == 0 and error() == 95
message = platform.string(buffer)
native_message = platform.string(call("strerror", native["ENOTSUP"]))
assert message == native_message and message
set_error(95)
assert invoke("strerror_r", 95, buffer, 1) == 34 and error() == 95
set_error(36)
assert platform.string(invoke("strerror", 95)) == native_message and error() == 36

# Base-PCS varargs must preserve the integer register and aligned stack double.
def reent_malloc():
    platform.return_value(platform.allocate(case.calls.reg("R1")))


def reent_calloc():
    size = case.calls.reg("R1") * case.calls.reg("R2")
    address = platform.allocate(size)
    case.calls.machine.mem_write(address, bytes(size))
    platform.return_value(address)


def reent_free():
    platform.release(case.calls.reg("R1"))
    platform.return_value(0)


for name, callback in (("_malloc_r", reent_malloc), ("_calloc_r", reent_calloc), ("_free_r", reent_free)):
    case.calls.register(case.target.symbols[name], callback)
format_address = case.calls.SCRATCH + 0x4000
case.calls.machine.mem_write(format_address, b"%d %.1f\0")
double = struct.unpack("<II", struct.pack("<d", 2.5))
set_error(95)
try:
    formatted = invoke("snprintf", buffer, 128, format_address, 7, *double)
except Exception:
    print("ARM errno formatting stop:", {name: hex(case.calls.reg(name)) for name in ("PC", "LR", "SP", "R0", "R1", "R2", "R3")})
    raise
assert formatted == 5
assert platform.string(buffer) == "7 2.5" and error() == 95
case.calls.machine.mem_write(format_address, b"%d %lf\0")
integer_output = case.calls.SCRATCH + 0x3000
double_output = case.calls.SCRATCH + 0x3008
assert invoke("sscanf", buffer, format_address, integer_output, double_output) == 2
assert case.word(integer_output) == 7 and struct.unpack("<d", case.calls.machine.mem_read(double_output, 8))[0] == 2.5
assert error() == 95
call("_reclaim_reent", case.calls.SCRATCH + 0x5000)
case.clean()

# Exercise actual variadic entry registers without replacing the adapters.
forwarded = []
def capture_open():
    forwarded.append(tuple(case.calls.reg("R" + str(index)) for index in range(3)))
    set_error(0)
    platform.return_value(77)


platform.calls.register(case.target.symbols["bridge_open"], capture_open)
open_file = entry("open")
path = case.string("/data/data/moe.low.arc/new")
set_error(123456)
assert case.calls.call(open_file, path, 0x42, 0o640) == 77 and error() == 123456
assert forwarded[-1] == (path, 0x42, 0o640)
assert case.calls.call(open_file, path, 0) == 77 and forwarded[-1] == (path, 0, 0)
case.clean()
print(f"Production ARM guest errno passed: {verified} independent native/Linux mappings, resolved syscall/file/map/directory/socket/pthread/strerror adapters, repeated/preserved errors, variadic mode and 64-bit returns (platform mocked).")
