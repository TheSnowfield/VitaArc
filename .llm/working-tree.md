# Current working tree and development state

## Repository

```text
Windows: M:\Projects\PSV Projects\VitaArc
WSL:     /mnt/m/Projects/PSV Projects/VitaArc
branch:  main
```

## Recent commits observed on 2026-07-31

```text
56561b5 build: pack arcara apk into vpk
04214b5 loader: refactor symbols
eba73f2 android: android asset manager
0fc2c47 chore: refactor
8807c14 llm: update readme
```

## State before this `.llm` update

`git status --short` was empty before editing `.llm` in the current task。

Source and packaging changes from the preceding work were already present in `main`：

- APK enumeration/selection/import。
- generated bridge symbol header。
- loader automatic symbol resolution。
- Android AssetManager。
- fake JNI runtime。
- `dynalib_t` 独立头。
- snake_case/private static naming refactor。
- resource paths moved to `app0:package`。

## Current uncommitted state observed during the update

在本轮后段重新检查时，除 `.llm` 文档外，工作树还出现以下变更：

```text
M  CMakeLists.txt
M  README.md
M  build.sh
?? .github/workflows/build-vpk.yml
?? .gitlab-ci.yml
```

这些文件不是本轮 `.llm` 编辑动作写入的；当前知识库按磁盘上的内容读取并
记录了它们：

- `VITAARC_PACKAGE_APK` 可关闭 APK 导入。
- `build.sh` 支持环境变量和透传 CMake 参数。
- GitHub Actions/GitLab CI 构建不含专有 APK 的基础 VPK。
- README 增加相同的 CI/本地构建说明。

当前所有 `.llm/*.md` 也因本轮知识库更新而处于修改或新增状态。

## Build cache

```text
build/CMakeCache.txt exists
CMAKE_BUILD_TYPE=Debug
CMAKE_TOOLCHAIN_FILE=/usr/local/vitasdk/share/vita.toolchain.cmake
ARCAEA_VERSION=1.6.1c
```

## Verified build

完整构建成功：

```text
[100%] Built target VitaArc.vpk-vpk
```

产物：

```text
build/VitaArc                      4,893,912 bytes
build/VitaArc.self                   531,751 bytes
build/VitaArc.vpk                494,398,780 bytes
build/generated/bridge_symbols.h     17,035 bytes
```

VPK：

```text
1742 total entries
1736 package/ entries
1049 package/assets regular files
```

已确认：

```text
package/AndroidManifest.xml
package/lib/armeabi-v7a/libcocos2dcpp.so
```

ELF strings 已确认：

```text
app0:package/assets
app0:package/lib/armeabi-v7a/libcocos2dcpp.so
```

## Current `.llm` update scope

本次更新写入：

- 当前 Windows/WSL 路径。
- APK/VPK 完整打包和验证。
- `app0:`/`ux0:` 资源边界。
- generated bridge symbols 和 loader 自动依赖绑定。
- 当前 AssetManager/fake JNI 状态。
- Vita3K guest logging、HLE SVC/NID dispatcher 和内核 debug export 调研。
- 当前构建产物与 Git 现场。
- 基础 VPK CI 模式和当前未提交 CI 文件。
