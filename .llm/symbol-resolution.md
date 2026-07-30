# Generated bridge symbols and automatic dependency resolution

## 目标

所有 Android ELF 未定义符号集中维护在 `src/bridges/symbol.def`。构建期生成一个 C 头文件，loader 每次加载 library 后自动把 `.rel.dyn` 和 `.rel.plt` 中能由本地 bridge 提供的依赖绑定到 VitaArc 函数。

不再需要每个 bridge 在 `main()` 中分别遍历自己的 `BRIDGEFUNC` 数组。

## `symbol.def`

格式：

```text
IMPORTED_ELF_SYMBOL|LOCAL_C_CALLBACK
```

示例：

```text
fopen|bridge_fopen
AAssetManager_open|a_asset_manager_open
glShaderSource|_gl_shader_source
_ZN13AudioProvider4initEff|audio_provider_init
```

支持空行和以 `#` 开头的注释。

生成器拒绝：

- 缺少或多于一个 `|` 的行。
- 空 symbol/callback。
- 非法 C callback identifier。
- 重复 imported symbol。
- 完全为空的 definition。

callback 前意外出现的 `&` 会被去掉。

## 生成头

命令：

```text
python tools/generate_symbols.py
  src/bridges/symbol.def
  build/generated/bridge_symbols.h
```

生成器：

1. 解析所有 definition。
2. 按 imported symbol 排序。
3. 生成所有 callback 所需 include。
4. 输出：

```c
static const patch_func_t __bridge_symbols[] = {
  {"symbol", (uintptr_t)&callback},
  ...
};
```

5. 文件内容未变化时不重写，避免无意义重编译。

当前生成结果：

```text
Symbol count: 354
```

## loader 集成

`src/loader.c` 直接 include 生成头。

静态函数 `__find_bridge_symbol` 对已排序数组执行二分查找，区间为 `[left, right)`。

静态函数 `__resolve_symbols`：

1. 检查 library、table、count。
2. 遍历 section headers。
3. 只处理 `.rel.dyn` 和 `.rel.plt`。
4. relocation symbol index 必须小于 `elf_dynamic_symbol_count`。
5. 只处理 `st_shndx == SHN_UNDEF`。
6. 从 `.dynstr` 取得 imported name。
7. 二分查找本地 bridge。
8. relocation target：

```text
0x98000000 + r_offset
```

9. 直接写入 `bridge_proc`。
10. 统计实际绑定的 relocation 数并写日志。

外部入口：

```c
void loader_symbol_ref(dynalib_t *library)
```

它把生成数组和编译期数组长度交给 `__resolve_symbols`。不再暴露 `bridge_symbol_count`，也没有独立生成 `.c` 文件。

## 调用位置

新 library：

```text
loader_load_sections
loader_symbol_ref
loader_clone_handle
```

已经加载的同名 library：

```text
loader_symbol_ref
loader_clone_handle
```

所以每次 `loader_load_library` 返回前都会重新保证本地符号依赖已绑定。

## library-specific patch

统一 symbol binding 之后，`main()` 仍显式调用：

```c
bridge_patch_gl(_cocos);
bridge_patch_cocos2dx(_cocos);
bridge_audio_provider(_cocos);
```

这些调用保留的原因是它们包含固定地址机器码补丁、hook 或 library-specific 初始化，而不是普通未定义符号解析。

## 写入语义

当前解析器直接写 relocation target：

```c
*relocation_address = bridge->bridge_proc;
```

没有：

- kuBridge memcpy。
- 页权限检查。
- 每次写入后的 cache flush。
- symbol version/binding/type检查。
- 对不同 library image base 的支持。

当前所有 target 仍假定固定映射基址 `0x98000000`。
