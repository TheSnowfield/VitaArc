from pathlib import Path
import struct
import sys

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'tests'))
from check_arm_bridges import ArmCalls, ArmImage
from unicorn import arm_const as reg

calls = ArmCalls()
image = ArmImage(calls.machine, root / 'build/arcaea-7.0.260c/runtime/check.elf')
machine = calls.machine
data = calls.SCRATCH
raw = struct.pack('<QII', 123456789, 0, 2)
raw += struct.pack('<BBhh8sH', 128, 31, 700, 800, b'\0' * 8, 0)
raw += struct.pack('<BBhh8sH', 4, 65, 1200, 500, b'\0' * 8, 0)
raw += b'\0' * 96
assert len(raw) == 144
result = 1

def sampled():
    if calls.reg('R1') and result == 1:
        machine.mem_write(calls.reg('R1'), raw)
    machine.reg_write(reg.UC_ARM_REG_R0, result)

def cleared():
    machine.mem_write(calls.reg('R0'), bytes([calls.reg('R1') & 255]) * calls.reg('R2'))

calls.register(image.symbols['sceTouchPeek'], sampled)
calls.register(image.symbols['memset'], cleared)
entry = image.symbols['__wrap_sceTouchPeek']

assert calls.call(entry, 0, data, 1) == 1
assert bytes(machine.mem_read(data, 144)) == raw
calls.word(image.symbols['touch'], 3)
calls.word(image.symbols['touch_id'], 31)
calls.word(image.symbols['x'], 400)
calls.word(image.symbols['y'], 250)
assert calls.call(entry, 0, data, 1) == 1
assert struct.unpack('<I', machine.mem_read(data + 12, 4))[0] == 1
assert struct.unpack('<BBhh', machine.mem_read(data + 16, 6)) == (31, 128, 800, 500)
calls.word(image.symbols['touch'], 0)
assert calls.call(entry, 0, data, 1) == 1
assert bytes(machine.mem_read(data, 144)) == bytes(144)
assert calls.call(entry, 0, data, 1) == 1
assert bytes(machine.mem_read(data, 144)) == raw
result = 0x80350001
assert calls.call(entry, 0, data, 1) == result
print('Probe ARM input: raw multi-touch passthrough, command-only injection, physical input restoration and SDK errors pass.')
