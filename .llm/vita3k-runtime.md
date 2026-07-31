# Vita3K runtime evidence, 2026-10-24

## Direct-executable MP3 failure, 2026-10-24 18:53

User reports song BGM missing. Current process 26284 launches Vita3K-gl44.exe
without the HLE launcher. Host log confirms missing kernel imports 0x8612B243
and 0x6C5AB07F, AUDIOCODEC module load 0xc05a1000, then MP3 disabled.
Title/menu Ogg loads normally. fairytale, infinityheaven, impurebird preview and
sayonarahatsukoi MP3 assets exist; this is not a missing-song-file failure.
The first copied launcher exposed an additional Windows PowerShell GUI-process
lifetime bug: `& Vita3K-gl44.exe` returns before the GUI exits, leaves LASTEXITCODE
null and restores bootimage too early. Replaced it with Start-Process -Wait
-PassThru, explicit quoted arguments and child ExitCode. Verified helper remains
alive and bootimage stays disabled through the emulator lifetime; stopping the
SDL test restores the original firmware SHA256. That SDL run could not accept
user touch, so the launcher now defaults to Qt (optional -Frontend sdl remains).
New Qt + HLE session: helper PID 23292, emulator PID 35128, fresh logs archived
under preview-fix-20261024-1900. User explicitly confirms: "能操作，也有预览声音".
Added root/runtime Run-VitaArc.cmd, updated bundle source and shipped launcher
patch plus a new Intel/audio bundle; older Intel-only ZIP is retained.
Evidence: build/vita3k/bgm-launch-fix-20261024.json. This repairs local emulator
launch only; it does not establish resolution of the separate PSV hardware crash.

## User-confirmed visible output, 2026-10-24

The user now explicitly confirms that local Vita3K displays the game, with
neither a black nor a white screen. This supplies human visual confirmation
beyond the earlier native framebuffer exports. The exact game version/build
and display setup were not specified, so do not infer visible verification of
all four APKs, physical touchscreen verification, or a changed monitor state on
the original host. Historical screen-verification blockers below are superseded
for the user's reported local run.

The user attributes earlier output failure to the missing connected display.
This is consistent with the original host's zero connected monitors, failed WGC
capture and valid game/display-buffer pixels; treat the headless/remote
presentation chain as the supported explanation, not a proven causal test.
Do not keep changing game shaders to chase that historical white/black output.
The separate Intel texture2D compile failure and its compatibility fix remain
independently documented below.

## Intel screen-shader compatibility correction

The user's Intel Graphics / OpenGL 4.6 system rejects texture2D in the host
forward-compatible screen shader, causing ScreenRenderer::init to fail before
game presentation. The original release ZIP's OpenGL render_main_bicubic.frag
has four such calls (SHA256 9fa3658ded5ef92617a8fb9f1e4f4818ade997a44c0061bdff77619f95b76b50).
Upstream master still has the same calls, and glutil/src/shader.cpp prepends
#version 410 core. screen_render.cpp unconditionally compiles normal, FXAA and
bicubic programs, so choosing Bilinear alone does not avoid the failing compile.

The local fix replaces only those four calls with texture, with a dated source
notice; executable/game data are unchanged by this fix. Fixed shader SHA256
11912e8119d5a23dae8bcc59e6d0675098269f9a71c75aa60e356b7cd62468db.
OpenGL SPIR-V semantics validation reproduces original rejection and passes
all nine corrected normal/FXAA/bicubic compile/link cases at GLSL 410/440/460
core. This is compiler validation, not Intel-driver execution on this host.

Small overlay: build/Vita3K-Intel-OpenGL-shader-fix.zip. Extract into the
directory containing Vita3K.exe and replace the shader, then restart. Complete
revised bundle: build/Vita3K-with-firmware-Intel-fix.zip. The older unpatched
Vita3K-with-firmware.zip remains preserved and is superseded for Intel use.
Packaging now explicitly includes the compatibility override and its validation
record, instead of silently taking that file unchanged from the release ZIP.
Vita3K.exe remains official; the separately named Vita3K-gl44.exe remains the
previous one-byte experimental context-version copy, not an official binary.

Primary sources:
https://github.com/Vita3K/Vita3K/blob/master/vita3k/shaders-builtin/opengl/render_main_bicubic.frag
https://github.com/Vita3K/Vita3K/blob/master/vita3k/glutil/src/shader.cpp
https://github.com/Vita3K/Vita3K/blob/master/vita3k/renderer/src/gl/screen_render.cpp

## Active multi-version goal

The local multi-version compatibility work fixes 1.9/2.0/3.0 and retains 1.6
with a symbol superset. All four now pass native offline gameplay, synthetic
touch, pause/resume, results, save/restart and final packaged eboot checks. See
apk-compatibility.md for the current SELF, imports/math/kuser/format/audio and
bitmap-text fixes, exact full-play/persistence evidence and online/hardware limits. The older
seven-item goal/display gate below is historical; it is not a reason to stop
the current software work. Runs 047/048 were closed for isolated tests. Final
1.6 SDL/OpenGL session 050 (PID 61864) uses SELF 878234348f410c8ebfdb62036b775a99f1f9cfa22662d12938a23dbda760c5ff;
its launch record, prior installation and prior persistent data are preserved
under build/vita3k. boot-050-evidence.json verifies Cocos/frame120, two BGM
channels, MP3 enabled and no native fault. Qt attempt 049 crashed in the host
audio-module load; its logs are retained, and it is not a successful session.
The following sections describe earlier stages.

## Historical external verification gate

This records the earlier state; the user's local visible-output confirmation
above supersedes its missing visual evidence. Physical touchscreen evidence is
still distinct from the successful synthetic game-panel tests.

The last display inspection finds zero connected Monitor devices. Run 046's WGC
capture fails with `FrameArrived timed out: timed out waiting on channel`; after
fresh window selection/activation, retry fails with `window capture timed out:
timed out waiting on channel`. No coordinate input is attempted without usable
window geometry. The same physical display/input gate has persisted across
three consecutive goal turns; the remaining requirement cannot be independently
verified on this host. The goal is blocked, not completed. Resume after display
connection or user physical verification; retain the normal process and VPK.

## Local APK batch

On the user's request, separately packaged 1.6.1c, 1.9.0c, 2.0.0c and 3.0.0c
VPKs pass CRC, APK-entry and common-resource verification. Actual packaged eboot
tests with fresh, isolated data confirm the 1.6.1c title at frame 1800; 1.9.0c
and 2.0.0c each refuse loading with 37 unresolved imports, and 3.0.0c with
__aeabi_memmove8. All failures precede JNI. See apk-compatibility.md and
build/apk-versions/compatibility.json for complete hashes and import lists.

Each test restores the original installation/data and checks their hashes.
The main VPK is unchanged (SHA256
4fba841accc37096b20ba591da3f51d9fca9f073e1c04047fa60ddc6933e8780).
After the batch, stock Qt/OpenGL run 047 is left live as PID 46816 with the
current 2eb9ac13... SELF. Cocos and frame 120 succeed, two BGM channels load,
MP3 remains available and no native fault is reported. Original private firmware
is restored. This replaces run 046 as the active normal session; the physical
display/input gate remains unverified.

## Final storage soak and wait audit

The post-storage-fix native soak in
build/vita-touch-probe/run-20261024-121526 passes 6600 frames, ten pause/resume
cycles, stable sampled JNI counts (53 objects/6 references from frame 1800),
production game unload and guest exit zero. Reported allocated heap falls from
8632888 at frames 1200–3000 to 8456248 at frames 3600–6600. The data-isolating
runner now supports lifecycle-probe/lifecycle-soak and restores original data.

The completion audit finds an additional named POSIX gap: on a failed Vita wait,
nanosleep returned the whole outstanding chunk rather than subtracting actual
elapsed time. It now measures from the original request, includes earlier chunk
oversleep, handles borrowing/clamping and avoids time_t multiplication overflow.
NULL remaining pointers keep the existing wait path. ASan/UBSan and TSan Vita
boundary tests pass; check_arm_nanosleep.py executes eight actual guest-export
cases with a 64-bit clock, 32-bit structure canaries and aliased pointers. This
is injected kernel-endpoint evidence, not physical signal/scheduling proof.
Reference: https://pubs.opengroup.org/onlinepubs/9799919799/functions/nanosleep.html

The rebuilt VPK has 1784 entries, 494634640 bytes, verified CRCs, all 39 shaders
and both codec notices. Current eboot SHA256:
2eb9ac13f9be250ad6f1de45a408d7939bb332cd41b0cd3b2afb78072261556c.
Normal stock Qt/OpenGL run 046 installs this SELF. The earlier hashes and run
045 below describe the gameplay-tested version before this wait-only correction.
Hardware inspection still returns no present monitor; physical host presentation
and actual touch hardware remain unproven, so the seven-item goal is not complete.

## Latest game-side touch/play verification

Windows capture remains unavailable without a connected monitor (run 043 first
capture and the retry after Raise both time out). The standalone touch SELF
therefore replaces only sceTouchPeek with synthetic front-panel reports and
runs production vita_input_poll/frame_step, coordinate scaling, JNI callbacks
and the actual game. Its checkpoints read the real sceDisplay surface through
GXM after swap. These pixels are game artifacts, never Windows input geometry.
This verifies game-side input and gameplay, not physical touch or host display.

Actual play exposes and fixes four independent defects:

- AppActivity.logCrashlytics(Ljava/lang/String;)V now retains breadcrumbs in the
  local log. The APK DEX body forwards only to Crashlytics.log(String). Tests
  cover ordinary/V/A calls, null, literal format characters and wrong signatures.
- Bionic _ctype_/_tolower_tab_/_toupper_tab_ are pointer data symbols. They now
  reference 257-entry byte/16-bit tables with EOF and legacy byte classifications.
  The old direct arrays caused TinyXML to read address 0x2020203d after Music Play.
  The ARM test checks all bytes, symbol indirection, and both actual game TinyXML
  whitespace parsers through production relocation. ABI reference:
  https://android.googlesource.com/platform/bionic/+/53e4329/libc/include/ctype.h
- fopen("/dev/urandom") now creates a native RNG-backed cookie stream. The
  previous descriptor-only support did not cover the game's C++ random_device.
  Native gameplay proceeds past the previous abort; host tests cover fread,
  read error/clearerr/recovery, fclose and unsupported write/update modes.
- st3 initially stays empty and scores disappear: the actual bundled SQLite
  returns extended error 3850 (IOERR_LOCK). Startup now selects its built-in
  unix-dotfile VFS before nativeInit. This uses atomic directory locking via the
  real filesystem. Standard POSIX record-lock calls remain explicitly unsupported.
  The native SQL test passes write, a second connection returning BUSY during an
  exclusive transaction, rollback, close/reopen and readback. Independent host
  SQLite integrity_check returns ok and reads value 37. VFS reference:
  https://www.sqlite.org/vfs.html

The full native input run at build/vita-touch-probe/run-20261024-114102 reaches
Fairytale Past 1, pauses/resumes, accepts paired ground taps, shows 2 PURE / 1 FAR
and score 74405, completes TRACK LOST, returns to selection, and scrolls the list
with a drag. This pre-storage-fix score is not retained. After the VFS fix,
run-20261024-115647 completes the same chart with 1 PURE, score 29761 and rank D;
selection immediately displays that score. The st3 database is 4096 bytes and its
integrity_check is ok. A fresh process in run-20261024-120315 loads the same score
and rank. Both exit zero through production game unload. Scores reflect coarse
test input and synthetic calibration, not play quality or timing calibration.
Every fixture uses a cloned persistent directory and restores the original;
tested data, raw frames, acknowledgments and logs remain in its run directory.
Named PNG artifacts: fairytale-result-with-storage-fix.png,
fairytale-score-after-return.png, fairytale-score-after-restart.png.

ASan/UBSan 21/21 and TSan 20/20 pass after the JNI/RNG changes. Linked ARM main
passes 40 cases including storage failure cleanup. Actual game constructors /
JNI_OnLoad still pass strict zero-retained-heap teardown; 104 GL lookups and the
actual audio vtable ABI checks pass. The latest VPK has 1784 entries, 494634640
bytes, all CRCs valid, all 39 shader binaries and both codec notices verified.
Its eboot SHA256 is f7b97b3c927289b949bfa768329b52ea92ce36b7bead6d93091d99c551723d9b.
Run 044 switches back to the normal visible Qt executable with this SELF. Its
native file still contains old trailing records despite the guest O_TRUNC flag,
so its loaded_bgm count must not be used as current-run evidence. Runners now
rotate the previous native log before launch; run 045 verifies a fresh file.
Run 045 remains live as PID 65460 with the official visible Qt/OpenGL executable.
The fresh native file has one startup, frame-120 presentation, exactly two title
Ogg streams and no previous Log stop/scene markers. SQLite directory locking is
configured and MP3 is available. The installed eboot matches the VPK SELF hash.
The private firmware is restored and default configuration remains unchanged.

Remaining gates: physical screen/final host presentation, real touch hardware,
device timing/performance and broader chart/download/network coverage. The
standard dotfile VFS can retain a lock directory after abnormal termination;
never delete a lock while a process may own it or mix different VFS locking
schemes against the same database. No automatic stale-lock deletion is added.

## Earlier continuation before native gameplay

The user subsequently authorizes actual touch/play testing. Run 043 starts the
normal interactive stock Qt/OpenGL executable with HLE kernel and F12 enabled;
its window reports about 60 FPS. Windows still reports no connected monitor.
Fresh window capture fails with `FrameArrived timed out: timed out waiting on
channel`; reselecting and raising the window then fails with `window capture
timed out: timed out waiting on channel`. Accessibility exposes only window
chrome, with no game controls. F12 is sent to the focused game window, but no
frame export is found. Actual touch/menu/chart/gameplay remains unverified;
internal framebuffer captures cannot supply Windows input geometry.

Independent native checks continued. The lifecycle probe found and fixed a real
resume failure: Cocos2dxRenderer.setAnimationInterval(F)V was unsupported. The
descriptor is confirmed from the local 1.6.1c APK. It now updates thread-safe
frame pacing. Default 60 FPS/faster use vitaGL vsync alone, while slower requests
delay only for time remaining after rendering/swap. Host JNI ordinary/V/A calls,
invalid values and timing boundaries pass; linked ARM main now has 34 cases.

Real-game lifecycle native probe passes on both HLE and normal firmware:
start real game -> 600 frames -> pause/resume -> 1200 frames -> production stop,
application/audio cleanup, game unload, GL buffer cleanup -> guest exit 0.
JNI counts are 51 objects/6 references at frame 600 and 53/6 at frame 1200.
The additional HLE `--soak` run passes 6600 frames and ten pause/resume cycles.
From frame 1800 through 6600, all sampled JNI counts remain exactly 53 objects /
6 references. Reported allocated heap is 8511320 bytes at frames 1200–3000 and
8334584 at frames 3600–6600; heap is informational, not an enforced leak check.
Production unload completes and the guest exits zero. Evidence:
build/vita-lifecycle-soak/probe-vita3k.log. Firmware and normal eboot hashes match.
No unbounded allocation-stability claim is made. This proves title-scene cleanup,
not teardown during gameplay/downloads. Logs are in build/vita-lifecycle-probe/.

Separate stock-emulator writer/reader processes run the actual game UserDefault
get/set/delete methods through production JNI and Vita I/O. Both read back
5350324, the reader deletes the reserved key and confirms its fallback value,
and both exit zero. The original private preference file is restored by SHA256.
The fixture initializes the game's JavaVM and real native context setter without
creating a scene, and keeps code mapped until process exit. This proves native
Cocos preferences, not chart-score/st3 save persistence. Logs:
build/vita-preferences-probe/write-vita3k.log and read-vita3k.log.

Default logs now truncate/write ux0:vitaarc/boot.log and mirror INFO+ to the
system console. The previous built-in UDP destination is removed; UDP requires
an explicit destination in a developer build. ARM logging tests verify bounded
writes, zero handle acceptance, truncation and final message before closure.
ASan/UBSan 21/21, TSan 20/20, real-game ARM bootstrap strict zero-retained-heap
check, canonical GL/audio ABI checks, 34-case ARM main and ARM logging all pass.
The JNI suite is rerun after the final default-vsync timing adjustment.

Run 041 uses the updated normal shader, stock Qt/OpenGL, HLE kernel and no GPU
capture flag. MP3 is available and both title Ogg streams load. Its Qt log sample
ends mid-record before frame-120 output, although the native file contains that
marker. The runner now archives a fresh native log for runtime markers and
records the installed SELF hash at launch. Run 042 installs the final timing
adjustment for the user's later verification; window visibility remains deferred.
The final installed SELF and VPK eboot both have SHA256
7d3c16babbd2952c3b064e671333d5fcd31728e195dd949b96d3522603ca31cc.
VPK: 494633301 bytes, 1784 entries, all CRCs valid; 39 offline shader binaries
and both codec notices match their inputs. Run 042's live title reports about
60 FPS after removing the default-vsync extra sleep (run 041 was about 53 FPS).
This is host-reported timing, not a physical display or gameplay verification.

Run 033 uses the unchanged official executable, visible Qt frontend, OpenGL 4.6
and the normal firmware. The user confirms audible game sound but reports a white
game window. Native GPU export still shows the title artwork. Rendering to an
internal surface is established, but visible host presentation is not verified.
The later monitor checks below identify an environment-level alternative to an
emulator presentation defect.
Run 034 adds independent GXM readback of the actual sceDisplay framebuffer after
swap to distinguish the display surface from vitaGL's active read framebuffer.
Its actual displayed framebuffer is 960x544, pitch 960, ABGR8888 at 0x602a4000.
GXM transfer readback after display-queue completion contains the real startup
image, establishing display-surface pixels independently of active GL read FBO.
The user's white host window therefore cannot be treated as successful display
merely because either internal capture has pixels. Final window presentation
remains the active issue.

The user corrected an accidental answer selection after run 034: the window is
still pure white, not the title artwork. There is no verified successful host
presentation in this run. Preserve the internal/display-surface evidence as
separate from the user's direct observation of the actual window.

Backend comparison: runs 035/036 requested Vulkan but actually initialized
OpenGL. Vita3K's config merge ignores values equal to the compiled default
(`Vulkan`), retaining the portable global OpenGL setting. The runner now writes
`VULKAN`, accepted by case-insensitive backend selection but distinct during
merge, and records the renderer observed in the log. Run 037 verifies actual
Vulkan initialization on the unchanged executable, then crashes with null read
0xC0000005 before loading the guest. This is not a game crash or a display pass.
Run 038 uses the pre-existing SDL/OpenGL-4.4 diagnostic executable, normal
firmware, and the same SELF. Cocos initialization, frames 120/1800, independent
display-buffer export and both Vorbis music loads pass. Host capture still times
out. The user confirms this SDL window is black; no visible game image, touch
response or gameplay success is inferred from its GPU export.

Run 039 temporarily uses a purple checkerboard in the host presentation shader,
leaving the native SELF unchanged. The shader file is restored immediately after
renderer initialization and its SHA256 matches the saved original. The user
instead raises a powered-off monitor/remote capture explanation; no checkerboard
result is supplied. F12 frame-export attempts have not produced an image file.
Run 040 restores the ordinary host shader with the unchanged official Qt/OpenGL
executable, normal firmware and the same native SELF; the F12 export key remains
enabled in this run's private config.

Host display inspection at 10:40 (host-display-20261024.json): pnputil finds no
connected Monitor devices; the cached Generic PnP Monitor has Present=false.
Legacy Win32_DesktopMonitor reports Default Monitor, Availability=8 (offline),
with no dimensions. The RTX 3060 Ti is present/OK and still reports a 2560x1440
239-Hz mode. Both monitor WMI classes return Not supported. The session is an
active console session. A fresh requested Windows.Graphics.Capture of the live
game window again times out. This supports a disconnected/offline display or
remote capture limitation, but does not prove the cause. Do not keep changing
game rendering based solely on white/black remote output under this condition.
The user cannot enable a virtual display and explicitly defers visible-window
verification until returning to the machine. Continue independent audio,
lifecycle and cleanup work; do not treat this as a request to pause the goal.

MP3 audit after that deferral: production logging now records the precise failing
SDK stage. Normal-kernel native probe fails in sceSysmoduleLoadModule(AUDIOCODEC),
returning 0xc05a1000 after missing kernel NIDs 0x8612b243 and 0x6c5ab07f
(ksceSblACMgrIsSystemProgram/IsNonGameProgram in the SDK database). The decoder is
never reached. The same newly rebuilt production MP3 probe with HLE kernel
decodes 48000 frames (46590 nonzero), passes seek/read/reset and muted output
position/pause/seek/teardown, and exits zero. Original firmware SHA256 is restored.
Logs: build/vita-audio-probe/probe-normal-firmware-20261024.log and
probe-vita3k.log. Host ASan/UBSan audio tests pass after the logging change.

The title assets in_full.ogg/in_loop.ogg are actual Ogg/Vorbis, not MP3. Run 030
exposed the old MP3 scanner accepting random headers inside those containers and
then failing sceAudiodecDecode. New stream.c dispatches by content and uses SDK
libvorbisfile 1.3.7/libogg 1.3.6 for seekable mono/stereo Vorbis, including 48 kHz
output positions, resampling, exact PCM seeks, loop/reset and failure recovery.
Vorbis works even if the native MP3 library is unavailable. Direct MP3 open now
rejects Ogg/RIFF. Native Vorbis probe on the stock executable and normal firmware:
48000 frames, 47999 nonzero; seek/read/reset passed; muted worker position
64 -> paused 64 -> resumed 542 ms; clean exit 0. This probe does not prove audible
output; the later user's full-game observation supplies that separate evidence.

The Qt launch was initially held at welcome/missing-font dialogs. The official
font PUP URL is linked by the exact emulator's welcome source via bit.ly/2P2rb0r,
redirecting to Sony's 2019_0924/sd_8b5f60b56c3da8365b973dba570c53a5/PSP2UPDAT.PUP.
Downloaded 56768512 bytes, MD5 8b5f60b56c3da8365b973dba570c53a5, SHA256
f372d306a7f2711125bea29aa75cbcb1eb19724e9648478fba07b8d655b37344. CLI installation
reaches 100%, sa0/data/font/pvf contains the fonts, and the original bootimage
SHA256 remains c2b8e36324544a08a6ffa6caaf79250ca02399400f4506c84479c68dd5ae21f6.
No warning was bypassed. All installation is confined to the private profile.
Qt avoids the SDL frontend's OpenGL 4.3 limitation; no emulator binary patch is
needed for this latest run. Computer Use keyboard focus/navigation works, but
screenshots still time out and coordinate geometry is unavailable.

Earlier run descriptions below are historical. Run 027 now reaches Cocos
initialization at 960x544, presents frames 1 and 120 and exports frame 120 through
native vglReadPixels. The 2088960-byte RGBA artifact, converted with a vertical
flip to build/vita3k/frame-120.png, visibly contains the real startup background,
central emblem and introductory English text. Log: boot-027-vita3k.log. This is
actual game rendering evidence, not proof of menu navigation or gameplay.

The native pixel unpack bridge now handles alignment and row length; the real
shader probe verifies padded RGB rows, translated geometry, GL error zero and
exit zero. Production EHABI lookup replaces the empty exidx shim. All 16746 real
game table entries pass independent ARM lookup checks. The separate stock-emulator
C++ probe returns 43 (payload 42 plus one RAII cleanup) and exits zero using
catch(...), the game's exception runtime and its validated personality code.
Typed cross-toolchain catches and concurrent unload are not established.

Run 028 installs the updated SELF to verify absolute Android files paths. JNI
writable/getFilesDir now return /data/data/moe.low.arc/files, which resolves to
ux0:vitaarc/persistent/data/files/. Java File exists/isDirectory uses the same
mapping. Host tests write and read binary files in separate processes and verify
the native destination plus the /data/user/0 alias. Native bridge preferences
keep their existing location; legacy game files at the data root are not moved.
Optional capture-frame diagnostics also export frame 1800 for later-stage review.
Run 028 reaches that frame and shows the complete Arcaea title illustration and
logo (build/vita3k/frame-1800.png). Real accesses now use files/st3 and files/dl
under the native directory, without the previous app0:/ux0: prefix. The APK DEX
also independently confirms Cocos2dxHelper.init stores Activity.getFilesDir().
getAbsolutePath() in sFileDirectory; excerpt: files-directory-dex.txt.

Run 029 adds an opt-in later-frame request: when capture-frame was present at
frame 120, create ux0:vitaarc/capture-next-frame to export frame-live.rgba. The
loop polls once per 120 frames only in this diagnostic mode and consumes the
request after a successful capture. Normal runs perform no recurring file polls.

Current host regression results after these path/pixel changes: ASan/UBSan
21/21 and TSan 20/20. Linked ARM main still passes 32 startup/failure cases.
Windows window capture twice times out (FrameArrived timed out, then window
capture timed out); clicking the Maximize accessibility control also reports
coordinate input geometry is unavailable. Accessibility-only window reads work.
After a fresh observation the Computer Use tool successfully sends the configured
Cross key (X), but a subsequent native GPU export still shows the title page.
This proves a host input attempt, not guest delivery or game response. No touch
input succeeded. Raising the accessible window did not restore screenshot capture.
Full gameplay, audible game audio, hardware inputs and full-game shutdown are
still incomplete. Use this section for the current boundary.

All emulator files, configuration, firmware and installed content are isolated
under `build/vita3k`; the desktop user's default Vita3K profile is not used.
These files and proprietary game/firmware content are ignored by Git.

## Environment and provenance

- Official [Vita3K continuous Windows build](https://github.com/Vita3K/Vita3K/releases/tag/continuous):
  version reported by the executable is `v0.2.1 4220-a32ee0ce`.
- Archive `build/vita3k/windows-latest.zip`, SHA-256
  `dbd18ec5c775aedf9ff2a1bd4b20bfee38f8c64cb7800a91b47d3b7bbfce6bd0`.
  This matches freshly fetched GitHub expanded-asset metadata saved locally as
  `build/vita3k/release-assets.html`; cached web metadata described an older file.
- Sony [official firmware download page](https://www.playstation.com/en-us/support/hardware/psvita/system-software/):
  3.74, 133834240 bytes, MD5 `f2c7b12fe85496ec88a0391b514d6e3b` matching
  the official download path. The page links HTTP; upgrading this legacy host
  to HTTPS gave a certificate-name mismatch, so the published HTTP URL was used.
  Firmware installation reached 100% in `build/vita3k/firmware-install.txt`.
- NVIDIA GeForce RTX 3060 Ti, driver 595.79.
- `runtime/portable/` was created before first launch. Vita3K uses
  `runtime/portable/fs/` for its emulated filesystem. Discord presence is off.
- Main firmware is installed; the separate font package and libshacccg are absent.
- The official [kubridge 0.3.1 hotfix](https://github.com/bythos14/kubridge/releases/tag/v0.3.1_hotfix)
  is now installed only in this emulator's `ur0/tai`. The release asset is
  11630 bytes, SHA-256
  `93361eafcdb17a5f40ad063ccdb4f1554ed831783559905f8f2bdea2535582d7`.
  Its private `ur0/tai/config.txt` contains `*KERNEL` and
  `ur0:tai/kubridge.skprx`; it was created only after verifying no config existed.

## Reproduction

Install/run the current VPK from the repository root in PowerShell:

```powershell
$runtime = (Resolve-Path 'build/vita3k/runtime').Path
$vpk = (Resolve-Path 'build/VitaArc.vpk').Path
Start-Process -FilePath "$runtime/Vita3K.exe" -WorkingDirectory $runtime `
  -ArgumentList ('--frontend sdl --console --log-level 1 --backend-renderer OpenGL "' + $vpk + '"') `
  -WindowStyle Hidden
```

Subsequent launches can use `-r VARC61616` instead of reinstalling the VPK.
For the latest recorded runs, the freshly built `VitaArc.self` was copied to
`runtime/portable/fs/ux0/app/VARC61616/eboot.bin` while no emulator process was
running. Runs 004-007 include the service cleanup and corrected vitaGL return
interpretation. Runs 008-010 additionally load official kubridge. Run 010 used
SELF SHA-256 `5d5a6914a1e62b6b0ee72b41a36156a4273fd95676b915576623672d0fb96a8c`
with SDKBOX initialization and libgcc repair. A copy remains as
`before-compiler-check.self` only for diagnostics. Current main startup adds
the compiler availability check verified in run 011. Audio probes use a separate
`audio-probe.self`; neither normal eboot nor VPK is replaced by a probe.

## Observed outcomes

1. Vulkan: the host renderer selected the RTX 3060 Ti, then reported a null-read
   access violation during renderer initialization. This precedes guest startup.
   Initial console capture is `build/vita3k/boot-001.txt` and the preserved log
   is `build/vita3k/boot-001-vita3k.log`.
2. OpenGL: Vita3K installed the VPK, loaded `eboot.bin` and firmware modules and
   started VitaArc. GXM initialization executes. The log also records failed
   reads for `ur0:/data/libshacccg.suprx` and
   `ur0:data/external/libshacccg.suprx`. VitaArc exits 1. Preserved initial log:
   `build/vita3k/boot-002-vita3k.log`.
3. Historical OpenGL run with the system-log mirror: the exact guest message is
   `[F] [init.c] VitaGL initialization failed`, followed by `Log stop.` and
   guest exit code 1. This was an incorrect application check, not evidence of
   vitaGL initialization failure: its return indicates resolution fallback.
   That preserved log is
   `build/vita3k/boot-003-vita3k.log`. After guest exit, Vita3K itself reports a
   DEP violation at address zero during shutdown.
4. Corrected startup with the official executable: runs 004-006 pass vitaGL
   initialization. Run 005 logs complete game SO read (7663640 bytes) and then
   a host DEP violation at zero before validation completes. Run 006 attempted
   `--cpu-opt false`, but its log still says optimization true: that CLI option
   is not supported and this is not evidence of a disabled-optimization run.
5. Host GDB diagnostics (`host-debug.txt` and `host-debug-002.txt`) catch an
   actual null indirect call on a host renderer thread, while a separate
   `VARC61616` guest thread is running. The call arguments are texture ID,
   level zero, RGBA (0x1908), UNSIGNED_BYTE (0x1401) and four clear bytes.
   This matches `sync_mask`'s `glClearTexImage` call in the exact emulator
   commit; the official frontend requests OpenGL 4.3, and that is the version
   reported by the driver. The crash's timing during ELF validation does not
   establish an ELF validation defect.
6. A separate local emulator copy requests OpenGL 4.4. Run 007 reports
   `GL_VERSION = 4.4.0 NVIDIA 595.79`, completes game ELF validation and reaches
   `kuKernelAllocMemBlock`. Vita3K reports missing NID 0x2EF7C290; the SDK's
   `libkubridge_stub.a` independently binds that NID to this function. The
   loader cannot map the first segment at 0x98000000 and the guest exits 1.
   The emulator exits without the previous DEP violation. No constructor,
   JNI initialization or game frame executes in this run. Preserved log:
   `build/vita3k/boot-007-vita3k.log`.
7. Installing the official kubridge plugin resolves the missing allocation
   export. Run 008 maps both game segments at 0x98000000, resolves 354 external
   relocations, executes constructors and reaches `JNI_OnLoad` returning
   JNI 1.4. The audio provider opens port 1 and preloads four WAV effects.
   SDKBOX then logs `getEnvAttach jni not initialized` and dereferences its
   null JavaVM. The faulting code is `JNIUtils::__getEnvAttach`, RVA 0x470b7c.
   The process was explicitly stopped after confirming repeated guest PC-zero
   faults; `boot-008-vita3k.log` preserves the run. Earlier apparent waits were
   buffered log output, not evidence of a stopped process.
8. The Cocos startup chain now calls the optional
   `Java_com_sdkbox_plugin_SDKBox_nativeInit` with the SDKBox class, activity and
   its class loader. APK DEX method 42082 declares
   `(Ljava/lang/Object;Ljava/lang/ClassLoader;)V`; the native routine obtains
   the JavaVM and stores owned global activity/loader references. Run 009
   logs `Sdkbox got VM` and `Sdkbox jni initialized`. It next encounters
   unsupported `SdkboxLog.setDefaultDebugLevel(I)V` and a libgcc helper call to
   Linux's unavailable 0xffff0fc0 entry (return PC 0x9863536c), ending with a
   host access violation. Preserved log: `boot-009-vita3k.log`.

The SDKBOX Java setter's DEX body is `sput p0, _defaultLevel:I; return-void`.
The bridge now performs that field store. The libgcc helper group now uses
ARMv7 compare/exchange and barrier replacements; the complete 1.6.1c code
fingerprint guards all literal changes. See `tests/check_arm_kuser.py`.

9. Run 010 includes the SDKBOX setter and kuser repair. It logs 43 compare-
   exchange and five barrier literals replaced, passes SDKBOX VM initialization,
   and reaches Cocos shader creation/attribute binding. It no longer hits the
   old 0xffff0fc0 call. SDKBOX reports further unsupported Java calls: tagged
   logging, NativeBridge event listeners, TrackingInfoAndroid queries, FileUtils
   checks and SDKBox plugin/application metadata. These remain compatibility
   work; Java exceptions are logged and cleared by the game at several points.
   Both first shaders report compile success through vitaGL even though compiler
   module loads fail, then Vita3K reports a host read violation at 0x24 after
   attribute binding. The subsequent GDB diagnosis identifies that fault below.
   No completed Cocos initialization or game frame is proven. The process
   exited; `boot-010-vita3k.log` preserves the run. No emulator/debugger remains
   running.

10. Host GDB (`shader-host-debug.txt`) stops on the guest application's host
    thread with a null program translated to R8=0, then a read at R8+0x24.
    The import dispatch retains NID 0xD5D5FCCD, independently identified in SDK
    `libSceGxm_stub.a` as `sceGxmProgramGetParameterCount`. The installed
    vitaGL and upstream source confirm postponed GLSL compilation: its compile
    status can be true before any GXM program exists. Compilation is deferred
    to linking. Missing compiler loads precede the null program fault.
11. Current startup verifies `shark_init` on the default and external module
    paths after vitaGL setup, before loading the Android SO. The public API
    returns zero when already initialized, so this does not load a second
    compiler after successful vitaGL setup. Run 011 logs the exact missing
    prerequisite and exits 1 normally, without loading the game or the prior
    0x24 exception. `boot-011-vita3k.log` preserves it. The linked-main test now
    passes 22 cases covering both resolution results, default/external/missing
    compiler paths and startup/frame failure shutdown.

[vitaGL's deferred compile/status behavior](https://github.com/Rinnegatamante/vitaGL/blob/master/source/custom_shaders.c)
explains why earlier compile-success logs were insufficient evidence. A
working compiler can still reveal other shader or game startup failures.

## Independent decoder and muted output-worker evidence

Runs 008-010 report missing imports 0x8612B243 and 0x6C5AB07F, then disabled BGM.
SDK stubs identify those as `ksceSblACMgrIsSystemProgram` and
`ksceSblACMgrIsNonGameProgram`, not MP3 decoder imports. The firmware sysmodule
path reaches these missing permission queries before codec initialization.
The production bridge already requests `SCE_SYSMODULE_AUDIOCODEC`.

The standalone SELF built by `tests/build_vita_audio_probe.py` compiles the
production MP3/parser/resampler, mixer, output worker and logging against SDK
libraries. It uses the local game's `audio/calibrate.mp3`; there are no decoder,
thread, audio or I/O mock endpoints and no graphics/JNI dependency.

- Run 012 tried Manual module policy with the archived pre-check game SELF;
  the host renderer reported `set_current fail` and the live process did not
  advance to audio. It was stopped after inspecting that failure. This run
  does not establish decoder behavior.
- Standalone runs 013 and 014 use normal and Manual module policies with the
  firmware bootimage present. Both fail the same permission-query imports
  before decoder initialization. Manual policy alone does not solve it.
- Run 015 temporarily omits the isolated `os0/kd/bootimage.skprx`, allowing the
  emulator's HLE kernel/sysmodule path. It decodes 48000 PCM frames, 46590 with
  a nonzero channel, and reports position 48000. Seek to 24000, one sample,
  reset to zero and cleanup all pass, with exit zero. The original bootimage
  was restored in `finally` and its SHA-256 verified.
- The current expanded probe also runs the production output worker on an
  actual emulated audio port, with both volumes zero. On the unchanged official
  Vita3K executable it advances to 64 ms, pauses at 64, seeks to 500, resumes
  to 542, then destroys the worker/decoder/port and exits zero. Current log:
  `build/vita-audio-probe/probe-vita3k.log`. This does not require the GL4.4 copy.

Reproduction:

```powershell
wsl -d Ubuntu -- python3 tests/build_vita_audio_probe.py
./tests/run_vita_audio_probe.ps1 -UseHleKernel
```

The runner uses only `build/vita3k/runtime`, preserves the normal configuration,
refuses to disturb a live emulator, and restores the temporarily moved bootimage
with a hash check. Original bootimage SHA-256:
`c2b8e36324544a08a6ffa6caaf79250ca02399400f4506c84479c68dd5ae21f6`.
No emulator/debugger remains running. These results establish real emulated
decoding and a muted output queue, not audible playback or Vita hardware
priming/latency. WAV preload into a port alone was not playback evidence.

[vitaGL requires an extracted/decrypted libshacccg.suprx](https://github.com/Rinnegatamante/vitaGL#prerequisites).
No such module was found in this project, the searched local project tree,
Downloads, or installed main firmware. No unrelated binary was substituted.
Its absence is an unmet runtime compiler prerequisite. It is not the reason
for vitaGL's false resolution-fallback result, which is expected at the
requested native resolution.

## Isolated OpenGL 4.4 diagnostic copy

The official `runtime/Vita3K.exe` remains unchanged. Its SHA-256 is
`3bf5e71b95c13312779b1a80ab537f0e9dc2588d508e08d4ff54dd216da0401f`.
The ignored `build/vita3k/patch-gl44.py` verifies a unique 36-byte instruction
profile containing major-version 4 and minor-version 3 SDL attribute calls,
then changes only the immediate byte at file offset 0xA8D4BD from 3 to 4 in
`runtime/Vita3K-gl44.exe`. The copy's SHA-256 is
`b8d02418722f362191ec82a8c7184ea447aed3fe0c59cc046044ea4cd744e817`.
This is an emulator diagnostic workaround, not a VitaArc runtime dependency or
a shipped emulator fix. Reproduction uses the same command above with
`Vita3K-gl44.exe` instead of `Vita3K.exe`.

Exact upstream sources used for the diagnosis:

- [SDL context request](https://github.com/Vita3K/Vita3K/blob/a32ee0ce/vita3k/sdl-frontend/src/session.cpp)
- [Mask texture clear](https://github.com/Vita3K/Vita3K/blob/a32ee0ce/vita3k/renderer/src/gl/sync_state.cpp)
- [vitaGL initialization return](https://github.com/Rinnegatamante/vitaGL/blob/master/source/vgl.c)

The vitaGL source interpretation was also checked against the installed static
library and linked ELF disassembly: the no-fallback path stores zero in the
return slot, sets the initialized flag and returns that slot.
`tests/check_arm_startup.py` now covers both return values and startup failure
paths through actual linked `main`, with subsystem endpoints intercepted.

## Offline compiler and real shader rendering probe

A targeted local SDK search found an existing `psp2cgc.exe` under
`F:/PSVITASDK/sdk/host_tools/bin`; it reports SDK 3.5.0 build 13276. No SDK binary
was copied into the repository, downloaded or distributed. The runtime module
remains absent. This host compiler successfully compiles the Cg equivalents of
the first dumped game shaders (vertex ec98300789fd9176, fragment 5e957238bf3f9ec3).

`tests/run_vita_shader_probe.ps1 -CompilerPath <local-psp2cgc.exe>` builds and runs
a separate graphics SELF with those outputs. The installed vitaGL's disassembly
confirms that glShaderBinary expects a matrix-parameter count/index list before
the raw GXP; this is also described by upstream custom_shaders.c. The probe
supplies the actual CC_MVPMatrix parameter index, creates Cg shader types to
avoid postponed GLSL compilation, then exercises real linking, attribute names,
matrix upload, texture sampling and vertex color modulation.

The latest probe log is `build/vita-shader-probe/probe-vita3k.log`. It reports
both binary statuses true, linked=1, live matrix/sampler locations, center
100,100,50,255, corner 0,0,255,255, GL error zero and guest exit zero. This is
actual emulator rendering/readback evidence; it is not full game rendering.
The first version of this probe preceded binary-cache integration. The current
normal game bridge now supports an explicitly enabled offline pack (see below).
The strengthened probe translates the square with a nonsymmetric matrix and
checks that screen center remains blue. This found a real conversion error that
the earlier diagonal scale could not detect: with this compiler/vitaGL storage
layout, Cg must use mul(vector, matrix) for GL column-major uploads. After fixing
all translated uniform-matrix expressions, center/color, outside/background,
error zero and exit zero pass.

Initial probe runs with default asynchronous pipelines/surface synchronization
read zeros. An attempted config argument placed a filename after --load-config,
which is a flag; the emulator logged an unsupported content type. The corrected
runner uses --load-config --config-location shader-probe-config.yml --keep-config
and a separate config with disable-surface-sync=false and
async-pipeline-compilation=false. Earlier Manual-module runs 012/014 have been
rechecked: their preserved logs explicitly report modules mode Manual, so those
earlier results remain valid. The successful graphics runner verifies that the
default config hash is unchanged. Firmware is not moved for the graphics probe.

## Production offline cache integration

`tools/compile_shaders.py` compiles only explicit manifest entries using the
existing local compiler. It serializes matrix parameter indices plus raw GXP,
requires named matrices to survive as uniforms, and can verify original-source
hashes with --originals. Output stays under ignored build/offline-shaders; no
compiler or SDK binary is included. The current 39 entries cover the 36 embedded
shader bodies with observed one-light-per-type definitions plus three normal-map
variants. Hashes for not-yet-dumped entries are assembled from the exact observed
Cocos preamble plus local game strings; runtime exact-hash matching remains the
coverage gate. This does not establish coverage of every later game variant.

If both runtime compiler paths fail, startup accepts a marker containing exactly
VitaArc offline shaders 1 plus newline in the user or package shader directory.
The shader bridge then creates Cg handles, preserves/hash-matches original GLSL,
loads .vgxp binaries (user first, app0 fallback), validates the envelope/reflection
and calls native glShaderBinary. It never tries to compile GLSL as Cg. Missing or
rejected binaries log their hash and exit 1 before null-program attachment.
Offline program links now check GL_LINK_STATUS and fail explicitly as well.
Successful native links are logged. Deletion clears tracking slots.

The optional CMake VITAARC_SHADER_BINARY_DIR setting packages generated binaries
and marker, with file dependencies so rebuilt binaries update the VPK. The local
build is now configured with build/offline-shaders; CI without that setting
continues to require a runtime compiler or separately installed user cache.

Preserved actual game runs:

- 018: first textured/color pair loads and game continues to the projection-only
  vertex f4a4c2a2d87dddc7, then normal missing-cache exit 1.
- 019: 25-entry pack advances through program 19 to missing skinned vertex
  0f0b0e532b67bcff; exit 1.
- 020: skinned vertex loads, then program 20 advances to missing lighting vertex
  c8747ea74aab7c83; exit 1.
- 021: one directional/point/spot light variants progress through program 23 to
  normal-map vertex 5769a50a0e497776; exit 1.
- 022: normal-map variants progress to skybox vertex dc8dd034f0ef6ebd; exit 1.
- 023: packaged formal eboot loads all 39 unique binaries and logs 30 successful
  GL_LINK_STATUS checks. Main-menu textures, fonts and StartScreen.csb load.
  The first guest fault is SDK pthread_equal(0,0) at 0x81021e34, called from the
  guest adapter at 0x8101b984; both input IDs are zero. Guest equality now uses
  Bionic value semantics. Native SDK pthread_equal dereferences its IDs;
  [Bionic compares their values](https://android.googlesource.com/platform/bionic/+/5d0ad38c72951ef180742efb27933ecd6e9d9593/libc/bionic/pthread_equal.cpp).
- 024: corrected equality advances through start_wreath/start_icon loading.
  AppActivity.startSceneLoadFinished()V is unsupported, so the main JNI failure
  path is correctly selected. Cleanup then throws C++ and reaches the placeholder
  __gnu_Unwind_Find_exidx; native _kill_r aborts. This is not successful teardown.

The JNI expansion continuation was checked against the local APK's DEX using
the existing local Android dexdump. Preserved excerpts are under
build/offline-shaders/start-scene-*-dex.txt and expansion-native-dex.txt.
startSceneLoadFinished posts AppActivity$1 to the UI thread; it checks Android
storage permissions and calls performExpansionCheck. That creates/connects an
Android downloader stub, sets expansionCheckPerformed and invokes native
notifyExpansionCheckComplete once. The 1.6.1c native callback simply sets its
application completion byte. Vita startup already delivers it before nativeInit
because assets are packaged. The new builtin acknowledges that completion and
sets the corresponding Java flag; it does not launch an Android downloader.
The callback remains owned by the active Cocos module, delivery is once per
startup, stop clears it, and a missing callback produces an explicit exception.
Host mock nativeInit calls the acknowledgement twice and checks no duplicate
native delivery. Missing completion prevents ready state. Actual continuation
will determine whether this closes the next real startup gap.

These historical runs also report GL_INVALID_ENUM in CCTexture2D.cpp line 656. This
matches the installed glPixelStorei disassembly: it accepts only enum 0x0CF2
(UNPACK_ROW_LENGTH), while Cocos sets UNPACK_ALIGNMENT. Upstream textures.c
agrees. The alignment/repacking and EHABI repairs and their native probe results
are described in Latest verified continuation above.

These use the separate OpenGL 4.4 diagnostic emulator and explicit probe config.
Runs 018-022 use offline-game.self and user-cache binaries. The new VPK now
contains 39 binaries and the marker, with a current packaged eboot installed in
the private profile for a formal-entry continuation. No full game frame is yet
established by the default-program initialization logs.

Current regressions pass: host ASan/UBSan 21/21, TSan 20/20, linked ARM main 32
cases, 104 GL/EGL canonical lookups and five float adapters, and linked ARM shader
cache assembly/dispatch/600 lifetimes/missing-invalid-link rejection with zero
remaining allocations and handles. All 39 generated binaries pass the production
envelope validator on host. The graphics probe passes scale/translation, texture,
color and actual GPU readback with error zero and guest exit zero.

## Remaining runtime evidence

Kubridge allocation is now established with the official plugin under the
isolated emulator. The missing runtime compiler has an explicit offline-cache
alternative; continue actual game startup, cover any newly encountered exact
shader variants. Scene completion, pixel unpack and exidx lookup are now repaired;
startup and title frames are visually established. Continue into menu navigation,
gameplay, actual save reload and full-game teardown. Unsupported SDKBOX Java
services remain logged. The libgcc kuser fault is repaired, and a standalone HLE-kernel audio
probe now passes actual decoding and muted worker/seek behavior. The firmware
kernel's module permission-query path still fails. Audible game sound, actual
input response and complete gameplay remain unverified. Independently, the ARM constructor/JNI
diagnostic now reaches JNI 1.4 and has reduced retained allocations from 96/3086
bytes to zero retained allocations after the supported bootstrap teardown. ObjectFactory,
AppDelegate and the bridge audio provider now clean up before static finalizers.
Analytics/achievement and platform objects now release after static finalizers
under the verified 1.6.1c layout. Platform members use the actual game COW-string
destructor, with nonempty/shared ownership tests. Purchase strings, vectors and
captured callbacks now use verified game destructors, with matching listener
detachment. The strict heap assertion passes for supported bootstrap/ownership
cases and still rejects unknown profiles/types. This remains instruction-level
evidence; actual emulator startup now passes vitaGL, ELF loading, constructors,
JNI_OnLoad, SDKBOX's VM initialization and verified startup/title frames. Successful
shutdown after full native renderer initialization still needs runtime evidence.
