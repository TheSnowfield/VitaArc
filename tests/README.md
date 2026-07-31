# JNI / Cocos host tests

## Local APK version comparison

`tools/package_apk_versions.py` builds separately named VPKs from an existing base
without changing the base package. `tests/run_vita_apk_version.ps1` extracts a
variant, temporarily swaps the private installation and the whole `ux0:vitaarc`
directory, starts its actual packaged eboot with fresh data, and observes it for
40 seconds. It uses the documented SDL/OpenGL diagnostic emulator and HLE
kernel. A running emulator must be closed first.

```powershell
./tests/run_vita_apk_version.ps1 -Version 1.9.0c -Python (Get-Command python).Source
```

The bounded diagnostic is stopped before restoring the baseline. Firmware,
default config, original eboot/library hashes and all original user-file hashes
are checked after restoration. The extracted variant, test data, logs and native
GPU frames remain under `build/apk-versions/run-<version>-<timestamp>/`.
`-PackageDirectory build/apk-compat-r2` selects another preserved package batch;
`-SelfOverride build/VitaArc.self` substitutes a development SELF and records its
hash. `-Program touch-probe` or `lifecycle-probe` runs a separately linked native
fixture with the selected APK and fresh data; touch commands use the usual
vita_touch_control.py mailbox. Wait for command completion before viewing its
PNG: a timed-out command leaves an older PNG in the shared output directory.
This does not verify physical display or hardware touch. Packaging now writes
the APK manifest version for setAppVersion; legacy packages default to 1.6.1c.
`-SeedData build/<previous-run>/tested-user/persistent` clones a prior test's
persistent state while retaining the original installation and fresh mailbox.
Current compatibility work includes real bitmap-text rendering and the newer
audio layouts; full gameplay verification is recorded per version.
See [the version compatibility record](../.llm/apk-compatibility.md).

## Run

Requirements: Linux or WSL, a C compiler, CMake 3.16+, Python 3, pthreads and
libffi, Vorbis and FreeType development headers/libraries, plus a test font
(`sudo apt-get install libffi-dev libvorbis-dev libfreetype-dev fonts-dejavu-core`
on Ubuntu). An alternate font can be selected with `-DVITAARC_TEST_FONT=<font.ttf>`.
No game files or VitaSDK are needed.

```sh
cmake -S tests -B build/host-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host-tests --parallel 2
ctest --test-dir build/host-tests --output-on-failure
```

From PowerShell, prefix each command with `wsl -d Ubuntu --`.
Use `ctest --test-dir build/host-tests -V` to see the individual scenario output.
AddressSanitizer and UndefinedBehaviorSanitizer are enabled by default; configure
with `-DVITAARC_SANITIZERS=OFF` only on hosts without sanitizer support.

`run_tests.py` creates an isolated temporary directory containing Vita-style
`app0:package/assets` and `ux0:vitaarc/persistent/data` paths. It runs nine fresh
processes and removes its own fixture directory on exit. Existing game data is
never used.

## Implementation boundary

`jni_text_bitmap` checks the real FreeType rasterizer's RGBA output, wrapping,
alignment, shrinking, outline and invalid inputs. The JNI suite also exercises
the game's bitmap callback contract and typed local analytics dispatch. Audio
tests distinguish integer PCM from IEEE float WAV, including clipping, NaN,
infinities, stereo resampling and rejection of unsupported float64 data.

The test executable compiles the production `runtime.c`, `runtime/*.c`, `thread.c`,
`references.c`, `descriptor.c`, `native_call.c`, `cocos.c`,
`preferences.c`, `jni.c` and `assetmgr.c` without replacing their behavior.
The mock layer provides Vita type definitions, deterministic random bytes,
logging and the loader's symbol lookup via the host `dlsym`.

`mocks/game.c` builds a separate shared library. It implements `JNI_OnLoad` and
the Cocos native entry points, calls the real JNI table and validates startup
order, non-null context/AssetManager, class loading and input values. It can
hide a required export, return an invalid JNI version, throw from `nativeInit`,
register a rendering function through `RegisterNatives`, or throw from each of
the seven rendering/lifecycle/input callbacks.

## Coverage

| Scenario | Behavior checked |
| --- | --- |
| runtime | Ordinary, `V`, `A` getters and defaults for bool/int/float/double/string; method signature matching; fields of all nine JNI types with instance isolation and static storage; modified UTF-8 with NUL/surrogates; UTF-16-indexed regions; array overflow; direct buffers; native registration/unregistration |
| dispatch | Complete method-descriptor validation before vararg reads; ordinary/`V`/`A` instance and nonvirtual calls; virtual String equality versus Object identity; Context/ClassLoader/Class/File services and File constructors; return/static/receiver/argument validation; typed and nested arrays, rejected stores and type confusion, direct elements and release validation; ExceptionDescribe clears the pending exception |
| write | Preference setters, deletion and null-string deletion through the JNI function table |
| read | A separate process reads committed values, verifies deleted keys, and checks Cocos double-to-float compatibility |
| startup | Mock SO → `JNI_OnLoad` → context/class loader → platform/analytics → native init → rendering/input/pause/resume/stop; required-export, version and exception failures; failed-start registration cleanup; registered-native fallback; exceptions in each of seven callbacks stop subsequent calls until restart, including after ExceptionClear; pending exceptions prevent callback entry |
| loop | Production frame-step controller reports failed initialization/input/render, suppresses later polling/render calls, and distinguishes normal frames from paused frames |
| references | Distinct local/global/weak handles; frame result promotion and capacity; weak clearing; object/static-field/array graph roots and cycles; pending exceptions and borrowed buffer pins; 3000 frame allocation cycles, 3000 Cocos getter/deletion cycles and 3000 rendering callbacks with stable object/reference counts |
| io_failure | Failed temporary-file creation reports a JNI exception and preserves prior preferences |
| corrupt | A malformed preferences file reports a JNI exception and is not overwritten |

Asset checks use the manager returned through JNI, then exercise the production
NDK bridge: open/read/remaining length, cached buffer without moving the cursor,
independent file-descriptor lifetime, traversal rejection and sorted directory
enumeration/rewind.

## Preference storage and supported services

`src/bridges/jni/cocos.c` contains the explicit class/name/signature table.
It implements the five Cocos preference types and deletion, package/language/
asset/writable paths, device model/DPI, context, AssetManager and UUID generation.
All three static-call forms share this dispatch. An unhandled static Java method
raises `UnsupportedOperationException` with a log identifying the exact call.

Ordinary, `V` and `A` instance/nonvirtual calls also share one dispatcher. It
validates descriptors, return types, static/instance use, receiver compatibility,
nonvirtual declaring classes and required argument arrays. This compatibility
dispatcher checks the Java 255-slot limit (254 explicit slots for instance
methods); long/double consume two slots and references consume one. Invalid
descriptors are rejected before reading varargs. `String` supports length/isEmpty/charAt/equals/toString;
`Object` supports getClass/equals. Virtual String equality and File toString use
their overrides; explicit nonvirtual calls select the declaring implementation.
Context supports class loader/assets/application context/package name/files dir;
Class supports name/class loader; ClassLoader supports loadClass. File supports
its String constructor, path/absolute path/toString/exists/isDirectory. Relative
File absolute-path conversion and other methods raise an explicit exception.

Arrays retain their primitive element tag or reference component class. Shared
checks reject mismatched accessors, incompatible reference stores and overflowing
regions. The limited class model includes the Activity/Context and exception
chains, array covariance, Object, Cloneable and Serializable. Primitive elements
are direct storage (`isCopy=false`); releasing with JNI_ABORT cannot undo writes.

Failed startup and callback exceptions clear callbacks, retained touch-array
references and native registrations. The original exception remains pending for
the caller, even though the standard ExceptionDescribe API clears it while
logging. Clearing that exception alone does not restart the bridge.

Preferences live in `ux0:vitaarc/persistent/data/cocos-preferences.bin`.
Keys and values are length-delimited modified UTF-8 data, not path components.
The versioned format uses little-endian lengths, a 1 MiB total limit and a 4096
entry limit. Writes serialize to a sibling temporary file and rename it after
successful flush/close, using the shared rename bridge. In-process mutations are
protected by a mutex. No
multi-process writer coordination or power-loss durability guarantee is made.
Malformed lengths, invalid types, duplicate keys and trailing data are rejected.

The signatures were checked against the local Arcaea 1.6.1c DEX. Cocos stores
double preferences as floats; the bridge preserves that behavior. References:
[Cocos2dxHelper](https://github.com/cocos2d/cocos2d-x/blob/cocos2d-x-3.17.2/cocos/platform/android/java/src/org/cocos2dx/lib/Cocos2dxHelper.java),
[JNI functions](https://docs.oracle.com/en/java/javase/17/docs/specs/jni/functions.html).

## Remaining limits

This verifies host execution of the bridge and a mock game. It does not execute
the Android ARM ELF loader, Vita hardware, graphics/audio backends, Vita3K or the
real game. The VitaSDK build separately checks target compilation/linking and
packaging; ARM calling conventions and Vita filesystem behavior still require
target runtime validation.

The runtime remains a compatibility implementation: no Java bytecode execution
or full class hierarchy. Registered standard JNI native methods use a general
descriptor-driven dispatcher. Java methods
beyond the explicit services above, including networking, billing and Android
bitmap rendering, are not implemented. Field lookup reuses existing declarations
along the limited class hierarchy; missing field declarations are synthesized,
so this does not establish a complete DEX-defined Java object schema. Inherited
instance access and static declaring-class storage are tested, with accessor
type validation. Native registration is independent of static/instance method
IDs, and all standard JNI primitive/reference signatures are callable through
ordinary, V and A static/instance/nonvirtual entry points.

The Cocos lifecycle/input/render bridge assumes one owner thread. JNI calls
from attached worker threads use their own environment and exception state.
Direct string/array pointers still require synchronization by their users when
shared between threads.

## JNI ownership and threading

`api.def` is the single declaration list for JNI entry wrappers and the function
table. Wrappers validate the current thread and serialize heap/metadata access.
Object identity must be tested with IsSameObject, since reference handles are
distinct. Local references belong to their creating thread and current frame;
global references are shared, and weak handles clear when their object dies.
PopLocalFrame promotes its result to the parent frame. DeleteLocalRef, frame
pop and detach remove ownership. EnsureLocalCapacity reserves stable handle
slots without moving existing references.

The heap uses iterative tracing of local/global roots, exception state, static
fields, object fields, object arrays, borrowed buffers and monitors. Unreachable
instance graphs, including cycles, are freed after outer JNI calls. Class and
member metadata remain interned for VM lifetime. Buffer pins last until release
or detach. Startup and native Cocos callbacks have automatic local frames.

`jni_threads` creates four attached workers with isolated exceptions and foreign
local-reference rejection. Reentrant monitors protect 2000 shared counter
increments while temporary frames are allocated and reclaimed. Explicit detach
and native thread exit both release monitors. Separate shutdown processes prove
that DestroyJavaVM waits for ordinary threads, does not wait for daemon threads,
rejects subsequent attachment, and releases all runtime objects/references after
the last thread leaves.

Run the same suite with ThreadSanitizer in a separate build:

```sh
cmake -S tests -B build/thread-tests -DCMAKE_BUILD_TYPE=Debug \
  -DVITAARC_SANITIZERS=OFF -DVITAARC_THREAD_SANITIZER=ON
cmake --build build/thread-tests --parallel 4
ctest --test-dir build/thread-tests --output-on-failure
```

The default ASan/UBSan build passes 22 CTest targets; the ThreadSanitizer build
passes 21. The extra blocking-sleep cancellation variant is described below
and excluded from TSan because of a reproduced interceptor limitation. These
counts are host concurrency evidence;
Vita thread scheduling and real-game execution still require target testing.

## ARM JNI calling convention

The JNI function table uses the Android base PCS on ARM, including in hard-float
Vita builds. Before this annotation, an instruction-level fixture returned a
mismatch for float calls despite all host tests passing. The corrected fixture
compiles separate C/C++ callers and base-PCS callees with VitaSDK, links a bare
ARM ELF and executes it with Unicorn. It verifies float arguments, float returns
and double returns through the C table and C++ convenience methods.

```sh
python3 -m pip install --target build/arm-tools unicorn==2.1.4
python3 tests/check_arm_jni_abi.py
```

The fixture checks that execution returns normally as well as checking values.
It exercises ARM/Thumb instructions and VFP registers, but does not execute the
production ELF loader, Vita system calls, graphics/audio backends or the game.

## libc and application frame tests

`libc_posix` compiles the production `posix.c` and runs in its own temporary
directory. It verifies actual mkdir/lstat/rmdir, failure errno, overflow-safe path
mapping, fsync, 64-bit seek/truncate using a sparse file larger than 4 GiB,
timestamps, symbolic link metadata/readback on the host, numeric conversions,
Bionic sysconf numbering, real/monotonic time and measurable nanosleep.

`libc_vita_boundary` compiles that same source with `__vita__` enabled. Mock
native calls assert retained-descriptor lifetime, full-width seek/truncate,
error translation, microsecond conversion, rounding and multi-hour sleep
chunking without waiting in real time. It does not execute the Vita SDK itself;
target compilation separately asserts the ARM timespec/timeval layouts.

Unsupported Linux ioctls/syscalls now report ENOTTY/ENOSYS rather than success.
Vita ownership changes report ENOTSUP, and readlink on a regular Vita file
reports EINVAL. The POSIX wrappers intentionally do not claim arbitrary Linux
kernel facilities exist on Vita.

The application uses `vita_frame_step` to detect callback failure before
presenting a frame. Its main error path stops JNI, waits for pending GPU work,
releases the library, closes logging and exits with failure status. This does
not yet prove hardware cleanup or successful real-game rendering.

`libc_file_abi` exercises production file wrappers against a real host filesystem:
Android open flags and creation mode, append/truncate, bounded shared Android
data paths, stream open/close, writev validation, descriptor duplication and
104-byte Bionic stat packing with a sparse file larger than 5 GiB.
`libc_vita_file_boundary` uses production wrappers with mocked Vita I/O and an
SDK-shaped descriptor pool. It checks pins, full-width metadata, native type
conversion, partial RNG failure, sync-write errors and cleanup. Descriptor
CLOEXEC state is separate from dup-shared status flags; ordinary dup clears it,
F_DUPFD_CLOEXEC sets it, and close/reuse clears stale state. Vita has no exec
operation here; these tests establish flag bookkeeping only.

After building the target, `python3 tests/check_arm_file.py` executes actual
linked file wrappers and SDK descriptor allocation/grab/drop/dup/close under
Unicorn. A separate C fixture validates Android-to-newlib-to-Sony flag conversion,
creation mode, stat buffer canaries, 64-bit size/seek/truncate, append/sync,
CLOEXEC and random reads. The fixture also calls actual linked mkdir and rename
wrappers, including directory enumeration. Requested mode, normalized same-file
names, missing sources, replacement, failed commit rollback, failed rollback,
committed cleanup failure and file/directory checks are covered. It proves
stdout survives RNG use and all tracked
descriptors/heap allocations are released. Only platform endpoints are mocked;
this does not establish actual filesystem permissions, entropy or game boot.

Remaining libc ABI work includes errno coverage audit, FILE/__sF
layouts, advisory locks and full mmap VM semantics. Vita stat
inode/device identities derive from paths, block counts are logical, and
link/ownership fields are approximations. Native stdio bypasses the custom RNG
reader and sync writer, so fdopen rejects those descriptors. Vita writev performs
sequential writes. mkdir forwards requested mode bits, but Vita has a different
user/system permission model and ignores Linux group bits.

Rename avoids newlib's delete-before-rename implementation. It checks source
existence, cross-device moves, directory ancestry, type compatibility and target
directory emptiness. A backend with native replacement uses one rename; Vita's
EEXIST backend stages the old destination in a sibling .vitaarc-rename-*.bak,
commits the source, then deletes the backup. Failed commits restore the old
destination. A failed rollback returns EIO and logs the retained backup; failed
cleanup after commit logs the backup and returns success. Existing backups are
never removed to claim a name; collision retries are bounded. Boundary tests
inject failures before staging, during staging/commit/rollback/cleanup and during
directory reads, preserve backup collisions, reject nonempty/type-mismatched
targets and run 16 two-writer replacements under the bridge's rename mutex.

This multi-operation fallback cannot provide POSIX atomic visibility to other
filesystem operations/processes or crash/power-loss recovery. Retained backups
require manual recovery/cleanup using the logged source, target and backup
paths. Actual Vita open-file replacement and storage errors remain unverified.
Primary implementation references:
[VitaSDK newlib](https://github.com/vitasdk/newlib/blob/master/newlib/libc/sys/vita/syscalls.c),
[Vita mode definitions](https://github.com/vitasdk/vita-headers/blob/master/include/psp2common/kernel/iofilemgr.h).

## Directory streams

Guest directory symbols now use `directory.c`, rather than exposing native
Vita DIR/dirent storage. The opaque stream owns its fd on successful
opendir/fdopendir and preserves caller ownership on failed fdopendir.
opendir uses the shared Android path and open-flag bridge. readdir and readdir64
return the same Bionic layout: 64-bit inode and next cookie, 16-bit record
length, 8-bit type and a 256-byte name at offset 19 (280-byte aligned structure).
readdir_r/readdir64_r copy into the caller's buffer, return errors directly and
preserve errno. EOF leaves errno unchanged. dirfd, closedir, rewinddir,
telldir and seekdir are bound as well.

`libc_directory_abi` uses a real host directory with files, a subdirectory,
symlink and 255-byte name. Native readdir is an independent inode/type oracle
(some WSL filesystems report different '..' inode values through stat).
It checks buffer canaries, cookies/rewind, failed seeks, fd ownership and zero,
OOM cleanup, CLOEXEC, separate streams and four shared-stream reentrant readers.
Host descriptors return to their initial count. After a target build, run:

```sh
python3 tests/check_arm_directory.py
```

The ARM test executes linked bridge code and native SDK fd lifetime, with Vita
directory/stat/mutex/allocator endpoints mocked. It verifies all eleven symbol
bindings, exact entry layout, names/types and path-derived inodes (including
dot entries), interrupted reads, unchanged EOF errno, malformed-name/position
bounds, reentrant output canaries and allocation failure cleanup. Vita seek
opens and replays a new native cursor before committing it. Injected open,
read, past-end and old-cursor-close failures preserve the old cursor/cookie
and release the temporary handle.

Vita cookies are ordinal replay positions, so mutations and multiple streams
over duplicated descriptors can invalidate them; those cases are not proven.
Inodes are path identities rather than stable filesystem IDs. The bridge does
not export scandir/getdents. The installed SDK allocates new fds from 3 and its
grab routine rejects zero, so the passing host fd-zero test does not establish
target fd-zero support. Native close failures during temporary-handle cleanup
and physical storage behavior still need target evidence.

References: [Bionic directory ABI](https://android.googlesource.com/platform/bionic/+/5d0ad38c72951ef180742efb27933ecd6e9d9593/libc/include/dirent.h),
[VitaSDK directory implementation](https://github.com/vitasdk/newlib/blob/master/newlib/libc/sys/vita/dirent.c).

## Memory mappings

`memory.c` replaces heap-pointer mmap/munmap and the NULL-returning mremap stub.
Private anonymous/file mappings round to 4096-byte pages, record live ranges,
and retain an SDK descriptor duplicate so closing the caller's descriptor does
not invalidate file-backed growth. Positional reads handle short reads/EINTR
without changing the open file's current offset. mmap2 multiplies its page
offset in 64-bit arithmetic, including offsets beyond 4 GiB on ARM32.
munmap validates alignment/overflow and only retires tracked pages; unknown
ranges succeed without freeing arbitrary caller pointers. mremap validates the
complete old range, shrinks in place, grows into retired tail pages or moves
with MAYMOVE, preserving private edits and the old mapping on failure.

`libc_memory_mappings` exercises real host files/anonymous storage, read-only
host page protection, >5 GiB sparse offsets, short reads/interruption, injected
metadata/storage/dup/read failures, partial ranges and four concurrent workers.
Backing-region and file-descriptor counts return to their initial values.
After a Vita target build, run:

```sh
python3 tests/check_arm_mappings.py
```

This runs the actual linked target mapping code and SDK descriptor allocation,
duplication and close routines. It checks read-only USER_MAIN_R versus USER_RW,
zero filling of poisoned kernel blocks, partial ranges, shrink/tail growth,
moved growth, positional reads, preserved offsets, >4 GiB mmap2, and cleanup
after allocation/get-base/unrestricted-copy/read failures. Only platform
memory/file/mutex endpoints are mocked; it does not prove kernel protection or
physical storage behavior.

This is eager private snapshot storage. Shared mappings, fixed placement,
PROT_NONE, execution, write-only protection and fixed mremap report ENOTSUP.
Partial holes retire logical pages while retaining their original backing
memblock until its final live page is unmapped; holes are not individually
protected or returned to the kernel. Linux shared coherence, lazy faults,
truncation/beyond-EOF SIGBUS and physical Vita protection remain open.
Guest error-number adapters are described below; native internal helpers keep
native codes while resolved guest entry points report Android codes.

## Poll readiness

`poll.c` replaces the old implementation that ignored every descriptor and
returned timeout. The 8-byte Android pollfd is converted explicitly. Arrays
are bounded to the target SDK's 256-fd capacity. Negative fds are ignored and
invalid fds report POLLNVAL regardless of requested events. On Vita,
files/directories are immediately ready for requested normal I/O, and the
read-only RNG bridge is readable. Socket UIDs are pinned and deduplicated for
epoll registration, with unioned interests; returned events are then expanded
back to each pollfd entry and counted per entry. Error/HUP are reported without
requiring input/output interest. Ready files/NVAL force a nonblocking socket
check, so the call does not wait despite an already-ready entry.

Android read/write-normal masks map to Sony's distinct epoll bits. Finite
millisecond deadlines use 64-bit monotonic microseconds and bounded signed
microsecond waits. Early empty returns continue to the deadline; negative
timeouts wait indefinitely until an event/error. Empty arrays use chunked
nanosleep, including indefinite sleep interrupted with EINTR. The existing
guest errno adapter translates native failures and preserves saved errno when
no error was reported.

`libc_poll` uses real host socketpairs, pipes and regular files. It checks
duplicate entries, aliases, delayed input, HUP/NVAL, timeout, ignored entries,
array validation and signal-interrupted infinite sleep under ASan/UBSan and a
separate TSan build. After building VitaArc, also run:

```sh
python3 tests/check_arm_poll.py
```

The ARM test executes actual linked poll/errno code and SDK descriptor pins.
Only network epoll, time/delay and allocator endpoints are mocked. It verifies
native mask translation, duplicate UIDs, error/HUP with zero requested events,
immediate files/RNG/NVAL, ignored/maximum arrays, zero/finite/infinite waits,
early return handling and INT_MAX-millisecond waits without real delays. Socket
waiting splits the maximum timeout into 1000 signed-microsecond chunks; empty
sleep verifies the full 64-bit duration. Allocation/create/control/wait errors,
malformed event counts/indices, output canaries, Android errno and pin/heap
cleanup are checked. A modeled concurrent close drops the open-fd reference;
the actual SDK final drop closes the socket only after epoll is destroyed.

Native message-pipe/TTY readiness and socket priority/band/RDHUP requests report
ENOTSUP. Host pipe success does not establish Vita message-pipe behavior.
Network initialization, actual traffic, physical scheduling/signals and native
fd-zero limitations still need target evidence. Failed epoll destruction is
reported and all bridge metadata/pins are released, but the native kernel
object's state is unknown; the test backend explicitly cleans up such retained
objects after verifying the failure. This is not full-game boot evidence.

References: [VitaSDK poll backend](https://github.com/vitasdk/newlib/blob/master/newlib/libc/sys/vita/poll.c),
[Vita epoll API](https://docs.vitasdk.org/group__SceNetUser.html).

## pthread_once control layout

Android ARM32 pthread_once_t occupies 4 bytes, while this installed SDK's
pthread_once_t occupies 16. `once.c` owns the guest implementation instead of
forwarding its pointer to the SDK. Guest states 0/1/2 are atomically published;
the completed state uses release/acquire ordering. An internal mutex, condition
and temporary per-control registry serialize initializers and waiting callers.
The initializer runs outside the registry lock, allowing nested initialization
of other controls. Completed entries are freed after their last active user.
Thread-exit/cancellation cleanup resets an interrupted initializer to state 0
and wakes waiters so one can retry. Canceled waiters release their registry
reference and unlock the native mutex. Incoming errno is preserved except for
the initializing thread's own callback changes. The import adapter translates
direct helper errors without discarding callback errno.

`libc_once_abi` uses four-byte canaries, injected OOM, reinitialized storage,
nested controls and eight contending callers. It verifies one initialization,
published payloads, per-thread errno, waiter cancellation, initializer exit and
initializer cancellation with successful waiting-thread retry. Wrapped
allocation/free counters return to zero. ASan/UBSan and TSan builds pass.
The default cancellation case uses an explicit pthread_testcancel point in
both builds. An additional `libc_once_blocking_cancel` test cancels an
initializer inside nanosleep and passes under ASan/UBSan. It is not registered
in the TSan build: the installed libtsan reports mutex-protected cleanup access
as unsynchronized when cancellation unwinds its nanosleep BlockingCall scope.
The same report occurs with system pthreads alone, without any once bridge:

```sh
cc -g -pthread -fsanitize=thread -fno-pie -no-pie \
  tests/repros/tsan_cancel_nanosleep.c -o build/once-cancel-repro
build/once-cancel-repro
```

This reproducer deliberately reports the tool issue; it is not a passing test.
Inspection of [libtsan's blocking-call interception](https://github.com/gcc-mirror/gcc/blob/releases/gcc-14/libsanitizer/tsan/tsan_interceptors_posix.cpp)
shows the interceptor disables nested interceptors while inside nanosleep;
the cancellation cleanup runs before that scope exits. This explains the
observed missing mutex on the canceled thread's warning stack. No race
suppression or production-code workaround was added. The passing TSan claim
covers actual initializer cancellation at pthread_testcancel and waiter
cancellation at pthread_cond_wait; it excludes cancellation inside nanosleep.
After building the target, run:

```sh
python3 tests/check_arm_once.py
```

Separate ARM and Thumb callers check exact SDK/guest sizes, adjacent canaries,
OOM/retry, nested initializers, errno and completed/reused controls through the
actual symbol resolver. Forwarding to native pthread_once is forbidden in this
test. Real SDK cleanup-stack push/pop execute; TLS/mutex/allocation endpoints
are modeled. No cleanup node or per-control allocation remains afterward.
This single-threaded ARM fixture does not establish actual Vita contention,
cancellation or scheduling. C++ initializer exception unwinding, fork and
asynchronous cancellation are unproven. Mutex/condition ABI coverage is described
below; thread attributes and opaque handles are checked separately below.

Reference: [Bionic pthread types and initializers](https://android.googlesource.com/platform/bionic/+/68a2f12304e960f2045f3819c48311e7bf4d3126/libc/include/pthread.h).

## Guest mutexes and condition variables

`sync.c` keeps the 4-byte Bionic controls and attributes separate from native
pthread objects. Static normal/recursive/error-check mutexes (0/0x4000/0x8000)
initialize lazily under a registry lock. Attribute type numbers are explicitly
converted to native constants. Process-shared objects report ENOTSUP. Publication
occurs only after native initialization succeeds. Active calls and acquisitions
pin their metadata; EBUSY or native destroy errors preserve the guest control
and native object for retry. Condition waits release the registry guard before
blocking and install cancellation cleanup; the caller's cleanup can then unlock
the reacquired mutex. Range cleanup first rejects active objects before destroying
any idle records.

`libc_sync_abi` uses actual host pthreads for eight-thread lazy initialization,
recursive/error-check behavior, timed locking, condition broadcast and canceled
waiters. Canaries, allocation failures, errno preservation, failed destruction
and range cleanup are checked under ASan/UBSan and TSan. Metadata returns to zero.
After building the target, run:

```sh
python3 tests/check_arm_sync.py
```

Independent ARM/Thumb callers execute the production symbol resolver, errno
adapters and registry. Actual SDK mutex attribute helpers and cleanup-stack
operations execute; native mutex/condition scheduling endpoints are modeled.
Tests cover 4-byte controls/attributes, adjacent canaries, native attribute OOM,
initialization/destruction retry and static types. A separately loaded DSO proves
that held synchronization defers unload before fini, idle sidecars release before
unmapping, and a native cleanup failure retains mappings. Such a module rejects
new handles/reinitialization and symbol lookup; retrying unload runs fini only once. A cleanup failure
can leave partially destroyed idle records, so retained mappings do not mean the
module can resume operation. Concurrent load/unload is still not established.

The installed SDK's disassembly shows that condattr_setclock stores a clock but
cond_init/timedwait do not use it: timedwait reaches sem_timedwait/pte_relmillisecs,
which reads realtime via ftime. On Vita, the bridge records the guest monotonic
flag and converts each wait into at most 100 ms of realtime, checking/rebasing
against process time after timeouts. ARM tests change realtime between slices and
verify the 250 ms monotonic deadline. There remains a wall-clock sampling race
between conversion and the native relative-time calculation; strict immunity to
clock changes during that interval and actual hardware scheduling are unproven.
Native realtime waits retain the SDK's millisecond granularity and range limits.
No claim is made for asynchronous cancellation, fork or robust/shared mutexes.

References: [Bionic mutex flags/attributes](https://android.googlesource.com/platform/bionic/+/532d6f0/libc/bionic/pthread.c),
[Bionic condition clock flags](https://android.googlesource.com/platform/bionic/+/1e52871/libc/bionic/pthread_cond.cpp).

## Guest thread attributes and handles

`thread_attributes.c` implements the 24-byte Android ARM attribute structure,
with fixed 32-bit stack address/sizes and priority fields, plus sixteen typed
attribute exports. Initialization supplies the Bionic 1 MiB stack/4 KiB guard
defaults; destroy poisons exactly 24 bytes. Getters/setters validate supported
fields and retain outputs/state on invalid input. Scope and detach constants are
interpreted as Android values, independently of native values. All guest entry
points translate direct errors and preserve errno. They do not overwrite native
pthread declarations used by internal bridge code.

`_pthread_create` converts attributes into a temporary native object and destroys
it after success or failure. It publishes the native ID only after successful
creation, so native failure writes cannot corrupt the caller's output. On Vita,
NULL attributes now select a 1 MiB stack instead of the SDK's 32 KiB default;
this increases per-worker stack reservation and still has no enforced guard.
Explicit nonzero guards, custom stack addresses and realtime scheduling requests
report ENOTSUP before creation: the installed SDK rejects guards and its create
path ignores stack addresses. Therefore a freshly initialized Bionic attribute
with its default nonzero guard is currently unsupported on Vita unless the
caller explicitly disables that guard. This is a remaining compatibility gap,
not complete Android stack protection. A Bionic-valid 8 KiB stack also fails the
native SDK's 32 KiB minimum. NULL-attribute host tests retain host defaults.
No realtime-priority mapping or newer inherit-scheduling flag support is claimed.

`libc_thread_attributes` checks layout/scalar canaries, all sixteen exports,
invalid-state preservation, errno and failure cleanup. Actual host joinable and
detached workers verify IDs, return values, per-thread keys and TLS destructors.
Fault injection covers native attribute init/setter/create failures, including a
native create that overwrites its temporary ID output before failing. ASan/UBSan
and TSan pass. After building the target, run:

```sh
python3 tests/check_arm_thread_attributes.py
```

Independent ARM/Thumb callers execute production resolver/errno/conversion code
and actual SDK attribute allocation, getters, setters and destruction. Only
native create/join/TLS endpoints are modeled; a separate C endpoint checks the
converted attribute through SDK getters. Tests prove opposite native detach
constants, NULL-default stack size, SDK minimum-stack failure, OOM/create rollback,
unsupported requests, exact 24-byte stores and 4-byte ID/key canaries. The SDK's
scope getter returns ENOSYS without populating output; the guest getter reports
the supported system scope directly. SDK pthread_t/key handles are four bytes,
so forwarding opaque values is valid for this target rather than requiring an
eight-byte ID registry. Compile-time assertions enforce that prerequisite.
Guest pthread_equal now compares opaque values like Bionic, including zero IDs
and equal unmapped sentinels, without dereferencing them. Actual game run 023
found that SDK pthread_equal(0, 0) faults; independent ARM/Thumb callers and host
workers verify the corrected equality. Physical
thread scheduling, invalid/expired-handle behavior, guest worker shutdown before
unload and target TLS destruction remain outside this instruction fixture.

References: [Bionic attribute layout](https://android.googlesource.com/platform/bionic/+/249a7ae70e3e051d55696d5028f2c3600ab6a6f0/libc/include/bits/pthread_types.h),
[Bionic attribute defaults/setters](https://android.googlesource.com/platform/bionic/+/2a1bb4e64677b9abbc17173c79768ed494565047/libc/bionic/pthread_attr.cpp).

## Guest error numbers

`guest_errno.c` and its manifest supply typed adapters for 146 symbol bindings, including file,
time, mapping, directory, stdio, encoding, socket and pthread functions. The
normal helper API stays native for JNI/preferences and other C callers. Only
the guest symbol table selects the adapters. They use a zero native errno
sentinel, translate fresh failures and restore the exact saved guest value when
no error was reported. This also avoids numeric collisions between unknown
guest values and unrelated native errors. `readdir_r` and pthread APIs convert
their direct error result while preserving errno. strerror/strerror_r/perror
translate incoming Android codes; ENOTSUP/EOPNOTSUPP aliases map to Android 95.
Unknown native codes become Android EIO instead of a different Linux condition.

`libc_guest_errno` checks common error classes, repeated failures, unknown saved
values, direct-return code behavior, strerror_r, mixed variadic formatting/scan
and per-thread isolation under ASan/UBSan and a separate TSan build. The actual
native/newlib numbers differ on ARM, so also run after building VitaArc:

```sh
python3 tests/check_arm_errno.py
```

Its independent oracle reads Linux UAPI and installed SDK headers, not the
production conversion table, and verifies all 87 supported named mappings and
aliases. Calls through the real symbol resolver check native ENOSYS=88 becoming
38, ENOTSUP=134 becoming 95, ENAMETOOLONG=91 becoming 36, timeout/deadlock
returns, repeated/saved errors, EOF, unknown-native fallback, strerror_r,
creation-mode varargs and a preserved 64-bit seek return. Actual newlib
snprintf/sscanf execute integer and double varargs. The CRT reent layout is
initialized using this SDK's macro; native allocator endpoints are modeled,
and reentrant formatting caches are reclaimed. File/kernel APIs are mocked;
socket/pthread failure injection proves adapter behavior, not real networking
or scheduling. The existing ARM file/map/directory tests intentionally continue
to call native helpers while checking that imports select the guest adapters.

This does not isolate every native/guest execution context: __errno remains the
native per-thread CRT slot, and other callbacks or unwrapped exports can still
write native values. Remaining coverage needs audit. Existing FILE/__sF,
socket structure/EAI and Android pthread initializer/attribute ABIs are
separate gaps; errno conversion does not validate or repair those layouts.

References: [Linux errno base numbers](https://github.com/torvalds/linux/blob/master/include/uapi/asm-generic/errno-base.h),
[Linux extended errno numbers](https://github.com/torvalds/linux/blob/master/include/uapi/asm-generic/errno.h),
[Bionic errno aliases](https://android.googlesource.com/platform/bionic/+/main/libc/include/errno.h).

References: [Bionic mmap implementation](https://android.googlesource.com/platform/bionic/+/249a7ae70e3e051d55696d5028f2c3600ab6a6f0/libc/bionic/mmap.cpp),
[Linux mmap/munmap](https://man7.org/linux/man-pages/man2/mmap.2.html),
[Linux mremap](https://man7.org/linux/man-pages/man2/mremap.2.html).

## BGM positions and seeking

`audio_position_and_seek` runs the production MP3 frame parser, decoder feeder,
resampler, mixer and provider. Only Vita decoding/audio/thread platform APIs
are mocked. The deterministic decoder emits PCM indexed by each synthetic MP3
frame; these fixtures do not prove real compressed MP3 decoding or audio output.

Five sample rates (48 kHz, 44.1 kHz, 32 kHz, 22.05 kHz and 11.025 kHz), mono/stereo,
ID3 and variable frame bitrate are covered. Seeked PCM matches sequential
resampled output, including forward/backward seek, frame boundaries, end
clamping, decode failure recovery and loop wrap. The real mixer tests millisecond
queries/seeks, pause, suspension, delayed playback, large delays, channel 2,
per-channel volume and preserving the old stream on failed replacement.
Provider/module/library lifetimes and initialization failures are also covered.

The local 1.6.1c ARM binary passes `position_ms` before `channel` in vtable slot 8;
the earlier bridge had these reversed. Slot 9 receives volume then channel.
The manager converts delay seconds to milliseconds and uses channel 2 for
special BGM, so the backend supports channels 0 through 2. The position is the
48 kHz rendered stream cursor converted to milliseconds; it does not compensate
for the queued hardware audio block (1024 frames, about 21 ms), decoder priming
or output latency. Those require real-device measurement.

The standalone target probe uses production MP3, mixer, logging and output-worker
code against the real SDK endpoints in Vita3K, without JNI or graphics:

```powershell
wsl -d Ubuntu -- python3 tests/build_vita_audio_probe.py
./tests/run_vita_audio_probe.ps1 -UseHleKernel
```

It requires the isolated `build/vita3k/runtime` VitaArc installation with the
local 1.6.1c `package/assets/audio/calibrate.mp3`. The normal VPK/eboot is not
replaced; `audio-probe.self` runs as a separate entry point in that installation.
On official Vita3K 4220 with its HLE kernel, 48000 output frames contained 46590
nonzero frames, with position 48000. Seek/read/reset passed. A real output port
and worker then ran muted: position advanced to 64 ms, paused at 64, sought to
500 and resumed to 542 before worker/decoder/port teardown and exit zero.
No decoder, thread or audio API is intercepted. This proves the tested emulator
path, not audible playback, native Vita decoder priming or hardware latency.

With the installed firmware bootimage, module initialization instead fails at
missing `ksceSblACMgrIsSystemProgram`/`ksceSblACMgrIsNonGameProgram` imports,
before MP3 decoding. `-UseHleKernel` temporarily moves only this project's
isolated `os0/kd/bootimage.skprx` aside, restores it in `finally`, and verifies its
SHA-256. The default firmware configuration is preserved. A normal probe run
without that switch reproduces the firmware path. The runner preserves any live
emulator session and reports failure if the expected success markers are absent.

## OpenGL object and dynamic lookup tests

`opengl_objects_and_procs` runs production buffer/VAO wrappers, the canonical
symbol binary search and `eglGetProcAddress`. Platform GL methods are mocked;
omitted shader/float callees use distinguishable sentinels generated from the
actual `symbol.def`. Tests check every supported GL/EGL canonical lookup and
representative extension aliases, shader/float adapter preservation, unknown
names and calling buffer wrappers through queried function pointers.

Guest buffer names never become vitaGL raw pointer handles. Tests distinguish
reserved names from first-bound objects, allow first binding an unused name,
ignore duplicate/unknown deletion, translate integer/float/boolean/attribute
binding queries, and cover allocation failure/rollback. Native allocation waits
until first binding. A deleted name can be reused while an unbound VAO retains
the older object. Tests assert its native storage survives until the last
reference is replaced or its VAO is deleted. Current VAO attachments are cleared
on buffer deletion, preserving attribute formats/offsets, and teardown releases
all bridge objects. A rejected native attribute update preserves the old object.

The bridge assumes the application's one render context and owner thread.
These tests do not establish multi-context behavior or correct GPU rendering.
Unknown dynamic procedures return NULL; only supported canonical adapters and
explicit aliases are exposed, since raw vitaGL float functions use another ABI.

## Production ARM bridge execution

After building the target ELF and installing Unicorn as described above:

```sh
python3 tests/check_arm_bridges.py
python3 tests/check_arm_startup.py
python3 tests/check_arm_kuser.py
# Optional: use the already available local 1.6.1c game binary.
python3 tests/check_arm_bridges.py \
  --game build/audit-so/lib/armeabi-v7a/libcocos2dcpp.so
```

The script maps the actual linked `build/VitaArc` segments in Unicorn and
executes production string lookup/search code. It checks all 104 supported
GL/EGL names, aliases and five float adapters, including `glUniform4f`'s fifth
base-PCS argument on the stack. Native GL entries are intercepted, where their
hard-float VFP arguments and integer location are asserted.

`check_arm_startup.py` executes the linked production `main` with platform and
subsystem entry points intercepted. Thirty-two scenarios verify both vitaGL
resolution results, primary/fallback/missing/offline shader compiler paths, startup
failures and ready/paused/failed frames before ordered shutdown. Missing compiler
initialization without an offline pack stops before ELF loading. The other failures stop at their stage.
The vitaGL return value indicates resolution fallback, not initialization
success. This test models that dependency result; it does not initialize a GPU
or execute the loader/JNI implementations.

`check_arm_kuser.py` executes the linked ARMv7 comparison/exclusive-store and
memory-barrier replacements for Linux's fixed kuser entries. It checks result
and carry agreement, preserved registers and forced exclusive-store retry and
interference. Optional `--game build/audit-so/lib/armeabi-v7a/libcocos2dcpp.so`
checks the complete 1.6.1c libgcc profile before replacement of 43 comparison
and five barrier literals, actual game fetch-add, repeated preparation, unknown
profile rejection and loader cleanup after a partial patch failure. The whole
profile is verified; other literal layouts are rejected before constructors.
These instruction tests model contention and do not prove hardware scheduling
or coherence. ABI details follow the
[Linux kuser specification](https://www.kernel.org/doc/html/latest/arch/arm/kernel_user_helpers.html).

Startup also calls the optional SDKBOX JNI initializer after the Cocos context
is established and before native renderer startup, passing the SDKBox class,
activity and its actual class loader. The local APK declares
`nativeInit(Ljava/lang/Object;Ljava/lang/ClassLoader;)V`. Direct/registered
exports, libraries without SDKBOX and thrown initialization errors are covered
by the host startup fixture. `SdkboxLog.setDefaultDebugLevel(I)V` stores the
argument in `_defaultLevel:I`, matching its three-instruction APK method;
the startup fixture reads the value back through JNI.

With `--game`, the actual local SO's `AudioManager::getBGMPosition`,
`debugSetBGMPosition(int)`, `internal_playBGMForChannel` and `playSpecialBGM`
execute through the linked production audio vtable/provider. Backend endpoints
assert state, position, loop, channel, volume and delay. The playback methods'
unrelated std::string and Node scheduling calls are intercepted, and the fixture
supplies an external stack guard. Their actual ARM code converts a 0.375-second
delay to 375 ms, sets per-channel volume and selects channel 2 for special BGM.
The slot 9 volume ABI is also checked with a separate 0.25-volume call.

This verifies actual game call sites and bridge argument order; it does not
initialize the game, execute the production ELF loader, emulate Vita system calls
or prove real decoder/GPU/input behavior. The optional 1.6.1c SO is local and is
not included in the test sources.

## Registered native method dispatch

`jni_registered_native_dispatch` invokes the production runtime and host native
dispatcher. All eight primitive return types plus object/void are exercised
through ordinary, V and A static/instance/nonvirtual calls. Mixed-width arguments
include signed byte/short, unsigned char, long, float, double, strings and arrays.
Generated signatures exercise 255 integer arguments and 127 long arguments plus
one int, while rejecting signatures exceeding the Java slot limit. The limit
follows [JVMS 4.3.3](https://docs.oracle.com/javase/specs/jvms/se17/html/jvms-4.html#jvms-4.3.3).

Registration stores one declaration per class/name/signature independently of
static/instance method IDs, so IDs created before registration also work. Invalid
batches leave existing function bindings unchanged. Unregister retains the
declaration and subsequent calls raise UnsatisfiedLinkError. Known superclass
declarations, virtual native overrides, explicit nonvirtual selection and static
declaring classes are tested. Class ancestry is still the compatibility model,
and declarations are not validated against a DEX method schema.
Method IDs obtained through a child before its parent registers a native become
stable declaration aliases. Later child overrides/static hiding get their own
IDs, while cached parent aliases retain nonvirtual/static selection. This late
registration path is covered on the host and in the linked ARM VM fixture.

Each native call gets a protected automatic local frame and fresh local handles
for its receiver/class and reference arguments. VM operand roots keep arguments
alive even if native code deletes their local handles. Object results are
resolved before all callee frames are removed and promoted into the caller's
frame; pending Java exceptions survive cleanup. The runtime lock is released
during execution, while the call remains active so detach/VM destruction cannot
invalidate its thread state. A callback can reenter JNI, collect temporary
objects, register/unregister methods and wait for another thread's JNI work.

Tests prove stable counts for 1000 object-return calls and for 1000 allocations
inside one native body, leaked explicit-frame cleanup, nested native calls,
invalid object arguments, exception propagation and concurrent unregister.
The worker/GC test removes both caller and callee local references, leaving only
the VM operand root. A separate native pthread_exit case proves TLS destruction
cleans active native frames, operand roots and owned monitors.

The host backend uses libffi's native ABI. Vita has no libffi dependency: its
packer follows the [AAPCS32 base PCS](https://github.com/ARM-software/abi-aa/blob/main/aapcs32/aapcs32.rst),
using r0-r3, eight-byte-aligned stack arguments, r0-r1 scalar results and BLX for
ARM/Thumb callbacks. Callbacks use standard JNI signatures; Java bytecode and
Android runtime-specific critical-native conventions are outside this VM model.

After a target build, run the additional ARM fixtures with Unicorn installed:

```sh
python3 tests/check_arm_native.py
python3 tests/check_arm_native_vm.py
```

The first compiles production packing/trampoline code and independent ARM/Thumb
callees. It checks all return types, mixed widths, the register/stack alignment
gap and maximum slot counts. The second maps the actual linked `build/VitaArc`
and a separate C caller, then executes its production JNI table, registration,
thread state, native dispatcher, local frames and collector. Only allocation,
pthread/TLS/Newlib thread state, logs and Vita memory endpoints are stubbed.
It verifies scalar and mixed A/V/variadic calls, escaped object results, stable
collection counts, exceptions, maximum signatures and DestroyJavaVM teardown.
All tracked mock heap allocations are freed, locks are balanced and TLS is clear.
This is single-threaded ARM runtime evidence; real Vita scheduling, ELF loading,
graphics/audio/input and complete game boot remain unverified.

## Production ARM ELF loader

After a VitaSDK target build and the Unicorn installation above:

```sh
python3 tests/check_arm_loader.py
# Optional, uses a local 1.6.1c SO and does not package it into tests:
python3 tests/check_arm_loader.py --game build/audit-so/lib/armeabi-v7a/libcocos2dcpp.so
```

This test executes the actual loader in `build/VitaArc`. Python maps that main
executable, then intercepts Vita file/memblock/cache and kubridge endpoints.
It does not map or relocate the test shared libraries itself: production
loader_load_library/load_sections perform those operations. Allocated memory
starts poisoned, RX/RW permissions are modeled, and every native block, file
handle and heap allocation is tracked through cleanup.

Separate shared libraries compiled from `test_loader_fixture.c` cover BSS
clearing, R_ARM_RELATIVE/ABS32/GLOB_DAT/JUMP_SLOT, internal/external/weak symbols,
SHN_ABS exports and both internal/external references to absolute symbols.
Relocated code calls another loaded library and a canonical libc import.
Constructors run in priority order once, finalizers run in reverse order after
the last handle closes, and duplicate loads retain the original handle.
Bare filenames and rejected missing/overlong filenames are checked as well.
The ARM ELF reference is [AAELF32](https://github.com/ARM-software/abi-aa/blob/main/aaelf32/aaelf32.rst).

Injected open/seek/large-size/short-read/memblock/base/memset/copy/relocation and
missing-import failures leave no tracked resources. Nineteen malformed ELF
variants cover endian/version, overflowing header tables, entry sizes, file and
virtual segment ranges, section/symbol strings and links, relocation targets
and symbols, init-array size and truncation. They fail before allocating any
loaded segment. These are targeted regression cases, not exhaustive ELF fuzzing.

For the optional real 1.6.1c SO, the production loader maps/relocates the image
and finds JNI_OnLoad. An independent oracle recomputes values from the original
file addends, matching all 48,281 applied relocations and 329 distinct imports.
It then unloads the SO without calling its constructors or JNI_OnLoad body.
Resolving an import proves its address, not the full behavior of that service.

This does not test actual kernel/kubridge installation, cache behavior, real
memory protection, Vita filesystem, GPU/decoder/input or successful game boot.
The loader still relies on section headers and REL relocations; these fixtures
do not establish automatic DT_NEEDED dependency ownership, concurrent loading
or long-term address-space reuse after repeated unloads.

## C++ termination and actual game bootstrap

`libc_cxx_finalizers` exercises `libc/cxx.c`: 1024 callbacks, per-DSO and
global reverse order, repeated finalization, failed allocation, registration
and nested finalization from callbacks, callback worker/join, concurrent
exactly-once invocation, module-range selection and guest exit in a child
process. ASan/UBSan and the separate TSan build pass all 12 host targets.
Callbacks run after removal from the registry and outside its mutex.
The production loader also drains module-owned callbacks before unmapping;
an ARM fixture verifies 128 callbacks whose code lives in another loaded DSO,
including modules without an explicit __cxa_finalize in their fini array.
This removes the installed newlib's 32-entry limit. Reference:
[C++ termination ABI](https://itanium-cxx-abi.github.io/cxx-abi/abi.html#dso-dtor).

```sh
python3 tests/check_arm_logging.py
# Supported 1.6.1c constructor/JNI/teardown path requires zero retained heap:
python3 tests/check_arm_game_bootstrap.py --game build/audit-so/lib/armeabi-v7a/libcocos2dcpp.so
```

Logging tests execute actual linked formatting with Vita console/file endpoints
intercepted. They verify fatal output with no active optional sink, inactive
verbose suppression, long tags/bodies and bounded raw writes. Info and higher
messages mirror to sceClibPrintf, enabling direct Vita3K diagnostics. They also
verify truncating file creation, acceptance of handle zero, and the final stop
message before closing. File logging is the default; optional UDP builds must
explicitly define both LOG_OVERUDP and UDP_HOST.

The bootstrap diagnostic executes the real 1.6.1c constructors and JNI_OnLoad,
registering 896 C++ callbacks and receiving JNI 1.4 without exception. A small
separate C helper initializes mock per-thread CRT state with the installed SDK's
actual _REENT_INIT_PTR and calls the JNI tables. Native pthread identity/key
deletion are modeled. No game constructor, allocator, destructor or JNI service
is replaced. Unload/destroy clears all VM objects/references, native memblocks,
file handles and C++ registry entries. The final heap assertion remains strict:
The supported 1.6.1c path now passes with zero allocations/bytes retained and
writes build/arm-game-bootstrap/report.json. Unsupported-profile/type injections
still fail that original whole-heap assertion. Bounded samples in retained.json
identify any retained concrete service types.
Saved stack words are conservatively symbolized as candidates; they are not an
unwound call stack. At most 128 retained objects are included in the samples.
It is outside the passing host CTest suite and is not successful game boot.

The production loader supports a before_unload owner callback on its final
reference, before fini arrays and static C++ finalization, plus after_finalize
while code/exports are still mapped. Preparation failure preserves the live
module/reference and callback for retry; it must precede destructive work. Both
successful hooks are consumed exactly once, including a later sync-destroy
failure/retry. The Cocos adapter
uses ObjectFactory::destroyInstance and the exported AppDelegate deleting
destructor when its concrete vtable matches. The inspected supported
AppDelegate prefix places its injected AudioProvider after the Application
vptr; only an exact Vita bridge provider match is deleted. These changes remove
all 90 constructor-retained allocations plus the application and bridge audio
provider. Independent ARM DSOs verify final-handle behavior, pre-fini order,
actual provider deletion and foreign application/provider preservation.

The 1.6.1c bootstrap creates complete four-byte AnalyticsManagerAndroid and
AchievementManagerMobile objects and passes them into AppDelegate's +12/+20
slots. These concrete objects contain only a vptr; their methods do not retain
per-object resources, and no destructor entry is emitted. Cleanup checks full
code fingerprints and sizes of both the 360-byte bootstrap and 48-byte storing
constructor, then concrete vptrs. It captures the owned allocations before
AppDelegate deletion and uses the game's named operator delete after module
fini/C++ callbacks finish. It does not guess virtual destructor slots or apply
this layout to another version/type. The small cleanup record also releases.
The platform object's two COW strings now destroy through the game's own string
destructor before the object deallocates. Purchase containers/callbacks now use
verified game destructors as described below; the whole-heap gate has not changed.

PlatformUtilsAndroid's verified 20-byte layout contains strings at +4/+8, a
data-check bool at +12 and billing integer at +16. Its concrete vptr must match.
A separate full fingerprint/size check covers the 132-byte Java setDeviceId
function. The bridge decodes its Thumb BL at +0x42 to locate the actual game
string destructor, then verifies that callee's 88 bytes as well. It does not
substitute the native SDK's string ABI, empty representation or allocator.
After module finalizers, members destroy in reverse order (+8 then +4), followed
by named game operator delete. A mismatching caller/callee preserves the platform
object while other supported services can still release. Thumb BL encoding is
described by the [LLVM implementation discussion](https://lists.llvm.org/pipermail/llvm-commits/Week-of-Mon-20101129/113074.html).

The diagnostic observes actual service frees and requires an empty C++ exit
registry before the service-cleanup phase. Optional --cleanup-case values
prepare-oom, foreign-analytics, foreign-achievement, foreign-platform,
bootstrap-mismatch, constructor-mismatch, string-caller-mismatch and
string-destructor-mismatch exercise retry and rejection. The callee-mismatch
case changes inert padding so other static string destructors can still execute.
For example:

```sh
python3 tests/check_arm_game_bootstrap.py --game build/audit-so/lib/armeabi-v7a/libcocos2dcpp.so --cleanup-case prepare-oom
```

OOM must preserve the application, callbacks, allocations and reusable library
until retry; all four owned services then release. Foreign types remain for their
owner, and a modified bootstrap/constructor rejects this service layout. Reports
for fault cases go in named subdirectories of build/arm-game-bootstrap. Every
case still ends at the original strict whole-heap assertion: normal/retried
cleanup leaves 0/0; foreign analytics/achievement leaves 1/4; foreign platform
leaves 1/20; a foreign purchase leaves 1/96; string-profile mismatch preserves
platform/purchase (2/116); and bootstrap/constructor mismatch leaves 4/124.
The rejection cases fail the original whole-heap assertion. Neither a passing
instruction fixture nor its heap count establishes complete game startup.

Two further --cleanup-case values, platform-nonempty and platform-shared, execute
actual game string construction/copying, with GNU objdump supplying an independent
call-target oracle. The former installs two independent nonempty allocations;
the latter uses two platform fields plus a third COW owner released by an added
C++ finalization callback. Actual game destructors must release both independent
reps or the shared rep exactly once, only after the other owner finalized.
Reports distinguish the extra fixture registration from the game's 896 callbacks.
Both variants now pass the zero-retained-heap gate after purchase cleanup.

Purchase cleanup verifies its concrete primary/listener vptrs and 96-byte layout,
four member-using functions, and the actual vector<string>/_Function_base
destructors. It destroys strings at +88/+84/+80, vectors +68/+56/+44, the function
at +24, then strings +20/+8/+4, and finally deallocates with game operator delete.
Base-class dispatch is restored before member/captured-target teardown. An
unknown member profile preserves the object and keeps the original heap gate
failing, rather than assuming empty containers. No native C++ string/vector ABI
is substituted for the game's.

Before any destructive work, the adapter inspects the existing IAP cache through
two verified PIC references in getInstance. It never invokes that factory during
cleanup. An enabled wrapper's listener is removed only when it equals this
purchase object's +92 interface. Foreign listeners remain unchanged. Unknown
wrapper/getter profiles defer the entire unload with app/refs/finalizers intact;
test repair then retries. Listener preparation remains non-destructive on OOM.

Additional --cleanup-case values exercise this path:

- purchase-members / purchase-members-shared: six strings, three nonempty
  vectors with unconstructed spare capacity, and an owned function target.
  Actual game move/copy/destructor routines release 10/5 allocations. The fixture
  function manager verifies clone/destruction counts and base dispatch; the
  business callback must never execute during destruction.
- purchase-listener / purchase-foreign-listener / purchase-listener-oom: actual
  enabled wrapper constructor/getter/listener/destructor code, matching detach,
  foreign preservation and intact OOM retry. The fixture explicitly owns and
  finalizes its SDK wrapper; this does not prove production singleton teardown.
- purchase-unknown-wrapper / purchase-iap-mismatch: unload must defer untouched,
  then succeed after repairing the wrapper/profile.
- purchase-schema-mismatch / purchase-vector-mismatch /
  purchase-function-mismatch: metadata fault injection leaves the unsupported
  purchase object (1/96); actual game destructor instructions remain unchanged.

Passing constructor/JNI/teardown and these controlled ownership fixtures still
do not establish target scheduling, in-flight SDK callback quiescence or full
graphics/audio/input/game startup.

Actual Vita3K runs now pass VitaGL initialization and read/validate the game SO
after correcting the resolution-result interpretation. A separate local emulator
copy requesting OpenGL 4.4 avoids the observed null GL call in the official
4.3 context. Installing official kubridge in that isolated profile resolves
allocation and executes actual segments, constructors and JNI_OnLoad. With
SDKBOX VM initialization and the libgcc kuser repair, diagnostic run 010 reaches
Cocos shader creation/attribute binding, then dereferences a null GXM program.
Current startup checks shader compiler initialization and exits with a specific
prerequisite error when it is absent. Other SDKBOX Java services remain unsupported.
MP3 decoding and a muted output worker now pass the standalone HLE-kernel probe;
the firmware kernel path fails its module permission-query imports. Actual game
frames, audible output and input remain unproven.
See [.llm/vita3k-runtime.md](../.llm/vita3k-runtime.md).

### Real offline shader rendering probe

`vita_shader_probe.c` exercises actual vitaGL/GXM binary loading, program linking,
matrix upload, texture sampling, vertex colors, drawing and GPU pixel readback.
The Cg sources in `resources/shaders/offline` include the first game shader pair dumped by
the bridge (`ec98300789fd9176` vertex / `5e957238bf3f9ec3` fragment). They are
compiled with an existing local `psp2cgc.exe`; no compiler/SDK binary is included
or downloaded. This is a route toward precompiled game shaders, not a GLSL
translator or a complete shader pack.

Run in PowerShell with the path to an existing native compiler:

```powershell
./tests/run_vita_shader_probe.ps1 -CompilerPath 'F:/PSVITASDK/sdk/host_tools/bin/psp2cgc.exe'
```

The runner builds a separate SELF using the current CMake target's SDK library
list. It requires the existing isolated app and OpenGL 4.4 diagnostic emulator,
refuses a live emulator, and preserves the regular eboot/VPK, firmware and default
configuration. A separate config enables surface synchronization and disables
asynchronous pipeline skipping; `--load-config` is a flag and the filename must
be passed using `--config-location`. A timeout preserves the live process for
inspection. The result is in `build/vita-shader-probe/probe-vita3k.log`.

Verified on the actual emulator: both binaries report compiled, the program
links with matrix/sampler uniforms, and pixel readback gives textured/color-
modulated center `(100,100,50,255)` and blue corner `(0,0,255,255)`, GL error zero,
guest exit zero. The probe checks the center channels within three levels and
requires the exact corner RGB. This is native rendering evidence for this shader
pair, not a game frame, general shader equivalence proof or physical Vita test.
The translated square also excludes screen center. This nonsymmetric matrix
check caught and verifies the correction from mul(M, v) to mul(v, M) for native
GL column-major uploads; the original diagonal-only check could not detect it.
The probe also uploads a two-row RGB texture through production pixel-store and
texture adapters with the default four-byte unpack alignment. Both rows read
back as (100,100,50), with GL error zero. Host tests and
`python3 tests/check_arm_pixel_store.py` additionally cover row length, subimage
argument forwarding, error/state preservation and allocation cleanup.

The production bridge now has an explicit offline fallback. See
[shader pack instructions](../resources/shaders/README.md). The current manifest
compiles 39 exact-source variants, but only actual game runs establish whether
all needed variants are covered. The GXP validator is not an instruction verifier.
`test_shader_binary` checks truncations, count/offset/name bounds, wrong stages,
bad matrix indices/categories and duplicate matrix entries under ASan/UBSan.
It also accepts optional generated `.vgxp` paths to validate actual build output:

```sh
build/host-tests/test_shader_binary build/offline-shaders/*.vgxp
python3 tests/check_arm_shader_cache.py
python3 tests/check_arm_startup.py
```

The linked ARM cache fixture runs actual source assembly/hash/cache/validator
code with GL and Vita filesystem endpoints modeled. It checks 600 create/source/
compile/delete lifetimes, native GLSL versus offline Cg selection, package-path
binary reads, missing/invalid binary rejection and failed program link shutdown,
with no retained allocations or file handles. It does not render pixels.
The linked startup fixture now passes 32 scenarios including compiler primary,
external-path fallback, offline pack and completely missing prerequisites.

### Native ARM exception unwinding

`python3 tests/check_arm_unwind.py` executes production lookup against real
PT_ARM_EXIDX and section tables, Thumb PCs, malformed bounds, native fallback,
module lifetime and count-reset cases. Optional `--game path/to/libcocos2dcpp.so`
compares every byte of the loaded game's 16746-entry table with an independent
ELF parser. This establishes lookup, not exception execution.

`wsl -d Ubuntu -- python3 tests/build_vita_unwind_probe.py` builds a separate
C++ DSO and probe SELF. Then `./tests/run_vita_unwind_probe.ps1`
uses the unchanged stock Vita3K executable and preserves the regular eboot,
firmware and VPK. The probe loads the real game library, validates its unique
EHABI-referenced generic personality against a code fingerprint, and imports
the game's C++ exception runtime. A thrown payload reaches catch(...) with one
RAII destructor: result 43, guest exit 0. The mappings stay alive until process
exit. Typed cross-toolchain catches and concurrent module unload are unproven.

### Android files-directory roundtrip

The JNI closed-loop write/read modes now obtain the path from the actual Cocos
getter, require an Android absolute path, append a binary save filename, and
write/read through production bridge_fopen in independent processes. They also
check the native persistent/data/files destination, the /data/user/0 alias and
Java File exists/isDirectory. Context.getFilesDir agrees with the Cocos getter.
These are host filesystem checks; actual game-save reload remains a runtime gate.
The APK DEX confirms that Cocos2dxHelper.init derives sFileDirectory from
Activity.getFilesDir().getAbsolutePath(). Runtime 028 reads files/st3 and
files/dl under the mapped native directory and renders the title screen.

Optional native GPU diagnostics are enabled by an existing
`ux0:vitaarc/capture-frame` file at frame 120. They export frame-120.rgba and
frame-1800.rgba (960x544 RGBA, bottom row first). In that mode a later
`ux0:vitaarc/capture-next-frame` request exports frame-live.rgba and is consumed
on success, allowing inspection after input without restarting the game.
This does not capture the host window or prove physical input/audio.

### Vorbis music and content detection

The real title music (`in_full.ogg` and `in_loop.ogg`) is Ogg/Vorbis. It must
not be scanned for incidental MP3 sync bytes. Production `stream.c` dispatches
by the Ogg signature and uses the SDK's libvorbisfile/libvorbis/libogg for
seekable mono/stereo streams. The existing MP3 decoder remains available when
sceAudiodec initializes; Vorbis does not require that initialization. Both expose
48 kHz output-frame positions to the same backend. VPKs include the library
copyright/license notices under licenses/.

Host audio tests generate synthetic Vorbis using the actual encoder library,
then decode through production stream code at 24/44.1/48/192 kHz. They check
mono duplication, stereo PCM, fractional resampling, seeks against sequential
output, EOF clamping, repeated loops and reset. Injected seek/read failures check
rollback or an explicit failed state; bounded OV_HOLE recovery prevents spinning.
Malformed Ogg containing MP3 data, format-changing chained streams and unsupported
surround channels are rejected. Backend tests prove Vorbis position/pause/seek
continues working when the MP3 module fails to initialize.

```powershell
wsl -d Ubuntu -- python3 tests/build_vita_audio_probe.py --vorbis
./tests/run_vita_audio_probe.ps1 -Vorbis
```

This separate SELF uses original game Ogg data and the real production decoder,
resampler and Vita output worker. The stock emulator with the normal firmware
decodes 48000 frames (47999 nonzero), seeks/resets, pauses at 64 ms, resumes past
500 ms to 542 ms, tears down and exits zero. The probe runs muted and validates
decoded samples, output timing and resource cleanup. Physical audio and display
need separate hardware verification.

`run_vita_game.ps1` provides bounded full-game observation using a visible Qt
window and the unchanged official emulator by default. Optional SDL/diagnostic
executable, HLE-kernel and GDB modes are explicit. GDB starts suspended and needs
a debugger continue. The script preserves live processes on observation timeout
and restores any temporarily moved private firmware with a SHA256 check.
The explicit renderer option is checked against renderer initialization in the
log (`observed_renderer`); a requested backend alone is not proof it was used.
For Vulkan the runner writes the case-insensitive spelling `VULKAN` to avoid
Vita3K's merge ignoring a value equal to its compiled default while the portable
global config specifies OpenGL. Run 037 confirms Vulkan is selected, but this
host build crashes in renderer initialization before loading the game.

### Real-game lifecycle and native preferences

```powershell
wsl -d Ubuntu -- cmake --build build --target VitaArc --parallel 2
wsl -d Ubuntu -- python3 tests/build_vita_lifecycle_probe.py
./tests/run_vita_lifecycle_probe.ps1
# Optional comparison using the emulator's HLE kernel:
./tests/run_vita_lifecycle_probe.ps1 -UseHleKernel
# Longer title-scene run: 6600 frames, ten pause/resume cycles:
wsl -d Ubuntu -- python3 tests/build_vita_lifecycle_probe.py --soak
./tests/run_vita_lifecycle_probe.ps1 -UseHleKernel -Soak
wsl -d Ubuntu -- python3 tests/build_vita_preferences_probe.py
./tests/run_vita_preferences_probe.ps1
```

The lifecycle SELF links production objects, loads the real game, renders 1200
frames, pauses/resumes at frame 600, stops callbacks and invokes the production
unloader and GL buffer cleanup. Both normal and HLE kernel runs pass, including
clean guest exit zero and removal of the game mapping. This covers the startup/
title scene; concurrent downloads, gameplay shutdown and physical input remain
unverified. JNI counts are 51 objects/6 references at frame 600 and 53/6 at
frame 1200, not evidence of zero allocation growth over an unbounded run.

The optional soak samples every 600 frames and requires JNI object/reference
counts to equal the frame-1800 baseline at every later sample. Its HLE run passes
with 53 objects/6 references through frame 6600, ten pause/resume cycles,
production unload and clean exit zero. Allocated heap is printed for context,
not used as a general leak assertion. Evidence is saved separately under
`build/vita-lifecycle-soak/`; the regular game eboot and firmware are preserved.

This test exposed a real resume failure: the supported APK calls
Cocos2dxRenderer.setAnimationInterval(F)V. The bridge now stores the requested
interval, and the main loop delays only for its remaining time after rendering
and vsync. The default 60 FPS and faster requests use vsync alone, matching the
Cocos Java renderer's default path. Host tests cover ordinary/V/A calls, zero and sub-microsecond requests,
invalid/nonfinite/overflow rejection and elapsed-time boundaries. The linked ARM
main now passes 40 cases, including the pacing branch after swap and storage
initialization failure cleanup.

The preferences SELF runs the actual exported Cocos UserDefault methods through
the real JNI bridge and Vita file I/O. It initializes the game's JavaVM/context
without creating a game scene. Two fresh stock-emulator processes write/read the
same reserved integer key, and the reader deletes it. The runner backs up and
restores the exact private preference file with SHA256 verification, retaining
the backup if a probe is still running. Code mappings remain until process exit;
the lifecycle probe separately covers unloading. This proves native Cocos
preference persistence, not chart scores or the game's st3 save format.

### Native synthetic-panel gameplay and SQLite storage

```powershell
wsl -d Ubuntu -- python3 tests/build_vita_touch_probe.py
./tests/run_vita_touch_probe.ps1
# In a separate terminal while the bounded fixture is running, use a Python
# with Pillow installed (the bundled desktop Python includes it):
python tests/vita_touch_control.py 0
python tests/vita_touch_control.py 1 capture 600
python tests/vita_touch_control.py 2 tap 480 430 120
# Inspect each PNG before choosing the next command/coordinates.
python tests/vita_touch_control.py 3 quit

wsl -d Ubuntu -- python3 tests/build_vita_touch_probe.py --sqlite
./tests/run_vita_touch_probe.ps1 -Program sqlite-probe -TimeoutSeconds 60
wsl -d Ubuntu -- python3 tests/check_arm_ctype.py --game <local-libcocos2dcpp.so>
```

The touch SELF wraps only sceTouchPeek; real production panel scaling, begin /
move / end tracking, frame_step, JNI and game callbacks execute. Commands use
960x544 game coordinates, never Windows coordinates. Its real display-surface
readbacks are top-down RGBA. Numbered command files accept capture, tap, drag,
pair and quit; acknowledgments follow complete frame writes. The controller can
queue alternating paired ground taps with `--pulse-pairs 1..100`. This is coarse
game-side input testing, not physical touch, Windows automation, beat timing or
latency calibration. The fixture stops after 36000 frames; failures are not pass.

The runner clones persistent data and restores the original after exit; tested
state and captures remain under build/vita-touch-probe/run-*. It preserves a live
process and backup on timeout. `-SeedData` continues from a previous tested copy
under build without replacing original data. Excessive log growth aborts only
this diagnostic process, and log inspection is bounded to prevent memory blowup.
Renderer settings match the game diagnostic config; default surface-sync settings
previously produced black native readbacks and must not be treated as evidence
of broken scene rendering.

Actual Fairytale Past 1 completes through TRACK LOST and return to selection;
paired reports produce real PURE/FAR judgments, pause/resume works and list
dragging scrolls songs. After fixing storage, score 29761/rank D survives restart.
The original score-loss bug is SQLite extended IOERR_LOCK=3850. Production startup
now selects its built-in unix-dotfile VFS before DBManager opens st3, using real
atomic directory locks instead of unsupported POSIX record locks. The SQLite
probe uses the same configuration, writes a reserved database, requires BUSY from
a conflicting connection, rolls back, closes/reopens and reads 37. Host SQLite
independently reports integrity_check=ok. Abnormal termination can leave a dotfile
lock directory; no automatic deletion of possibly owned locks is performed.

### Native wait remainder and final storage soak

The Vita nanosleep bridge now measures the entire elapsed interval when a
remaining-time pointer is supplied, including oversleep in earlier successful
chunks. A failed wait returns the unelapsed request, borrows nanoseconds safely,
clamps completed intervals to zero and avoids multiplying a large time_t value.
Regressing counters conservatively retain the request. Host Vita-boundary tests
pass under ASan/UBSan and TSan. `check_arm_nanosleep.py` executes the actual guest
export with 64-bit clock/kernel endpoints, eight cases, 32-bit timespec canaries,
aliased request/remainder and the time_t limit. Real signal delivery and physical
kernel scheduling remain outside these injected-endpoint checks.

```powershell
wsl -d Ubuntu -- python3 tests/check_arm_nanosleep.py
wsl -d Ubuntu -- python3 tests/build_vita_lifecycle_probe.py --soak
./tests/run_vita_touch_probe.ps1 -Program lifecycle-soak -TimeoutSeconds 300
```

The data-isolating runner also accepts lifecycle-probe/lifecycle-soak. The soak
after the storage/JNI/ctype/RNG fixes passes 6600 frames, ten pause/resume cycles,
53 objects/6 references at every sample from frame 1800 onward, production unload
and guest exit zero. Native heap reporting is 8632888 bytes at frames 1200–3000
and 8456248 at frames 3600–6600, without a general leak assertion. Its evidence is
`build/vita-touch-probe/run-20261024-121526/`. This soak precedes the isolated
nanosleep remainder fix above; that fix has separate host/ARM boundary coverage.
