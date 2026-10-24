"""Execute the added version-superset ABI entries in the linked Vita ELF.

Runs native SDK math with Android base-PCS arguments and checks all four real
libraries through the production relocation code. Platform I/O/TLS is injected;
this is not a replacement for actual game startup/play tests.
"""
import argparse
import math
from pathlib import Path
import struct

from check_arm_loader import LoaderCase, check_game_relocations
from check_arm_bridges import float_bits
from arm_fixture_build import root


def words(value):
    return struct.unpack('<II', struct.pack('<d', value))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-dir', type=Path, default=root / 'build/apk-versions/inspection')
    parser.add_argument('--elf', type=Path, default=root / 'build/VitaArc')
    args = parser.parse_args()
    case = LoaderCase({}, args.elf)
    calls = case.calls

    def lookup(name):
        result = case.call('loader_get_bridge_proc_address', case.string(name))
        assert result, name
        return result

    def double_call(name, *arguments):
        calls.call(lookup(name), *arguments, instruction_limit=1000000)
        return struct.unpack('<d', struct.pack('<II', calls.reg('R0'), calls.reg('R1')))[0]

    samples = [('atan', .5, math.atan(.5)), ('cosh', .75, math.cosh(.75)),
               ('exp', .5, math.exp(.5)), ('sinh', -.5, math.sinh(-.5)),
               ('tanh', .5, math.tanh(.5)), ('acosh', 2., math.acosh(2.)),
               ('asinh', -.5, math.asinh(-.5)), ('atanh', .25, math.atanh(.25)),
               ('cbrt', 27., 3.), ('erf', .5, math.erf(.5)),
               ('erfc', .5, math.erfc(.5)), ('expm1', .125, math.expm1(.125)),
               ('lgamma', 3.5, math.lgamma(3.5)), ('log1p', .25, math.log1p(.25)),
               ('logb', 8., 3.), ('nearbyint', 2.25, 2.), ('rint', 2.25, 2.),
               ('tgamma', 4.5, math.gamma(4.5)), ('exp2', 3.25, 2.**3.25)]
    for name, value, expected in samples:
        actual = double_call(name, *words(value))
        assert math.isclose(actual, expected, rel_tol=1e-12, abs_tol=1e-14), (name, actual, expected)
    actual = calls.call(lookup('logf'), float_bits(2.))
    assert math.isclose(struct.unpack('<f', struct.pack('<I', actual))[0], math.log(2.), rel_tol=1e-6)
    assert double_call('scalbnl', *words(1.25), 5) == 40.
    assert double_call('nextafter', *words(2.), *words(3.)) == math.nextafter(2., 3.)
    assert calls.call(lookup('nextafterf'), float_bits(2.), float_bits(3.)) == 0x40000001
    assert double_call('hypot', *words(3.), *words(4.)) == 5.
    assert double_call('remainder', *words(11.), *words(3.)) == -1.
    quotient = calls.SCRATCH + 0x1000
    calls.machine.mem_write(quotient, b'\x99'*12)
    assert double_call('remquo', *words(11.), *words(3.), quotient + 4) == -1.
    output = bytes(calls.machine.mem_read(quotient, 12))
    assert output[:4] == output[8:] == b'\x99'*4
    assert struct.unpack_from('<i', output, 4)[0] & 7 == 4
    assert calls.call(lookup('lrint'), *words(2.25)) == 2
    calls.call(lookup('llrint'), *words(4294967297.))
    assert calls.reg('R0') | calls.reg('R1') << 32 == 4294967297

    for bits, expected in [(0,16),(1<<63,16),(1,8),(0x0010000000000000,4),
                           (0x7fefffffffffffff,4),(0x7ff0000000000000,1),
                           (0xfff0000000000000,1),(0x7ff8000000000000,2),
                           (0xfff0000000000001,2)]:
        arguments = [bits & 0xffffffff, bits >> 32]
        assert calls.call(lookup('__fpclassifyd'), *arguments) == expected
        assert calls.call(lookup('__signbit'), *arguments) == bits >> 63
        assert calls.call(lookup('__isfinite'), *arguments) == (expected not in (1,2))
    for bits, expected in [(0,0),(0x7f800000,0),(0xff800000,0),(0x7fc00000,1),(0x7f800001,1)]:
        assert calls.call(lookup('isnanf'), bits) == expected

    memory = calls.SCRATCH + 0x2000
    assert lookup('__aeabi_memmove8') == lookup('__aeabi_memmove')
    for source, destination in [(memory, memory+8), (memory+8, memory)]:
        calls.machine.mem_write(memory, bytes(range(40)))
        calls.call(lookup('__aeabi_memmove8'), destination, source, 24)
        expected = bytearray(range(40))
        expected[destination-memory:destination-memory+24] = bytes(range(40))[source-memory:source-memory+24]
        assert bytes(calls.machine.mem_read(memory, 40)) == expected

    callbacks = []
    callback = calls.RETURN + 0x100
    calls.register(callback, lambda: callbacks.append(calls.reg('R0')))
    assert calls.call(lookup('__aeabi_atexit'), 0x12345, callback | 1, 0x56789) == 0
    calls.call(lookup('__cxa_finalize'), 0x56789)
    calls.call(lookup('__cxa_finalize'), 0x56789)
    assert callbacks == [0x12345]
    case.clean()
    print('ARM version imports pass: SDK math/base PCS, binary64 long double, 64-bit return, stack pointer/canaries, Bionic classification, overlapping memmove8, atexit argument order/exactly once.', flush=True)

    for version in ['1.6.1c', '1.9.0c', '2.0.0c', '3.0.0c']:
        data = (args.game_dir / ('arcaea_' + version) / 'libcocos2dcpp.so').read_bytes()
        case = LoaderCase({'app0:/libcocos2dcpp.so': data}, args.elf)
        library = case.load('app0:/libcocos2dcpp.so')
        assert library, (version, case.platform.messages)
        check_game_relocations(case, library, data)
        case.call('loader_free_library', library)
        case.clean()
        print(f'{version}: actual library load/relocation/unload passed (before constructors).', flush=True)


if __name__ == '__main__':
    main()
