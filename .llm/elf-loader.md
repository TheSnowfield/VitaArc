# ELF shared-object loader

## 对外类型和 API

当前 SO 句柄类型是 `dynalib_t *`，定义在 `src/bridges/loader.h`。

对外 API：

- `solibLoadLibrary(path)`
- `solibInitLibrary(library)`
- `solibFreeLibrary(library)`
- `solibGetProcAddress(library, symbol)`
- `solibInstallProc(library, symbol, destination)`
- `solibGetLibraryImageBase(library)`
- `solibFindLibrary(name)`
- `solibCloneHandle(library)`

内部 API：

- `solibFindEmptySlot()`
- `solibLoadSections(library)`
- `solibDebugPrintElfTable(library)`

## `dynalib_t`

内部实例保存：

- `nRefCount`
- `nSlotIndex`
- 64 字节库名缓冲区
- 128 字节完整路径缓冲区
- 原 ELF 文件镜像地址
- 原 ELF 文件镜像 Vita memblock ID
- ELF header
- program header 数组
- section header 数组
- symbol section 指针
- `.init_array` section header
- section string table
- dynamic symbol table
- dynamic string table
- dynamic symbol数量
- executable text base

全局最多 32 个内部库实例：

```c
#define MAX_LIBRARY 32
static uint32_t libraryLoaded = 0;
static dynalib_t *librarySlots[MAX_LIBRARY] = {NULL};
```

## `solibLoadLibrary`

### 输入校验

1. `utilFileExists(path)` 必须为真。
2. `utilGetFileSize(path)` 必须大于 0。
3. 通过最后一个路径组件得到库名。
4. 如果同名库已存在，返回同一个 `dynalib_t *` 并递增引用计数。

### 原始文件镜像

内部对象使用 `calloc(1, sizeof(*libraryNew))` 创建，因此引用计数和缓存字段初始为 0。

申请：

```c
sceKernelAllocMemBlock(
    "elf_image",
    SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
    PAGE_ALIGN(file_size),
    NULL);
```

然后：

- 取得 memblock base。
- `sceIoOpen` 原 SO。
- 将整个文件读进 memblock。
- 关闭文件。
- 在 32 个槽位中找空位。
- 保存路径、库名和槽位。
- 增加 `libraryLoaded`。
- 打印 ELF 调试信息。
- 调用 `solibLoadSections`。
- 返回 `solibCloneHandle(libraryNew)`，也就是同一指针并递增引用计数。

仍未检查 `sceIoOpen`/`sceIoRead` 返回值，也未验证实际读取字节数。

`strcpy` 直接写入固定 64/128 字节数组，没有长度保护。

## ELF 映射地址

运行时映射固定基址：

```c
uintptr_t lpLinearAddressBase = 0x98000000;
```

槽位系统允许多个库，但 `solibLoadSections`、`solibGetProcAddress`、`solibInstallProc` 和补丁器都固定使用这个运行时基址。第二个不同库会与第一个库地址空间冲突。

## Program header/segment 处理

遍历 `e_phnum`，只处理 `p_type == PT_LOAD`。

对每个 loadable segment：

1. 根据前一段结束地址计算 gap。
2. `nBlockSize = align(p_memsz + gap, p_align)`。
3. 填充 `SceKernelAllocMemBlockKernelOpt`：
   - `size = sizeof(...)`
   - `field_C = lpLinearAddress`
   - `attr = 0x01`
4. 若 `p_flags & PF_X`：
   - 通过 kuBridge 申请 `SCE_KERNEL_MEMBLOCK_TYPE_USER_RX`
5. 否则：
   - 通过 kuBridge 申请 `SCE_KERNEL_MEMBLOCK_TYPE_USER_RW`
6. 取得实际 memblock base。
7. 将 program header 中的 `p_vaddr` 原地增加 `0x98000000`。
8. 可执行段保存为 `lpTextBase`，并刷新 cache。
9. 使用 `kuKernelCpuUnrestrictedMemset` 清零整个分配块。
10. 从 ELF 原始文件镜像的 `p_offset` 复制 `p_filesz` 字节到运行地址。
11. 推进线性分配地址。

各运行时 segment 的 memblock ID 只存在于局部变量，未存入 `dynalib_t`，因此当前释放流程无法释放这些映射。

`solibLoadSections` 返回 `bool`。调用方记录 “Load and relocated all sections.”，但没有检查返回值。

## Section 扫描

从 section header string table 查找：

- `.dynsym`
- `.dynstr`
- `.init_array`

动态符号数为：

```c
dynsym_section.sh_size / sizeof(Elf32_Sym)
```

若 `.dynsym` 或 `.dynstr` 缺失，映射函数返回 `false`。

`.init_array` 缺失时没有错误处理；之后 `solibInitLibrary` 会解引用空 section pointer。

## ARM relocation

处理 section 名：

- `.rel.dyn`
- `.rel.plt`

relocation 表地址的计算使用：

```c
lpImageBase + section.sh_addr
```

待改写运行时地址：

```c
0x98000000 + relocation.r_offset
```

支持 relocation type：

### `R_ARM_ABS32`

```c
*target += 0x98000000 + OFFRST(symbol.st_value);
```

### `R_ARM_RELATIVE`

```c
*target = OFFRST(*target) + 0x98000000;
```

### `R_ARM_GLOB_DAT`

```c
*target = 0x98000000 + OFFRST(symbol.st_value);
```

### `R_ARM_JUMP_SLOT`

与 `R_ARM_GLOB_DAT` 相同。

未知 relocation 只记录错误，继续处理。

ARM/Thumb symbol bit 0 的说明写在源码注释中，但当前：

```c
#define OFFRST(x) (x)
```

即不清除 Thumb bit。被注释的原意是 `x & ~1`。

初次 relocation 阶段连 `SHN_UNDEF` 导入符号也会暂时写成 `0x98000000 + st_value`。后续 bridge 通过符号名覆盖实际导入目标。

## `.init_array`

初始化函数表地址计算：

```c
lpInternal->lpTextBase +
lpInternal->lpElfSectionInitArray->sh_addr
```

函数表按 4 字节一项遍历；非空函数指针直接调用。

这里假定 `.init_array` 的 `sh_addr` 相对 `lpTextBase`，而不是统一相对映射基址。

## `solibGetProcAddress`

遍历 dynamic symbol table，比较 `.dynstr + st_name`。

命中后返回：

```c
0x98000000 + OFFRST(st_value)
```

不检查：

- symbol binding/type
- `SHN_UNDEF`
- symbol 所属 segment
- 另一个库的基址

未命中返回 `NULL`。

## `solibInstallProc`

再次遍历 `.rel.dyn` 和 `.rel.plt`：

1. 用 relocation symbol index 找到 `Elf32_Sym`。
2. 比较动态字符串中的符号名。
3. 命中时直接将运行时 relocation target 写为 Vita 侧函数地址。

它不返回原地址，函数签名虽然是 `void *`，结尾始终 `return NULL`。

匹配同名符号的所有 relocation entries 都会被覆盖。

写入目标不经过 kuBridge copy，也没有在每次符号安装后显式 cache flush。

## 句柄克隆和查找

`solibFindLibrary`：

- 遍历 32 个槽位。
- 按库文件名而不是完整路径匹配。
- 命中时返回同一个 `dynalib_t *`。

`solibCloneHandle`：

- 递增 `library->nRefCount`。
- 返回同一个 `dynalib_t *`，不再分配单独句柄。

## 释放逻辑当前行为

`solibFreeLibrary`：

1. 空指针直接返回。
2. 若 `nRefCount > 0`，先递减；递减后仍大于 0 则返回。
3. 清除全局槽位。
4. 减少 `libraryLoaded`。
5. 释放原 ELF 文件镜像 memblock。
6. `free(library)`。

已经不再存在旧实现中“把 `SOINTERNAL *` 当 `HSOLIB` 递归传入”的错误。

仍存在的问题：

- 若 `nRefCount == 0` 时被调用，会直接释放。
- 映射后的 RX/RW segment memblocks 没有记录，也无法释放。
- 正常 `main()` 当前不会调用 `solibFreeLibrary`。

## Patcher 与装载器关系

`patchSymbols` 遍历 `BRIDGEFUNC` 数组，对每个条目调用 `solibInstallProc`。

所有按地址补丁都忽略 `dynalib_t` 的真实 image base，固定使用：

```c
uintptr_t lpImageBase = 0x98000000;
```

补丁类型：

- `patchUint16`
- `patchUint32`
- `patchThumb` → `patchUint16`
- `patchARM` → `patchUint32`
- `patchAddress` → 最多写 7 字节

写机器码使用 `kuKernelCpuUnrestrictedMemcpy`。

`hookStubProc` 在目标写 8 字节：

```text
E51FF004    LDR PC, [PC, #-4]
callback    callback absolute address
```

当前 hook 后没有显式调用 `kuKernelFlushCaches`。
