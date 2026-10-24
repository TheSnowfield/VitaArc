# PSV hardware startup reports, 2026-10-24

## Core 1792857926: JPEG bytes passed to the layout parser

Input: `build/texture-profile/VitaArc-1.6.1c-textures-vita/psp2core-1792857926-0x0002f82793-eboot.bin.psp2dmp`.
Decoded core and note records: `build/crash-1792857926/`.
The accompanying eboot SHA256 is
`2e888ea9717f6da8b61bac563a7fba3ba0f3bdd5c45ce3363c13b47dd642bc2f`,
matching the saved texture-profile release SELF. Preserve
`build/texture-profile/release/` for this crash's original build artifacts.

The main thread `0x40010003` stops on a data abort at `0x9840cf9a`, in
`flatbuffers::Table::GetOptionalFieldOffset`. LR `0x9840fa35` returns to
`CSLoader::createNodeWithFlatBuffersData`. The library is the local 1.6.1c
image mapped at `0x98000000`.

The stack contains `assets/layouts/songselect/HighscoreEntry.csb`. Its `Data`
object at `0x81600b74` holds buffer `0x83291660` and length `0x3c54` (15,444).
The original CSB is 2,936 bytes and begins with root offset `0x14`. The dump
buffer instead begins `FF D8 FF E0`, a JPEG header. Its first 1,024 bytes and
reported length match the converted `songs/lostcivilization/base_256.jpg`.
The remaining captured bytes differ, so this is not a claim that the entire
buffer is an intact copy of that JPEG. Treating its first word `0xe0ffd8ff`
as a FlatBuffers root offset produces `0x6428ef5f`; the subsequent vtable read
faults at `0xcf8de460`.

The APK and converted complete VPK retain the original CSB bytes. The texture
overlay does not contain CSB files. The dump establishes mixed resource data
in memory; it does not independently verify files on the installed memory card.

The linked newlib has an unprotected global FILE allocator. In the saved ELF,
`__sfp` at `0x8115b064` checks a shared slot's flags before reserving it, with
no lock or atomic reservation. `__sfp_lock_acquire/release` at
`0x8115b140/144` are just `BX LR`. Native asset, codec and library stdio calls
can bypass the guest bridge's descriptor mutex. Concurrent resource workers
and audio prefetch make duplicate FILE-slot allocation possible, consistent
with the mixed length/header and 1,024-byte boundary observed in the core.
The exact thread interleaving cannot be reconstructed from this snapshot.

`src/platform/stdio.c` now wraps `__sfp`, `__sinit` and the FILE-pool lock hooks
at link time. A shared recursive mutex covers allocation, lazy initialization
and slot release, including native callers in linked libraries. File reads,
writes and native opens stay outside that lock. The linked `pthread_setup`
constructor initializes pthreads before its first wrapped `__sinit` call.

Release and Debug SELF targets compile. Disassembly confirms fopen, fdopen
and fopencookie reach the guarded allocator, and fclose/open-failure cleanup
reach the guarded release hooks. No test suites or emulator runs were executed.
Hardware confirmation is pending. Replacement artifacts are retained under
`build/crash-1792857926/fix/`; the existing texture assets remain applicable.

## Core 1792850220: database open failure

Input: `build/psp2core-1792850220-0x000087285f-eboot.bin.psp2dmp`.
The compressed ELF core is decoded at `build/crash-1792850220.raw`.
The app RX mapping starts at `0x81028000`, with size `0x1d7808`.
The preceding Debug artifacts are retained under
`build/crash-1792850220/before/`; their text size differs from the dump, so
they must not be treated as an exact address match.

The faulting thread is pthread `0x400301e1`, PC `0x8118b1d2`.
Registers contain signal 6 and the process ID in the SDK kill path; the
undefined instruction is the abort trap. The main thread is in file I/O.
Game code windows at `0x985f2400`, `0x98626800` and `0x986298a0`
match the local 1.6.1c library byte for byte, using base `0x98000000`.

The active exception at `0x81ccf440` has the game's database_error vtable
`0x9871a8d8`; its message at `0x81ccf45c` is `can't connect database`.
The saved throw PC `0x9834fb8d` follows sqlite3_open_v2's failure branch
in DBManager's constructor. SQLite's retained database handle has error 14
(CANTOPEN). Paths in the worker stack resolve to
`ux0:/data/vitaarc/persistent/data/files/st3`.

The last retained native open failed with `0x80010002` after SQLite had
fallen back to read-only flags `0xa0000`. This does not establish the
first read/write/create error: that invocation reused the same stack frame.
The directory exists and st3 is absent. The shader program 30 message is
the last visible main-thread progress, not the exception source.

Storage initialization now prepares the data directory and opens/closes st3
through the game's SQLite VFS before JNI starts the worker. A failed open
returns through normal startup cleanup with its SQLite message/code logged.
Failed writable native opens also log their path, flags, mode and raw Vita
error before SQLite can replace that error with its read-only fallback.
The subsequent hardware log captures the initial creation error:
`flags=0xa0042 mode=0644 error=0x80010016`. The directory is already present.
Vita's unsafe executable permission rules require system RW bits. POSIX 0644
omits SCE_S_IWSYS, so forwarding it unchanged is invalid. The installed SDK
documents this rule in psp2common/kernel/iofilemgr.h; its newlib _open_r uses
0666 rather than forwarding the caller's creation mode.

The libc boundary now normalizes open, mkdir and fchmod permissions through
vita_io_mode, retaining only 0777 and adding SCE_S_IRSYS | SCE_S_IWSYS.
SQLite's 0644 becomes 0646 at the native call. Error logs retain both the
requested mode and the translated Vita mode. Hardware confirmation of the
updated binary remains pending.

These reports are actual PSV hardware failures, separate from the completed
Vita3K-only offline play tests. Do not label hardware compatibility verified.

## Before enabling kubridge

User screenshot: prefetch abort, EPC/PC/BadVAddr zero, LR 0x81088cbd.
The user then explicitly noticed kubridge's kernel module was not enabled.
They enabled it and reported a different subsequent failure. Do not treat
enabling kubridge as proof that all startup failures are fixed. The first LR
cannot be safely symbolized without the executable's actual load base/build.

## After enabling kubridge: 2.0 immediate startup

User confirms version 2.0 and crash immediately on launch. Screenshot attachment:
`C:/Users/CirnoBaka/.codex/attachments/17c422b8-5cf1-42b6-aeb1-274067ed995f/codex-clipboard-d935161a-ad80-46c0-8d17-9f70414ce41f.png`.

- Data abort, EPC/PC 0x9870c278, BadVAddr 0x140, LR 0x9853e721.
- r0 0x819a3510, r1 zero. Game library maps at 0x98000000.
- Exact 2.0 ELF offset 0x70c278 is cocos2d::Node::addChild(Node*), instruction
  `ldr.w r2, [r1, #0x140]`. The child pointer is null.
- Caller at 0x53e71e is StartScene::init()+0x41e, `blx r2`; returns to 0x53e720.
- The preceding Sprite::create call at 0x53e6f6 receives the 24-byte asset name
  `startup/start_wreath.png`, and stores the result in StartScene+0x23c before
  invoking addChild. Thus this sprite creation failed. The next code would
  dereference it again; suppressing addChild alone would not fix the failure.
- Asset resolved by literal at 0x53ed08 plus Thumb PC at 0x53e6ea -> 0x965ff6.

The local 2.0 APK and final r3 VPK contain identical valid PNG bytes:
137677 bytes, 745x576 RGBA, SHA256
45754e36c93751dd968dbe1b21a430558c91519e32d7200b1737266b39610263.
This verifies the packaged input, not the user's installed file or the game
decoder/texture allocation on real hardware.

Root failure remains unresolved: resource read/decode/texture creation and
memory/compiler failures can all lead to a null Sprite. Request the actual
`ux0:vitaarc/boot.log` (and optionally .psp2dmp) before claiming a specific cause.
An unanswered optional question asks whether libshacccg.suprx is installed.
Current main attempts runtime compilation first, enabling the packaged offline
shader mode only if both compiler-init paths fail. Vita3K tests lacked the runtime
compiler and used the 39 packaged offline binaries. Hardware may take the other
path; this is a hypothesis to distinguish with logs, not an established cause.
Do not bypass setup, replace missing graphics with success stubs, or silently
patch game null checks to claim a fix. The first screenshot alone did not
establish a specific reason to change the game code.

## After adding shacccg: heap corruption, diagnostic r4

The user subsequently added shacccg and supplied a different hardware data
abort: EPC `VitaArc + 0x0015615e`, PC 0x811b915e, BadVAddr 0x1a881370.
For the original released ELF, offset 0x15615e maps to `_free_r+0x5e`, at
`str r6, [r4, #12]`. Screenshot r4=0x1a881364 matches that invalid write.
This is corrupted allocator free-list state during free, not proof that the
runtime compiler itself caused the earlier corruption. LR is zero in this
allocator instruction sequence and does not supply its caller.

The exact original ELF, VELF and SELF (878234348f410c8ebfdb62036b775a99f1f9cfa22662d12938a23dbda760c5ff)
are preserved at `build/psv-2.0-r4/baseline/`. Use that ELF for the reported
offset, not a subsequently rebuilt ELF with shifted function addresses.

Diagnostic r4 now prefers a valid offline shader pack before trying shark_init,
even when a runtime compiler is installed. Without a valid pack it retains the
two runtime compiler paths. Main, touch and lifecycle fixtures use this same
selection order. This removes a hardware-specific compiler path from the
packaged configuration; it is a controlled test, not a proven heap fix.
Startup logs also identify the selected path and record used/free/arena heap
sizes before library loading, after constructors, and before/after JNI.

SELF SHA256: 7a2ae2841a4ad5c5be345eea419bd82a5d89984e7a75f570709e5d5144ac3674.
`build/psv-2.0-r4/VitaArc-2.0.0c-PSV-r4.vpk` differs from the final r3 2.0
package only in eboot.bin; all other entries are byte-identical and CRC-checked.
The companion eboot ZIP is for replacing a backed-up eboot in an existing 2.0
installation that already has the offline pack. No game assets, null checks or
save data are changed. Instructions and package-verification.json are alongside.

40 linked ARM startup/controller/cleanup cases pass, including no runtime init
when both pack and compiler are available. ARM shader cache coverage also passes
online/offline dispatch, malformed/missing/link failure and handle cleanup.
The actual r4 eboot runs in Vita3K for 45 seconds and restores original data
after bounded observation: qa/run-2.0.0c-20261024-163600. Actual PSV validation
of r4 and the hardware boot.log remain pending.
