# VitaArc knowledge base

本目录记录对当前 VitaArc 工作区和本地 vitaGL 工作区的已读取知识。文档按实现领域拆分，不替代源代码，不假定远端仓库或最新上游状态。

## 文档分类

- `project-layout.md`：仓库目录、文件职责、外部输入和输出。
- `build-and-packaging.md`：VitaSDK、CMake、链接库、VPK 和本地构建产物。
- `runtime-flow.md`：从 `main()` 到 Cocos2d-x 原生初始化的精确调用顺序。
- `elf-loader.md`：Android ELF/SO 装载、固定地址映射、重定位、符号查找和补丁机制。
- `bridges-eabi-libc.md`：EABI、Bionic/libc、pthread 和文件路径兼容。
- `bridges-android-jni.md`：Android 日志、AssetManager、JavaVM、JNI 和 Cocos 平台方法。
- `bridges-graphics-audio-cocos.md`：OpenGL、着色器、音频提供器及特定版本二进制补丁。
- `utils-kubridge-logging.md`：工具函数、日志系统、调试设施及内核 kuBridge。
- `vitagl-local-source.md`：`M:\Projects\PSV Projects\vitaGL` 中与 VitaArc 直接相关的实现。
- `working-tree.md`：当前分支、提交、未提交改动、未跟踪文件和旧构建现场。
- `known-issues.md`：从当前代码直接观察到的缺陷、未实现接口和版本耦合。
- `source-inventory.md`：已读取的源文件清单及职责。

## 路径约定

- VitaArc 根目录：`M:\Projects\PSV Projects\VitaArc`
- 本地 vitaGL 根目录：`M:\Projects\PSV Projects\vitaGL`
- Vita 设备数据根目录：`ux0:vitaarc`
- Android 主原生库：`ux0:vitaarc/library/armeabi-v7a/libcocos2dcpp.so`
- Android 主原生库运行时固定映射基址：`0x98000000`

