# JNI / Cocos API update, 2026-10-24

The current work prioritizes JNI/Cocos per the user's clarification.

- `cocos.c`: declarative class/name/signature table; bool, int, float, double,
  string preferences and deletion; package/language/paths, context, AssetManager,
  device model/DPI and RNG-backed version-4 UUIDs.
- `preferences.c`: bounded, versioned binary storage under `PATH_TO_DATA`,
  directory creation, serialized mutations, temporary-file replacement and
  malformed-file rejection. Cocos double preferences intentionally use float
  precision, matching `Cocos2dxHelper`.
- `runtime.c`: shared static dispatch for ordinary/`V`/`A` calls; correct vararg
  float promotion; instance/static field storage; canonical class names;
  modified UTF-8 and UTF-16-indexed regions; overflow-safe array-region checks;
  typed exceptions; registered-native lookup and clearing.
- `descriptor.c`: bounded method descriptor parsing shared by method lookup and
  argument decoding; complete validation precedes any vararg consumption.
- Instance/nonvirtual ordinary/`V`/`A` calls now share checked dispatch for
  Object, String, Context, Class, ClassLoader and File services. Unsupported
  methods raise an exception. Virtual String equality uses value comparison;
  explicit Object nonvirtual equality retains identity semantics.
- Arrays carry element tags/component classes and canonical array classes.
  Shared checks prevent primitive/object accessor confusion, incompatible stores
  and region overflow; nested-array covariance follows the limited class model.
- `jni.c`: calls `JNI_OnLoad` before resolving registered native callbacks,
  supplies context and AssetManager, checks startup exceptions, and gates all
  runtime callbacks until initialization succeeds. Stop clears callbacks and
  native registrations. Touch arrays use retained global references.
- Failed startup now clears partial registrations. All seven runtime callbacks
  stop the bridge on exception, preserve the exception for the caller and require
  explicit restart. ExceptionDescribe itself clears exceptions per JNI semantics.
- `abi/calling.h`: floating-point calling-convention annotation shared without
  pulling vitaGL into JNI compilation.
- `tests/`: real production bridge plus host mock shared library. Independent
  writer and reader processes verify persistence. Failure injection checks
  missing exports, unsupported JNI versions, initialization exceptions, write
  failure and malformed stored data. Field, string, array and asset tests run
  with AddressSanitizer and UndefinedBehaviorSanitizer.
- The seventh `dispatch` process exercises the new services and type checks;
  startup tests inject exceptions into each callback and verify no later callback
  executes, including after clearing the exception.
- GitHub Actions and GitLab CI include the host tests without requiring an APK.

See `tests/README.md` for commands, the mock boundary and remaining runtime gaps.
The latest continuation adds `thread.c`, `references.c` and the `api.def` entry
list: per-thread environments/exceptions, real reentrant monitors, distinct
local/global/weak handles, local frames, borrowed-buffer pins and iterative graph
reclamation. Cocos startup/callbacks create automatic local frames. Tests prove
stable heap/reference counts over repeated allocations, getter/deletion loops
and rendering callbacks; four workers exercise locking and isolated state.
VM shutdown releases all runtime objects after the last attached thread leaves.
ASan/UBSan passes 20 CTest targets and TSan passes 19. The additional blocking-
sleep cancellation variant stays under ASan because the installed libtsan's
nanosleep cancellation path loses mutex interception; a native-only reproducer
and the exact validation split are recorded in tests/README.md.

Existing field declarations resolve through the supported parent chain; field
access validates type/static use and inherited receivers, and static storage is
owned by the declaring class. Missing declarations are still synthesized rather
than loaded from a DEX class schema.

Successful host tests and a VitaSDK build do not establish real-game boot on
Vita/Vita3K. Java bytecode execution, graphics/audio hardware and remaining
libc ABI gaps remain separate work. Cocos lifecycle/render/input remain confined
to their owner thread, even though attached worker JNI calls are synchronized.

Registered standard JNI natives now use `native_call.c`: base-PCS scalar packing
and an ARM/Thumb trampoline on Vita, libffi on the host. The descriptor limit is
255 Java slots rather than the earlier 16 arguments. Separate native declaration
records support static/instance IDs, replacement, unregister and known-parent
virtual/nonvirtual lookup. Calls create protected automatic frames and operand
roots, release the runtime lock during native execution and preserve returned
objects/exceptions. Host tests cover worker JNI/join, concurrent unregister,
reentry-time reclamation and pthread_exit cleanup from an active native frame.
Two ARM fixtures check independent callees and the actual linked production VM;
the latter stubs platform endpoints and proves complete VM allocation cleanup.

File coverage now includes host filesystem tests, a __vita__ mock boundary and
`check_arm_file.py` against the actual target ELF and SDK descriptor routines.
These cover Android open flags/mode, the 104-byte stat layout and large sizes,
append/sync writes, CLOEXEC/dup isolation and RNG chunk/error/cleanup behavior.
Platform I/O remains mocked in ARM execution. Remaining errno coverage, native stdio
layouts, advisory locks and full VM mapping behavior remain open. Private
anonymous/file mappings now have page alignment, tracked partial ranges,
shrink/in-place tail growth/moved growth, descriptor lifetime, positional reads
and 64-bit mmap2 offsets. `libc_memory_mappings` tests real host protection,
failure rollback, concurrent ranges and zero backing/descriptor leaks.
`check_arm_mappings.py` executes the linked target and SDK descriptor routines
with Vita memory/file endpoints mocked, including failed kernel allocation,
base lookup and unrestricted copy. Shared/fixed maps and unsupported protection
modes report ENOTSUP; partial holes retain their backing block until all pages
are retired. There is no individual hole protection, lazy paging or EOF SIGBUS.
mkdir forwards caller mode bits;
rename now validates its source/target, stages an existing destination and rolls
back failed commits without deleting old data. Preferences use these same
wrappers. Host failure/collision/concurrency tests and linked ARM replacement,
rollback, retained-backup and directory checks pass. Replacement through staging
is not atomic across other filesystem operations and has no crash recovery;
see `known-issues.md` for the current status and recovery limitations.

`check_arm_loader.py` adds a production loader execution path under Unicorn.
It loads independent ARM DSOs through real target code, checks RX/RW segments,
BSS and relocations, executes their exports/constructors/finalizers and verifies
unload/failure cleanup. Nineteen malformed cases fail before segment mapping.
The optional local real game SO maps and relocates through the same code; an
independent oracle verifies 48,281 entries and 329 distinct imports. This closes
a loader-instruction test gap, not game boot: platform memory/I/O/cache endpoints
are mocked. That loader-only test leaves game constructors/JNI_OnLoad/startup
unexecuted; the newer bootstrap diagnostic below reaches the next stages.

The newer `check_arm_game_bootstrap.py` executes the actual constructors and
JNI_OnLoad using the production VM (JNI 1.4, no pending exception). It exposed
the installed newlib's 32-entry __cxa_atexit limit; `cxx.c` now provides a dynamic
DSO registry and unload fallback. Host tests and an ARM loader fixture prove
128 cross-DSO callbacks drain in reverse order exactly once. The real game
registers 896 callbacks. VM objects/references reach zero after unload/destroy,
and the strict native heap gate now passes with zero allocations/bytes after
the supported bootstrap teardown. Cocos pre-unload cleanup destroys its ObjectFactory, matching
AppDelegate and matching bridge AudioProvider before static finalizers; this
eliminates all 90 constructor-retained allocations and two bootstrap objects.
The purchase service now has verified member destruction and listener detachment.
Analytics/achievement allocations now release after module finalizers, gated by
the inspected bootstrap/constructor fingerprints and exact concrete vptrs. This
remains a diagnostic, separate from the
passing host targets. ARM fixtures verify final-handle cleanup order and
foreign application/provider preservation.

Vita3K has also been installed in an isolated project-local portable directory.
It starts VitaArc with OpenGL. vitaGL's return is a resolution-fallback flag;
the erroneous failure branch is fixed and the game SO is read and validated.
A separate OpenGL 4.4 emulator copy avoids a host null GL call. Official kubridge
installed only in that profile resolves allocation: actual segments, constructors
and JNI_OnLoad execute. The startup bridge now initializes SDKBOX's JavaVM,
activity and class loader, and the verified libgcc group uses ARMv7 replacements
for Linux compare/exchange and barrier entries. Actual startup reaches Cocos
shader creation/binding, then a null GXM program fault at 0x24. Current startup
checks the compiler first and reports the missing module before game loading.
`check_arm_startup.py` covers both resolution flags, three compiler availability
paths and startup/frame shutdown in 22 linked-main cases with endpoints intercepted.
The standalone audio SELF now decodes a real MP3 and verifies muted output-thread
position/pause/seek/teardown on stock Vita3K's HLE kernel; its runner restores
the isolated firmware bootimage and verifies the original hash. Native firmware
permission queries still block its codec path. Audible output remains untested.
See `vita3k-runtime.md`; full startup remains incomplete.

`directory.c` now owns guest directory streams and converts readdir/readdir64
to the Bionic 280-byte layout. Host and `check_arm_directory.py` tests cover
path aliases, file/directory/link types, inodes, 255-byte names, EOF/error,
reentrant caller buffers, CLOEXEC, ownership and seek/rewind cookies. Failed
Vita cursor replacement preserves the original stream, with temporary-handle
cleanup. Host shared-stream reentrant reads pass under TSan. Vita file APIs
are mocked in ARM validation. Directory mutation/dup-stream cookies, scandir,
getdents, stable physical inode identities and target fd-zero support remain
outside this evidence; the installed SDK rejects descriptor zero.

`guest_errno.c` now supplies typed guest entry adapters for 146 symbol bindings.
Underlying implementation helpers keep native error codes for internal callers.
Host tests cover saved/repeated errors, direct-return codes, formatting and
thread isolation. `check_arm_errno.py` uses independent Linux/SDK headers to
verify 87 supported mappings, then calls resolved exports to check ENOSYS,
ENOTSUP, ENAMETOOLONG, timeout/deadlock codes, unknown native errors, EOF,
strerror, 64-bit returns and variadic creation/format/scan arguments. Reentrant
newlib formatting allocations and initialized CRT state are modeled and
reclaimed. Socket/pthread endpoints inject failures for boundary verification;
this does not establish network or scheduling behavior. The shared native CRT
errno slot and unwrapped callbacks still require coverage audit, and network
data/EAI and stdio/FILE ABI gaps remain.

`poll.c` replaces the timeout-only implementation. Host tests use real socket
and pipe readiness, HUP/NVAL, duplicate array entries, delayed input, finite
timeout and signal-interrupted infinite waits. `check_arm_poll.py` executes
the linked poll/errno adapters and actual SDK descriptor grab/drop logic with
network epoll, time and delay endpoints mocked. Android/native masks, error/HUP
without requested input, duplicate native UIDs, ready files/RNG/NVAL, long
timeout chunks, early returns, infinite interruption, malformed events and
allocation/create/control/wait/cleanup failures are covered. A modeled close
during wait verifies the final native socket release occurs after epoll teardown.
Native pipes/TTYs and socket priority/band/RDHUP remain unsupported; actual
network behavior and the kernel resource state after failed destroy are open.

`once.c` replaces the raw pthread_once forwarding that passed a 4-byte Android
control to a 16-byte SDK object. `libc_once_abi` covers eight contending callers,
nested initialization, canaries, OOM/reuse, errno, canceled waiters and retry
after initializer exit/cancellation. Transient registry allocations return to
zero. `check_arm_once.py` executes independent ARM and Thumb callers through
the production import adapter, verifies both control sizes and canaries, and
executes real SDK cleanup push/pop while TLS/mutex/allocator endpoints are
modeled. C++ exception unwinding and real Vita scheduling remain unproven.
Mutex/condition controls and attributes now have a separate bridge in `sync.c`;
thread attribute/opaque-handle validation is described below.

The once test's explicit pthread_testcancel cancellation is checked with TSan.
Cancellation inside nanosleep is separately checked by
`libc_once_blocking_cancel` under ASan/UBSan. The native-only
`tests/repros/tsan_cancel_nanosleep.c` reproduces the installed libtsan warning
without bridge code. No TSan reports are suppressed; that blocking-interceptor
path remains outside the passing TSan claim.

`libc_sync_abi` now checks actual host contention, recursive/errorcheck types,
lazy publication, deadlines, broadcast, waiter cancellation, busy/native destroy
failure preservation and zero retained registry allocations under ASan/UBSan and
TSan. `check_arm_sync.py` executes independent ARM/Thumb callers against production
adapters and actual SDK mutex attribute/cleanup-stack helpers, while modeling
native scheduling. Its loaded DSO proves that busy synchronization prevents fini
and unmapping; a later native cleanup failure retains mappings and blocks reuse,
and retry does not rerun fini. Vita's native condition implementation ignores
its clock attribute, so monotonic waits are converted into bounded realtime
slices checked against process time. Tests rebase across between-slice realtime
changes; a clock-sampling race and native millisecond/range limits remain.
Actual Vita scheduling, process sharing, robust mutexes, fork and async cancel
are outside this evidence. The real-game bootstrap still reaches JNI 1.4 and
retains zero service allocations after the newer cleanup described below;
the supported constructor/JNI/teardown heap gate passes.

`thread_attributes.c` now stores the Android 24-byte attribute, with sixteen
guest exports and explicit native detach conversion. `_pthread_create` uses a
temporary native attribute and publishes only a successful ID. NULL attributes
on Vita request a 1 MiB stack instead of 32 KiB; guard enforcement remains absent.
Explicit nonzero guards (including initialized Bionic defaults), custom stacks
and realtime scheduling report ENOTSUP; they are not silently discarded. Host
joinable/detached workers verify identity, TLS destructors, return values and
error cleanup under ASan/TSan. `check_arm_thread_attributes.py` executes actual
SDK attribute helpers and pthread_equal, independent ARM/Thumb callers and
production adapters, while modeling native creation/join/TLS endpoints. It proves
24-byte attribute writes, four-byte opaque IDs/keys, reversed detach constants,
native minimum-stack failure, OOM and failed-ID rollback. Physical scheduling,
guard/custom-stack implementation, priority mapping and worker shutdown before
unload remain open; see tests/README.md for the exact support boundary.

The loader now has a retryable before_unload preparation hook and a one-shot
after_finalize hook while mappings remain live. The 1.6.1c adapter captures
analytics/achievement objects from the verified AppDelegate layout, then uses
the named game operator delete after all static C++ finalizers. These complete
four-byte concrete types contain only a vptr, with no resource members or emitted
destructor. Other types/versions are preserved. The real-game diagnostic observes
frees and an empty exit registry before cleanup; preparation OOM must leave the
live app/ref/callbacks intact until retry. Profile/type fault cases retain the
unsupported objects and still run the strict whole-heap gate. Normal cleanup
now reaches zero allocations after the newer purchase cleanup. This remains
instruction-level initialization/teardown, not full game boot.

Platform cleanup now verifies its concrete 20-byte layout and a 132-byte game
string-using JNI caller. A Thumb BL resolves the actual game string destructor,
whose full 88-byte body is also fingerprinted. After static finalizers, the two
COW members destroy in reverse order and the object uses game operator delete.
Unknown caller/callee versions preserve the object. Real-game nonempty/shared
string fixtures exercise original constructors, copy/refcounts and destructors;
GNU objdump is an independent target oracle. The shared case adds a third owner
released by C++ finalization before platform cleanup, and the rep frees once.
Normal and these ownership cases now pass zero retained heap; the whole-heap
assertion was not weakened.

Purchase cleanup verifies the six COW members, three vector<string> objects,
std::function and listener subobject through four member-using function profiles
and actual game destructor profiles. Nonempty/shared fixtures release 10/5
allocations with actual move/copy/destruction and no business callback invocation.
Matching IAP listeners detach before destructive work; foreign listeners remain.
The adapter reads the existing cache without initializing an SDK singleton.
Unknown callback-wrapper/getter profiles defer the whole live unload until
repaired; OOM also preserves all state for retry. Unknown member profiles retain
the unsupported object and fail the original strict heap assertion. Typed SDK
wrapper fixtures have explicit fixture owners. Physical SDK singleton lifetime,
active-callback quiescence and game startup remain unproven.
