# Precise seeks and reusable audio feeds

## Remaining synchronous work

The first indexed-seek build was reported at approximately 200 ms per switch on hardware. Its native Vita3K baseline still scanned the compressed prefix on every reopen, decoded 39–42 frames per preview seek, and destroyed/recreated a feed worker on every selection. This page records the next implementation and a matched comparison against that build.

## Module ownership

- `mp3_frame.c` parses packet sizes and main-data payload sizes, including CRC and side-information overhead.
- `mp3_index.c` retains the 32-frame sparse index. It locates exact warm-up boundaries by reading headers around the requested frame. It keeps at least four preroll frames, extending up to sixteen when the payload is smaller. Main-data coverage must fill the maximum MPEG bit reservoir before the final two synthesis history frames. Decoder warm-up is still checked by `mp3.c`; unsupported layouts retain the linear fallback.
- `mp3_marks.c` retains sixteen exact preview boundaries in a fixed ring, guarded by one short mutex. Keys include canonical path, first-frame offset, target frame and stream format. Only immutable `app0:` paths shorter than 256 bytes enter this cache. Writable files never use it. The cache occupies 4,736 bytes for rows plus its cursor and mutex, is cleared when the MP3 library is released, and never retains PCM or file handles.
- `feed.c` owns one worker and its codec. Explicit open and seek commands run on that worker. A loaded, unplayed feed primes four blocks and then parks; read-ahead starts when playback or a direct queue read needs it. This avoids decoding the beginning of a song while its preview seek is about to replace that PCM.
- `bgm.c` retains at most one detached feed per backend. Unloading removes it from the mixer before parking it. The next load reopens its codec/file on the existing worker and primes the new track. The mixer lock is released throughout I/O and command waits. A feed with outstanding retained references cannot be parked or reopened; those references retain their original resource lifetime.
- `backend.c` closes both live channels and the detached feed at destruction. The cache can retain one worker, ring and previous decoder while unloaded; it cannot grow with the number of selections. During the measured unload/load sequence, the same worker moves between active and detached ownership.

The reopened feed belongs exclusively to the load operation. A failed file open leaves an empty feed that is closed by the backend; a separately active track remains intact when its replacement load fails. Commands continue to complete synchronously and report failure. Consumer positions, loop changes, playback delays and explicit rewinds keep their existing contracts.

## Validation

`build/song-switch-fast-20261025/seek.log` records the precise-boundary PCM check. `seek-reuse.log` adds worker reuse, retained-owner rejection, missing-file recovery, and PCM checks after reopening. The final `seek-release.log` additionally counts live SDK decoders and feed workers: both are zero before exit 0.

The probe compares four actual tracks at 44.1 and 48 kHz, including forward seek, nearby backward seek, reset/reseek and repeated seek. It checks exact output positions and permits at most one unit of difference in 16-bit PCM, matching the fixed-point rounding behavior already observed in the first implementation. Backend checks run twelve MP3/Vorbis unload/load cycles, preserve requested playback delay and preview position, reject a missing replacement without losing a live track, and destroy the final detached worker.

Release and Debug SELF targets compile. Changed audio sources compile with `-Wall -Wextra -Werror` for VitaSDK and host fixture headers. The general regression suite was not executed during this profiling session.

## Matched repeated selection

The native scene probe alternates Fairytale, Infinity Heaven and Sayonara Hatsukoi 48 times for each implementation: 24 ordinary changes and 24 at approximately half-second intervals. `baseline.log`, `optimized.log` and `comparison.json` preserve the results. The two implementations upload matching texture counts for every corresponding selection. All seeks succeed and all requested preview positions survive playback start.

| Metric | First indexed build | Precise seek and reused worker |
| --- | ---: | ---: |
| Switch frame, median | 60.01 ms | 20.97 ms |
| Switch frame, maximum | 81.02 ms | 47.49 ms |
| Rapid switch frame, median | 59.78 ms | 20.90 ms |
| Rapid switch frame, maximum | 63.54 ms | 39.12 ms |
| BGM load, median | 20.45 ms | 1.94 ms |
| Preview seek, median | 9.16 ms | 0.29 ms |
| Index lookup, median | 5.27 ms | 0.003 ms |
| MP3 decode calls per preview | 39–42 | 5 |
| Feed joins during 48 selections | 48 | 0 |

Median switch frame time fell 65.06%. The two newly encountered tracks still scan compressed headers on their first use: index lookup takes 1.81 ms and 2.44 ms in this host run, rather than the cached few microseconds. Initial entry into the selection scene is excluded. Texture upload itself remains around 0.25 ms; this does not include PNG decoding or other scene work.

These are host emulation wall times. They cannot be substituted for the next hardware measurement. The replacement binary is `build/song-switch-fast-20261025/eboot.bin`, without diagnostic wrappers; no resource package replacement is required. `previous-eboot.bin` preserves the first indexed-seek build.
