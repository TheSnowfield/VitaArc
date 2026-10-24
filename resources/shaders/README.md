# Shader replacements

Place pre-converted VitaGL/Cg shader sources in this directory. Each file name
must be the lowercase, 16-digit FNV-1a hash of the exact original GLSL source:

`0123456789abcdef.cg`

At runtime, VitaArc first checks `ux0:vitaarc/shaders/<hash>.cg` for development
overrides and then `app0:/shaders/<hash>.cg` for replacements packaged here.
Unknown shaders are dumped to `ux0:vitaarc/shaders/missing/`.

## Offline binary pack

The checked-in `offline/manifest.json` maps exact assembled GLSL hashes to
explicit Cg translations. Build with an existing local native shader compiler:

```powershell
python tools/compile_shaders.py --compiler 'F:/PSVITASDK/sdk/host_tools/bin/psp2cgc.exe'
```

The tool writes `.vgxp` binaries and an `offline.enabled` marker under
`build/offline-shaders`. Optional `--originals <directory>` verifies the exact
hash of every supplied `<hash>.vert.glsl` / `<hash>.frag.glsl` before compilation.
The generated format is the installed vitaGL ABI: little-endian matrix count,
matrix parameter indices, then GXP. Matrices named by the manifest must exist
as uniforms in the compiler output. This is not a general GLSL translator.

To package the generated files, configure CMake with
`-DVITAARC_SHADER_BINARY_DIR=<absolute-pack-directory>`, then build normally.
To test a development override, place the generated `.vgxp` files and marker
in `ux0:vitaarc/shaders/`. Runtime compiler initialization is tried first; only
when it fails will a valid offline marker enable binary loading. Both user and
app markers must contain exactly `VitaArc offline shaders 1` plus a newline.
The user binary takes precedence over `app0:/shaders/<hash>.vgxp`.

The runtime checks size, stage, GXP reflection bounds/names and matrix indices
before passing a binary to vitaGL. These checks are not a GPU instruction
verifier; use local compiler output. Missing or rejected binaries log the hash
and terminate the process with code 1 before attaching a null GXM program.
The original source remains available in the dump directory.

With this SDK/vitaGL combination, GLSL column-major matrix uploads require
`mul(vector, matrix)` in the Cg translations. The native rendering probe tests
both scale and translation; a diagonal-only matrix does not verify this rule.
The pack is incomplete until actual game runs establish coverage for all
needed variants. Successful binary compilation alone does not prove game boot.

## Arcaea 7.0 effects

The APK contains additional GLSL effects. Generate their Cg sources with the
vitaGL translator, then compile both manifests into the same output directory.
The Linux host adapter was verified with vitaGL commit
`76f43e6fbe1513cee44118da7ff2c16728c4fed2`. It uses that source tree's translator
and preprocessor; no compiler binary is downloaded or included.

```bash
python3 tools/build_shader_translator.py --source /path/to/vitaGL --output build/shader-tools
python3 tools/prepare_effects.py --apk third-party/arcaea/arcaea_7.0.260c.apk \
  --translator build/shader-tools/translate --output build/effects-7
```

Run `compile_shaders.py` on the native Cg compiler's host, first with the normal
manifest and then with `--manifest build/effects-7/manifest.json`, using the same
`--output` directory. Pass that directory to `VITAARC_SHADER_BINARY_DIR` when
packaging 7.0. Generated sources remain in the build directory. The adapter
retains the supplied vitaGL implementation and its LGPL-3.0 license.

Use `-DARCAEA_VERSION=7.0.260c` for this version. The default native heap is
256 MiB for every APK version. Existing CMake caches retain their configured
value; use `-DVITAARC_HEAP_MB=256` to update one. Packaging 7.0 rejects heap
reservations smaller than 128 MiB.

Android Surface/MediaCodec video decoding remains unavailable. Its external
texture shader is excluded, and the media bridge reports unsupported formats.
