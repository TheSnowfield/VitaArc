# Vita3K guest logging, HLE imports and SVC dispatch

## 调研基线

读取的 Vita3K 主仓库 commit：

```text
5014b525e4508710933a131c10e4b5b4f8f7a2a2
2026-07-28
```

本地检查的 VitaSDK 位于：

```text
/usr/local/vitasdk
```

已确认以下 stub archive 中存在对应导出：

```text
libSceLibKernel_stub.a  -> sceClibPrintf
libSceLibDbg_stub.a     -> sceDbgLoggingHandler
```

## `sceClibPrintf`

VitaSDK 头：

```c
#include <psp2/kernel/clib.h>

int sceClibPrintf(const char *fmt, ...);
```

链接：

```text
SceLibKernel_stub
```

NID：

```text
sceClibPrintf = 0xFA26BC62
```

当前 Vita3K HLE 实现：

1. 创建 1 KiB `std::vector<char>`。
2. 取得当前 guest thread。
3. 使用 Vita3K 自己的 guest varargs formatter。
4. `LOG_INFO("{}", buffer.data())`。
5. 返回 `SCE_KERNEL_OK`。

因此这是 VitaArc 向 `vita3k.log` 输出调试日志的最直接接口：

```c
sceClibPrintf("[VitaArc] base=%p\n", base);
```

它是正式 Vita 用户态导出，不需要利用内核接口。Vita3K 日志 threshold 必须允许 INFO，CLI 可用：

```text
--log-level 2
```

更低数值 Trace/Debug 也会包含 INFO。

当前 VitaArc CMake 尚未显式链接 `SceLibKernel_stub`。

## stdout/TTY

Vita3K 的 `sceKernelGetStdout`/`sceKernelGetStderr` 打开 `tty0:`。`sceIoWrite` 调用 `write_file`。

TTY write 行为：

- `redirect_stdio=true`：原字节写入宿主 `std::cout`。
- `redirect_stdio=false`：移除末尾换行后，以 TRACE 输出：

```text
*** TTY: <guest text>
```

CLI：

```text
--console
-z
```

会启用 console mode，并作为 I/O 初始化的 `redirect_stdio` 参数。

非 console 模式看 TTY 日志需要：

```text
--log-level 0
```

用户态示例：

```c
SceUID fd = sceKernelGetStdout();
sceIoWrite(fd, text, length);
```

这条路线需要对应 process manager/I/O imports。

## `SceLibDbg`

VitaSDK：

```c
#define SCE_DBG_COMPONENT "VitaArc"
#include <psp2/libdbg.h>

SCE_DBG_LOG_INFO("value=%d", value);
```

链接：

```text
SceLibDbg_stub
```

官方 usage 还标记：

```text
SCE_SYSMODULE_DBG
```

`SCE_DBG_LOG_*` 宏在定义 `NDEBUG` 时展开为空。需要 release build 仍输出时，可以直接调用 `sceDbgLoggingHandler`，或者使用 `sceClibPrintf`。

当前 Vita3K `sceDbgLoggingHandler`：

- 记录 severity。
- 可记录 component。
- 可记录 source file 和 line。
- 使用 1 KiB guest formatter buffer。
- 最终统一以 Vita3K `LOG_INFO` 输出。

`sceDbgSetMinimumLogLevel`、`sceDbgSetBreakOnErrorState` 和 `sceDbgSetBreakOnWarningState` 当前在 Vita3K 中仍是 `UNIMPLEMENTED()`。

## `ksceDebugPrintf`

NID：

```text
ksceDebugPrintf = 0x391B74B7
```

当前 Vita3K `SceDebugForDriver` 实现直接转发：

```text
ksceDebugPrintf -> sceClibPrintf
```

其他 assertion/panic/TTY-info 相关 kernel debug exports 多数仍未实现。

正常 Vita 用户态程序不应导入 `ksce*` driver symbol。对于普通日志，它和 `sceClibPrintf` 在 Vita3K 中最终效果相同，没有调用内核导出的必要。

## Vita3K HLE import stub

Vita3K 给未解析 HLE import 写入 ARM stub：

```asm
svc #0
mov pc, lr
.word NID
```

机器码布局：

```text
0xEF000000  svc #0
0xE1A0F00E  mov pc, lr
NID
```

同样布局也用于 Vita3K 动态生成的 HLE vtable entries。

## SVC 分发

Dynarmic 的 `CallSVC(uint32_t svc)`：

1. 设置 `cpu->svc_called = true`。
2. 保存 immediate 到 `cpu->svc`。
3. HaltExecution。

guest thread run loop 看到 `svc_called` 后：

```text
nid = *(uint32_t *)(read_pc(cpu) + 4)
kernel.call_import(cpu, nid, thread_id)
```

当前这条 run-loop 路径没有检查保存的 SVC immediate；实际分发依据 stub 内 NID。

`call_import`：

1. 可选记录 HLE import trace。
2. `resolve_import(nid)` 在 Vita3K 全局 HLE 表查找函数。
3. 命中后直接调用宿主 C++ wrapper。
4. 未命中时把 guest `r0` 写 0，并记录 missing NID。

在该分发路径中没有按导入 library 或 user/kernel privilege 再做隔离。

## emulator-only “后门”性质

可以手工构造上述 ARM stub，把 NID 写成 `sceClibPrintf` 或 `ksceDebugPrintf`，直接触发 Vita3K 全局 HLE dispatcher。这可以绕过正常 SELF import declaration，也可能触达正常用户态不能导入的 HLE NID。

该行为是 Vita3K HLE import 实现细节，不是稳定公开 API：

- 真机不能依赖。
- Vita3K 更新可改变 stub 或 dispatcher。
- ARM/Thumb state、PC 位置和 varargs ABI 必须完全匹配。
- 调用 kernel NID 不提供比 `sceClibPrintf` 更好的日志能力。

项目日志优先级：

1. `sceClibPrintf`：最简单，同时保留真实 Vita export 语义。
2. stdout/TTY + Vita3K `-z`：实时宿主控制台。
3. `sceDbgLoggingHandler`：需要 component/file/line 元数据。
4. 手工 HLE SVC/NID stub：只用于研究 Vita3K dispatcher 或权限边界。

## Vita3K 日志文件

Vita3K logging 初始化始终增加文件 sink：

```text
vita3k.log
```

Windows 非 portable 模式的 log path 是 Vita3K 可执行文件目录。存在 `portable/` 目录时 log path 是 portable 根。

Linux：

```text
$XDG_CACHE_HOME/Vita3K/vita3k.log
```

未设置 `XDG_CACHE_HOME`：

```text
~/.cache/Vita3K/vita3k.log
```

Android 使用应用 external storage root。

## 与当前 VitaArc logger 的关系

当前 `src/logcat/logcat.c` 编译期定义 `LOG_OVERUDP`，发送到：

```text
10.20.0.227:23333
```

Vita3K 调试接口可作为第二 sink 或 emulator build 专用 sink，不需要移除 UDP logger。

把格式化后的 VitaArc `log_buffer` 交给 `sceClibPrintf("%s", log_buffer)` 可以让同一条日志同时进入 UDP 和 `vita3k.log`；实施时需要：

- 加 `SceLibKernel_stub`。
- 避免把已格式化字符串直接当 format。
- 处理现有 logger buffer 长度缺陷。
