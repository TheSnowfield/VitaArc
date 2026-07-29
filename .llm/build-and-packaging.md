# Build and packaging

## 工具链选择

顶层 `CMakeLists.txt` 最低 CMake 版本为 2.8。若没有显式设置 `CMAKE_TOOLCHAIN_FILE`：

1. 检查环境变量 `VITASDK`。
2. 使用 `$ENV{VITASDK}/share/vita.toolchain.cmake`。
3. 未设置 `VITASDK` 时直接 `FATAL_ERROR`。

项目定义后加载 `${VITASDK}/share/vita.cmake`。

## 应用元数据

- CMake project：`VitaArc`
- LiveArea 显示名：`VitaArc`
- Title ID：`VARC61616`
- 应用版本：`01.00`
- SFO 参数：`PARENTAL_LEVEL=1`
- SELF：`VitaArc.self`
- VPK：`VitaArc.vpk`
- `vita_create_self` 使用 `UNSAFE`

## 编译标志

顶层：

- C：在现有标志后追加 `-Wall`
- C++：在现有标志后追加 `-std=gnu++11`

部分桥接子库追加 `-Wno-int-conversion`：

- EabiBridge
- CBridge
- OpenGLBridge

libc 中的 Bionic fortify shim 使用 C++ 编译，其余主体主要是 C。

## 头文件目录

顶层公开：

- `src`
- `src/bridges`

各子项目重复执行 Vita toolchain/vita.cmake 检查，而不是只继承顶层配置。

## 静态子库

顶层依次加入：

1. `src/utils` → `Utils`
2. `src/logcat` → `Logcat`
3. `src/solibrary` → `SoLibrary`
4. `src/bridges/android` → `AndroidBridge`
5. `src/bridges/cocos2dx` → `Cocos2dxBridge`
6. `src/bridges/eabi` → `EabiBridge`
7. `src/bridges/jni` → `JniBridge`
8. `src/bridges/libc` → `CBridge`
9. `src/bridges/opengl` → `OpenGLBridge`
10. `src/bridges/audio` → `AudioBridge`

主程序仅直接编译 `src/main.c`。

## 主程序链接项

普通/第三方库：

- `pthread`，使用 `-Wl,--whole-archive pthread -Wl,--no-whole-archive`
- `m`
- `stdc++`
- `vitaGL`
- `vitashark`
- `mathneon`

本项目静态库：

- `Utils`
- `Logcat`
- `SoLibrary`
- `CBridge`
- `EabiBridge`
- `JniBridge`
- `AndroidBridge`
- `OpenGLBridge`
- `Cocos2dxBridge`
- `AudioBridge`

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

`src/kubridge` 没有在顶层 `add_subdirectory`，而主程序直接链接 `kubridge_stub`。这意味着 kuBridge 需要独立构建并安装进 VitaSDK，或使用已有安装版本。

## kuBridge 独立构建

`src/kubridge/CMakeLists.txt`：

- project：`kubridge`
- C 标志：`-Wl,-q -Wall -O3 -nostdlib`
- C++ 标志：`-fno-rtti -fno-exceptions`
- 输出：`kubridge.skprx`
- 导出配置：`exports.yml`
- 链接：
  - `taihenForKernel_stub`
  - `taihenModuleUtils_stub`
  - `SceSysclibForDriver_stub`
  - `SceSysmemForDriver_stub`
  - `SceThreadmgrForDriver_stub`
- 生成并安装 stub archive 和 `kubridge.h`

## `build.sh`

脚本是 Bash 脚本：

```sh
cmake . -B build \
  && cd build    \
  && make clean  \
  && make -j 8
```

它每次构建先清理，然后使用 8 个并行 job。

## 已存在构建现场

`build/` 中存在 2023-01-14 左右生成的：

- `VitaArc`
- `VitaArc.velf`
- `VitaArc.self`
- `VitaArc.vpk`
- `VitaArc.vpk_param.sfo`
- `compile_commands.json`
- `Makefile`

还存在逆向和调试数据：

- `libcocos2dcpp.so`
- `libcocos2dcpp.so.idb`
- `memdump.bin`
- Vita core dump
- `log.txt`
- Cg/GXP/VP/fragment shader实验文件
- `glslcc.exe`

该构建目录的 `CMakeCache.txt`：

- `CMAKE_BUILD_TYPE=Debug`
- `VITASDK=/usr/local/vitasdk`

根目录旧 `CMakeCache.txt` 同样指向 `/usr/local/vitasdk`。当前主机是 Windows PowerShell 环境，因此这些缓存不能视为当前 Windows 环境已配置完成。

当前源码改动晚于旧 VPK 产物，旧构建不能证明当前工作区可编译。

