"""Execute linked texture upload adapters with independent ARM stack arguments.

Only native GL and allocator endpoints are modeled. Actual GPU pixel checks live
in vita_shader_probe.c; this checks row data, restoration and the nine-word ABI.
"""

import struct

from check_arm_loader import LoaderCase

case = LoaderCase({})
calls, target, platform = case.calls, case.target, case.platform
seen, rows = [], []


def native_store():
    assert calls.reg("R0") == 0x0cf2
    rows.append(calls.reg("R1"))


def upload(sub=False):
    arguments = [calls.reg("R" + str(index)) for index in range(4)]
    arguments += list(struct.unpack("<5I", calls.machine.mem_read(calls.reg("SP"), 20)))
    expected = [0x0de1, 0, 2, 3, 1, 2, 0x1907, 0x1401] if sub else [0x0de1, 0, 0x1907, 1, 2, 0, 0x1907, 0x1401]
    assert arguments[:8] == expected, arguments
    assert bytes(calls.machine.mem_read(arguments[8], 6)) == bytes((1, 2, 3, 4, 5, 6))
    seen.append((sub, arguments[8]))


calls.register(target.symbols["glPixelStorei"], native_store)
calls.register(target.symbols["glTexImage2D"], upload)
calls.register(target.symbols["glTexSubImage2D"], lambda: upload(True))
calls.register(target.symbols["glGetError"], platform.success)
source, output = calls.SCRATCH, calls.SCRATCH + 128
calls.machine.mem_write(source, bytes((1, 2, 3, 99, 4, 5, 6)))
case.call("bridge_gl_pixel_store_i", 0x0cf5, 4)
case.call("bridge_gl_tex_image_2d", 0x0de1, 0, 0x1907, 1, 2, 0, 0x1907, 0x1401, source)
assert seen[-1][1] != source and not platform.allocations
case.call("bridge_gl_pixel_store_i", 0x0cf5, 1)
calls.machine.mem_write(source, bytes((1, 2, 3, 4, 5, 6)))
case.call("bridge_gl_tex_sub_image_2d", 0x0de1, 0, 2, 3, 1, 2, 0x1907, 0x1401, source)
assert seen[-1] == (True, source)
case.call("bridge_gl_pixel_store_i", 0x0cf5, 8)
case.call("bridge_gl_pixel_store_i", 0x0cf2, 2)
calls.machine.mem_write(source, bytes((1, 2, 3, 99, 98, 97, 96, 95, 4, 5, 6)))
case.call("bridge_gl_tex_image_2d", 0x0de1, 0, 0x1907, 1, 2, 0, 0x1907, 0x1401, source)
assert rows[-3:] == [2, 0, 2] and not platform.allocations
case.call("bridge_gl_get_integer_v", 0x0cf5, output)
assert case.word(output) == 8
case.call("bridge_gl_pixel_store_i", 0x0cf5, 3)
assert case.call("bridge_gl_get_error") == 0x0501
assert case.call("bridge_gl_get_error") == 0
assert not platform.allocations and not platform.handles
print("Linked ARM pixel unpack passed: nine-word image/subimage ABI, padded RGB rows, "
      "tight identity, explicit row-length restoration, state query and invalid alignment.")
