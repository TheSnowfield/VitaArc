# Utilities, logging, debugging and kuBridge

## 文件工具

### `utilFileExists`

使用：

```c
sceIoGetstat(path, &status) >= 0
```

返回本地 `bool`。

### `utilGetFileSize`

1. `sceIoOpen(path, SCE_O_RDONLY, 0777)`
2. `sceIoLseek(fd, 0, SCE_SEEK_END)`
3. `sceIoClose(fd)`
4. 将结果作为 `uint32_t` 返回

未检查 open 和 seek 错误；负错误码转换成 `uint32_t` 后可能成为大正数。

### `utilsReadFileAll`

当前代码：

```c
if (utilFileExists(path))
    return false;
```

即文件存在时立即失败；条件与函数目的相反。

文件不存在时继续尝试打开。只有 buffer 非空检查。

返回值也不检查 `sceIoOpen`/`sceIoRead`。

旧 OpenGL shader binary 加载路径曾调用此函数；当前 `_glShaderSource` 改成 GLSL→Cg 后不再调用。

## 字符串工具

`utilGetFileName` 用路径最后一个分隔符取得文件名，供 SO loader 按 basename 查找已加载库。

`utilUcharLen` 按 16-bit 字符遍历到 0，供伪 JNI `GetStringLength` 使用。

## 用户态 kuBridge 包装

当前仓库内的 `src/bridges/kubridge.c` 只实现：

```c
int kuKernelCpuUnrestrictedMemset(void *dst, int val, size_t len)
```

行为：

1. `malloc(len)` 临时缓冲区。
2. 用户态 `memset(temp, value, len)`。
3. `kuKernelCpuUnrestrictedMemcpy(dst, temp, len)`。
4. `free(temp)`。
5. 返回 `kuKernelCpuUnrestrictedMemcpy` 的结果。

大段清零会额外分配相同大小的临时内存。

真实内核插件来自外部 `kubridge_stub`/`kubridge.h`，当前顶层 CMake 不构建 `third-party/kubridge`。

## Debug helpers

### `debugPrintMemoryBlock`

- 逐字节输出十六进制。
- 默认 alignment 为 16。
- 每 16 字节换行。

### `debugMemoryDump`

- `sceIoOpen(file, SCE_O_CREAT | SCE_O_WRONLY, 0777)`
- 默认直接 `sceIoWrite` 用户态内存。
- 没有 `SCE_O_TRUNC`，覆盖较短 dump 时可能保留旧文件尾部。
- 未检查 open/write。

可选 `MEMDUMP_IN_KERNEL` 路径：

- 申请 0x1000 临时块。
- 使用 kuBridge 分块复制。
- 每块写入文件。

当前未定义 `MEMDUMP_IN_KERNEL`。

### `debugBreakPoint`

向目标地址写入六字节机器码：

```text
mov r0, #0
ldr r0, [r0]
```

用途是人为触发空指针异常。

## 日志配置

当前编译期定义：

```c
#define LOG_OVERUDP
#define UDP_PORT 23333
#define UDP_HOST "10.20.0.227"
```

因此日志发送到固定局域网地址 `10.20.0.227:23333`。

仓库提供 `tools/udp_logger.py`，默认监听 `0.0.0.0:23333`。

若删除 `LOG_OVERUDP`，则使用 `logBegin` 参数指定的设备文件，当前为 `ux0:vitaarc/boot.log`。

## `logBegin` UDP 路径

1. 检查/加载 `SCE_SYSMODULE_NET`。
2. 分配 65535 字节网络内存。
3. `sceNetInit`。
4. `sceNetCtlInit`。
5. 获取本机 IP。
6. 转换本机 IP 到 `sNetInAddr`，但之后没有使用该变量。
7. 转换固定 UDP host。
8. 创建 UDP socket。
9. `sceNetConnect` 到 host/port。
10. socket handle 大于 0 时设置 `logStarted=true`。

分配给 `sceNetInit` 的 65535 字节内存没有保存到可释放字段。

没有检查大多数网络初始化返回值。

## 日志级别

```c
VERBOSE = 0
INFORMATION = 1
WARNING = 2
ERROR = 3
FATAL = 4
```

字符：

```text
V I W E F
```

`TAG` 宏使用 `__FILE__` 最后一个 `/` 后的文件名。Windows 风格反斜杠不会被该宏识别，但实际 Vita 编译器传入的路径格式取决于构建环境。

## `logBase`

本地栈 buffer 为 1024 字节。

写入顺序：

1. `snprintf("[level] [tag] ...")`
2. `vsnprintf(format, args)`
3. `snprintf("\n")`

每次调用都传 `sizeof(logBuffer)`，而不是剩余空间。`snprintf`/`vsnprintf` 返回“本应写入”的长度，截断时 `logPosition` 仍可能推进到 buffer 外，最后发送/写入的长度也可能越界。

UDP 模式每条结构化日志后 `sceKernelDelayThread(50)`。

## `logPrintf`

- 使用 1024 字节 buffer。
- 将调用者 format 直接传给 `vsnprintf`。
- 没有自动 level/tag/newline。
- UDP 模式直接 `sceNetSend`。

当前 `_glShaderSource` 使用 `logPrintf("%s", shader)` 形式输出 shader 文本，已经不再把 shader 文本本身当 format string。

## `logEnd`

先将 `logStarted=false`，再调用 `logI(TAG, "Log stop.")`。因为 logger 已被标记停止，该消息不会发送。

UDP 分支只关闭 socket：

- 不调用 `sceNetCtlTerm`
- 不调用 `sceNetTerm`
- 不释放 network init memory
- 不卸载 sysmodule

当前 `main()` 在 `ExitProgram` 进入无限循环，正常不会执行到 `logEnd()`。
