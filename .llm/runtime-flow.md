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

### 2. 配置 vitaGL 运行时编译器

调用：

```c
vglSetupRuntimeShaderCompiler(
    SHARK_OPT_UNSAFE,
    SHARK_ENABLE,
    SHARK_ENABLE,
    SHARK_ENABLE);
```

这只写入 vitaShaRK 的编译选项；vitaGL 在第一次 `glCompileShader` 时按需启动编译器。

### 3. 设置 GXM/vitaGL 内部缓冲区

- VDM：`128 * 1024`
- Vertex buffer：`2 * 1024 * 1024`
- Fragment buffer：`512 * 1024`
- USSE：`16 * 1024`
- Vertex pool：`48 * 1024 * 1024`

这些调用发生在 `vglInitExtended` 前。

本地 vitaGL 源码中 `vglSetVertexPoolSize()` 仅在编译 vitaGL 时定义 `HAVE_CIRCULAR_VERTEX_POOL` 才会修改值。默认 Makefile 未自动定义该宏。

### 4. 初始化 vitaGL

调用：

```c
vglInitExtended(
    0,
    960,
    544,
    24 * 1024 * 1024,
    SCE_GXM_MULTISAMPLE_NONE);
```

参数含义：

- legacy/pool size：0
- 宽：960
- 高：544
- RAM threshold：24 MiB
- MSAA：关闭

随后调用 `vglUseVram(GL_TRUE)`。本地 vitaGL 的此函数只设置后续分配使用的全局 `use_vram` 标志；因为调用发生在初始化之后，不会改变初始化阶段已经完成的内存池和默认资源分配。

### 5. 生成第一帧

依次调用：

```c
glClear(GL_COLOR_BUFFER_BIT);
glClearColor(1, 1, 1, 0.5);
vglSwapBuffers(GL_FALSE);
```

顺序是先 clear、后设置 clear color，因此本次 `glClear` 使用的是调用前的清除颜色状态；新颜色供后续 clear 使用。

### 6. 加载 Android 主库

调用：

```c
HSOLIB hLibCocos2dx =
    solibLoadLibrary("ux0:vitaarc/library/armeabi-v7a/libcocos2dcpp.so");
```

失败时记录 fatal 日志并跳转 `ExitProgram`。

源码保留了 Crashlytics、FMOD 和 FMOD provider 路径常量，但当前 `main()` 只加载 `libcocos2dcpp.so`。

### 7. 安装导入符号和机器码补丁

顺序固定为：

1. `bridgePatchEABI`
2. `bridgePatchAndroid`
3. `bridgePatchLibC`
4. `bridgePatchGL`
5. `bridgePatchCocos2DX`
6. `bridgeAudioProvider`

前三类和 OpenGL/音频主要通过 `.rel.dyn`、`.rel.plt` 中的动态符号名替换导入地址。

`bridgePatchCocos2DX` 和 `bridgeAudioProvider` 还会按固定偏移修改映射后的机器码。

`main.c` 当前没有包含 `bridges/audio/audio.h`，因此 `bridgeAudioProvider` 在调用点没有可见函数声明。

### 8. 执行 ELF `.init_array`

调用：

```c
solibInitLibrary(hLibCocos2dx);
```

装载器取得 `.init_array`，按 4 字节函数指针遍历并调用非空项。

### 9. 调用 `JNI_OnLoad`

`bridgeCallJNIMain`：

1. 用 `solibGetProcAddress` 查找 `JNI_OnLoad`。
2. 将从 `0x98000000` 开始的 `0xA60AA0` 字节写入 `ux0:vitaarc/memdump.bin`。
3. 以伪 `JNIInvokeInterface` 和 `NULL` reserved 参数调用 `JNI_OnLoad`。

函数未检查 `JNI_OnLoad` 查找结果是否为空。

### 10. 调用游戏/Cocos2d-x JNI 导出

顺序：

1. `Java_low_moe_AppActivity_initJVMPlatformUtils`
   - 参数是本地伪 `JNINativeInterface`。
2. `Java_low_moe_AppActivity_setDeviceId`
   - 参数包含 UTF-16 字面量 `"61616161"`。
3. `Java_org_cocos2dx_lib_Cocos2dxRenderer_nativeInit`
   - 传入 `960, 544`。

存在 `bridgeJNICocosSetAppVersion()`，使用字符串 `"v6.1.6"`，但当前 `main()` 未调用它。

### 11. 退出

到达 `ExitProgram`：

```c
logEnd();
return 0;
```

当前入口没有显式调用 `vglEnd()`，没有调用 `solibFreeLibrary()`，也没有自己实现帧循环。后续控制流依赖 `nativeInit` 内部行为。
