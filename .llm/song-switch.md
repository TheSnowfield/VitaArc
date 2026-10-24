# Song selection audio

This page records the first indexed-seek implementation and its measurements. The current precise seek boundaries, bounded mark cache and reusable feed workers are described in `song-switch-fast.md`.

## Synchronous work

`vita_audio_seek_bgm()` submits a seek to the PCM feed worker and waits for completion without holding the mixer lock. The previous MP3 implementation decoded every intervening frame from the current position, or from the start when seeking backward. Selecting a song sought to its preview offset, usually 36–50 seconds into the file.

Native 1.6.1c selection measurements identified 1,381 decoder calls for Fairytale, 1,890 for Infinity Heaven, and 1,699 for Sayonara Hatsukoi. These calls blocked the rendering thread through `audio_feed_seek()`.

The old `vita_audio_play_bgm()` then sought back to zero. The original Android `libfmodProvider.so` implementation sets loop count, volume, clock delay and paused state in `playBGM`; it does not call `setPosition`. Its separate `setBGMPosition` export performs that operation. The replacement now preserves the prepared consumer position.

## MP3 boundaries and warm-up

- `mp3_frame.c` owns MPEG Layer III header parsing, frame size, channel/rate matching, and main-data payload size. Bitrate and padding may vary between frames.
- `mp3_index.c` lazily scans compressed bytes up to a requested frame. It records one file offset every 32 frames, grows in groups of 64 offsets, and restores the decoder's file position after scanning. The three measured preview indices each reserve 256 bytes on Vita.
- `mp3.c` owns SDK decoder state, resampling and seeking. Long forward seeks and backward seeks beyond the opening second can use the index. A normal decoded frame must first establish full PCM frame output; buffering/partial output or format changes disable this path.

An indexed seek starts at a checkpoint at least 16 encoded frames before the target frame. Preroll discards all PCM, requires at least 511 main-data bytes for MPEG-1 or 255 for MPEG-2/2.5 before the final two history frames, and requires two consecutive full successful decoded frames. If these conditions fail, the stream disables indexed seeking and uses the original linear path. Invalid headers, truncation, rate/channel changes, allocation failures and out-of-range index requests also retain the linear fallback.

Target position calculations restore the original 48 kHz integer resampler phase. The decoder remains exclusively owned by the feed worker. No full-track PCM cache or additional worker is created. Index memory is released when the MP3 closes.

Bit reservoir and synthesis history references: [minimp3 seek implementation](https://github.com/lieff/minimp3/blob/master/minimp3_ex.h). FFmpeg's fixed synthesis implementation retains a rounding residue: [mpegaudiodsp_template.c](https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/mpegaudiodsp_template.c). Native Vita3K comparisons below observe a maximum difference of one 16-bit PCM unit, rather than claiming bit-identical decoder state.

## Feed controls

`audio_feed_start()` captures the consumer position while holding the command and queue locks. With the same loop mode, a prepared queue is reused. Changing loop mode seeks to that consumer position and rebuilds queued PCM so that previously prefetched loop boundaries cannot leak into the new mode. Explicit `audio_feed_seek(..., 0, ...)` still rewinds.

The mixer continues to read available PCM without waiting for file I/O or decoding. Synchronous control calls still report decoder failures; they do not return success while a preview is pending.

## Native measurements

Artifacts are in `build/song-switch-20261025/`. `.llm/song-switch/` contains the build helpers and native probes. The private Vita3K filesystem reuses read-only assets and a copy of persistent data. It does not change the installed application or original saved data.

The scene probe uses production game code and synthetic touches delivered through `sceTouchPeek`. Each run alternates Fairytale, Infinity Heaven and Sayonara Hatsukoi, with 24 selections at the normal interval and 24 more at approximately half-second intervals. Captures and command acknowledgements establish that the game remains in song selection.

`seek.c` runs production MP3 and feed code against SDK decoder endpoints. It compares 4,096-frame PCM windows after forward seek, nearby backward seek, reset/reseek and repeated seek for four tracks, covering 44.1 and 48 kHz. All windows preserve exact output positions, with a maximum PCM difference of one unit. Feed checks cover seek before play, both loop-mode transitions, queued consumption and explicit rewind. `seek-final.log` records exit 0.

The codec measurements for the three selection previews become 42, 39 and 40 decode calls respectively. `comparison.json` records matched repeated-selection frame times and seek times. Each implementation completed 48 changes: 24 at the normal interval and 24 at the rapid interval. No preview seek failed. All 48 new play calls retained their requested preview offset; all 48 baseline play calls reset it to zero.

| Metric | Baseline | Indexed seek |
| --- | ---: | ---: |
| Switch frame, median | 132.84 ms | 60.34 ms |
| Switch frame, maximum | 151.62 ms | 85.05 ms |
| Preview seek, median | 79.87 ms | 9.13 ms |
| Rapid switch frame, median | 133.93 ms | 60.34 ms |
| Rapid switch frame, maximum | 151.62 ms | 75.67 ms |

Median switch frame time fell 54.58%; median preview seek time fell 88.56%. Initial scene entry is excluded because it also creates UI objects and textures. `baseline-matched.log` and `optimized-full.log` include clean exits. The final linked decoder/feed check is in `seek-release.log`, also exit 0. Release and Debug SELF targets compile, and changed audio sources compile with `-Wall -Wextra -Werror` under both VitaSDK and host fixture headers. The existing general test suite was not executed in this profiling session.

Host emulation timings do not establish the duration of the reported five-second stall on physical hardware. The replacement binary is `build/song-switch-20261025/eboot.bin`; it contains no profiling wrappers and requires no asset replacement.
