from pathlib import Path
import struct
import sys

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'tests'))
from check_arm_bridges import ArmCalls, ArmImage, float_bits
from unicorn import arm_const as reg

calls = ArmCalls()
target = ArmImage(calls.machine, root / 'build/release-7.0.260c/VitaArc')
machine = calls.machine
buf = calls.SCRATCH + 0x100
machine.mem_write(buf - 8, b'\xa5' * (260 + 16))
core = list(range(reg.UC_ARM_REG_R4, reg.UC_ARM_REG_R11 + 1))
for i, number in enumerate(core):
    machine.reg_write(number, 0x12340000 + i)
for i in range(8, 16):
    machine.reg_write(getattr(reg, f'UC_ARM_REG_D{i}'), 0x3ff0000000000000 + i)
assert calls.call(target.symbols['bridge_sigsetjmp'], buf, 1) == 0
for number in core:
    machine.reg_write(number, 0)
for i in range(8, 16):
    machine.reg_write(getattr(reg, f'UC_ARM_REG_D{i}'), 0)
assert calls.call(target.symbols['bridge_siglongjmp'], buf, 71) == 71
for i, number in enumerate(core):
    assert machine.reg_read(number) == 0x12340000 + i
for i in range(8, 16):
    assert machine.reg_read(getattr(reg, f'UC_ARM_REG_D{i}')) == 0x3ff0000000000000 + i
assert bytes(machine.mem_read(buf - 8, 8)) == bytes(machine.mem_read(buf + 260, 8)) == b'\xa5' * 8
print('ARM jump: core/VFP restoration and Android buffer canaries pass.')

provider = calls.SCRATCH + 0x1000
state = calls.SCRATCH + 0x1100
path = calls.SCRATCH + 0x1200
calls.word(provider + 4, state)
machine.mem_write(path, b'audio.wav\0')
table = target.symbols['vita_audio_provider_vtable_v7']
def entry(slot):
    return struct.unpack('<I', machine.mem_read(table + slot * 4, 4))[0]

seen = []
def played():
    assert [calls.reg(f'R{i}') for i in range(4)] == [state, path, 1, 0]
    assert calls.reg('S0') == float_bits(.375)
    assert struct.unpack('<II', machine.mem_read(calls.reg('SP'), 8)) == (20, 50)
    machine.reg_write(reg.UC_ARM_REG_R0, 71)
    seen.append('play')
calls.register(target.symbols['vita_audio_play_sfx_range'], played)
assert calls.call(entry(16), provider, path, 1, 0, float_bits(.375), 20, 50) == 71
def master():
    assert (calls.reg('R0'), calls.reg('R1'), calls.reg('S0')) == (state, 1, float_bits(.25))
    seen.append('master')
calls.register(target.symbols['vita_audio_set_master_volume'], master)
calls.call(entry(12), provider, float_bits(.25), 1)
def getter():
    assert calls.reg('R0') == state
    machine.reg_write(reg.UC_ARM_REG_S0, float_bits(.5))
calls.register(target.symbols['vita_audio_get_sfx_volume'], getter)
assert calls.call(entry(21), provider) == float_bits(.5)

vector = calls.SCRATCH + 0x2000
pairs = calls.SCRATCH + 0x2100
machine.mem_write(pairs, bytes([18]) + b'audio.wav\0' + b'\0' + struct.pack('<i', 60))
calls.word(vector, pairs)
calls.word(vector + 4, pairs + 16)
calls.word(vector + 8, pairs + 16)
def scheduled():
    assert (calls.reg('R0'), calls.reg('R1'), calls.reg('R2')) == (state, pairs + 1, 60)
    machine.reg_write(reg.UC_ARM_REG_R0, 1)
    seen.append('schedule')
calls.register(target.symbols['vita_audio_schedule_sfx'], scheduled)
calls.call(entry(13), provider, vector)
assert seen == ['play', 'master', 'schedule']
print('ARM AudioProvider 7: verified slots, stack loop points, base-PCS float input/output and NDK vector/string ABI pass.')
