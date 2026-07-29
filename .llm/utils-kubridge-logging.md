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

## 越权 memset

`kuKernelCpuUnrestrictedMemset`：

1. `malloc(len)` 临时缓冲区。
2. 用户态 `memset(temp, value, len)`。
3. `kuKernelCpuUnrestrictedMemcpy(dst, temp, len)`。
4. `free(temp)`。

函数声明返回 `int`，但没有返回语句。

大段清零会额外分配相同大小的临时内存。

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
#define UDP_HOST "192.168.1.2"
```

因此日志发送到固定局域网地址 `192.168.1.2:23333`。

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

分配给 `sceNetInit` 的 65535 字节内存没有保存到可释放字段。`logUdpBuffer` 全局变量已声明但未使用。

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

当前 shader wrapper 使用：

```c
logPrintf(gl_shader);
logPrintf(cg_shader);
```

如果 shader 文本包含 `%`，会被解释成格式占位符。

## `logEnd`

先将 `logStarted=false`，再调用 `logI(TAG, "Log stop.")`。因为 logger 已被标记停止，该消息不会发送。

UDP 分支只关闭 socket：

- 不调用 `sceNetCtlTerm`
- 不调用 `sceNetTerm`
- 不释放 network init memory
- 不卸载 sysmodule

## kuBridge 导出

内核模块导出 syscall：

- `kuKernelAllocMemBlock`
- `kuKernelFlushCaches`
- `kuKernelCpuUnrestrictedMemcpy`

### `module_start`

通过 `module_get_export_func` 从 `SceSysmem` 解析 cache 操作函数，并针对不同 firmware 尝试两套 NID：

- D-cache writeback/invalidate
- I-cache invalidate
- I-cache/L2 writeback/invalidate

实际 `kuKernelFlushCaches` 调用：

- D-cache writeback/invalidate
- I-cache invalidate

I-cache/L2 调用被注释。

输入地址按 32 字节向下对齐，长度向上覆盖到 32 字节边界。

### `kuKernelAllocMemBlock`

1. `ENTER_SYSCALL`。
2. 从用户态复制 32 字节 name 到内核栈。
3. 从用户态复制完整 options。
4. `ksceKernelAllocMemBlock`。
5. 将内核 UID 转换为当前进程 user UID。
6. `EXIT_SYSCALL`。

调用者传 `opt=NULL` 时仍尝试 `ksceKernelMemcpyUserToKernel(&k_opt, opt, sizeof(k_opt))`。VitaArc loader始终传非空 option。

### `kuKernelCpuUnrestrictedMemcpy`

1. 保存 CP15 DACR。
2. 将 DACR 写成 `0x15450FC3`。
3. 对每个字节执行 ARM `ldrbt` 和 `strbt`。
4. 恢复原 DACR。
5. 返回 0。

它逐字节复制，速度低，但用于绕过用户态页访问权限，向 RX 内存和固定地址写入代码/数据。

函数使用 `src + i`/`dst + i` 的 void pointer arithmetic，依赖 GCC 扩展。

