# Known incomplete behavior and directly observed defects

本文档只记录从当前源码和已记录的 vitaGL 行为直接可见的行为。

## Runtime currently stops after `JNI_OnLoad`

`main()` 只调用 `bridgeCallJNIMain(hLibCocos2dx)`。

以下 Cocos JNI 启动调用被注释：

- `bridgeJNICocosInitApp`
- `bridgeJNICocosSetDeviceId`
- `bridgeJNICocosNativeInit`

`ExitProgram` 是无限循环：

```c
for(;;);
```

因此 `logEnd()`、`return 0` 和任何正常清理都不可达。

## vitaGL/GXM initialization disabled

`main()` 当前只配置 `vglSetupRuntimeShaderCompiler` 和若干 buffer size setter。

`vglInitExtended(...)`、首帧 `glClear/glClearColor/vglSwapBuffers` 整段被注释。若目标库在 `JNI_OnLoad` 或后续路径调用 OpenGL/vitaGL API，图形后端可能尚未初始化。

## Shader source 生命周期

VitaArc `_glShaderSource`：

1. 分配 `cg_shader`。
2. 调用 vitaGL `glShaderSource(handle, 1, &cg_shader_source, NULL)`。
3. 立即 `free(cg_shader)`。

历史读取的本地 vitaGL `glShaderSource` 不复制 source，只保存该地址，直到稍后的 `glCompileShader` 使用。

结果：`glCompileShader` 读取已释放内存，属于 use-after-free。

## Shader `length[]` 未处理

Android OpenGL ES `glShaderSource` 允许每段由 `length[i]` 指定长度且不以 NUL 结束。

VitaArc 忽略 `length`，对每段 `strlen`。非 NUL source 会越界读；包含内嵌 NUL 时会截断。

## Shader output file handling

`_glShaderSource` 每次写固定文件：

```text
ux0:vitaarc/shader/1.glsl
```

不区分 shader handle/type，不检查目录是否存在，也不检查 `fopen` 是否成功。若 `fopen` 返回 `NULL`，随后的 `fwrite`/`fclose` 会崩溃。

## Shader output buffer

GLSL→Cg converter 固定 `malloc(0x8000)`，没有完整容量检查。输入、类型替换、main 参数扩展或 varying 数量可导致输出越界。

## Shader parser assumptions

转换器通过 `strstr` 匹配：

- 可误匹配注释、标识符子串和宏。
- 不处理不同空格/换行/qualifier组合。
- 多处查找失败后仍做 `+ offset`。
- 固定 main 函数拼写仅覆盖三种形式。
- varying/attribute semantic 分配是手工规则。

## `glBindAttribLocation`

当前 bridge 包装只记录，不执行绑定，同时固定偏移补丁跳过 Cocos 预绑定流程。

可能后果：

- Cocos 固定 attribute index 与 vitaGL 自动分配不一致。
- `glVertexAttribPointer(index, ...)` 的 index 与 GXM parameter resource mapping 不一致。
- 绘制时 attribute 数据错位或 vertex program patch 失败。

日志调用参数顺序当前是正确的：

```c
logI(TAG, "_glBindAttribLocation(%d, %d, %s)", program, index, name);
```

## `glGetShaderSource`

当前空实现不写任何输出。目标代码若依赖 source length/content，会读取未初始化 caller buffer。

## Renderbuffer/VAO/map buffer

Renderbuffer、VAO OES、map/unmap buffer 均为空或失败返回。Cocos2d-x 的 FBO depth/stencil、VAO cache、映射更新路径不能按 OpenGL ES 语义工作。

## SO 固定基址与多库模型冲突

loader允许 32 个库实例，但所有库都映射到 `0x98000000`。第二个不同库会与第一个地址空间冲突。

README目录列出了多份 `.so`，源码也有 Crashlytics/FMOD 常量，但当前装载器不是通用多库动态链接器。

## ELF 校验缺失

loader不验证：

- ELF magic
- ELF class/endianness
- ARM machine
- shared object type
- program/section table范围
- segment file range
- alignment有效性
- relocation target范围
- symbol string范围

损坏或不同格式文件可导致任意地址读写。

## `solibLoadSections` 返回值被忽略

`solibLoadSections` 返回 `bool`，但 `solibLoadLibrary` 调用后没有检查返回值，仍记录 “Load and relocated all sections.” 并返回克隆句柄。

## Segment memblock 泄漏

每个 `PT_LOAD` 申请的 memblock ID 未保存。释放时只释放原文件镜像，不释放运行时 RX/RW 段。

当前 `main` 没有调用 `solibFreeLibrary`，所以正常启动路径表现为泄漏而不是释放段。

## `.init_array` 地址假设

函数表地址用 `lpTextBase + sh_addr`。若 text base 已包含映射基址且 `sh_addr` 是 ELF virtual address，此计算可能重复/错误偏移，取决于目标 ELF layout。

`.init_array` 缺失时没有空检查。

## 重定位覆盖范围

仅支持：

- `R_ARM_ABS32`
- `R_ARM_RELATIVE`
- `R_ARM_GLOB_DAT`
- `R_ARM_JUMP_SLOT`

其他 relocation 只记录错误继续。TLS、COPY、ARM/Thumb call 等 relocation 不处理。

## 导入项安装写权限/cache

`solibInstallProc` 直接写 relocation target，不使用 kuBridge，也不检查 segment writable 状态，不刷新 cache。

## 固定偏移补丁版本耦合

以下偏移完全绑定特定 `libcocos2dcpp.so`：

- `0x69A342`
- `0x50E30E`
- `0x6E6AD4`
- audio hook `0x38FD1C`

没有在写入前检查原始指令签名或游戏版本。错误 SO 会被无条件修改。

## AssetManager为空

README要求复制 `assets`，但 AssetManager全部为空。依赖 Android NDK asset API 的资源无法读取。

`AAsset_getLength` 原型也不是标准返回类型。

## JNI 大面积为空

函数表只填一部分，填入的函数中也有大量固定伪对象或零值返回。目标库调用其他 JNI 方法会跳空指针。

## JNI string表示不一致

有的 `jstring` 是 UTF-16 字面量地址，有的是 UTF-8 `char *` 地址。`GetStringChars` 和 `GetStringUTFChars` 只是强制转换，无法保证同一对象两种访问都正确。

## JNI attach 不返回 env

`AttachCurrentThread` 和 `AttachCurrentThreadAsDaemon` 返回成功但不设置 `JNIEnv **`。

## `RegisterNatives` 不注册

`RegisterNatives` 返回 0，但实际调用表没有保存。

## Platform persistence为空

- `getStringForKey` 固定 `"NULL"`
- `getCocos2dxWritablePath` 固定 `"NULL"`
- `setStringForKey` 不写入
- integer/bool key方法没有实现

游戏设置、设备状态或持久化路径无法按 Android/Cocos预期工作。

## libc成功型占位

多项未实现系统调用返回 0，伪装成成功但不填输出或不执行操作，例如：

- `mkdir`
- `lstat`
- `fsync`
- `ftruncate64`
- `ioctl`
- `rmdir`
- `readlink`
- `utimes`
- `munmap`
- `nanosleep`

这种行为比明确失败更容易让上层继续使用未初始化数据或假定状态已变更。

## `clock_gettime`

把 Vita RTC raw tick 直接写入 `tv_sec`，没有按 tick frequency 换算，也把纳秒固定为 0。

## `/dev/urandom` fd 冲突

伪 fd 固定为 1，通常与 stdout 数字相同。目标库若同时操作普通 fd 1，`bridgeRead/bridgeClose` 会错误视为 urandom。

## 文件路径重定向边界

`bridgeFopen` 用 256 字节 buffer 和 `sprintf` 拼接，长路径可覆盖栈。

只有 `fopen` 被重定向。`open/stat/access/remove/rename/opendir` 等路径没有同样重定向，文件 API 间路径语义不一致。

## `utilsReadFileAll` 条件反转

文件存在时返回 false；文件不存在时尝试 open/read。

## `utilGetFileSize` 错误码类型

open/seek失败值可能转换成巨大 `uint32_t`，使 loader误判文件很大并申请异常内存。

## 日志 buffer长度

`logBase` 每一步都传完整 1024 长度，不传剩余空间；截断返回值会推进指针到 buffer 外。

Android log priority 直接转换成本地五级 enum，也可能造成 `logLvString[level]` 越界。

## UDP日志资源

- host 固定 `10.20.0.227`
- net memory 泄漏
- 无 net termination
- `logEnd` 在发 stop 日志前先禁用 logger
- 无法连接 UDP 时没有 file fallback

## Debug dump固定范围

`bridgeCallJNIMain` 每次 dump `0xA60AA0` 字节，不根据实际映射段范围，可能读取未映射区域，或少 dump/多 dump不同版本 SO。

## AudioProvider释放

provider/destructor 不释放内存。AudioManager失败路径也不 free 自身。

## 性能配置未使用

`setupPerformanceProfile` 定义但从不调用。

## 清理缺失

正常控制流没有：

- `vglEnd`
- `solibFreeLibrary`
- net shutdown
- AudioProvider释放
- shader转换资源的长期生命周期管理

## 构建验证缺失

当前工作区没有 `build/`，本次 `.llm` 对齐没有运行 VitaSDK 构建。
