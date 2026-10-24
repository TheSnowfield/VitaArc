# Memory budget

## Baseline

The 2026-10-25 baseline is commit `862ae682f7853f8f1314182bb7c58843763a4875`.
The memory block note in `build/crash-1792857926/core.raw` records these
independent reservations from the previous hardware dump:

| Block | MiB |
| --- | ---: |
| Newlib heap | 128 |
| ram_mempool | 196 |
| phycont_mempool | 26 |
| cdram_mempool | 96 |
| SceGxmParamBuffer | 16 |
| elf_image | 7.3125 |
| elf_rx | 7.0703125 |
| elf_rw | 0.56640625 |

The first three blocks reserve 350 MiB of CPU memory. This is the size of
allocated kernel blocks, not the number of live application allocations.
The graphics streaming buffers are allocated within these pools and must
not be counted a second time.

The old `vglInitExtended` argument of 24 MiB is a free-memory threshold.
It does not limit the graphics RAM pool to 24 MiB. That entry point also
takes the remaining PHYCONT memory. The linked newlib startup code defaults
to a separate 128 MiB heap when `_newlib_heap_size_user` is absent.

## Ownership

`src/platform/memory.c` owns the native heap declaration, graphics pool
budgets and memory snapshots. `src/init.c` retains the display/GXM command
buffer configuration and calls the memory module during video startup.

CMake settings apply only to the memory translation unit:

| Setting | Default MiB | Purpose |
| --- | ---: | --- |
| VITAARC_HEAP_MB | 64 | Fixed newlib reservation before constructors |
| VITAARC_GPU_RAM_MB | 32 | Upper bound for the graphics RAM pool |
| VITAARC_GPU_STREAM_MB | 6 | Total circular streaming buffers within graphics pools |

The strong `_newlib_heap_size_user` data symbol supplies the heap budget
to SDK startup. Its value in the Release ELF is 67,108,864 bytes.

The graphics initializer reads available RAM before vitaGL starts GXM and
reserves everything beyond the configured graphics RAM budget. It passes
that reserve to `vglInitWithCustomThreshold`. GXM then allocates its own
resources before vitaGL calculates pool sizes, so the resulting RAM pool
can be smaller than 32 MiB. Computing a CDRAM size before GXM initialization
would incorrectly include memory needed by the parameter buffer.

PHYCONT and common-dialog thresholds are `INT_MAX`; neither pool is
reserved eagerly. CDRAM continues to use all available graphics memory.
The system VMEM meter can therefore still show 112/112 MiB even when the
graphics allocator has free space.

The native heap and graphics RAM budget now total at most 96 MiB, down
from the previous three-block total of 350 MiB. This excludes executable
segments, library images, thread stacks and system allocations. It is not
a measurement of the new process total or evidence of a 120 MiB target.

## Streaming and fallback

The linked vitaGL build uses separate circular buffers for each display
buffer, without `CIRCULAR_POOL_SPEEDHACK`. The configured 6 MiB is divided
among them; it is not multiplied by the display buffer count. The old
default total was 32 MiB.

When a frame exceeds its circular allocation, vitaGL requests a separate
mapped allocation and marks it for deferred release. Existing RAM/VRAM
and newlib allocation fallbacks remain enabled. A smaller pool therefore
does not overwrite queued GPU data, but allocation pressure still needs
hardware observation.

## Measurements

Info snapshots go to `ux0:data/vitaarc/boot.log` during startup and every
30 seconds during active rendering:

- `Memory`: native live allocation bytes, configured reservation, arena
  and maximum live allocation bytes seen by these snapshots.
- `Graphics RAM` / `Graphics VRAM`: allocator used/free/reserved bytes
  and the maximum used bytes seen by these snapshots.
- `System free`: RAM, CDRAM and PHYCONT available outside reserved blocks.

All printed sizes use KiB. Peaks are sampled maxima, not allocation-level
high-water marks. Native fallback graphics allocations contribute to the
native heap figures. The 64 MiB heap remains a provisional budget until
song loading, gameplay and scene changes have been observed on hardware.

## Build snapshot

`build/memory-budget-20261025/` contains Release and Debug symbols,
`eboot.bin`, `previous-eboot.bin`, budget data and source copies.

Release SELF SHA-256:

```text
533f9218ab49d155bcceae41c6b4b7d83451bf7fa3b2dc24f19d29ee4a48144c
```

Both configurations compiled successfully. Linked startup parameters and
the native heap data symbol were inspected. No test suites, emulator
probes or hardware runs were performed for this change. Existing texture
assets and writable data paths remain compatible.

The retained ELF image is another visible 7.3125 MiB block. Releasing it
requires checking loader metadata lifetimes; it has not been changed.
