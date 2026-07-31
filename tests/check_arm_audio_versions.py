"""Check real AudioManager callers against each retained provider ABI table."""
from pathlib import Path
from arm_fixture_build import root
from check_arm_bridges import ArmCalls, ArmImage, float_bits
from unicorn import arm_const as regs

for version, offset, table_name, update_slot, play_slot, loops in [
        ('1.6.1c', 0x1e4, 'vita_audio_provider_vtable', 15, 12, 0),
        ('1.9.0c', 0x218, 'vita_audio_provider_vtable_v2', 16, 15, 2),
        ('2.0.0c', 0x218, 'vita_audio_provider_vtable_v2', 16, 12, 2),
        ('3.0.0c', 0x218, 'vita_audio_provider_vtable_v3', 18, 17, -1)]:
    calls = ArmCalls()
    target = ArmImage(calls.machine, root / 'build/VitaArc')
    game = ArmImage(calls.machine, root / f'build/apk-versions/inspection/arcaea_{version}/libcocos2dcpp.so', 0x98000000)
    manager, provider, state, path = [calls.SCRATCH + n for n in (0x1000,0x2000,0x3000,0x4000)]
    table = target.symbols[table_name] + 8
    calls.word(manager + offset, provider)
    calls.word(provider, table)
    calls.word(provider + 4, state)
    calls.machine.mem_write(path, b'audio/test.wav\0')
    seen = []

    def updated():
        assert calls.reg('R0') == provider
        seen.append('update')

    calls.register(target.symbols['audio_provider_update'], updated)
    calls.call(game.symbols['_ZN12AudioManager6updateEf'], manager, float_bits(.016))
    assert seen == ['update']
    assert int.from_bytes(calls.machine.mem_read(table + 4*update_slot,4),'little') == target.symbols['audio_provider_update']

    def position():
        assert (calls.reg('R0'),calls.reg('R1')) == (state,0)
        calls.machine.reg_write(regs.UC_ARM_REG_R0,12345)

    calls.register(target.symbols['vita_audio_get_bgm_position'], position)
    assert calls.call(game.symbols['_ZN12AudioManager14getBGMPositionEv'],manager) == 12345

    def played():
        expected_volume = -1. if version == '1.6.1c' else .375
        assert (calls.reg('R0'),calls.reg('R1'),calls.reg('R2'),calls.reg('R3'),calls.reg('S0')) == (
            state,path,loops & 0xffffffff,0,float_bits(expected_volume)), version
        calls.machine.reg_write(regs.UC_ARM_REG_R0,71)

    calls.register(target.symbols['vita_audio_play_sfx_ex'],played)
    entry = int.from_bytes(calls.machine.mem_read(table+play_slot*4,4),'little')
    arguments = (provider,path,1 if loops == -1 else loops,0,float_bits(.375))
    assert calls.call(entry,*arguments) == 71
    if version == '3.0.0c':
        def get_volume():
            assert (calls.reg('R0'),calls.reg('R1')) == (state,path)
            calls.machine.reg_write(regs.UC_ARM_REG_S0,float_bits(.25))
        calls.register(target.symbols['vita_audio_get_sfx_path_volume'],get_volume)
        getter=int.from_bytes(calls.machine.mem_read(table+14*4,4),'little')
        assert calls.call(getter,provider,path) == float_bits(.25)
        def set_volume():
            assert (calls.reg('R0'),calls.reg('R1'),calls.reg('S0')) == (state,path,float_bits(.5))
        calls.register(target.symbols['vita_audio_set_sfx_path_volume'],set_volume)
        setter=int.from_bytes(calls.machine.mem_read(table+15*4,4),'little')
        calls.call(setter,provider,path,float_bits(.5))
    print(f'{version}: real AudioManager update/get-position, correct vtable slots, SFX loop/volume/async ABI pass.',flush=True)
