# VitaArc knowledge base

本目录记录当前 VitaArc 工作区已经读取、实现和验证过的知识。内容按实现领域拆分；事实以当前源码和构建产物为准，历史外部源码知识会明确标记来源和时间。

## 文档分类

- `project-layout.md`：仓库目录、当前源码布局、第三方输入和生成物。
- `source-inventory.md`：当前源码、工具和资源文件清单。
- `build-and-packaging.md`：VitaSDK、CMake、生成目录、符号表生成、SELF/VPK、APK 选择和无 APK 的 CI 基础包。
- `apk-vpk-resources.md`：VPK ZIP 格式验证、APK 全量导入规则、`app0:` 运行时映射和可写路径边界。
- `runtime-flow.md`：当前 `main()` 从日志、vitaGL、SO 加载到 `JNI_OnLoad` 的执行顺序。
- `elf-loader.md`：`dynalib_t`、Android ELF/SO 映射、relocation、自动符号解析和初始化。
- `symbol-resolution.md`：`symbol.def`、构建期 Python 生成头文件、loader 内二分查找和批量依赖绑定。
- `bridges-eabi-libc.md`：EABI、Bionic/libc、pthread、网络、文件路径兼容。
- `bridges-android-jni.md`：Android 日志、AssetManager、JavaVM/JNIEnv fake runtime 和 Cocos 平台方法。
- `bridges-graphics-audio-cocos.md`：OpenGL、shader 转换、音频 provider 和固定版本机器码补丁。
- `utils-kubridge-logging.md`：工具函数、UDP/file logger、调试设施和用户态 kuBridge 包装。
- `vita3k-debugging.md`：Vita3K 的 `sceClibPrintf`、TTY、SceLibDbg、HLE SVC/NID 分发和内核日志接口。
- `vitagl-local-source.md`：历史读取过的本地 vitaGL 源码知识。
- `known-issues.md`：当前仍可从源码直接观察到的缺陷、占位实现和版本耦合。
- `working-tree.md`：当前分支、提交、工作区状态、构建缓存和产物现场。

## 当前主机路径

Windows 工作区：

```text
M:\Projects\PSV Projects\VitaArc
```

WSL 对应路径：

```text
/mnt/m/Projects/PSV Projects/VitaArc
```

构建目录：

```text
M:\Projects\PSV Projects\VitaArc\build
```

## Vita 运行时路径

VPK 安装后的应用根目录是只读设备 `app0:`。APK 内容被打包到 VPK 的 `package/`，所以当前路径定义为：

```text
app0:package/
app0:package/assets
app0:package/lib/armeabi-v7a/
app0:package/obb/
```

主 Android 原生库：

```text
app0:package/lib/armeabi-v7a/libcocos2dcpp.so
```

可写数据不能放在 `app0:`，仍使用：

```text
ux0:vitaarc/boot.log
ux0:vitaarc/persistent/data/
ux0:vitaarc/memdump.bin
ux0:vitaarc/shader/1.glsl
```

SO 固定映射基址：

```text
0x98000000
```

## 当前 APK 输入

`third-party/arcaea/` 当前存在：

```text
arcaea_1.6.1c.apk
arcaea_1.9.0c.apk
arcaea_2.0.0c.apk
arcaea_3.0.0c.apk
```

当前 `build/CMakeCache.txt` 明确选择：

```text
ARCAEA_VERSION=1.6.1c
```
