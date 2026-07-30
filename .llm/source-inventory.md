# Source inventory

## Root and tooling

- `.gitignore`
- `.gitmodules`
- `README.md`
- `CMakeLists.txt`
- `build.sh`
- `.gitlab-ci.yml`
- `.github/workflows/build-vpk.yml`
- `tools/generate_symbols.py`
- `tools/package_apk.py`
- `tools/udp_logger.py`

## LiveArea

- `livearea/template.xml`
- `livearea/startup.png`
- `livearea/icon0.png`
- `livearea/background.png`

## Entry, loader and shared types

- `src/init.c`
- `src/config.h`
- `src/types.h`
- `src/elf.h`
- `src/dynalib.h`
- `src/loader.h`
- `src/loader.c`
- `src/patcher.h`
- `src/patcher.c`
- `src/kubridge.h`
- `src/kubridge.c`

## Utilities and logging

- `src/utils/fs.h`
- `src/utils/fs.c`
- `src/utils/string.h`
- `src/utils/string.c`
- `src/utils/debug.h`
- `src/utils/debug.c`
- `src/logcat/logcat.h`
- `src/logcat/logcat.c`

## Unified symbols and EABI

- `src/bridges/symbol.def`
- `src/bridges/eabi.h`

EABI callback implementations来自 compiler runtime symbols；统一表引用 `__aeabi_*`。

## Android

- `src/bridges/android/log.h`
- `src/bridges/android/log.c`
- `src/bridges/android/assetmgr.h`
- `src/bridges/android/assetmgr.c`

## JNI

- `src/bridges/jni/aosp/jni.h`
- `src/bridges/jni/runtime.h`
- `src/bridges/jni/runtime.c`
- `src/bridges/jni/cocos.h`
- `src/bridges/jni/cocos.c`
- `src/bridges/jni/jni.h`
- `src/bridges/jni/jni.c`

## libc/Bionic/pthread

- `src/bridges/libc/impl/libc.h`
- `src/bridges/libc/impl/libc.c`
- `src/bridges/libc/impl/pthread.h`
- `src/bridges/libc/impl/pthread.c`
- `src/bridges/libc/impl/bionic/bionic.h`
- `src/bridges/libc/impl/bionic/libc_logging.h`
- `src/bridges/libc/impl/bionic/__memcpy_chk.cpp`
- `src/bridges/libc/impl/bionic/__memmove_chk.cpp`
- `src/bridges/libc/impl/bionic/__memset_chk.cpp`
- `src/bridges/libc/impl/bionic/__strchr_chk.cpp`
- `src/bridges/libc/impl/bionic/__strlen_chk.cpp`
- `src/bridges/libc/impl/bionic/__vsnprintf_chk.cpp`
- `src/bridges/libc/impl/bionic/__vsprintf_chk.cpp`

## OpenGL

- `src/bridges/opengl/opengl.h`
- `src/bridges/opengl/opengl.c`
- `src/bridges/opengl/impl/opengl.h`
- `src/bridges/opengl/impl/opengl.c`
- `src/bridges/opengl/impl/glsl2cg.h`
- `src/bridges/opengl/impl/glsl2cg.c`

## Cocos2d-x

- `src/bridges/cocos2dx/cocos2dx.h`
- `src/bridges/cocos2dx/cocos2dx.c`

## Audio

- `src/bridges/audio/provider.h`
- `src/bridges/audio/provider.c`
- `src/bridges/audio/manager.h`
- `src/bridges/audio/manager.c`

## third-party

- `third-party/kubridge/CMakeLists.txt`
- `third-party/kubridge/exports.yml`
- `third-party/kubridge/kubridge.h`
- `third-party/kubridge/main.c`
- `third-party/arcaea/arcaea_1.6.1c.apk`
- `third-party/arcaea/arcaea_1.9.0c.apk`
- `third-party/arcaea/arcaea_2.0.0c.apk`
- `third-party/arcaea/arcaea_3.0.0c.apk`

## Removed historical layout

当前不再使用：

```text
src/main.c
src/common/
src/solibrary/
src/bridges/loader.*
src/bridges/patcher.*
src/bridges/kubridge.*
src/bridges/jni/impl/jvm.*
src/bridges/jni/impl/runtime.*
src/bridges/android/impl/
src/bridges/audio/impl/
各 bridge 子目录 CMakeLists.txt
```

## Build-generated inventory

- `build/generated/bridge_symbols.h`
- CMake configure/cache files
- object files under `build/CMakeFiles` and nested target directories
- `build/VitaArc`
- `build/VitaArc.velf`
- `build/VitaArc.self`
- `build/VitaArc.vpk_param.sfo`
- `build/VitaArc.vpk.out`
- `build/VitaArc.vpk`
