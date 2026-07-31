"""Execute SDK formatting via production fortified entry points, with canaries."""
from check_arm_loader import LoaderCase

case = LoaderCase({})
calls, target = case.calls, case.target
buffer, fmt, text, args = [calls.SCRATCH + offset for offset in (0x1000,0x2000,0x3000,0x4000)]
calls.machine.mem_write(fmt, b'%s/%d\0')
calls.machine.mem_write(text, b'ok\0')
calls.word(args, text)
calls.word(args + 4, 123)

class ExpectedFortify(Exception):
    pass

def reject():
    assert 'vsprintf buffer overflow' in case.platform.string(calls.reg('R0'))
    raise ExpectedFortify()

calls.register(target.symbols['__fortify_chk_fail'], reject)
for bound in (7, 32, 0xffffffff):
    calls.machine.mem_write(buffer, b'\xa5'*48)
    assert calls.call(target.symbols['__vsprintf_chk'], buffer+8, 0, bound, fmt, args) == 6
    output = bytes(calls.machine.mem_read(buffer, 48))
    assert output[:8] == b'\xa5'*8 and output[8:15] == b'ok/123\0' and output[15:] == b'\xa5'*33
for bound in (0, 1, 6):
    calls.machine.mem_write(buffer, b'\xa5'*48)
    try:
        calls.call(target.symbols['__vsprintf_chk'], buffer+8, 0, bound, fmt, args)
    except ExpectedFortify:
        pass
    else:
        raise AssertionError('Fortify failed to reject a known undersized buffer')
    output = bytes(calls.machine.mem_read(buffer, 48))
    assert output[:8] == b'\xa5'*8 and output[8+bound:] == b'\xa5'*(40-bound)
case.clean()
print('ARM fortified formatting passed: SDK va_list, exact/known/unknown bounds, canaries, and rejection of zero/undersized buffers.')
