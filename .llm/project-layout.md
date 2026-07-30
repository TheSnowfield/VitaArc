# Project layout and artifacts

## 项目用途

VitaArc 的目标是在 HENkaku PSVita 上运行 Arcaea Android ARMv7 原生代码。实现不是重写游戏逻辑，而是：

- 读取 APK 中的 ARMv7 ELF shared object。
- 在 Vita 用户进程中手动映射 `libcocos2dcpp.so`。
- 将 Android/Bionic/JNI/OpenGL ES 等导入符号重定向到 Vita 侧实现。
- 对特定版本 Cocos2d-x/Arcaea 机器码按固定偏移打补丁。
- 调用原始库导出的 `JNI_OnLoad`，并保留后续 Cocos JNI 导出调用封装。

README 明确声明项目与 Lowiro 和 Arcaea 无隶属关系，并提示项目仍在开发、不能保证游戏正确运行。

## 根目录

- `.git/`：Git 仓库元数据。
- `.agents/`、`.codex/`：Codex 桌面环境只读元数据。
- `.llm/`：本知识库。
- `livearea/`：VPK 的 LiveArea 图标、背景、启动图和模板。
- `src/`：VitaArc 用户态源码。
- `third-party/`：第三方依赖目录；`.gitmodules` 声明 `third-party/kubridge` 子模块。
- `tools/udp_logger.py`：UDP 日志接收器，默认监听 `0.0.0.0:23333`。
- `.gitignore`：忽略构建缓存、Makefile、`build/` 和 VS Code目录。
- `.gitmodules`：声明 `third-party/kubridge`，上游为 `https://github.com/TheOfficialFloW/kubridge.git`。
- `build.sh`：执行 `cmake -S . -B build` 和 `cmake --build build --parallel 8`。
- `CMakeLists.txt`：顶层 VitaSDK 构建和 VPK 打包配置。
- `README.md`：项目目的、免责声明、安装、APK资源目录。

当前仓库根目录没有 `build/`，也没有旧文档中提到的根目录 `CMakeCache.txt`。

## `src/` 当前布局

### `src/main.c`

Vita 用户程序入口。负责日志、vitaGL shader compiler 参数、SO 装载、导入符号桥接、二进制补丁、SO 初始化和 `JNI_OnLoad` 调用。

### `src/bridges`

当前大量基础桥接文件都在这个目录下：

- `define.h`：Android 库路径、日志路径、持久化数据路径和 OBB 路径。
- `types.h`：本地 `bool`、`true`、`false` 定义。
- `elf.h`：ELF32 类型、常量、ARM 重定位常量等定义。
- `loader.h/.c`：SO 状态结构、固定地址映射、ARM 重定位、符号查找、导入项安装、`.init_array` 调用、引用计数释放。
- `patcher.h/.c`：符号补丁、地址补丁、ARM/Thumb 指令补丁和绝对跳转 hook。
- `kubridge.h/.c`：用户态辅助包装，目前只实现 `kuKernelCpuUnrestrictedMemset`。

### `src/bridges/*`

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
- `debug.c/.h`：内存十六进制输出、内存 dump 和人为崩溃断点。

### `src/logcat`

日志抽象，可输出到 UDP 或设备文件。当前编译期启用 UDP。

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
