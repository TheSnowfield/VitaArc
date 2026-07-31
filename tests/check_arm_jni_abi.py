"""Compile separate Vita ARM callers/callees and execute them with Unicorn.

Requires VitaSDK, Python 3 and the unicorn package. This verifies instruction-
level call ABI only; it does not emulate Vita APIs, a GPU or the game loader.
"""

import os
from pathlib import Path
import struct
import subprocess
import sys


root = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(root / "build/arm-tools"))
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_FPEXC, UC_ARM_REG_C1_C0_2

sdk = Path(os.environ.get("VITASDK", "/usr/local/vitasdk"))
output = root / "build/arm-jni-abi"
output.mkdir(parents=True, exist_ok=True)
compiler = str(sdk / "bin/arm-vita-eabi-gcc")
objects = []
for source in ("test_arm_abi.c", "test_arm_abi_entry.c", "test_arm_abi_cpp.cpp"):
    target = output / f"{source}.o"
    driver = str(sdk / "bin/arm-vita-eabi-g++") if source.endswith(".cpp") else compiler
    options = ["-fno-exceptions", "-fno-rtti"] if source.endswith(".cpp") else []
    subprocess.run([
        driver, "-O2", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
        "-fno-unwind-tables", "-fno-asynchronous-unwind-tables", *options,
        "-I", str(root / "src/bridges"), "-c", str(root / "tests" / source),
        "-o", str(target),
    ], check=True)
    objects.append(str(target))
executable = output / "jni-abi.elf"
subprocess.run([
    compiler, "-nostdlib", "-static", "-Wl,-Ttext=0x10000",
    "-Wl,-e,test_jni_arm_entry", *objects, "-o", str(executable),
], check=True)
data = executable.read_bytes()
header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
assert header[0][:6] == b"\x7fELF\x01\x01" and header[2] == 40
entry, offset, entry_size, count = header[4], header[5], header[9], header[10]
segments = []
for index in range(count):
    kind, file_offset, address, _, file_size, memory_size, _, _ = struct.unpack_from("<IIIIIIII", data, offset + index * entry_size)
    if kind == 1:
        segments.append((file_offset, address, file_size, memory_size))
base = min(address for _, address, _, _ in segments) & ~4095
end = (max(address + size for _, address, _, size in segments) + 4095) & ~4095
machine = Uc(UC_ARCH_ARM, UC_MODE_ARM)
machine.mem_map(base, end - base)
for file_offset, address, file_size, _ in segments:
    machine.mem_write(address, data[file_offset:file_offset + file_size])
machine.mem_map(0x100000, 0x10000)
machine.mem_map(0x200000, 0x1000)
machine.reg_write(UC_ARM_REG_SP, 0x110000 - 16)
machine.reg_write(UC_ARM_REG_LR, 0x200001)
machine.reg_write(UC_ARM_REG_C1_C0_2, 0xF00000)
machine.reg_write(UC_ARM_REG_FPEXC, 0x40000000)
machine.emu_start(entry, 0x200000, count=100000)
assert machine.reg_read(UC_ARM_REG_PC) == 0x200000, "ARM fixture did not return"
result = machine.reg_read(UC_ARM_REG_R0)
assert result == 0, f"ARM JNI float argument/float return/double return mismatch: {result}"
print("ARM JNI PCS passed: C and C++ float arguments/returns and double returns across separate objects.")
