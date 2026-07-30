# Historical vitaGL source notes used by VitaArc

## Scope

本文档保留此前读取过的本地 vitaGL 源码知识，用于解释当前 VitaArc 的 OpenGL bridge 为什么要把 GLSL 转成 Cg、为什么 shader source 生命周期敏感。

此前记录的外部路径是：

```text
M:\Projects\PSV Projects\vitaGL
```

当前 VitaArc 工作区是 Linux 路径：

```text
/home/thesnowfield/Desktop/projects/VitaArc
```

本次 `.llm` 对齐没有重新读取或验证外部 vitaGL 工作区，因此下面内容应视为“历史已读实现背景”，而不是当前仓库内源码清单。

## 历史读取到的 vitaGL 版本

历史记录：

- 分支：`master`
- HEAD：`666b029c19d96942105674e9023f9d0f6209c4d2`
- commit subject：`Fix warnings for shared rentertargets build.`
- commit date：2021-06-05
- remote tracking：`origin/master`
- 工作区只有未跟踪 `.vscode/`
- 根目录存在预编译 `libvitaGL.a`，时间为 2021-06-06

## vitaGL 职责

vitaGL：

- 暴露一部分 OpenGL API。
- 将调用转换为 SceGxm。
- 使用 Vita GPU 硬件加速。
- 不实现完整 OpenGL/OpenGL ES。
- 自定义 shader 路径使用 Cg source 或预编译 GXP。

运行时 shader 编译需要设备上的已解密 `libshacccg.suprx`。

不依赖 runtime compiler 的路径：

- 只用预编译 shader (`glShaderBinary`)。
- 使用旧 legacy fixed-function pipeline 分支。

VitaArc 当前链接 `vitaGL`、`vitashark`、`SceShaccCg_stub`，并额外链接 `SceShaccCgExt`。

## vitaGL 构建背景

历史读取到的 Makefile：

- target：`libvitaGL.a`
- compiler prefix：`arm-vita-eabi`
- C：`arm-vita-eabi-gcc`
- C++：`arm-vita-eabi-g++`
- archive：`arm-vita-eabi-gcc-ar`
- 基本 C flags：
  - `-g`
  - `-Wl,-q`
  - `-O3`
  - `-ffast-math`
  - `-mtune=cortex-a9`
  - `-mfpu=neon`
- C++：
  - C flags
  - `-fno-exceptions`
  - `-std=gnu++11`
  - `-Wno-write-strings`

可选 flags：

- `HAVE_SHARK_LOG=1`
- `LOG_ERRORS=1/2`
- `NO_DEBUG=1`
- `SOFTFP_ABI=1`
- `DRAW_SPEEDHACK=1`
- `HAVE_UNFLIPPED_FBOS=1`
- `SHARED_RENDERTARGETS=1`
- `CIRCULAR_VERTEX_POOL=1`
- `SAMPLER_UNIFORMS=1`
- `UNPURE_TEXTURES=1`
- `HAVE_RAZOR=1/2`
- `HAVE_DEVKIT=1/2`

`make install` 历史行为：

- 复制 `libvitaGL.a` 到 `$VITASDK/arm-vita-eabi/lib/`
- 复制 `source/vitaGL.h` 到 `$VITASDK/arm-vita-eabi/include/`

VitaArc 顶层 `target_link_libraries(vitaGL)` 使用 VitaSDK/library path 中的 archive。仅修改外部 vitaGL 源码不会自动影响 VitaArc 链接结果；需要重建并安装或让 VitaArc 显式链接本地 archive。

## 初始化接口语义

### `vglSetupRuntimeShaderCompiler`

只保存全局设置：

- optimization level
- fastmath
- fastprecision
- fastint

不在调用时加载 `libshacccg.suprx`。

### GXM buffer setters

直接写全局 size：

- `vglSetVDMBufferSize`
- `vglSetVertexBufferSize`
- `vglSetFragmentBufferSize`
- `vglSetUSSEBufferSize`

必须在 `initGxm` 分配对应资源前调用。

### `vglSetVertexPoolSize`

历史实现受：

```c
#ifdef HAVE_CIRCULAR_VERTEX_POOL
```

保护。只有 vitaGL 以 `CIRCULAR_VERTEX_POOL=1` 编译时才会设置 `vertex_data_pool_size`；默认构建为空操作。

当前 VitaArc `main()` 已不调用 `vglSetVertexPoolSize`。

### `vglUseVram`

历史实现只执行：

```c
use_vram = usage;
```

影响后续 `vgl_malloc`/相关分配的内存类型。

当前 VitaArc `main()` 已不调用 `vglUseVram`。

### `vglInitExtended`

历史实现转调：

```c
vglInitWithCustomThreshold(
    pool_size,
    width,
    height,
    ram_threshold,
    256 * 1024,
    1 * 1024 * 1024,
    msaa);
```

当前 VitaArc `main()` 中 `vglInitExtended` 调用被注释。

## Shader object

历史读取到的 `glCreateShader`：

- 在固定 shader slot 数组找空项。
- handle 从 1 开始。
- 保存 `GL_FRAGMENT_SHADER` 或 `GL_VERTEX_SHADER`。
- 标记 valid。

历史读取到的 `glGetShaderiv`：

- `GL_SHADER_TYPE` → slot type。
- `GL_COMPILE_STATUS` → `s->prog != NULL`。
- `GL_INFO_LOG_LENGTH` → `strlen(s->log)` 或 0。

## `glShaderSource`

历史读取到的签名：

```c
void glShaderSource(
    GLuint handle,
    GLsizei count,
    const GLchar *const *string,
    const GLint *length);
```

行为：

1. debug build 下只检查 `count < 0`。
2. 找到 shader slot。
3. 不拼接 `count` 个字符串。
4. 不复制 source。
5. 只使用第一个 `*string`。
6. 将其地址强制转换为 `SceGxmProgram *` 并存入 `s->prog`。
7. 保存 `*length` 或 `strlen(*string)`。

因此调用者必须：

- 提供单段 Cg source。
- 保持 source 内存在 `glCompileShader` 完成前有效。

VitaArc 当前 `_glShaderSource` 在调用 vitaGL `glShaderSource` 后立即释放 Cg 字符串，不符合此生命周期。

vitaGL 没有 `glGetShaderSource` 实现，也没有在 `vitaGL.h` 声明此函数。

## `glCompileShader`

历史读取到的流程：

1. 若 vitaShaRK 未在线，调用 `startShaderCompiler()`。
2. 失败则设置 `GL_INVALID_OPERATION`。
3. 使用 `shark_compile_shader_extended(...)` 将 Cg 编译成 GXM program。
4. 编译成功后：
   - 使用 `vgl_malloc(size, VGL_MEM_EXTERNAL)` 申请长期内存。
   - 复制 GXP/GXM program。
   - 注册到 `sceGxmShaderPatcher`。
   - 把 `s->prog` 替换为 patcher 返回的 program。
5. 可选保存 shark log。
6. 清理 VitaShaRK output。

## Program attach/create/link

历史读取到的行为：

- `glCreateProgram` 创建未 linked program，并初始化 attribute/uniform/stream/shader fields。
- `glAttachShader` 要求 program 仍是 `PROG_UNLINKED`。
- `glLinkProgram` 扫描 fragment/vertex shader 参数，建立 sampler、uniform 和 attribute 信息。
- `glGetProgramInfoLog` 始终返回空 log。

## Attribute binding

历史读取到的 `glBindAttribLocation(program, index, name)`：

1. 取得 `program.vshader->prog`。
2. `sceGxmProgramFindParameterByName`。
3. 要求 category 为 attribute。
4. 将 GXM resource index 写入 `p->attr[index].regIndex`。
5. 更新 `attr_highest_idx`。

它需要：

- program 已附加 vertex shader。
- vertex shader 已经被 vitaShaRK 编译，`vshader->prog` 已成为 GXM program。
- Cg 编译后的 parameter name 与 Cocos传入 name 一致。

VitaArc 当前：

- 把 Android `glBindAttribLocation` 替换为只记录日志的 `_glBindAttribLocation`。
- 对 Cocos `bindPredefinedVertexAttribs` 打机器码补丁。
- 依赖后续 `glGetAttribLocation`/draw pipeline 的自动映射。

## Buffer/renderbuffer/VAO兼容背景

VitaArc 自己声明 `_gpubuffer` 结构并用非空指针判断 `glIsBuffer`，但该结构是对 vitaGL 内部 handle 的假设，没有从本地 vitaGL public API 保证。

VitaArc renderbuffer 和 VAO OES 方法为空实现。目标 Cocos2d-x 若依赖实际 FBO depth/stencil renderbuffer 或 VAO state，将得到不完整行为。
