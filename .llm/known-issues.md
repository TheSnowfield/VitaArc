# Known incomplete behavior and directly observed defects

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

The UDP destination is compile-time fixed at `10.20.0.227:23333`.
Network initialization memory is not retained for cleanup, and `log_end`
disables the logger before attempting its final “Log stop” message.

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
