# Source inventory

## Root files

- `.gitignore`：忽略构建缓存、Makefile、build和 VS Code目录。
- `.gitmodules`：kuBridge submodule声明。
- `README.md`：用途、免责声明、安装、APK资源目录。
- `CMakeLists.txt`：Vita应用、静态子库、链接项、SELF/VPK。
- `build.sh`：Bash构建入口。
- `CMakeCache.txt`：遗留缓存。

## LiveArea

- `livearea/template.xml`
- `livearea/startup.png`
- `livearea/icon0.png`
- `livearea/background.png`

## Program/common

- `src/main.c`
- `src/common/types.h`
- `src/common/define.h`
- `src/common/elf.h`

## SO loader

- `src/solibrary/solib.h`
- `src/solibrary/internal.h`
- `src/solibrary/solib.c`
- `src/solibrary/CMakeLists.txt`

## General utilities

- `src/utils/fs.h`
- `src/utils/fs.c`
- `src/utils/string.h`
- `src/utils/string.c`
- `src/utils/patcher.h`
- `src/utils/patcher.c`
- `src/utils/debug.h`
- `src/utils/debug.c`
- `src/utils/kubridge.h`
- `src/utils/kubridge.c`
- `src/utils/CMakeLists.txt`

## Logging

- `src/logcat/logcat.h`
- `src/logcat/logcat.c`
- `src/logcat/CMakeLists.txt`

## Kernel kuBridge

- `src/kubridge/main.c`
- `src/kubridge/kubridge.h`
- `src/kubridge/exports.yml`
- `src/kubridge/CMakeLists.txt`

## EABI bridge

- `src/bridges/eabi/eabi.h`
- `src/bridges/eabi/eabi.c`
- `src/bridges/eabi/impl/eabi.h`
- `src/bridges/eabi/CMakeLists.txt`

## Android bridge

- `src/bridges/android/android.h`
- `src/bridges/android/android.c`
- `src/bridges/android/impl/log.h`
- `src/bridges/android/impl/log.c`
- `src/bridges/android/impl/assetmgr.h`
- `src/bridges/android/impl/assetmgr.c`
- `src/bridges/android/CMakeLists.txt`

## JNI bridge

- `src/bridges/jni/jni.h`
- `src/bridges/jni/jni.c`
- `src/bridges/jni/impl/jvm.h`
- `src/bridges/jni/impl/jvm.c`
- `src/bridges/jni/impl/jni.h`
- `src/bridges/jni/impl/jni.c`
- `src/bridges/jni/impl/cocos.h`
- `src/bridges/jni/impl/cocos.c`
- `src/bridges/jni/impl/android/jni.h`
- `src/bridges/jni/CMakeLists.txt`

## libc bridge

- `src/bridges/libc/libc.h`
- `src/bridges/libc/libc.c`
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
- `src/bridges/libc/CMakeLists.txt`

## OpenGL bridge

- `src/bridges/opengl/opengl.h`
- `src/bridges/opengl/opengl.c`
- `src/bridges/opengl/impl/opengl.h`
- `src/bridges/opengl/impl/opengl.c`
- `src/bridges/opengl/impl/glsl2cg.h`
- `src/bridges/opengl/impl/glsl2cg.c`
- `src/bridges/opengl/CMakeLists.txt`

## Cocos2d-x patch bridge

- `src/bridges/cocos2dx/cocos2dx.h`
- `src/bridges/cocos2dx/cocos2dx.c`
- `src/bridges/cocos2dx/CMakeLists.txt`

## Audio bridge

- `src/bridges/audio/audio.h`
- `src/bridges/audio/audio.c`
- `src/bridges/audio/impl/provider.h`
- `src/bridges/audio/impl/provider.c`
- `src/bridges/audio/CMakeLists.txt`

## Local vitaGL files directly inspected

Root:

- `M:\Projects\PSV Projects\vitaGL\README.md`
- `M:\Projects\PSV Projects\vitaGL\Makefile`
- `M:\Projects\PSV Projects\vitaGL\libvitaGL.a`

Public/source:

- `M:\Projects\PSV Projects\vitaGL\source\vitaGL.h`
- `M:\Projects\PSV Projects\vitaGL\source\vitaGL.c`
- `M:\Projects\PSV Projects\vitaGL\source\custom_shaders.c`
- `M:\Projects\PSV Projects\vitaGL\source\gxm.c`
- `M:\Projects\PSV Projects\vitaGL\source\lookup.c`
- `M:\Projects\PSV Projects\vitaGL\source\shared.h`
- `M:\Projects\PSV Projects\vitaGL\source\state.h`

Other vitaGL implementation files identified:

- `source/debug.cpp`
- `source/ffp.c`
- `source/framebuffers.c`
- `source/get_info.c`
- `source/matrices.c`
- `source/misc.c`
- `source/state.c`
- `source/tests.c`
- `source/textures.c`
- `source/texture_callbacks.c`
- `source/shaders.h`
- `source/utils/gpu_utils.c/.h`
- `source/utils/math_utils.c/.h`
- `source/utils/mem_utils.c/.h`
- `source/utils/stb_dxt.h`
- built-in generated shader headers under `source/shaders/`
- clear shader Cg sources under `shaders/`
- samples 1 through 10

