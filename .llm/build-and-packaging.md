# Build and packaging

## 工具链选择

顶层 `CMakeLists.txt` 最低 CMake 版本为 3.12，并禁止 in-source build：

```text
cmake -S . -B build
```

若没有显式设置 `CMAKE_TOOLCHAIN_FILE`：

1. 优先检查环境变量 `VITASDK`。
2. 其次检查 `/usr/local/vitasdk/share/vita.toolchain.cmake`。
3. 两者都不存在时 `FATAL_ERROR`。

项目定义后加载 `${VITASDK}/share/vita.cmake`。

## 应用元数据

- CMake project：`VitaArc`
- LiveArea 显示名：`VitaArc`
- Title ID：`VARC61616`
- 应用版本：`01.00`
- SFO 参数：`PARENTAL_LEVEL=1`，自定义打包命令还写入 `ATTRIBUTE2=12`
- SELF：`VitaArc.self`
- VPK：`VitaArc.vpk`
- `vita_create_self` 使用 `UNSAFE`

## 编译标志和 include

顶层：

- C：在现有标志后追加 `-Wall`
- C++：在现有标志后追加 `-std=gnu++11`

公开 include：

- `src`
- `src/bridges`
- `third-party`

## 源码收集方式

当前没有 `src/*/CMakeLists.txt` 子项目，也没有静态桥接子库。

顶层使用：

```cmake
file(GLOB_RECURSE VITAARC_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
```

所有 `src` 下 C/C++ 源文件一起编进主可执行文件。

## 主程序链接项

普通/第三方库：

- `pthread`，使用 `-Wl,--whole-archive pthread -Wl,--no-whole-archive`
- `m`
- `stdc++`
- `vitaGL`
- `vitashark`
- `mathneon`
- `SceKernelDmacMgr_stub`
- `SceShaccCgExt`

Vita stubs：

- `taihen_stub`
- `kubridge_stub`
- `SceAppMgr_stub`
- `SceAppUtil_stub`
- `SceAudio_stub`
- `SceAudioIn_stub`
- `SceCtrl_stub`
- `SceCommonDialog_stub`
- `SceDisplay_stub`
- `SceFios2_stub`
- `SceGxm_stub`
- `SceShaccCg_stub`
- `SceSysmodule_stub`
- `ScePower_stub`
- `SceTouch_stub`
- `SceVshBridge_stub`
- `SceNetCtl_stub`
- `SceRtc_stub`

`third-party/kubridge` 是 Git submodule 声明，但当前顶层 CMake 不构建它；主程序直接链接已安装的 `kubridge_stub`。

## VPK 打包

当前没有使用 `vita_create_vpk` 宏直接打包资源，而是使用自定义命令：

1. 生成 `${PROJECT_NAME}.vpk_param.sfo`。
2. 调用 `${VITA_PACK_VPK}`，逐个传入 LiveArea `-a source=dest` 映射。
3. 生成 `${PROJECT_NAME}.vpk.out`。
4. 自定义 target 再复制成 `${PROJECT_NAME}.vpk`。

注释说明原因：`vita_create_vpk` 会先拼接资源路径再拆分，含空格路径会坏掉。

## `build.sh`

脚本是 Bash 脚本：

```sh
#!/usr/bin/env bash
set -e

cmake -S . -B build
cmake --build build --parallel 8
```

它不再执行 `make clean`，也不再直接进入 `build` 目录。

## 当前构建现场

当前工作区根目录没有 `build/` 目录，也没有可用于证明当前源码可编译的本地构建产物。
