# Known incomplete behavior and directly observed defects

## Current continuation status, 2026-10-24

Actual PSV hardware verification now exposes an unresolved 2.0 immediate startup
data abort after enabling kubridge. PC 0x9870c278 is Node::addChild dereferencing
a null child at +0x140; StartScene's startup/start_wreath.png Sprite::create
returned null. The packaged PNG is valid and identical to the APK. The actual
hardware boot.log is still needed to distinguish loading/decoding/texture or
memory/compiler failure. See psv-hardware-crashes.md. Emulator-only success
below does not establish PSV hardware startup compatibility.
After installing shacccg, a new screenshot maps to the original `_free_r+0x5e`
with a corrupted free-list write. Diagnostic 2.0 r4 now prefers the existing
offline shader pack and logs startup heap snapshots; ARM tests and a 45-second
Vita3K run pass, but the actual PSV result/corruption origin are still unknown.

Multi-version offline compatibility is implemented and verified for
1.6.1c/1.9.0c/2.0.0c/3.0.0c: native play, synthetic panel input, pause/resume,
results, score persistence/restart and actual packaged eboot checks pass.
Real FreeType bitmap text, newer AudioProvider layouts, imports/kuser/format
fixes and float WAV support are implemented; the compatibility goal is complete.
Current details/hashes/online-service limits are in apk-compatibility.md.

The user now confirms that local Vita3K has visible game output, neither black
nor white. Missing connected display / remote presentation is the supported
explanation for the earlier headless host output, not a proven causal test.
Do not infer all-version physical display testing or physical touchscreen
testing from that confirmation. Intel core-profile texture2D startup rejection
has a separate four-call shader compatibility fix and revised portable package;
see vita3k-runtime.md. The original host's monitor state was not rechecked.

The sections below this status block are a historical snapshot and contain
superseded issues. Consult current source and `tests/README.md` before treating
an old entry as an outstanding defect. In particular, Cocos startup/render/input,
preferences, shader cache and several GL wrappers have already changed.

The active seven-item goal remains incomplete:

External verification gate: the same missing-display/physical-input condition
persists across the gameplay turn and the next two goal continuations. Fresh
PnP inspection returns no connected Monitor devices. Stock run 046 previously
reported about 60 FPS, but screenshot capture fails
with FrameArrived timeout; fresh selection/activation and retry also time out.
All independently actionable named fixes have been implemented and tested; final
physical display and hardware-touch evidence is still missing. The goal is
blocked pending a connected display or the user's physical verification, not
complete. Preserve the live normal game and current VPK for that verification.

The later requested APK comparison is complete: all four VPKs package/verify;
1.6.1c reaches a captured title frame, 1.9.0c/2.0.0c each fail on 37 required
imports, and 3.0.0c on __aeabi_memmove8. None of the other versions reaches JNI.
See apk-compatibility.md. The original installation/data and main VPK are
preserved. Normal stock run 047 (PID 46816) now replaces run 046 and is left live
with Cocos/frame 120 and two BGM channels. Physical display/input remains open.

Latest completion audit: the storage-fixed build passes a further 6600-frame
native soak with ten lifecycle cycles, stable JNI counts and clean unload/exit.
An additional nanosleep remainder bug is fixed: failed waits subtract total
monotonic elapsed time, including earlier-chunk oversleep, with bounded arithmetic.
ASan/UBSan, TSan and eight actual ARM guest-export cases cover the correction.
No connected monitor is detected. Physical display/touch and device scheduling
are still unverified; synthetic panel reports are not hardware input evidence.

Latest touch/play continuation supersedes the old gameplay/storage limitations
below. A separate native SELF injects synthetic sceTouchPeek panel reports into
production input polling and JNI, using real game code and GXM checkpoints.
Title/menu/setup/pack/chart selection, Fairytale Past 1, paired touch judgments,
pause/resume, list dragging, result and return all work. It exposes and fixes
logCrashlytics dispatch, Bionic ctype data-symbol indirection/short tables,
stdio /dev/urandom support, and SQLite IOERR_LOCK. Startup now chooses SQLite's
standard unix-dotfile locking VFS. A native write/reopen/read and conflicting
connection test passes; host integrity_check is ok. Actual score 29761/rank D
survives a fresh game process. Test data is cloned and the original restored.
See vita3k-runtime.md for exact run directories and limits. Physical touch and
host presentation remain unverified; this is not Windows mouse automation.
The dotfile VFS may leave a stale lock directory after an abnormal termination;
there is no automatic deletion or mixed-VFS access support.

Earlier display investigation: Ogg/Vorbis title BGM now decodes correctly; the user
confirms actual sound in run 033. The official executable with Qt/OpenGL 4.6 and
normal firmware reaches the title, but the user reports a white host window while
internal GPU readback contains the title artwork. Final host presentation and
physical touch remain the runtime gate. See the latest
section of vita3k-runtime.md; older MP3-only/full-audio statements below are
historical. The private font package is now installed and bootimage is unchanged.
Run 034 independently reads valid image pixels from the actual sceDisplay surface,
but the user corrected an accidental option selection and confirms the host
window is still pure white. Host presentation has not passed verification.
Run 038's SDL window is black according to the user. On the user's request,
Windows hardware inspection now finds zero connected/present monitor devices;
the cached monitor is not present and the legacy default monitor is offline.
The GPU still reports a display mode, and window capture times out. An offline
display or remote capture limitation is now a plausible explanation; an emulator
presentation defect is not established. Run 040 restores the normal host shader
and official Qt executable while awaiting a connected display for verification.
The user initially defers visual verification, then authorizes actual touch/play
testing. Fresh interactive run 043 still cannot be captured: both first capture
and retry after raising the window time out. Accessibility exposes only window
chrome; F12 produces no located export. No connected monitor is detected.
These host-window checks do not verify physical touch or display. Subsequent
native game-side input/play and score persistence pass as described above.
The real-game lifecycle probe subsequently exposes an unsupported
Cocos2dxRenderer.setAnimationInterval(F)V call during resume. It is now handled
with actual main-loop pacing; both HLE and normal-kernel native probes resume,
render 1200 frames, unload the game through production cleanup and exit zero.
The additional HLE soak runs 6600 frames with ten pause/resume cycles, stable
sampled JNI counts of 53 objects/6 references from frame 1800 onward, production
unload and guest exit zero. This remains title-scene coverage, not gameplay.
Native UserDefault preferences also pass writer/reader process roundtrip and key
deletion through the actual game/JNI/Vita I/O code; the private preference file
is restored by SHA256. Separate native gameplay subsequently establishes st3
score persistence and reload, with physical device verification still pending.

1. JNI references/frames/reclamation: implemented with separate handles,
   iterative heap tracing, weak clearing, buffer pins and automatic callback
   frames. Host tests prove stable counts over three 3000-iteration workloads.
2. Per-thread VM state and real monitors: implemented; attached worker isolation,
   recursive mutual exclusion, detach/exit cleanup and VM shutdown are covered.
   ASan/UBSan passes 21 CTest targets; TSan passes 20, with the blocking-sleep
   cancellation variant kept under ASan because of a reproduced libtsan
   nanosleep cancellation-interceptor limitation (see tests/README.md).
   The Cocos lifecycle/render owner-thread contract remains in place.
3. libc placeholders: POSIX time/wait and 64-bit seek/truncate are implemented.
   `libc/impl/file.c` now translates Android ARM open flags, passes creation mode,
   packs the 104-byte Bionic stat layout without truncating file size, and shares
   bounded Android data-path mapping across file operations. Regular writes
   honor append and synchronous-write errors. RNG uses allocated/pinned SDK
   descriptors, supports chunked reads/partial failure/dup, and leaves stdout
   untouched. Stdio fopen("/dev/urandom") additionally uses an RNG cookie stream;
   descriptor-based RNG fdopen remains unsupported. Legacy Bionic ctype data
   imports now expose pointer variables with byte/16-bit tables, verified by
   actual game TinyXML parsing. CLOEXEC is tracked per descriptor separately from shared status
   flags; ordinary dup clears it and F_DUPFD_CLOEXEC sets it. Host and actual
   linked ARM file fixtures pass, with Vita I/O endpoints mocked.
   Directory imports now use an opaque bridge and the 280-byte Bionic
   dirent/dirent64 layout rather than Vita's SceIoStat-prefixed layout. Data
   aliases, fd ownership, names/types/inodes, EOF/error and reentrant reads,
   rewind/tell/seek and failure rollback are covered on host and linked ARM.
   Vita seek prepares a separate native cursor before replacing the original;
   failed open/read/old-close leaves the old cursor and cookie intact. Directory
   inode IDs remain path hashes. Cookies are ordinal replay positions requiring
   stable enumeration; mutation and multiple streams over dup'ed descriptions
   are not established. scandir/getdents are not exported. Host fd zero is
   tested; this VitaSDK's allocator starts at fd 3 and __vita_fd_grab rejects
   fd zero, so target support for such a transferred descriptor remains absent.
   Guest errno adapters now bind 146 file/time/map/directory/stdio/encoding/
   network/pthread symbols. Internal bridge calls retain native error values;
   exported calls clear native errno to a sentinel and translate fresh failures,
   preserving the caller value if no error was reported. Direct-return APIs
   (pthread and readdir_r) translate their result and preserve caller errno.
   strerror/strerror_r/perror consume Android values. An independent SDK/Linux
   header oracle verifies all 87 supported named mappings in linked ARM code,
   plus ENOTSUP and unknown-native fallback to EIO. Resolved entry tests cover
   repeated errors, arbitrary saved errno, direct codes, variadic mode/format
   arguments and a 64-bit return. Host threads keep independent errno values.
   This is not global namespace isolation: __errno still uses the native CRT
   thread slot, and effects from other native callbacks/exports need audit.
   Network structure/EAI ABIs and the native FILE/__sF ABI are not repaired by
   changing error numbers. poll now monitors readiness through a dedicated
   native epoll adapter, with Android masks, fd pins, duplicate-socket merging,
   per-entry counts and monotonic microsecond deadlines. Invalid fds report
   POLLNVAL; files/directories and RNG do not require networking initialization.
   Long deadlines are chunked; zero/finite/infinite waits, interruptions and
   create/control/wait failures are tested. Host tests use actual sockets/pipes;
   linked ARM tests execute actual SDK pins, including a modeled concurrent fd
   close that releases the socket only after epoll destruction. Native pipe/TTY
   waiting and socket priority/band/RDHUP requests explicitly report ENOTSUP.
   The bridge limits poll arrays to the SDK's 256-fd capacity. Kernel cleanup
   failure is reported, but the native epoll object's state is then unknown.
   Physical network and signal/scheduling behavior remain unverified.
   Guest pthread_once no longer forwards a 4-byte Bionic control to this SDK's
   16-byte object. The bridge uses acquire/release state publication and a
   temporary registry while initialization is active, retaining no per-control
   allocation after completion. Host tests cover contention, nested controls,
   OOM/reuse, waiter cancellation, initializer exit/cancellation and errno.
   Independent ARM/Thumb callers verify exact SDK/guest sizes, canaries,
   nested calls, retry and actual SDK cleanup-stack restoration. Native mutex,
   condvar, TLS and allocator endpoints remain modeled in ARM validation.
   C++ initializer exception unwinding, fork/async-cancel and actual target
   scheduling are unproven. `sync.c` now separates 4-byte Bionic mutex/condition
   controls and attributes from native objects. Static normal/recursive/errorcheck
   types, explicit type mapping, serialized lazy publication, call/hold pins,
   native failure preservation and cancellation cleanup pass host ASan/TSan.
   Independent ARM/Thumb callers execute resolved production adapters and real
   SDK mutex attributes/cleanup stacks, with native scheduling endpoints modeled.
   The installed SDK ignores its condition clock attribute. Vita monotonic waits
   therefore use bounded realtime slices rebased against process time; simulated
   realtime changes between slices preserve the tested deadline. A sampling race
   before the native realtime-to-relative conversion remains, along with native
   millisecond/range limitations. Real Vita scheduling, robust/shared objects,
   fork and async cancellation remain unproven or unsupported.
   Thread creation now converts the 24-byte Bionic attribute instead of treating
   it as a native pointer. Sixteen attribute exports preserve exact 32-bit layouts,
   translate detach/scope values and retain invalid inputs. Native init/setter/
   create failures destroy temporary attributes without publishing a failed ID.
   NULL attributes on Vita select a 1 MiB stack instead of the SDK's 32 KiB,
   increasing stack reservation; no guard is enforced. Explicit guards, custom
   stacks and realtime scheduling report ENOTSUP. In particular, default Bionic
   attributes with a 4 KiB guard remain unsupported unless that guard is explicitly
   disabled. Native minimum stack is 32 KiB versus Bionic's accepted 8 KiB.
   Host joinable/detached workers, keys/destructors and failure cleanup pass
   ASan/TSan. ARM/Thumb tests execute actual SDK attribute helpers and guest ID equality
   with modeled creation/join/TLS endpoints and a live-handle backing object.
   Current SDK thread/key IDs are four bytes; compile-time assertions and ARM
   canaries enforce forwarding compatibility. Actual Vita scheduling, worker
   termination before module unload, guard protection, custom stacks, realtime
   priority mapping and newer inheritance flags remain incomplete or unverified.
   Loader unload now checks module synchronization before any fini/cleanup. Busy
   modules retain their handle and mappings. Idle sidecars are destroyed before
   unmapping. A native destroy error retains mappings but may partially tear down
   the module; new handles, symbol lookup and reinitialization are then rejected. Retrying unload
   does not rerun fini. An actual ARM DSO fixture verifies these paths.
   TSan still covers actual initializer cancellation at pthread_testcancel and
   waiter cancellation in pthread_cond_wait, without suppressing reports. The
   nanosleep-cancel report reproduces in tests/repros/tsan_cancel_nanosleep.c
   using only native system pthreads and a correctly locked shared value.
   Remaining: errno coverage audit, FILE/__sF ABI, advisory
   file locks and full mmap semantics. Private anonymous/file mappings now use
   page-aligned tracked storage, bounded positional reads, retained file
   descriptors and 64-bit mmap2 offsets. Partial ranges, shrink, reclaimed-tail
   growth, moved growth and failure rollback are tested. Read-only mappings
   request Vita USER_MAIN_R; RW mappings use USER_RW. Host tests prove actual
   host read protection, concurrency and zero backing/descriptor leaks; linked
   ARM tests execute kernel allocation/get-base/copy failures and real SDK
   descriptor lifetime with the Vita endpoints mocked. This is eager private
   snapshot storage, not a Linux VM/page cache. Shared/fixed mappings, PROT_NONE,
   executable/write-only mappings and fixed mremap report ENOTSUP. Partial holes
   are tracked but not individually protected/released; the entire memblock
   releases with its last live page. File truncation/beyond-EOF SIGBUS, lazy
   faults, shared coherence and hardware protection remain unimplemented or
   unverified. RNG/synchronous fdopen is explicitly
   unsupported. Vita stat inode/device IDs are path-derived, block counts are
   logical, and link/ownership fields are approximations. Vita writev is
   sequential, not one atomic vector operation. mkdir now forwards requested
   mode bits directly to Vita rather than using newlib's fixed 0777. Vita's
   user/system permission model still differs from Linux group/ownership bits.
   Rename validates source, device, types and directory emptiness before any
   replacement. It uses native replacement when available; on Vita's EEXIST
   backend it stages the old destination in a sibling backup and rolls back
   after a failed commit. No destination is unlinked before commit. Backup
   collisions, each failure stage and 16 rounds of concurrent replacement pass
   host boundary tests; linked ARM tests verify the actual production path.
   Preferences use this same bridge. Multi-step replacement is not atomic
   across other filesystem operations/processes and has no crash recovery.
   Failed rollback retains old data in a logged .vitaarc-rename-*.bak and returns
   EIO; failed cleanup after commit logs the retained backup and returns success.
   Target filesystem behavior, permissions and entropy still require runtime
   evidence, including open-destination replacement on actual Vita storage.
   C++ termination now uses a dynamic bridge registry rather than this SDK's
   32-entry newlib exit table. DSO selection, reverse order, reentry, concurrent
   exactly-once execution, OOM and guest exit are tested; loader unload drains
   callbacks by module range before unmapping, including absent fini calls.
4. BGM position and seeking: implemented in milliseconds, including resampler-
   accurate seek, pause/delay/loop behavior and three BGM channels. Synthetic
   VBR MP3 fixtures use a mocked Vita decoder; five sample rates and mono/stereo
   cases pass. Actual 1.6.1c ARM get/debug-set and playback methods execute
   through the production provider vtable, confirming position argument order,
   channel zero, volume ABI, seconds-to-ms delay and special BGM channel 2.
   A standalone SELF using production decoder/mixer/worker code now passes
   stock Vita3K's HLE kernel: 48000 real game MP3 frames, nonzero PCM, exact
   cursor/seek/reset, a native muted audio port/worker, stable paused position
   and resumed position after seeking, followed by teardown and exit zero.
   The installed firmware kernel path instead fails two module permission
   queries before codec initialization; the probe runner temporarily selects
   HLE kernel and restores the bootimage with a SHA-256 check. Audible playback,
   physical Vita decoding, priming and hardware queue latency remain untested.
5. Application callback failure: `platform/loop.c` now returns failure after
   input/render errors; main performs shutdown instead of continuing frames.
   The frame controller is host-tested; actual process/hardware cleanup remains
   part of target validation.
6. Existing parent field declarations now resolve through the supported class
   hierarchy, with inherited instance storage and canonical static storage.
   Full DEX field schemas remain incomplete. Arbitrary registered standard JNI
   native methods now dispatch by descriptor, including all return types,
   mixed arguments, inherited/virtual/nonvirtual calls and static registration.
   Cached child IDs can discover a late parent declaration through stable
   aliases, preserving later child overrides and explicit base-class calls.
   Native frames and operand roots protect references while the runtime lock is
   released. Reentry allows collection; host tests prove worker JNI/join,
   concurrent unregister and native pthread_exit cleanup without leaked roots
   or monitors. ARM native ABI and linked production VM fixtures pass.
   GL lookup now shares the canonical import symbol resolver, with extension
   aliases. Guest buffer names are tracked separately from vitaGL pointer
   handles; reserved/created/deleted state, current bindings, VAO references,
   name reuse and teardown are tested. ASan/UBSan and TSan tests pass.
7. Full physical Vita graphics/audio/input verification remains pending. Native
   Vita3K game boot, synthetic-panel gameplay, judgments, results, score save /
   restart reload and production unload now pass as detailed above. The actual
   production ARM ELF loader now has instruction-level execution evidence,
   described below; platform endpoints are mocked. Successful host tests and
   target linking alone remain insufficient evidence for this item.
   `tests/check_arm_jni_abi.py` now executes a separate C/C++ ARM calling-
   convention fixture with Unicorn. It caught and verifies the fix for missing
   base-PCS annotations in the JNI table. This is not real-game boot evidence.
   `tests/check_arm_bridges.py` executes actual linked VitaArc code: all 104
   GL/EGL canonical lookups and five float adapters pass. Its optional local
   game SO test executes real AudioManager position/playback methods and the
   production provider/vtable, intercepting unrelated scheduling/string calls
   and the backend before platform APIs. It does not
   execute the production ELF loader or validate graphics/audio hardware.
   `check_arm_native.py` checks independent ARM/Thumb callees against production
   native packing, including the 255-slot boundary. `check_arm_native_vm.py`
   executes the actual linked runtime/table/collector/trampoline with allocation,
   TLS, locks, logs and Vita memory calls stubbed. Native object frames, scalar
   and mixed calls, maximum signatures, exceptions and VM teardown pass, with
   zero remaining mock allocations. This is still not Vita scheduling or boot.
   `check_arm_file.py` additionally executes actual linked file wrappers and
   SDK descriptor allocation/dup/close, checking flag conversion, CLOEXEC,
   creation mode, stat layout, 64-bit offsets, sync errors and RNG cleanup.
   mkdir mode, rename replacement/rollback/retained backups and directory type
   and emptiness checks execute against mocked filesystem endpoints as well.
   `check_arm_loader.py` executes loader_load_library/load_sections, relocation
   resolution, symbol lookup, init/fini arrays and unload from build/VitaArc.
   Separate ARM shared libraries test RX/RW mappings, poisoned BSS clearing,
   relative/absolute/internal/external/weak relocations, canonical libc imports,
   cross-library symbols, actual relocated code, ctor/finalizer order and cloned
   handles. It found and verifies fixes for biased SHN_ABS values and treating
   R_ARM_RELATIVE's zero symbol as a required import. Reads, allocation, mapped
   base, kubridge memset/copy/relocation failures release all tracked resources.
   Nineteen malformed ELF cases are rejected before segment mapping. File-size
   queries now reject failed seeks/overflow, and bare library names work.
   The optional local 1.6.1c game SO loads through the production loader; an
   independent oracle matches all 48,281 applied relocations and 329 distinct
   imports, then the library unloads with no tracked allocations/handles left.
   Its constructors, JNI_OnLoad body and game startup are not executed by this
   test. Kernel/kubridge availability, caches, real memory protection and actual
   graphics/decoder/input behavior still need target evidence. This loader
   remains section-header/REL based, with a limited symbol model; automatic
   DT_NEEDED dependency lifetime management, concurrent load/unload and address
   reuse after repeated unloads are not established by these fixtures.
   New continuation: `check_arm_game_bootstrap.py` executes actual 1.6.1c
   constructors, registers 896 C++ callbacks and invokes JNI_OnLoad against the
   production VM. JNI 1.4 returns without exception. Unload and DestroyJavaVM
   leave zero JNI objects/references and no memblocks/file handles. The strict
   whole-heap gate now PASSES with zero allocations/bytes for the supported
   local 1.6.1c bootstrap. ObjectFactory/type records,
   AppDelegate and the exact bridge audio provider now release on the final
   library handle before static finalizers. The earlier 96 allocations/3086
   bytes first fell to four services/124 bytes. Version-checked analytics and
   achievement ownership cleanup first left platform/purchase objects (2/116).
   Platform COW-member destruction then left PurchaseManagerMobile (1/96).
   Verified purchase member destruction and listener detachment now close that
   instruction-level teardown gate. This still is not complete game boot.
   Its JSON report is under
   build/arm-game-bootstrap/report.json; bounded retained-object samples and
   conservative saved-stack symbol candidates are in retained.json. These are
   not an unwound call stack. No game allocator/destructor is mocked.
   Native newlib per-thread CRT state and pthread identity/key deletion are
   modeled, in addition to the loader fixture's platform endpoints.
   The loader's optional before_unload callback can reject preparation without
   modifying a live module, retaining its reference and callback for retry. A
   successful callback runs exactly once on final release. after_finalize runs
   after fini/C++ callbacks and before unmapping, and is consumed once even if a
   later synchronization destroy fails. Cocos cleanup uses exported deleting/singleton destruction APIs;
   AudioProvider access follows the inspected supported AppDelegate prefix and
   is guarded by concrete application/provider vtables. Independent ARM fixtures
   verify pre-fini ordering, cloned handles, and preservation of foreign types.
   The 1.6.1c bootstrap allocates two four-byte concrete analytics/achievement
   services without resource members or emitted destructors; the matching
   constructor stores them at AppDelegate +12/+20. Full function fingerprints
   (bootstrap 360 bytes, constructor 48) and concrete vptr checks gate this layout.
   Cleanup captures their ownership before deleting AppDelegate, then invokes
   the game's named operator delete after all static finalizers. A 44-byte
   temporary cleanup record is also freed. OOM defers before any destruction.
   Actual-game diagnostic assertions prove the exit registry is empty before
   service frees; foreign-type/profile-change injections preserve unsupported
   objects and preparation OOM retries without losing the live application.
   The strict whole-heap gate is zero for supported cases; rejection fault variants
   write reports in build/arm-game-bootstrap/<cleanup-case>/.
   PlatformUtilsAndroid is 20 bytes, with COW strings at
   +4/+8, data-check bool +12 and billing integer +16 (getter disassembly).
   Java setDeviceId's validated 132-byte body calls the game string destructor
   at Thumb BL offset +0x42 (local target 0x623f35); its FNV-1a64 is
   0xb2c12cf9e38b0837. Game destructor code compares the shared empty rep,
   decrements COW references and calls the game operator delete. The adapter now
   decodes that verified call and fingerprints its 88-byte callee as well
   (0xe008284efb12123d), destroying +8/+4 before the platform allocation. Both
   caller/callee mismatches preserve the platform type. Independent nonempty
   strings and a shared rep with a third C++-finalized owner execute actual game
   constructors/copy/destructors; both reps/shared rep release exactly once after
   the required finalization phase. GNU objdump independently supplies test call
   targets. Test-added callbacks are counted separately from the game's 896.
   PurchaseManagerMobile's verified 96-byte layout owns COW strings at
   +4/+8/+20/+80/+84/+88, std::function at +24 and three vector<string> objects
   at +44/+56/+68, plus the IAP-listener subobject at +92. Four member-using
   functions and the named game vector/function destructors are fingerprinted.
   Base dispatch is restored for captured-target destruction; members release
   in reverse order with game helpers, then named operator delete. The cached
   IAP singleton pointer is read from its verified getter without invoking its
   factory. A matching live enabled listener is detached before destruction;
   foreign listeners are preserved. Unknown wrapper/getter profiles defer the
   whole unload intact until retry, preventing a dangling callback into unmapped
   code. Missing member-destructor profiles preserve the unsupported purchase
   object, retaining the strict failing heap gate rather than guessing cleanup.
   Nonempty/shared member fixtures release 10/5 allocations and destroy cloned
   captured targets without invoking the business callback. Actual game helpers
   move vectors with unused capacity, clone functions and destroy members. IAP
   fixtures use real enabled wrappers with explicit fixture finalization owners;
   matching/foreign listeners, OOM and unknown-profile repair are checked. This
   does not prove real SDK singleton lifetime or in-flight callback quiescence,
   which still require target startup/shutdown evidence.
   A portable official Vita3K 0.2.1 4220-a32ee0ce plus Sony 3.74 firmware now runs
   under build/vita3k. The old `VitaGL initialization failed` was a bridge bug:
   the linked library returns resolution fallback, not initialization success.
   Corrected return handling let diagnostic builds read and validate the game
   SO. Linked ARM main cases cover both resolution results and failure shutdown.
   Official OpenGL 4.3 runs hit a host null GL function call on a renderer
   thread. A separate single-byte-patched emulator copy requests OpenGL 4.4;
   it passes that point. Installing official kubridge 0.3.1 hotfix in the
   isolated taiHEN profile resolves the previously missing allocation export.
   Real segment mapping, constructors and JNI_OnLoad now run. Startup initializes
   SDKBOX's VM/activity/class-loader state, fixing its null-VM crash. The APK's
   SDKBOX logging-level setter now writes its actual static integer field.
   The loader fingerprints the supported libgcc group and replaces 43 Linux
   compare-exchange and five barrier literals with ARMv7 implementations.
   The helper ABI preserves r1/r2 and carry; unknown profiles fail before
   constructors. Diagnostic run 010 reaches shader creation/binding, then passes
   a null program to sceGxmProgramGetParameterCount (NID 0xD5D5FCCD), verified
   by host GDB and SDK stubs. vitaGL defers GLSL compilation and reports compile
   status true before linking. Current startup verifies shader compiler
   initialization; run 011 now exits clearly before ELF loading when the
   required module is absent. Twenty-two linked-main scenarios cover primary,
   fallback and missing compiler paths. Further SDKBOX Java services remain
   unsupported. The normal firmware audio path fails permission-query imports;
   the standalone HLE-kernel decoder/muted-worker probe passes as noted above.
   The official emulator executable is unchanged. libshacccg remains absent;
   an explicit offline pack now provides an alternate startup path. Vulkan's
   host initialization crash remains. See vita3k-runtime.md for commands,
   hashes and preserved logs. Run 027 now proves an actual game startup image
   through native GPU readback. Physical audio, input and complete gameplay
   remain unverified.
   An independent offline shader probe now proves real vitaGL/GXM rendering of
   the first game shader pair using the existing local psp2cgc compiler. Its Cg
   translations preserve matrix, sampler and vertex-color names/semantics;
   binary loading/linking, scaled textured geometry and GPU pixel readback pass
   with center (100,100,50,255), blue corner, GL error zero and guest exit zero.
   The probe now also checks a nonsymmetric translated matrix, exposing and
   fixing Cg multiplication order: column-major GL uploads require mul(v, M).
   The bridge now creates Cg handles and loads exact-hash .vgxp binaries when
   compiler initialization fails and a versioned offline marker is present.
   Bounds/stage/reflection/matrix checks run before deserialization. Missing,
   invalid or unlinked offline shaders terminate with a clear error/exit 1.
   Handle deletion frees tracking slots; linked ARM checks cover 600 lifetimes
   and zero remaining allocations/handles, plus online/offline/failure dispatch.
   Thirty-two linked-main cases include the offline fallback and shutdown paths.
   The manifest currently compiles 39 explicit game variants, including Cocos
   2D, 60-joint skinning, one-light-per-type lighting/normal maps, skybox and
   terrain. The generated binaries are packaged by the optional CMake
   VITAARC_SHADER_BINARY_DIR setting. Actual game runs 018-022 advance through
   progressively later default shader programs with normal missing-cache exits;
   further variants and actual frames remain to be verified. No complete game
   frame follows from the standalone result or successful cache compilation.
   Runs 023/024 load all 39 unique binaries and explicitly confirm 30 successful
   native program links, then read main-menu textures/fonts and StartScreen.csb.
   Run 023 found SDK pthread_equal(0,0) dereferencing NULL; the guest adapter now
   matches Bionic's opaque value comparison, with zero/unmapped ID regressions.
   Run 024 passes that point but fails AppActivity.startSceneLoadFinished. APK
   DEX inspection shows a UI permission/expansion-check path ending in native
   notifyExpansionCheckComplete, which startup already supplies for packaged
   assets. The adapter now acknowledges that completion and the Java flag,
   preserving exactly-once native delivery and clearing the callback on stop.
   Host tests cover duplicate requests and unavailable completion callbacks.
   Runs 025-027 reach Cocos initialization at 960x544. Run 027 presents frames
   1 and 120 and exports the latter as 2088960 bytes of RGBA; visual inspection
   confirms the real startup background, emblem and English introductory text.
   Run 028 additionally exports frame 1800 showing the complete title artwork
   and logo. This does not establish menu navigation or gameplay.
   Texture uploads now implement UNPACK_ALIGNMENT/ROW_LENGTH with checked row
   repacking for supported native formats, query/reset semantics and a shared
   error queue. Host and linked ARM fixtures pass; actual GPU tests confirm
   padded two-row RGB uploads at both sample points with GL error zero.
   ARM EHABI lookup now returns bounded PT_ARM_EXIDX or section-fallback tables
   for executable guest PCs, with native linker-table fallback. Independent
   tests verify all 16746 entries of the real game table. A stock-emulator C++
   probe using the game's runtime/personality catches a thrown payload through
   catch(...) with exactly one RAII cleanup (result 43, exit 0). Typed catches
   across the probe/game toolchains and concurrent module unloading remain
   unproven; successful full-game shutdown still requires runtime evidence.
   JNI writable paths and Context.getFilesDir now return the absolute Android
   /data/data/moe.low.arc/files alias, mapped to persistent/data/files/ by the
   existing bridge. Java File queries use that resolver as well. Cross-process
   binary file roundtrips and the second /data/user/0 alias pass on host. This
   corrects Cocos treating the previous ux0: path as relative. Existing native
   preferences remain at persistent/data/cocos-preferences.bin; legacy root
   game files are not automatically migrated. Actual save reload is pending.
   Run 029 confirms later on-demand GPU export without restarting. Host Cross
   input through Computer Use completes, but the resulting frame still shows
   the title page; guest input delivery/game response is not established. Window
   screenshots time out and coordinate input geometry is unavailable. Actual
   touch navigation, gameplay, audible output and physical Vita checks remain.
   Fatal/info messages now reach sceClibPrintf even when the optional log sink
   is inactive; ARM tests verify bounded long-tag/body/raw formatting.

Current checks: `cmake --build build/host-tests`,
`ctest --test-dir build/host-tests --output-on-failure`, and VitaSDK
`cmake --build build --target VitaArc --parallel 8`.

## Historical snapshot

This file records behavior visible in the current source tree. It separates
known implementation gaps from historical problems that have already changed.

## Startup and runtime reachability

`src/init.c` currently initializes logging and vitaGL, loads
`libcocos2dcpp.so`, applies fixed patches, runs `.init_array`, and calls
`JNI_OnLoad`.

The calls that would continue into Cocos are still commented:

- `bridge_jni_cocos_init_app`
- `bridge_jni_cocos_set_device_id`
- `bridge_jni_cocos_native_init`

Control then reaches `for (;;);`. Consequently `log_end`, `vglEnd`,
`loader_free_library`, network termination and normal cleanup are unreachable.

## `vglInitExtended` crash remains a runtime blocker

`vglInitExtended(0, 960, 544, 24 * 1024 * 1024,
SCE_GXM_MULTISAMPLE_NONE)` is enabled. The first clear/swap test remains
commented. The repository contains no captured crash trace proving whether the
failure is in vitaGL initialization, shader-compiler startup, memory setup, an
unsupported Vita3K path, or earlier memory corruption.

The current build succeeds, but no successful Vita/Vita3K runtime boot has been
recorded in `.llm`.

## Shader-source lifetime

The OpenGL bridge allocates generated Cg, gives its pointer to vitaGL
`glShaderSource`, then frees it immediately. The historically inspected vitaGL
implementation retains the pointer until `glCompileShader`, so this is a
use-after-free candidate.

The wrapper also ignores `length[]` and uses `strlen` for every source segment.
Non-NUL input can be read out of bounds.

Shader conversion uses a fixed `0x8000` output allocation and hand-written
`strstr`/pointer transformations. Complex or unexpected GLSL can overflow,
misparse, or dereference failed searches.

The dump path is fixed at `ux0:vitaarc/shader/1.glsl`; it does not distinguish
shader IDs and does not safely handle `fopen` failure.

## Incomplete OpenGL surface

- `_gl_get_shader_source` does not return source data.
- `_gl_bind_attrib_location` only logs.
- renderbuffer calls are placeholders.
- VAO OES calls are placeholders.
- map/unmap buffer returns failure.
- `glIsBuffer` and `glIsRenderbuffer` infer validity from pointer-like values.
- `eglGetProcAddress` recognizes only three VAO OES names.

The fixed Cocos and audio offsets are tied to one exact
`libcocos2dcpp.so` layout and are not guarded by instruction signatures:

- `0x69A342`
- `0x50E30E`
- `0x6E6AD4`
- audio hook `0x38FD1C`

Selecting another APK can therefore patch unrelated instructions.

## Loader constraints

All libraries map at `0x98000000`, so the apparent 32-slot model is not a
working multi-library address-space model.

ELF input validation is missing for magic, class, byte order, machine, object
type, table bounds, segment ranges, string offsets and relocation targets.
Malformed or incompatible input can cause arbitrary reads/writes.

Only four relocation kinds are implemented. Unknown relocation types are
logged and ignored:

- `R_ARM_ABS32`
- `R_ARM_RELATIVE`
- `R_ARM_GLOB_DAT`
- `R_ARM_JUMP_SLOT`

Segment memblock IDs are not stored and cannot be freed. `.init_array` is not
required during loading but is dereferenced during initialization. Its address
is calculated from `text_base + sh_addr`, which depends on the target ELF
layout.

`loader_get_proc_address` can return an address for an undefined symbol because
it matches by name without checking `st_shndx`.

Automatic local-symbol resolution directly writes relocation cells without
checking page permissions or explicitly flushing caches.

## Symbol definition drift

`tools/generate_symbols.py` currently emits 354 entries, while the comment at
the top of `src/bridges/symbol.def` says “Unique symbols: 349”. The generated
count is authoritative; the comment is stale.

The resolver only binds imports present in `symbol.def`. Missing imports remain
zero after the initial relocation pass. There is no unresolved-symbol failure
report or mandatory completeness check against the selected APK.

## AssetManager limitations

The AssetManager bridge is implemented, not empty. Current limitations:

- the manager is a fake local object rooted at `app0:package/assets`;
- asset mode is accepted but does not change behavior;
- paths are limited to a 1024-byte composed buffer;
- directory enumeration returns regular files from one opened directory;
- nested recursion is not part of `AAssetManager_openDir`;
- seek/length behavior depends on Vita libc `fseek`/`ftell`;
- `AAsset_openFileDescriptor` returns the `fileno` of a `FILE *`, so ownership
  and descriptor lifetime remain coupled to the asset;
- APK compression no longer matters at runtime because files are expanded into
  the VPK.

## Fake JNI runtime limitations

The JNI table is now broadly populated and supplies real local representations
for strings, classes, members, arrays, exceptions, native registration,
JavaVM access and direct byte buffers. It is still a compatibility runtime, not
a JVM:

- no Java bytecode execution or class loading exists;
- most method calls return conventional defaults unless explicitly recognized;
- object/reference lifetime is simplified and local/global reference ownership
  is not JVM-accurate;
- monitors are no-op success paths;
- native registrations are stored on fake member objects, but there is no
  general Java dispatch engine to invoke arbitrary registered natives;
- class hierarchy and assignability are simplified;
- field values and object state are not a complete Java object model;
- thread attachment returns the single process-global fake environment rather
  than per-thread VM state.

## Cocos platform services and persistence

`get_cocos2dx_writable_path` now returns `PATH_TO_DATA`
(`ux0:vitaarc/persistent/data/`), so the old literal `"NULL"` writable-path
behavior is gone.

Persistence is still incomplete: string key reads return `"NULL"`, writes do
not persist, and integer/bool preference behavior is not a real Android
SharedPreferences implementation.

## libc compatibility gaps

Several system calls report success without performing the operation or
populating outputs, including `mkdir`, `lstat`, `fsync`, `ftruncate64`,
`ioctl`, `rmdir`, `readlink`, `utimes`, `munmap`, and `nanosleep`.

`clock_gettime` writes raw Vita RTC ticks to `tv_sec` without frequency
conversion.

The fake `/dev/urandom` descriptor is `1`, colliding with the conventional
stdout descriptor.

Only `fopen` redirects `/data/data/` into
`ux0:vitaarc/persistent/data/`. Other path-bearing calls do not share that
translation. The redirect uses a 256-byte `sprintf` buffer.

The libc `dlopen`/`dlsym` placeholders are not connected to
`loader_load_library`/`loader_get_proc_address`.

## File utility defects

`read_file_all` currently returns `false` when the requested file exists, then
tries to open only a missing file. Its existence test is reversed.

`util_get_file_size` stores open/seek failures in an unsigned 32-bit result; a
negative error may become a huge positive size.

## Logging and debug limitations

Default logging now truncates/writes ux0:vitaarc/boot.log and mirrors INFO and
higher to sceClibPrintf. UDP requires an explicit LOG_OVERUDP/UDP_HOST build;
no fixed destination remains. The final stop message precedes sink closure,
including a valid zero handle. The optional UDP initialization path still needs
its own network-resource ownership audit before use.

`log_base` advances a 1024-byte buffer using the would-have-written values from
`snprintf`/`vsnprintf` while repeatedly passing the full buffer size. Truncated
messages can move the cursor past the allocation.

Android log priorities are passed into the local five-level log model without
a complete bounds/mapping layer.

`debug_memory_dump` lacks `SCE_O_TRUNC`; a shorter replacement dump can retain
the tail of an older file. The JNI entry path dumps a fixed `0xA60AA0` byte
range rather than deriving mapped bounds.

For Vita3K-native logging alternatives, see `vita3k-debugging.md`.

## Audio lifetime

The fake AudioProvider destructor does not release its object or methods table.
The AudioManager failure path calls a destructor obtained by handwritten
offsets but does not free the allocated manager. Constructor/init symbol lookup
results are not null-checked before use.

## Build status

An out-of-source Debug build is present and verified under `build/`, including
`VitaArc`, `VitaArc.self`, `VitaArc.vpk`, and the generated bridge header.
This confirms compilation and packaging only, not execution correctness.
