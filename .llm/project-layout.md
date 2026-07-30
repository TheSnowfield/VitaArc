# Project layout and artifacts

## 项目用途

VitaArc 的目标是在 HENkaku PSVita 用户进程中运行 Arcaea Android ARMv7 原生代码：

- 从被打包进 VPK 的 APK 内容读取 ARMv7 ELF shared object。
- 把 `libcocos2dcpp.so` 映射到 Vita 用户地址空间。
- 将 Android/Bionic/JNI/OpenGL ES 导入绑定到 VitaArc 本地实现。
- 对特定 Arcaea/Cocos2d-x 二进制偏移打机器码补丁。
- 提供 fake JavaVM/JNIEnv 和 Android AssetManager。
- 调用目标库 `.init_array` 和 `JNI_OnLoad`。

项目不隶属于 Lowiro 或 Arcaea。

## 根目录

```text
.git/                       Git metadata
.llm/                       本知识库
.vscode/                    VS Code/WSL C/C++ 配置
build/                      全部 CMake 和最终构建输出
livearea/                   LiveArea 图像和 XML
src/                        Vita 用户态实现
third-party/arcaea/         可选择的官方 APK 输入
third-party/kubridge/       kuBridge submodule source
tools/generate_symbols.py   bridge symbol 生成器
tools/package_apk.py        APK -> VPK importer
tools/udp_logger.py         UDP 日志接收器
.gitignore
.gitmodules
build.sh
CMakeLists.txt
README.md
```

## `src/` 顶级

```text
src/init.c          Vita 程序入口
src/config.h        app0/ux0 路径
src/types.h         项目基础类型
src/elf.h           ELF32/ARM definitions
src/dynalib.h       dynalib_t 独立类型定义
src/loader.h/.c     SO loader 和自动 symbol binding
src/patcher.h/.c    地址、ARM/Thumb、hook 补丁
src/kubridge.h/.c   用户态 kuBridge helper
```

旧路径 `src/main.c`、`src/bridges/loader.*`、`src/bridges/patcher.*`、`src/bridges/kubridge.*` 已不属于当前布局。

## `src/bridges/`

```text
android/     Android logging 和 AssetManager
audio/       AudioManager/AudioProvider compatibility
cocos2dx/    固定版本 Cocos2d-x patch
jni/         AOSP ABI header、fake runtime、Cocos platform methods
libc/        libc/Bionic/pthread bridge
opengl/      OpenGL ES symbols、vitaGL wrappers、GLSL->Cg
eabi.h       ARM EABI declarations
symbol.def   统一 imported symbol -> callback definition
```

不再有每类 bridge 的 `BRIDGEFUNC` 表和显式 `bridge_patch_eabi/android/libc` 启动调用。普通导入由生成符号表统一解析。

## JNI 当前布局

```text
src/bridges/jni/aosp/jni.h
src/bridges/jni/runtime.h
src/bridges/jni/runtime.c
src/bridges/jni/cocos.h
src/bridges/jni/cocos.c
src/bridges/jni/jni.h
src/bridges/jni/jni.c
```

旧的 `jvm.c` 已合并入 `runtime.c`。公开 runtime 函数不带 `fake_` 前缀；内部 fake object 类型仍使用 `FakeJni*` 名称。

## Android bridge 当前布局

```text
src/bridges/android/log.h/.c
src/bridges/android/assetmgr.h/.c
```

AssetManager 已有完整文件/目录读取实现，不再位于 `impl/` 子目录。

## Audio 当前布局

```text
src/bridges/audio/provider.h/.c
src/bridges/audio/manager.h/.c
```

旧的 `audio.c` 和 `impl/provider.c` 布局已拆平。

## `src/utils`

```text
debug.h/.c
fs.h/.c
string.h/.c
```

私有 helper 使用 snake_case；static symbol 以 `__` 开头。`fs.c` 的公开函数不再带 `utils_` 前缀，例如 `util_file_exists`、`util_get_file_size`。

## 命名约定

项目私有函数、变量、参数使用小写下划线。static 函数和变量以前缀 `__` 标识。

Vita/外部 ABI 名字不按私有命名规则改写：

- `sce*`
- `kuKernel*`
- JNI/AAsset exported ABI
- Android/C++ imported symbol spelling

include guard 按相对头文件路径生成，忽略顶级 `src`，格式：

```text
__PATH_TO_HEADER_NAME_H
```

例如：

```text
src/bridges/jni/runtime.h -> __BRIDGES_JNI_RUNTIME_H
src/config.h              -> __CONFIG_H
```

## third-party APK

当前四个文件：

```text
arcaea_1.6.1c.apk  493455233 bytes
arcaea_1.9.0c.apk  466613809 bytes
arcaea_2.0.0c.apk  336090287 bytes
arcaea_3.0.0c.apk  511009155 bytes
```

各版本均包含：

```text
lib/armeabi-v7a/libcocos2dcpp.so
lib/armeabi-v7a/libfmod.so
lib/armeabi-v7a/libfmodProvider.so
```

1.6.1c/1.9.0c 还包含 crashlytics envelope/unwind 库；不同版本 APK native library 集合不完全相同。

## LiveArea 映射

```text
livearea/icon0.png       -> sce_sys/icon0.png
livearea/background.png  -> sce_sys/livearea/contents/background.png
livearea/startup.png     -> sce_sys/livearea/contents/startup.png
livearea/template.xml    -> sce_sys/livearea/contents/template.xml
```

## 当前生成物

```text
build/generated/bridge_symbols.h
build/VitaArc
build/VitaArc.velf
build/VitaArc.self
build/VitaArc.vpk_param.sfo
build/VitaArc.vpk.out
build/VitaArc.vpk
```

生成文件不写入 `src`。
