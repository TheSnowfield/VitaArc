# Utilities, kuBridge, logging and debugging

## File utilities

`src/utils/fs.c` exports:

- `util_file_exists`: `sceIoGetstat(path, &status) >= 0`.
- `util_get_file_size`: opens, seeks to end, closes, returns `uint32_t`.
- `read_file_all`: intended to open and read a whole buffer.

`read_file_all` has a reversed existence test:

```c
if (util_file_exists(file_path))
    return false;
```

It also does not validate open/read results. `util_get_file_size` converts Vita
open/seek failures to an unsigned value.

`src/utils/string.c` exports:

- `util_get_file_name`, used by the loader for basename identity.
- `util_uchar_len`, a zero-terminated 16-bit string length helper used by JNI.

The requested removal of the broad `utils_` prefix is reflected here:
`read_file_all` no longer carries that prefix. The two narrower `util_*`
helpers retain their current names.

## kuBridge

External kernel-facing declarations come from the kubridge stub/header. The
project-local `src/kubridge.c` adds:

```c
int kuKernelCpuUnrestrictedMemset(
    void *destination, int value, size_t length);
```

The `kuKernel*` spelling is intentionally preserved. The function:

1. allocates a temporary user buffer of `length`;
2. fills it with user-mode `memset`;
3. calls `kuKernelCpuUnrestrictedMemcpy` to copy to the destination;
4. frees the temporary buffer.

It therefore allocates an additional buffer as large as every cleared loader
segment. The target project links `kubridge_stub`; the kernel module source
under `third-party/kubridge` is not recursively compiled into the user
executable.

kuBridge is used for fixed-address RX/RW loader memblocks, unrestricted copies,
cache operations and code patching. ARM `mrc`/`mcr` instructions do not replace
privileged address-space allocation or arbitrary kernel-mediated memory access;
they access permitted coprocessor/system registers subject to privilege rules.

## Debug helpers

`src/utils/debug.c` contains:

- `debug_print_memory_block`: hex output, normally 16 bytes per line.
- `debug_memory_dump`: writes a memory range to a Vita path.
- `debug_break_point`: writes instructions that deliberately dereference null.

`debug_memory_dump` uses create/write without `SCE_O_TRUNC`, does not fully
check I/O errors, and has an optional `MEMDUMP_IN_KERNEL` chunk-copy path. That
path is not currently enabled.

## Local logger

`src/logcat/logcat.c` exports:

- `log_begin`
- `log_end`
- `log_base`
- `log_printf`
- `log_v`
- `log_i`
- `log_w`
- `log_e`
- `log_f`

Default logging truncates and writes `ux0:vitaarc/boot.log`. INFO and higher also
mirror to sceClibPrintf, even when the optional file sink cannot open. UDP builds
must explicitly define LOG_OVERUDP and UDP_HOST; no fixed peer is embedded.
`tools/udp_logger.py` defaults to listening on `0.0.0.0:23333`.

UDP startup loads/initializes Vita networking, allocates 65535 bytes for
`sceNetInit`, creates a datagram socket and connects it to the fixed peer. Most
intermediate return values are not checked, and the initialization allocation
is not saved for release.

Levels are:

```text
VERBOSE=0 INFORMATION=1 WARNING=2 ERROR=3 FATAL=4
V         I             W         E       F
```

`TAG` derives the final slash-separated component of `__FILE__`.

`log_base` formats `[level] [tag] message\n` into 1024 bytes, bounds the remaining
capacity and clamps would-have-written lengths. Linked ARM tests verify long
tags/bodies and raw writes. Optional UDP mode delays 50 microseconds after each
structured log; that delay and network initialization are absent by default.

`log_printf` formats raw text without level, tag or automatic newline. The
shader bridge correctly uses `log_printf("%s", shader)` rather than treating
shader text as a format string.

`log_end` writes the final stop message before clearing its active flag, closes
the sink and resets its handle to -1. Zero is accepted as a valid open result.
The optional UDP path still needs its separate network ownership/cleanup audit;
it is not enabled in ordinary builds. Runtime failure shutdown and native
lifecycle probes now exercise the logger's stop path.

## Vita3K logging paths

The least invasive HLE logging path is the standard `sceClibPrintf` import from
`SceLibKernel_stub`; Vita3K implements it and emits an INFO log entry.
`SceLibDbg` and TTY paths also exist with caveats. An emulator-only raw HLE stub
can invoke a NID through Vita3K's import-dispatch convention, but is not a real
hardware syscall and should be isolated behind an emulator-only build option.

Full NIDs, stub layout, Vita3K options and log-file locations are recorded in
`vita3k-debugging.md`.
