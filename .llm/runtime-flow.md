# Runtime flow

## `setup_performance_profile`

`src/init.c` 定义：

```text
ARM       444 MHz
Bus       222 MHz
GPU       222 MHz
GPU Xbar  166 MHz
```

当前 `main()` 没有调用该函数。

## 1. 启动日志

```c
log_begin(PATH_TO_LOGFILE);
```

`PATH_TO_LOGFILE` 是：

```text
ux0:vitaarc/boot.log
```

当前 `LOG_OVERUDP` 已定义，所以实际不会打开该文件，而是初始化网络并向：

```text
10.20.0.227:23333
```

发送 UDP。`tools/udp_logger.py` 默认监听 `0.0.0.0:23333`。

## 2. vitaGL 配置和初始化

```c
vglSetupRuntimeShaderCompiler(
    SHARK_OPT_UNSAFE,
    SHARK_ENABLE,
    SHARK_ENABLE,
    SHARK_ENABLE);
```

buffer：

```text
VDM       128 KiB
Vertex      2 MiB
Fragment  512 KiB
USSE       16 KiB
```

当前会调用：

```c
vglInitExtended(
    0,
    960,
    544,
    24 * 1024 * 1024,
    SCE_GXM_MULTISAMPLE_NONE);
```

此前该调用曾因参数顺序/语义问题崩溃；当前源码使用上述顺序并已通过编译，但知识库没有真机运行成功证据。

首帧 `glClear/glClearColor/vglSwapBuffers` 仍被注释。

## 3. 加载 Android 主库

```c
dynalib_t *_cocos =
    loader_load_library(LIBRARY_LIBCOCOS2DCPP);
```

展开路径：

```text
app0:package/lib/armeabi-v7a/libcocos2dcpp.so
```

该文件来自 VPK 内：

```text
package/lib/armeabi-v7a/libcocos2dcpp.so
```

加载失败记录 fatal 并跳到 `exit_program`。

## 4. loader 内自动符号解析

`loader_load_library` 内：

1. 检查文件存在和大小。
2. 读原始 ELF image。
3. 固定映射 PT_LOAD segments 到 `0x98000000`。
4. 处理支持的 ARM relocations。
5. 调用 `loader_symbol_ref`。
6. `loader_symbol_ref` 使用生成的 `__bridge_symbols` 自动绑定所有已知未定义符号。
7. 返回 clone handle。

已经加载的同名 library 返回前也会再次执行 `loader_symbol_ref`。

`main()` 不再依次调用 EABI/Android/libc 普通 symbol patch 函数。

## 5. library-specific patch

```c
bridge_patch_gl(_cocos);
bridge_patch_cocos2dx(_cocos);
bridge_audio_provider(_cocos);
```

这些处理包含：

- 固定偏移机器码 patch。
- Cocos2d-x 特定逻辑跳过。
- AudioManager hook。
- AudioProvider C++ import 兼容。

普通 GL/libc/Android imports 已由统一 symbol table 解析，但这些 library-specific patch 仍需显式调用。

## 6. `.init_array`

```c
loader_init_library(_cocos);
```

loader 取得 `.init_array` section，以 4 字节函数指针逐项调用非空项。

## 7. `JNI_OnLoad`

```c
bridge_call_jni_main(_cocos);
```

流程：

1. `loader_get_proc_address(library, "JNI_OnLoad")`。
2. 找不到时记录 warning 并返回。
3. dump：

```text
address 0x98000000
length  0xA60AA0
file    ux0:vitaarc/memdump.bin
```

4. 调用：

```c
on_load(jni_runtime_get_java_vm(), NULL);
```

5. 记录返回版本。
6. 返回值不是 JNI 1.1/1.2/1.4/1.6 时记录 warning。

## 8. Cocos JNI 导出

以下 wrapper 存在且会检查 symbol 是否为空：

- `bridge_jni_cocos_native_init`
- `bridge_jni_cocos_init_app`
- `bridge_jni_cocos_set_app_version`
- `bridge_jni_cocos_set_device_id`

当前 `main()` 中 Cocos init/device/native init 仍被注释，所以实际只调用 `JNI_OnLoad`。

## 9. 退出路径

```c
exit_program:
  for (;;);

log_end();
return 0;
```

无限循环使 `log_end()` 和正常 return 不可达。当前也没有：

- `loader_free_library(_cocos)`。
- `vglEnd()`。
- 网络 shutdown。
- 音频对象释放。
