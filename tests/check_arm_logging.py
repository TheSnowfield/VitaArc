"""Execute linked formatting and sink boundaries, including truncation."""

from check_arm_loader import LoaderCase

case = LoaderCase({})
calls, target, platform = case.calls, case.target, case.platform
system, sent, closed, shown = [], [], [], []


def console():
    assert platform.string(calls.reg("R0")) == "%s"
    system.append(platform.string(calls.reg("R1")))
    platform.return_value(len(system[-1]))


def send():
    assert calls.reg("R0") in (0, 55) and calls.reg("R2") <= 65536
    sent.append(bytes(calls.machine.mem_read(calls.reg("R1"), calls.reg("R2"))).decode())
    platform.return_value(calls.reg("R2"))


calls.register(target.symbols["sceClibPrintf"], console)
calls.register(target.symbols["sceIoWrite"], send)
calls.register(target.symbols["sceKernelDelayThread"], platform.success)
calls.register(target.symbols["pthread_mutex_lock"], platform.success)
calls.register(target.symbols["pthread_mutex_unlock"], platform.success)


def view():
    shown.append(platform.string(calls.reg("R1")))
    platform.return_value(0)


view_address = calls.SCRATCH + 0x8000
calls.register(view_address, view)


def open_sink():
    assert platform.string(calls.reg("R0")) == "ux0:vitaarc/boot.log"
    assert calls.reg("R1") == 0x602  # Vita WRONLY | CREAT | TRUNC
    assert calls.reg("R2") == 0o666
    platform.return_value(0)  # A nonnegative handle is valid, including zero.


def close_sink():
    closed.append(calls.reg("R0"))
    assert sent[-1].endswith("Log stop.\n")
    platform.return_value(0)


calls.register(target.symbols["sceIoOpen"], open_sink)
calls.register(target.symbols["sceIoClose"], close_sink)
# Use real log entry points rather than the loader fixture's diagnostic hooks.
for name in ("log_begin", "log_end", "log_v", "log_i", "log_w", "log_e", "log_f", "log_printf"):
    calls.endpoints.pop(target.symbols[name] & ~1, None)
tag, fmt, value = calls.SCRATCH, calls.SCRATCH + 0x2000, calls.SCRATCH + 0x3000
calls.machine.mem_write(tag, b"topic\0")
calls.machine.mem_write(fmt, b"%s\0")
calls.machine.mem_write(value, b"message\0")
case.call("log_f", tag, fmt, value)
assert system == ["[F] [topic] \tmessage\n"] and not sent
case.call("log_v", tag, fmt, value)
assert len(system) == 1
case.call("log_set_view", view_address)
case.call("log_i", tag, fmt, value)
assert shown[-1] == "[I] [topic] \tmessage\n" and not sent
case.call("log_v", tag, fmt, value)
assert len(shown) == 1
case.call("log_set_view", 0)
calls.word(target.symbols["__log_stream"], 55)
calls.machine.mem_write(target.symbols["__log_started"], b"\1")
calls.machine.mem_write(value, b"x" * 5000 + b"\0")
case.call("log_e", tag, fmt, value)
assert len(system[-1]) == 1023 and system[-1].endswith("\n") and sent[-1] == system[-1]
calls.machine.mem_write(tag, b"t" * 2000 + b"\0")
case.call("log_w", tag, fmt, value)
assert len(system[-1]) == 1023 and system[-1].endswith("\n")
before = len(sent)
assert sent[-1] != system[-1]
case.call("log_flush")
assert len(sent) == before + 1 and sent[-1] == system[-1]
case.call("log_printf", fmt, value)
assert len(sent) == before + 1
case.call("log_v", tag, fmt, value)
assert len(sent) == before + 1

# Ordinary Info messages accumulate into one write instead of one syscall each.
calls.machine.mem_write(tag, b"topic\0")
calls.machine.mem_write(value, b"message\0")
before = len(sent)
for _ in range(100):
    case.call("log_i", tag, fmt, value)
assert len(sent) == before
case.call("log_flush")
assert len(sent) == before + 1
assert sent[-1] == "[I] [topic] \tmessage\n" * 100
case.call("log_end")
assert closed == [55] and bytes(calls.machine.mem_read(target.symbols["__log_stream"], 4)) == b"\xff" * 4
calls.machine.mem_write(tag, b"ux0:vitaarc/boot.log\0")
case.call("log_begin", tag)
case.call("log_flush")
assert sent[-1].endswith("Log started.\n")
case.call("log_end")
assert closed == [55, 0] and bytes(calls.machine.mem_read(target.symbols["__log_stream"], 4)) == b"\xff" * 4
assert not platform.allocations
print("Production ARM logging passed: Info view without a file, default verbose/raw suppression, batched Info writes, immediate errors, bounded formatting, explicit flush and final message before close.")
