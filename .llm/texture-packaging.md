# Vita texture profile

## Package conversion

`tools/package_textures.py` owns image selection, conversion and metadata.
`package_apk.py` imports the selected profile into a complete VPK.
`package_apk_versions.py` shares the conversion plan when building variants.

The `vita` profile halves independent PNG/JPEG dimensions, with further
halving above a 960-pixel long edge. Images smaller than 64 pixels on either
axis or below a 256-pixel long edge retain their source bytes. Sprite atlases
referenced by frame plists, particle/default resources and nine-patch images
also retain their source bytes. Filtering uses premultiplied alpha for images
with transparency.

The source APK is never modified. Complete VPK imports use a temporary sibling
archive and replace the destination after conversion finishes. Overlay archives
contain only the changed images, the layout manifest, the conversion report
and the matching `eboot.bin`.

`--textures original` preserves the source assets without requiring Pillow.
`--textures vita` requires `tools/requirements.txt`. CMake exposes the selection
as `VITAARC_TEXTURE_PROFILE`, defaulting to `vita`.

## Runtime boundary

`src/bridges/cocos2dx/textures.c` reads
`app0:package/vitaarc-textures.bin` before Cocos constructors. Missing metadata
keeps the original texture path. Invalid metadata or an unsupported image ABI
stops startup with an error.

The little-endian manifest has a 16-byte header: eight-byte `VARCTX1\0` magic,
32-bit row count and 32-bit row stride. Each 264-byte row stores a zero-terminated
256-byte APK asset path and four 16-bit dimensions: original width/height and
packed width/height. Rows are sorted by path for binary lookup.

The bridge verifies the image initialization function fingerprint and the
texture dimension getters against the local 1.6.1c, 1.9.0c, 2.0.0c and 3.0.0c
libraries. These establish the Image file-path string ABI and Texture2D fields.
Only the aligned Thumb `initWithImage(Image*)` wrapper is redirected; its
original behavior is reproduced through the two-argument overload and default
alpha pixel format getter.

The original function decodes and allocates the smaller image. After success,
the bridge restores logical pixel dimensions and content size for matching
immutable assets, keeping sprite rectangles and layout coordinates unchanged.
GPU allocation and normalized UV limits keep their packed values. Dynamic
textures and retained atlases are outside this profile. No global content-scale
factor is changed. Metadata is released after library teardown.

## 1.6.1c artifacts

`build/texture-profile/VitaArc-1.6.1c-textures-vita.zip` is an overlay for the
existing 1.6.1c installation. Both `eboot.bin` and `package/` must be copied to
the app directory together while the app is closed. `vitaarc-textures.json`
contains the conversion report and can be kept on the host.

391 images are resized. The aggregate RGBA estimate across every PNG/JPEG
falls from 576.5 MiB to 162.1 MiB, a 71.89% reduction. This estimate is not a
measurement of simultaneous resident RAM or VRAM. The actual device budget
still includes render targets, geometry, audio, the loaded library and heaps.

Release and Debug ELF/VELF/SELF files are retained under
`build/texture-profile/release/` and `build/texture-profile/debug/`.
Both SELF targets compiled successfully. Archive integrity checks confirmed
CRC, dimensions, decoded images and manifest agreement for all converted files.
No test suites, emulator runs or hardware performance measurements were run.
