# Project layout and artifacts

## 项目用途

VitaArc 的目标是在 HENkaku PSVita 上运行 Arcaea Android ARMv7 原生代码。实现不是重新实现游戏逻辑，而是：

- 读取 Android APK 中的 ARMv7 ELF shared object。
- 在 Vita 用户进程中手动映射 `libcocos2dcpp.so`。
- 将 Android/Bionic/JNI/OpenGL ES 等导入符号重定向到 Vita 侧实现。
- 对特定版本的 Cocos2d-x/Arcaea 机器码按固定偏移打补丁。
- 调用原始库导出的 JNI 和 Cocos2d-x 初始化函数。

README 明确声明项目与 Lowiro 和 Arcaea 无隶属关系，并警告项目仍在开发、不能保证游戏正确运行。

## 根目录

- `.git/`：Git 仓库元数据。
- `.vscode/`：本地 VS Code 配置；被 `.gitignore` 忽略。
- `build/`：旧构建目录和调试现场；被 `.gitignore` 忽略。
- `livearea/`：VPK 的 LiveArea 图标、背景、启动图和模板。
- `src/`：VitaArc 源码。
- `.gitignore`：忽略 CMake 中间文件、Makefile、`build/` 和 `.vscode/`。
- `.gitmodules`：声明 `src/kubridge` 子模块，上游为 `https://github.com/TheOfficialFloW/kubridge.git`。
- `build.sh`：执行 `cmake . -B build`、进入 `build`、`make clean`、`make -j 8`。
- `CMakeLists.txt`：顶层 VitaSDK 构建和 VPK 打包配置。
- `README.md`：项目目的、设备安装和 Android 游戏文件目录说明。
- `CMakeCache.txt`：根目录遗留 CMake 缓存，不是源码。

## `src/` 分类

### `src/main.c`

Vita 用户程序入口。负责日志、vitaGL、SO 装载、导入符号桥接、二进制补丁、SO 初始化、JNI 启动及 Cocos2d-x 原生初始化。

### `src/common`

- `define.h`：Android 库路径、日志路径、持久化数据路径和 OBB 路径。
- `types.h`：本地 `bool`、`true`、`false` 定义。
- `elf.h`：完整 ELF32 类型、常量、ARM 重定位常量等定义，约 3527 行。

### `src/solibrary`

- `solib.h`：对外 SO 句柄和装载器 API。
- `internal.h`：`SOINTERNAL`、槽位和 ELF 缓存字段。
- `solib.c`：文件读取、段映射、ARM 重定位、符号查找、导入项安装和初始化数组调用。

### `src/bridges`

- `eabi/`：ARM EABI helper 符号桥接。
- `libc/`：Bionic fortify、C 标准库、POSIX、网络和 pthread 桥接。
- `android/`：Android log 和 AssetManager API。
- `jni/`：JNI ABI 头、JavaVM/JNIEnv 函数表和游戏使用的平台方法。
- `opengl/`：OpenGL ES 到 vitaGL 的符号映射、自定义包装及 GLSL 到 Cg 转换。
- `audio/`：替换游戏 `AudioProvider`，绕开 OpenSL ES。
- `cocos2dx/`：按固定偏移修改游戏/Cocos2d-x 机器码。

### `src/utils`

- `fs.c/.h`：文件存在、文件大小和全文件读取。
- `string.c/.h`：文件名和 UTF-16 字符串长度辅助。
- `patcher.c/.h`：符号补丁、地址补丁、ARM/Thumb 指令补丁和绝对跳转 hook。
- `debug.c/.h`：内存十六进制输出、内存 dump 和人为崩溃断点。
- `kubridge.c/.h`：基于 `kuKernelCpuUnrestrictedMemcpy` 实现越权 memset。

### `src/logcat`

日志抽象，可输出到 UDP 或设备文件。

### `src/kubridge`

独立内核插件源码及导出配置，提供固定地址内存分配、缓存刷新和不受用户态访问权限限制的复制。

## LiveArea/VPK 资源

- `livearea/icon0.png` → `sce_sys/icon0.png`
- `livearea/background.png` → `sce_sys/livearea/contents/background.png`
- `livearea/startup.png` → `sce_sys/livearea/contents/startup.png`
- `livearea/template.xml` → `sce_sys/livearea/contents/template.xml`

## 设备目录要求

README 要求从官方 Android 游戏 APK 中解压资源，并在 Vita 建立：

```text
ux0:vitaarc/
├── assets/
│   ├── app-data/
│   ├── audio/
│   ├── char/
│   ├── Default/
│   ├── Fonts/
│   ├── img/
│   └── ...
├── library/
│   └── armeabi-v7a/
│       ├── libcocos2dcpp.so
│       ├── libcrashlytics.so
│       ├── libcrashlytics-common.so
│       ├── libcrashlytics-handler.so
│       ├── libcrashlytics-trampoline.so
│       └── ...
└── persistent/
```

源码中的路径常量还包括：

- `ux0:vitaarc/library/armeabi-v7a/libfmod.so`
- `ux0:vitaarc/library/armeabi-v7a/libfmodProvider.so`
- `ux0:vitaarc/boot.log`
- `ux0:vitaarc/persistent/data/`
- `ux0:vitaarc/persistent/obb/`

`define.h` 中 `LIBRARY_LIBFMOD` 被重复定义；`LIBRARY_LIBFMODPROVIDER` 和 `LIBRARY_LIBFMOD_PROVIDER` 同时存在。

