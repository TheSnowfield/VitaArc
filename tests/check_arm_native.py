"""Execute production native-call packing/trampoline against independent callees.

The callees use Android base PCS, in separate ARM/Thumb objects. No platform
system calls, GPU or game startup are emulated by this calling-convention test.
"""

from pathlib import Path
import subprocess
import sys

from check_arm_bridges import ArmCalls, ArmImage
from arm_fixture_build import compile_source, link_fixture

root = Path(__file__).resolve().parent.parent
output = root / "build/arm-native"
output.mkdir(parents=True, exist_ok=True)
fixture = output / "native-fixture.c"
subprocess.run([sys.executable, str(root / "tests/generate_native_fixture.py"), str(fixture)], check=True)
objects = []
for source, mode in (
        (root / "src/bridges/jni/native_call.c", "-mthumb"),
        (root / "src/bridges/jni/descriptor.c", "-mthumb"),
        (root / "tests/test_arm_native_entry.c", "-mthumb"),
        (root / "tests/test_arm_native_callees.c", "-marm"),
        (root / "tests/test_arm_memory.c", "-mthumb"),
        (fixture, "-mthumb")):
    objects.append(compile_source(source, output / (source.stem + ".o"), mode, ("VITAARC_ARM_FIXTURE",)))
executable = output / "native.elf"
link_fixture(objects, executable, "test_arm_native_entry")
calls = ArmCalls()
image = ArmImage(calls.machine, executable)
try:
    # This entry runs the whole matrix, including 255-slot parsing and pure
    # byte-copy platform stubs, rather than one short production function.
    result = calls.call(image.symbols["test_arm_native_entry"], instruction_limit=2000000)
except AssertionError:
    pc = calls.reg("PC")
    nearest = max((address, name) for name, address in image.symbols.items() if address <= pc)
    print(f"ARM execution stopped at {pc:#x}, near {nearest}", flush=True)
    raise
assert result == 0, f"Native ARM call fixture failed case {result}"
print("ARM native dispatch passed: all returns, mixed widths, alignment, ARM/Thumb interworking, 255 ints and 127 long + int arguments.")
