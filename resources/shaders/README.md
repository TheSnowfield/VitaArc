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
