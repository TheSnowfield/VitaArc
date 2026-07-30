# Build and packaging

## CMake 与 VitaSDK

顶层 `CMakeLists.txt` 最低版本为 3.12，并禁止 in-source build：

```text
cmake -S . -B build
cmake --build build --parallel 8
```

所有 CMake 中间文件、生成头、ELF、SELF、SFO 和 VPK 都位于 `build/`。

未显式传入 `CMAKE_TOOLCHAIN_FILE` 时：

1. 检查环境变量 `VITASDK`。
2. 否则检查 `/usr/local/vitasdk/share/vita.toolchain.cmake`。
3. 两者都不可用时终止配置。

项目定义后加载：

```cmake
include("${VITASDK}/share/vita.cmake" REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
```

当前构建缓存：

```text
CMAKE_BUILD_TYPE=Debug
CMAKE_TOOLCHAIN_FILE=/usr/local/vitasdk/share/vita.toolchain.cmake
ARCAEA_VERSION=1.6.1c
```

## 应用元数据

```text
project             VitaArc
LiveArea name       VitaArc
Title ID            VARC61616
version             01.00
PARENTAL_LEVEL      1
ATTRIBUTE2          12
SELF                build/VitaArc.self
VPK                 build/VitaArc.vpk
```

`vita_create_self` 使用 `UNSAFE`。

## 编译标志和 include

```text
CFLAGS    existing flags + -Wall
CXXFLAGS  existing flags + -std=gnu++11
```

include：

```text
src
src/bridges
third-party
build
```

## 源文件自动发现

顶层使用：

```cmake
file(GLOB_RECURSE VITAARC_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
```

新增或删除 `src` 下的 C/C++ 文件时，CMake 会重新配置源文件列表。不再依赖各桥接目录自己的 `CMakeLists.txt`。

## 构建期 bridge symbol 生成

输入：

```text
src/bridges/symbol.def
```

生成器：

```text
tools/generate_symbols.py
```

输出：

```text
build/generated/bridge_symbols.h
```

生成头被标记为 `GENERATED` 和 `HEADER_FILE_ONLY`，并加入主 target 依赖。`src/loader.c` 直接：

```c
#include <generated/bridge_symbols.h>
```

当前生成头记录 354 个符号，按导入符号名字典序排序。`symbol.def` 顶部注释仍写着 349，注释数字已落后于实际解析结果。

## 链接项

普通库和运行库：

```text
pthread（whole archive）
m
stdc++
vitaGL
vitashark
mathneon
SceKernelDmacMgr_stub
SceShaccCgExt
```

Vita stubs：

```text
taihen_stub
kubridge_stub
SceAppMgr_stub
SceAppUtil_stub
SceAudio_stub
SceAudioIn_stub
SceCtrl_stub
SceCommonDialog_stub
SceDisplay_stub
SceFios2_stub
SceGxm_stub
SceShaccCg_stub
SceSysmodule_stub
ScePower_stub
SceTouch_stub
SceVshBridge_stub
SceNetCtl_stub
SceRtc_stub
```

当前没有显式链接 `SceLibKernel_stub`、`SceLibDbg_stub` 或 `SceIofilemgr_stub`。若直接调用 `sceClibPrintf`、SceLibDbg 或额外 I/O stub，需要把相应库加入此列表。

## VPK 基础包

项目不调用 `vita_create_vpk` 宏，因为该宏会先拼接资源参数再拆分，项目路径中的空格会破坏参数。

自定义命令先调用 `vita-pack-vpk`，逐项传入：

```text
livearea/icon0.png      -> sce_sys/icon0.png
livearea/background.png -> sce_sys/livearea/contents/background.png
livearea/startup.png    -> sce_sys/livearea/contents/startup.png
livearea/template.xml   -> sce_sys/livearea/contents/template.xml
build/VitaArc.self      -> eboot.bin
build/VitaArc.vpk_param.sfo -> sce_sys/param.sfo
```

基础输出是：

```text
build/VitaArc.vpk.out
```

## APK 自动选择与导入

CMake 使用 `CONFIGURE_DEPENDS` 枚举：

```text
third-party/arcaea/*.apk
```

APK 导入由 CMake option 控制：

```text
VITAARC_PACKAGE_APK=ON
```

默认 `ON`。为 `ON` 时，目录没有 APK 会在配置阶段失败；错误消息会提示用
`-DVITAARC_PACKAGE_APK=OFF` 构建不含专有 APK 内容的基础 VPK。

`ARCAEA_VERSION` 是 cache string：

```text
cmake -S . -B build -DARCAEA_VERSION=1.6.1c
```

指定后，打包不出现交互选择。

未指定时：

- 只有一个 APK：自动选择。
- 多个 APK且 stdin 是 TTY：打印版本列表并要求输入序号。
- 多个 APK且非交互：失败并要求 `--version`。

基础 VPK 建立后运行：

```text
python tools/package_apk.py
  --vpk build/VitaArc.vpk.out
  --apk-dir third-party/arcaea
  --prefix package
  [--version <version>]
```

APK 中每个非目录 ZIP entry 原样进入：

```text
package/<APK entry name>
```

最后复制：

```text
build/VitaArc.vpk.out -> build/VitaArc.vpk
```

当 `VITAARC_PACKAGE_APK=OFF` 时，不创建 APK import command，
`VitaArc.vpk.out` 只包含 SELF、SFO 和 LiveArea 资源，之后仍复制为
`VitaArc.vpk`。此模式用于公开 CI。

## `package_apk.py` 输入规则

版本从 `arcaea_<version>.apk` 或 `arcaea-<version>.apk` 得到；其他文件名使用完整 stem。

`--version` 可匹配：

- 解析后的版本。
- 文件 stem。
- 完整文件名。

比较不区分大小写。重复版本报错。

`--list` 只打印候选，不要求 `--vpk`。

ZIP 安全检查：

- 拒绝绝对路径。
- 拒绝反斜杠和 NUL。
- 拒绝空、`.`、`..` 路径组件。
- 拒绝 APK 内重复目标路径。
- 拒绝覆盖基础 VPK 已有 entry。
- 只接受 Store 和 Deflate。
- 允许 Zip64。

写入时保留 APK entry 的：

- compression type
- timestamp
- comment
- internal/external attributes
- create system

数据以 1 MiB buffer 流式复制，不把整个 APK 或单个大文件一次性载入内存。

## 当前已验证构建

完整构建成功：

```text
[100%] Built target VitaArc.vpk-vpk
```

当前产物：

```text
build/VitaArc                  4,893,912 bytes
build/VitaArc.self               531,751 bytes
build/VitaArc.vpk            494,398,780 bytes
build/generated/bridge_symbols.h 17,035 bytes
```

当前 VPK 使用 Arcaea 1.6.1c：

```text
total ZIP entries       1742
package/ entries        1736
package/assets files    1049
```

已确认存在：

```text
package/AndroidManifest.xml
package/lib/armeabi-v7a/libcocos2dcpp.so
package/assets/Default/Button_Disable.png
```

此前逐 entry 对比过源 APK 和 VPK 中 `package/` 内容的文件大小、CRC 和 compression type，结果一致。

## `build.sh`

```sh
#!/usr/bin/env bash
set -euo pipefail

export VITASDK="${VITASDK:-/usr/local/vitasdk}"

build_dir="${BUILD_DIR:-build}"
build_type="${CMAKE_BUILD_TYPE:-Release}"
package_apk="${VITAARC_PACKAGE_APK:-ON}"

cmake \
  -S . \
  -B "${build_dir}" \
  -DCMAKE_BUILD_TYPE="${build_type}" \
  -DVITAARC_PACKAGE_APK="${package_apk}" \
  "$@"
cmake --build "${build_dir}" --parallel
```

环境变量：

- `VITASDK`：默认 `/usr/local/vitasdk`。
- `BUILD_DIR`：默认 `build`。
- `CMAKE_BUILD_TYPE`：脚本默认 `Release`。
- `VITAARC_PACKAGE_APK`：默认 `ON`。

额外参数原样传给 CMake，例如：

```sh
bash build.sh -DARCAEA_VERSION=1.6.1c
VITAARC_PACKAGE_APK=OFF bash build.sh
```

多 APK、未指定版本且处于非交互环境时，APK 模式会按设计失败。

## Continuous integration

当前工作树包含两个公开 CI 配置：

- `.github/workflows/build-vpk.yml`
- `.gitlab-ci.yml`

两者都使用 `vitasdk/vitasdk:latest` 容器，递归获取 submodule，设置
`VITAARC_PACKAGE_APK=OFF`，运行 `bash build.sh`，检查
`build/VitaArc.vpk` 非空，并把基础 VPK 保存为 14 天的 pipeline artifact。

GitHub Actions 在 push、pull request 和手动触发时运行；artifact 名为
`VitaArc-${{ github.sha }}`。它设置只读 repository content 权限，并按
ref 取消重复的进行中构建。

GitLab CI 只有 `build` stage，artifact 名为
`VitaArc-${CI_COMMIT_SHORT_SHA}`。

CI 有意不包含 `third-party/arcaea` 中的专有 APK 资源。
