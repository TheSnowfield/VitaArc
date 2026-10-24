# Local APK comparison, 2026-10-24

The 2026-10-25 verification of the current 256 MiB build is recorded in
[current compatibility verification](compat/current.md). The SELF hashes and
package descriptions below belong to the earlier comparison batch.

## Final local verification (supersedes historical status below)

Current production SELF is
`878234348f410c8ebfdb62036b775a99f1f9cfa22662d12938a23dbda760c5ff`.
Four matching VPKs with correct APK metadata, common resources, 39 offline
shaders and all six codec/font dependency notices are in `build/apk-compat-r3`.
`packages.json` records hashes and complete CRC/resource validation. The earlier
1.6 VPK is preserved as `build/VitaArc-1.6.1c-before-compatibility.vpk`.

New implementation: real FreeType bitmap text, wrapping/alignment/shrinking and
stroked premultiplied RGBA returned through the native bitmap callback; typed
local analytics event dispatch for audited APK methods; audited AppDelegate
fingerprints release our newer audio providers on unload; IEEE float32 WAV SFX
decoding. All actual APK WAV headers were inspected: 1.6/1.9/2.0 use PCM16/24;
3.0 adds 13 float32 files. Unknown JNI signatures still report unsupported.
Analytics records events locally and performs no network action.

ASan/UBSan 22/22 and TSan 21/21 pass after these changes. ARM real AudioManager
callers pass for all four versions; startup/shutdown controller 40 cases and
fortified formatting pass. Final ARM import/relocation/unload test also passes
all four real libraries (48281/73200/76279/87022 relocations).

3.0 native touch evidence:

- r2/run-3.0.0c-20261024-133104: real bitmap/calibration/settings, four calibration
  taps, synthetic offset adjusted to zero, first main-menu onboarding completed.
- r2/run-3.0.0c-20261024-135129: tutorial and song-selection onboarding, Arcaea
  pack, Fairytale Past 1, native synthetic ground touches produce score 14880;
  clean production unload and exit zero. This predates the float WAV fix.
- r2/run-3.0.0c-20261024-140209: current objects, Fairytale Past 1 from selection
  through full chart, ground touch pairs, pause/frozen frame/resume, result and
  return. Score 163692, PURE 4/FAR 3/LOST 329; actual SQLite row matches the UI,
  integrity_check=ok. No unsupported JNI, SFX load failure or native exception;
  audio provider destroyed and guest exit zero. `result-inspection.json` retained.
- r3/run-3.0.0c-20261024-140830: fresh process cloned prior test's persistent
  state, title/main/selection display retained score 163692, clean exit zero.
- r3/run-1.9.0c-20261024-140928: fresh first launch/calibration (offset reset
  from 216 to zero), tutorial, simultaneous sky holds reaching combo 23 and
  score 2774614, pause/resume and selection onboarding. Clean exit zero.
- r3/run-1.9.0c-20261024-141726: normal Fairytale Past 1, paired ground taps,
  full result 193454 (PURE 5/FAR 3/LOST 328), return/save, SQLite integrity ok,
  no fatal JNI failure/SFX load/native fault; provider cleanup and exit zero.
- r3/run-1.9.0c-20261024-142241: new process retains score 193454/rank D.
- r3/run-2.0.0c-20261024-142349: testing a cloned 1.9 setup-complete profile
  with no chart scores, Fairytale Past 1 complete including input/pause/resume,
  result 163691 (PURE 4/FAR 3/LOST 329), return/save, SQLite integrity ok,
  no fatal JNI failure/SFX load/native fault; provider cleanup and exit zero.
- r3/run-2.0.0c-20261024-142932: new process retains score 163691/rank D.
- r3/run-1.6.1c-20261024-143038: final regression in progress, cloned old 1.6
  data loads the original score 29761, completes a new chart with input and
  pause/resume, result 208336 (PURE 5/FAR 4/LOST 327), return/save, SQLite
  integrity ok, provider cleanup and exit zero.
- r3/run-1.6.1c-20261024-143554: new process retains score 208336 and list drag
  scrolls from Fairytale to Dandelion/One Last Drive/Reinvent. Exit zero.

Every completed runner verifies original firmware/config/app/user-file hash
restoration. The original stock Qt run 048 is stopped. These are native game
framebuffer and synthetic Vita-panel tests, not physical touch or Windows
presentation proof. New-version service heap teardown beyond the audio provider
is not audited as deeply as the original 1.6 path; do not claim heap-leak freedom.

All four local compatibility gates are now met: real native play, synthetic
touch/paired input, pause/resume, full results, SQLite persistence/restart and
actual packaged eboot smoke tests. The r3 main-eboot runs are
143711 (1.6), 143801 (1.9), 143851 (2.0), 143941 (3.0); each reaches Cocos/frame
120 and captures frame 1800, remains running through 45 seconds and is then
intentionally stopped before original-file restoration. These bounded main
runs do not claim clean exits; the separate touch/restart runs do exit zero.
`build/apk-compat-r3/play-verification.json` and `smoke-verification.json`
validate the exact SELF, database scores/integrity, cleanup markers, restored
originals and known optional Java warnings. Named screenshots are in `evidence/`.
glReadPixels frame-1800 exports have bottom-left origin and are decoded with
vertical row reversal; native touch sceDisplay captures are already top-down.

Boundary: local offline play is verified in Vita3K SDL/OpenGL/HLE. Online,
login and purchase support is not established. 1.6/1.9/2.0 retain optional
SDKBOX Java-service warnings; 2.0 also reports unsupported HTTP-connection
creation when entering the main menu, then continues normally. These warnings
are retained in the JSON, not hidden by success stubs. The user subsequently
confirms visible local game output (not black/white); the exact build/version
was not specified, and physical touch is not established. All variants use VARC61616 and the same persistent
paths: install one at a time and back up saves before version changes.

The final normal 1.6 installation is restored separately for later physical
inspection as SDL/OpenGL session 050 (PID 61864 at launch); original installation and persistent data
backups are kept under `build/vita3k/install-before-049` and
`persistent-before-049`. See `boot-050-evidence.json`: Cocos/frame120 and two
BGM channels pass, MP3 remains enabled, no native fault. Read the app's native
boot.log for those markers; the host vita3k.log buffers recent lines. Firmware
and default config are restored and verified. Stock Qt attempt 049 terminated
with a host null-read access violation while loading SCE_SYSMODULE_AUDIOCODEC;
the validated SDL environment is retained. Do not claim stock Qt success.

## Earlier compatibility implementation

The following section is historical evidence from before the continuation above.

The active user goal now requires fixing 1.9/2.0/3.0 while keeping 1.6 usable
and retaining a symbol superset. This is in progress, not complete. The original
comparison below describes the pre-fix build only.

Implemented since that comparison:

- 37 required imports added without removing old entries: base-PCS math,
  Bionic classification constants, ARM atexit argument reordering, memmove8,
  wide output and string functions. The ARM version-import test executes SDK
  math and verifies all four real ELF relocation sets (48281/73200/76279/87022)
  at the pre-audio-selection revision, with full loader cleanup.
- 1.9.0c's separate 48-literal libgcc kuser profile is audited/fingerprinted
  (anchor 3de61c19ff6cb59d, group 0xf68/767f46830cd20206). The original 1.6
  profile remains. Native fetch-add, ABI flags, profile rejection and partial
  write cleanup pass. 2.0/3.0 contain no such literals.
- Fortified vsprintf accepts the compiler's unknown-size sentinel using ordinary
  vsprintf, and preserves negative formatting results. SDK ARM formatting tests
  cover known/exact/unknown bounds, canaries and real undersize rejection.
- Packaging reads versionName from binary AndroidManifest.xml and writes
  vitaarc-apk-version.txt. JNI reads it, validates it and passes the actual
  version; legacy VPKs without the file retain 1.6.1c. Host startup tests check
  all four values. The main base VPK remains unchanged.
- The four actual libfmodProvider.so vtables were decoded (evidence:
  build/apk-versions/audio-vtables.json). 1.6 has 18 slots, 1.9/2.0 have 19,
  3.0 has 21. Three immutable bridge tables are retained. Import resolution
  selects from the audited ten-byte AudioManager::update fingerprint, rejecting
  unknown layouts. Using the old table on newer versions caused incorrect
  suspend/resume and out-of-bounds calls into SDK fread; this is fixed.
- SFX supports loop counts, explicit volume, async entry points, and 3.0's
  filename-based stop/get/set of the most recently played channel. Host PCM
  tests cover finite/infinite loops, mixing, file-volume control, stop isolation,
  unload and old global volume. Real ARM AudioManager update/get-position and
  vtable SFX float/stack/return conventions pass for all four versions.

Current production SELF SHA256:
377bd72c358f38118765c38ee7cbb1a67ec0a9175eed246c818954b2c3916bd4.
Latest ASan/UBSan suite 21/21 and TSan suite 20/20 pass, including the new
version/atexit/SFX tests. New tests: check_arm_version_imports.py,
check_arm_audio_versions.py, check_arm_fortify_format.py; check_arm_kuser.py now
supports both profiles. Native compatibility work uses build/apk-compat-r1/r2.
The r2 VPKs contain the earlier SELF 356d19ac...; newer SELF overrides and
standalone fixtures are explicitly recorded in each run's evidence.json.
They are development artifacts, not final verified compatibility deliverables.

Native evidence and remaining work:

- r2/run-1.9.0c-20261024-125554: version 1.9.0c, actual title captured at 1800
  before the later audio table correction. New table has ARM caller proof;
  repeat native play verification with the latest SELF.
- r2/run-3.0.0c-20261024-125701: version 3.0.0c, actual title at 1800 after the
  formatting fix; still had first-frame SDK faults from the old audio table.
- r2/run-3.0.0c-20261024-130856: latest audio-corrected fixture
  2dc1156cdf8572352be8a98d846c6817503c8f07ef94b2d7dc407a7a428eb3cd
  reaches title and Welcome through synthetic panel input without native memory
  faults. Tapping Setup stops rendering on the unsupported Java method:
  org/cocos2dx/lib/Cocos2dxBitmap.createTextBitmapShadowStroke
  ([BLjava/lang/String;IIIIIIIIZFFFFZIIIIFZI)Z. Guest cleanly exits 1;
  original app/data restored. This is the next concrete implementation task.
- r2/run-2.0.0c-20261024-131441: actual latest 377bd72c... production eboot,
  correct version, title/display frame 1800 retained, no exception_handler;
  original app/data restored after 50-second observation.
- r2/run-1.6.1c-20261024-131556: current production objects linked into lifecycle
  fixture 88d085e9b9c5f4c447feb7f7f40ed18e8f7c6075a7a54c9416ef7e19004672e5;
  frames 600/1200, pause/resume, production unload and guest exit zero pass.
  Native evidence has no exception_handler; original app/data restored.
- r2/run-1.9.0c-20261024-131821: actual latest 377bd72c... production eboot,
  correct version and frame 1800 retained, no exception_handler; originals
  restored after observation. This supersedes the earlier title run's audio ABI.
- Normal stock Qt/OpenGL run 048 is left live as PID 61632 (started 13:20:04),
  SELF 377bd72c..., Cocos/frame120 and two BGM channels pass; no native fault,
  MP3 available, private firmware/default config restored. Evidence is
  build/vita3k/boot-048-evidence.json. The original 1.6 VPK remains unchanged.

Bitmap continuation pointers: the native game exports
Java_org_cocos2dx_lib_Cocos2dxBitmap_nativeInitBitmapDC (3.0 VA 0x597419).
Implement a real text rasterization service and return its pixels through that
callback; do not make the method an empty success stub or skip setup. The Vita
SDK already contains freetype2 headers/libfreetype.a, but Ubuntu host freetype2
development headers/pkg-config are not installed yet. No font bridge/dependency
change has been made. Upstream Cocos2dxBitmap.java v3 documents layout/alignment,
RGBA callback, wrapping/shrink/stroke; the APK signature predates its added
lineSpacing float. Inspect APK DEX for the exact older behavior. Local helper
build/offline-shaders/dump_dex_method.py currently hardcodes the 1.6 APK and can
be adapted for inspection. Font assets (Fonts/Kazesawa, Exo, CJK etc.) exist in
all four APKs. Physical display/hardware touch remains external evidence, but
does not prevent implementing this software compatibility goal.

Continue: fix bitmap/text setup, then menu/selection/chart/touch/pause/result/
score-restart tests for each supported version; revisit new-version service
ownership/clean unload, rerun the native 1.6 regression and publish four final
VPKs with the same final SELF and matching metadata/resources/CRCs. Do not mark
the compatibility goal complete from title frames alone.

References: https://github.com/ARM-software/abi-aa/blob/main/rtabi32/rtabi32.rst
and https://android.googlesource.com/platform/bionic/+/b8320b8/libm/include/math.h
and https://raw.githubusercontent.com/cocos2d/cocos2d-x/v3/cocos/platform/android/java/src/org/cocos2dx/lib/Cocos2dxBitmap.java

## Initial batch before fixes

The four local APKs have manifest versionName values matching their filenames
(versionCodes 175, 248, 285 and 834651). All contain the required ARM32 library;
3.0.0c also contains ARM64. Each APK is packaged separately with the same current
SELF SHA256 2eb9ac13f9be250ad6f1de45a408d7939bb332cd41b0cd3b2afb78072261556c
and the 39 existing offline shaders. Main build/VitaArc.vpk is preserved.

| APK | VPK validation | Actual packaged eboot result |
| --- | --- | --- |
| 1.6.1c | Pass | Cocos initialized, frames 120/1800 captured, two BGM channels loaded; title frame visually inspected |
| 1.9.0c | Pass | Required ELF relocation resolution refused: 37 missing imports; guest exits 1 |
| 2.0.0c | Pass | Same 37 missing imports; guest exits 1 |
| 3.0.0c | Pass | Missing __aeabi_memmove8; guest exits 1 |

The 37 imports include __aeabi_atexit, __aeabi_memmove8, strncasecmp,
wcscmp/fputwc and additional math functions/classifiers. Complete lists and
package/library hashes are retained in build/apk-versions/compatibility.json.
Do not bypass the loader or substitute success-returning stubs to label a
version compatible. Supporting a new version requires its own ABI/JNI/audio,
shader and lifecycle validation after the first loading barrier is resolved.
The current JNI setAppVersion string is 1.6.1c; none of the three failing
versions reaches that call. These runs do not establish later-stage support.

Evidence directories:

- build/apk-versions/run-1.9.0c-20261024-123724
- build/apk-versions/run-2.0.0c-20261024-123738
- build/apk-versions/run-3.0.0c-20261024-123754
- build/apk-versions/run-1.6.1c-20261024-123807

Tests run the actual packaged eboot, not a mock, using the existing diagnostic
Vita3K-gl44.exe SDL/OpenGL setup and HLE kernel. Each receives a fresh separate
user directory. The 1.6.1c run is intentionally stopped after bounded observation;
it is not recorded as a clean application exit. All four runs verify restoration
of firmware/config, original eboot/library hashes and the complete original user
file snapshot. The original installation is restored after each run.

VPK variants share title ID VARC61616 and persistent paths; install one at a time.
The baseline 1.6.1c gameplay/score persistence evidence remains in
vita3k-runtime.md. Physical screen presentation and actual hardware touch still
need the user's later inspection.
