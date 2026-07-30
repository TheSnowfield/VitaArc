# Runtime flow

## `setupPerformanceProfile`

`src/main.c` 定义 `setupPerformanceProfile()`：

- ARM：444 MHz
- Bus：222 MHz
- GPU：222 MHz
- GPU Xbar：166 MHz

当前 `main()` 没有调用此函数。

## `main()` 当前顺序

### 1. 启动日志

调用：

```c
logBegin(PATH_TO_LOGFILE);
```

虽然参数是 `ux0:vitaarc/boot.log`，当前 `logcat.c` 定义了 `LOG_OVERUDP`，因此实际走 UDP 分支，不创建日志文件。

当前 UDP host 是：

```text
10.20.0.227:23333
```

仓库提供 `tools/udp_logger.py`，默认监听 `0.0.0.0:23333`。

### 2. 配置 vitaGL 运行时编译器和缓冲区

调用：

```c
vglSetupRuntimeShaderCompiler(
    SHARK_OPT_UNSAFE,
    SHARK_ENABLE,
    SHARK_ENABLE,
    SHARK_ENABLE);
```

随后设置：

- VDM：`128 * 1024`
- Vertex buffer：`2 * 1024 * 1024`
- Fragment buffer：`512 * 1024`
- USSE：`16 * 1024`

这些设置发生在 SO 装载前。

`vglInitExtended(...)`、第一帧 `glClear/glClearColor/vglSwapBuffers` 当前整段被注释掉。因此当前入口只配置 shader compiler 参数和若干全局 buffer size，不实际初始化 vitaGL/GXM。

### 3. 加载 Android 主库

调用：

```c
dynalib_t *hLibCocos2dx =
    solibLoadLibrary(LIBRARY_LIBCOCOS2DCPP);
```

路径来自：

```text
ux0:vitaarc/library/armeabi-v7a/libcocos2dcpp.so
```

失败时记录 fatal 日志并跳转 `ExitProgram`。

源码保留了 Crashlytics、FMOD 和 FMOD provider 路径常量，但当前 `main()` 只加载 `libcocos2dcpp.so`。

### 4. 安装导入符号和机器码补丁

顺序固定为：

1. `bridgePatchEABI`
2. `bridgePatchAndroid`
3. `bridgePatchLibC`
4. `bridgePatchGL`
5. `bridgePatchCocos2DX`
6. `bridgeAudioProvider`

前三类和 OpenGL/音频主要通过 `.rel.dyn`、`.rel.plt` 中的动态符号名替换导入地址。

`bridgePatchCocos2DX` 和 `bridgeAudioProvider` 还会按固定偏移修改映射后的机器码。

当前 `main.c` 已包含：

```c
#include "bridges/audio/audio.h"
```

### 5. 执行 ELF `.init_array`

调用：

```c
solibInitLibrary(hLibCocos2dx);
```

装载器取得 `.init_array`，按 4 字节函数指针遍历并调用非空项。

### 6. 调用 `JNI_OnLoad`

`bridgeCallJNIMain`：

1. 用 `solibGetProcAddress` 查找 `JNI_OnLoad`。
2. 将从 `0x98000000` 开始的 `0xA60AA0` 字节写入 `ux0:vitaarc/memdump.bin`。
3. 以伪 `JavaVM` 和 `NULL` reserved 参数调用 `JNI_OnLoad`。

函数未检查 `JNI_OnLoad` 查找结果是否为空。

### 7. Cocos JNI 导出当前未调用

`main.c` 中以下调用目前被注释：

- `bridgeJNICocosInitApp(hLibCocos2dx)`
- `bridgeJNICocosSetDeviceId(hLibCocos2dx)`
- `bridgeJNICocosNativeInit(hLibCocos2dx, 960, 544)`

`bridgeJNICocosSetAppVersion()` 存在，使用字符串 `"v6.1.6"`，但当前 `main()` 没有调用它。

### 8. 退出路径

`ExitProgram` 当前是：

```c
for(;;);

logEnd();
return 0;
```

因此无论成功或失败跳到 `ExitProgram`，都会进入无限循环；`logEnd()` 和 `return 0` 正常不可达。

当前入口没有显式调用 `vglEnd()`，没有调用 `solibFreeLibrary()`，也没有自己实现帧循环。
