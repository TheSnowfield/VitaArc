# Source inventory

## Root files

- `.gitignore`：忽略构建缓存、Makefile、`build/` 和 VS Code目录。
- `.gitmodules`：`third-party/kubridge` submodule 声明。
- `README.md`：用途、免责声明、安装、APK资源目录。
- `CMakeLists.txt`：Vita应用、自动收集源码、链接项、SELF/VPK。
- `build.sh`：Bash 构建入口。

## Tooling

- `tools/udp_logger.py`：UDP 日志接收器，默认监听 `0.0.0.0:23333`。

## LiveArea

- `livearea/template.xml`
- `livearea/startup.png`
- `livearea/icon0.png`
- `livearea/background.png`

## Program entry

- `src/main.c`

## Shared bridge/base files

- `src/bridges/types.h`
- `src/bridges/define.h`
- `src/bridges/elf.h`
- `src/bridges/loader.h`
- `src/bridges/loader.c`
- `src/bridges/patcher.h`
- `src/bridges/patcher.c`
- `src/bridges/kubridge.h`
- `src/bridges/kubridge.c`

## General utilities

- `src/utils/fs.h`
- `src/utils/fs.c`
- `src/utils/string.h`
- `src/utils/string.c`
- `src/utils/debug.h`
- `src/utils/debug.c`

## Logging

- `src/logcat/logcat.h`
- `src/logcat/logcat.c`

## EABI bridge

- `src/bridges/eabi/eabi.h`
- `src/bridges/eabi/eabi.c`
- `src/bridges/eabi/impl/eabi.h`

## Android bridge

- `src/bridges/android/android.h`
- `src/bridges/android/android.c`
- `src/bridges/android/impl/log.h`
- `src/bridges/android/impl/log.c`
- `src/bridges/android/impl/assetmgr.h`
- `src/bridges/android/impl/assetmgr.c`

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

## OpenGL bridge

- `src/bridges/opengl/opengl.h`
- `src/bridges/opengl/opengl.c`
- `src/bridges/opengl/impl/opengl.h`
- `src/bridges/opengl/impl/opengl.c`
- `src/bridges/opengl/impl/glsl2cg.h`
- `src/bridges/opengl/impl/glsl2cg.c`

## Cocos2d-x patch bridge

- `src/bridges/cocos2dx/cocos2dx.h`
- `src/bridges/cocos2dx/cocos2dx.c`

## Audio bridge

- `src/bridges/audio/audio.h`
- `src/bridges/audio/audio.c`
- `src/bridges/audio/impl/provider.h`
- `src/bridges/audio/impl/provider.c`

## Current layout notes

当前只有顶层 `CMakeLists.txt`。旧文档中提到的下列目录或文件已不属于当前源码布局：

- `src/common/`
- `src/solibrary/`
- `src/kubridge/`
- 各桥接子目录下的 `CMakeLists.txt`

`third-party/kubridge` 是 submodule 声明，但当前本地文件列表没有读到其源码文件。

## Local vitaGL files historically inspected

先前知识库记录曾读取过本地 Windows 路径：

```text
M:\Projects\PSV Projects\vitaGL
```

这些记录保留在 `vitagl-local-source.md`，但当前 Linux 工作区内没有该源码树。本次对齐未重新验证外部 vitaGL 工作区。
