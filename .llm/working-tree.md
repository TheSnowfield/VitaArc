# Current working tree and development state

## Git state

Repository:

```text
M:\Projects\PSV Projects\VitaArc
```

Current branch:

```text
main
```

Observed refs:

- `HEAD -> main`
- `github/main` points to current HEAD
- `origin/main` is behind current HEAD

Recent commits:

1. `6af88d3 add read all file`
2. `fb7ea4f update bridges`
3. `40c4b0d clean project`
4. `55adb3b fix set string for key`
5. `a421547 add audio provider`

## Existing uncommitted source changes

Modified:

- `src/bridges/cocos2dx/cocos2dx.c`
- `src/bridges/opengl/CMakeLists.txt`
- `src/bridges/opengl/impl/opengl.c`
- `src/bridges/opengl/impl/opengl.h`
- `src/bridges/opengl/opengl.c`
- `src/main.c`

Untracked:

- `src/bridges/opengl/impl/glsl2cg.c`
- `src/bridges/opengl/impl/glsl2cg.h`

这些改动在 `.llm` 创建前已经存在，不属于知识文档写入操作。

## `src/main.c` 当前改动

相对 HEAD：

- 把 vitaGL 初始化从 SO/JNI 初始化之后移动到 SO 加载之前。
- 运行时 shader compiler 在加载 Android SO 前配置。
- buffer size 在 `vglInitExtended` 前配置。
- 加入实际 clear、clear color 和 swap。
- SO 加载、bridge patch 和 `.init_array` 调用的结构重新排版。
- 删除/移除当前可见的 Crashlytics、FMOD commented load declarations。
- `JNI_OnLoad` 保持在 SO init 后调用。
- Cocos platform/device/native init 在 `JNI_OnLoad` 后调用。

## Cocos2d-x bridge 当前改动

新增固定偏移补丁：

```c
patchARM(hSoLibrary, 0x6E6AD4, 0x00F020E3);
```

注释目的：

- `cocos2d::GLProgram::bindPredefinedVertexAttribs`
- 阻止调用 `glBindAttribLocation`

## OpenGL CMake 当前改动

`OpenGLBridge` 新增编译源：

```text
impl/glsl2cg.c
```

该文件当前未被 Git 跟踪，但已进入 CMake source list。

## OpenGL bridge table 当前改动

- `glBindAttribLocation`
  - 原：vitaGL `glBindAttribLocation`
  - 现：VitaArc `_glBindAttribLocation`
- `glGetShaderiv`
  - 原：VitaArc `_glGetShaderiv`
  - 现：vitaGL `glGetShaderiv`
- `glGetShaderSource`
  - 原：未注册/注释
  - 现：VitaArc `_glGetShaderSource`

## OpenGL implementation 当前改动

旧 `_glShaderSource`：

- 只在 `count == 4` 且第三段为 `#define guy_empty` 时做特殊处理。
- 从 `ux0:vitaarc/shader/cc.gxp` 读取预编译 shader。
- 使用 `glShaderBinary`。
- 否则不替换。

新 `_glShaderSource`：

- 合并全部 GLSL source segments。
- dump GLSL。
- 判断 shader type。
- 调用 GLSL→Cg 转换器。
- 把 Cg 交给 vitaGL `glShaderSource`。

旧 `_glGetShaderiv` wrapper：

- 调用真实 vitaGL `glGetShaderiv`。
- 可打印 compile status。

当前该 wrapper 被整段注释，bridge 直接指向 vitaGL。

新增：

- 空 `_glGetShaderSource`
- logging-only `_glBindAttribLocation`

## `.llm` 写入操作新增文件

本次知识持久化新增：

- `.llm/README.md`
- `.llm/project-layout.md`
- `.llm/build-and-packaging.md`
- `.llm/runtime-flow.md`
- `.llm/elf-loader.md`
- `.llm/bridges-eabi-libc.md`
- `.llm/bridges-android-jni.md`
- `.llm/bridges-graphics-audio-cocos.md`
- `.llm/utils-kubridge-logging.md`
- `.llm/vitagl-local-source.md`
- `.llm/working-tree.md`
- `.llm/known-issues.md`
- `.llm/source-inventory.md`

## Build age versus source age

`build/VitaArc.vpk`、SELF 和 VELF 的时间是 2023-01-14。

当前 Git commit 日期、工作区修改时间和新增 GLSL converter 均不能由该旧构建产物验证。当前工作区尚未在本次阅读/知识写入过程中执行构建。

## 本地 vitaGL working tree

路径：

```text
M:\Projects\PSV Projects\vitaGL
```

状态：

- tracked source clean
- untracked `.vscode/`

未对 vitaGL 做任何修改。

