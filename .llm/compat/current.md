# Current compatibility verification — 2026-10-25

Production SELF SHA256:
`9f7dfbccba6656381afee7ddc1ed90fdfb8ece767beab4e47cfeaaf8db8729f4`.
Native heap reservation: 256 MiB. Graphics RAM cap: 32 MiB; stream: 6 MiB.

The loader obtains a free address window from the kernel before creating
individual RX/RW segment blocks. A fixed 0x98000000 base collided with the
256 MiB Newlib heap. Native runs now load successfully at allocator-selected
addresses; loader instruction tests cover permissions and failure cleanup.

| APK | Native Vita3K verification |
| --- | --- |
| 1.6.1c | Title, menu, repeated preview changes, full Fairytale Past 1 result, return; separate input run reaches score 14880, pause/resume |
| 2.0.0c | Title, menu, repeated preview changes, full Fairytale Past 1 result, return; separate input run reaches score 44542, pause/resume and return |
| 3.0.0c | Title, menu, repeated preview changes, full Fairytale Past 1 result with score 44542, pause/resume and return |
| 7.0.260c | Title, menu, song changes and Brand new world Past 2 gameplay with the same 256 MiB production objects |

Runs use private, version-matched APK assets and cloned test saves under
`build/compat-current/<version>`. Real shaders, audio, JNI and game code run
in Vita3K SDL/OpenGL/HLE. Framebuffer capture and synthetic panel input are
diagnostic wrappers. Screenshots and `heap256-native.log` are retained per
version; 7.0 evidence is under `build/arcaea-7.0.260c/runtime`.

The input probe now emits an explicit release report after a synthetic tap.
Outside injection/release it preserves physical input. The ARM probe test
checks raw multi-touch passthrough, injection, release, restoration and errors.

One instrumented 1.6 cold start faulted in Newlib `_malloc_r` before its first
capture. A production-eboot startup and a subsequent instrumented run did not
reproduce it. Its cause remains undetermined; these checks do not establish
absence of intermittent faults or leaks. Production startup evidence is
`build/compat-current/1.6.1c/production-heap256.log`.

ASan/UBSan host tests: 26/26. Actual linked ARM ELF checks pass for loader
mapping, cleanup and malformed inputs, SDK ABI adapters, and old AudioManager
callers. Real-library relocation/import counts are 48281/329 (1.6.1c),
76279/347 (2.0.0c), and 87022/352 (3.0.0c). The 1.9.0c instruction checks also
pass. Logs are `build/arcaea-7.0.260c/compat-*.log`.

The release script defaults to a base VPK. The generated
`build/release-7.0.260c/VitaArc.vpk` is 1862957 bytes with 80 entries, including
67 offline shader binaries. ZIP CRCs pass; no APK, `package/` tree or APK
version marker is present. Its eboot equals the production SELF above.

Physical PSV input/GPU behavior and online/login/purchase flows are not
established by these emulator checks. Optional SDKBOX Java warnings remain
on older versions. Diagnostic process exits are bounded stops, not proof of
complete game-service teardown.
