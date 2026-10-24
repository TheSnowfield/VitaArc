# VitaArc

Run Arcaea on your HENkaku PSVita.  
This is a cross-system ABI bridge to simulate the android environment of Arcaea in the SCE system.

`Arcaea` is a mobile game who developed by Lowiro.  
This project is not affiliated with Lowiro or Arcaea.

Bitmap text rendering is based in part on the work of the FreeType Team
(https://freetype.org). Its license is included in `licenses/freetype.txt` in
the VPK. Text uses fonts from the selected APK.

## Hint
This project now **under development**.  
You may not get the game running correctly.

## Get started
### Install
 - Download the `VitaArc.vpk` from the release page.  
 (random laughing) ~~cause now there's no release exists. XD~~
 - Connect your PSVita to PC via VitaShell and copy `VitaArc.vpk` into it.
 - Install VitaArc.

### Package game resources

To include game resources, place the official APK in `third-party/arcaea/`
and enable `VITAARC_PACKAGE_APK`. The packaging script imports the selected
APK into `package/` in the VPK. At runtime these
read-only files are available below `app0:package/`; no manual extraction of
the APK to the memory card is required.

Writable files such as logs and persistent game data remain below
`ux0:vitaarc/`.

### Continuous integration

GitHub Actions and GitLab CI build a base `VitaArc.vpk` with the official
`vitasdk/vitasdk` container and publish it as a pipeline artifact. The CI
artifact intentionally excludes the proprietary Arcaea APK resources.

To build the same base VPK locally:

```sh
bash build.sh
```

Local builds also exclude APK resources by default. To package a selected APK:

```sh
bash build.sh -DVITAARC_PACKAGE_APK=ON -DARCAEA_VERSION=1.6.1c
```

To compare every local APK without replacing the main VPK:

```sh
python tools/package_apk_versions.py --base-vpk build/VitaArc.vpk --apk-dir third-party/arcaea --output-dir build/apk-versions
```

For an iterative bridge build, add `--self build/VitaArc.self` and select a new
output directory. Each package records its manifest version for JNI startup.

Use a new output directory for another batch. The tool verifies ZIP CRCs, each
APK file's size/CRC and every unchanged base resource, and writes SHA256 hashes
to `packages.json`. All variants inherit the base SELF and title ID; installing
one replaces the other. Packaging does not establish game compatibility. See
[the local version test results](.llm/apk-compatibility.md) for the current build.

### Play & PM
 - Click VitaArc bubble.
 - Wait for the game load.
 - Try your best to `PM` all of the songs.

### Local JNI / Cocos tests

Run the real bridge against a small mock game shared library on Linux or WSL.
These tests do not require VitaSDK, a Vita, or an Arcaea APK.
Install host test dependencies (`libffi-dev libvorbis-dev libfreetype-dev fonts-dejavu-core` on Ubuntu) first.

```sh
cmake -S tests -B build/host-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host-tests --parallel 2
ctest --test-dir build/host-tests --output-on-failure
```

The suite enables AddressSanitizer and UndefinedBehaviorSanitizer by default.
It covers JNI dispatch, Cocos preferences across process restarts, asset access,
startup, input, native registration and failure handling. See
[tests/README.md](tests/README.md) for the mock boundary and remaining limitations.
