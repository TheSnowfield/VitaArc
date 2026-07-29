# Local vitaGL source used as graphics backend

## 本地路径和版本

路径：

```text
M:\Projects\PSV Projects\vitaGL
```

Git：

- 分支：`master`
- HEAD：`666b029c19d96942105674e9023f9d0f6209c4d2`
- commit subject：`Fix warnings for shared rentertargets build.`
- commit date：2021-06-05
- remote tracking：`origin/master`
- 工作区只有未跟踪 `.vscode/`

根目录存在预编译 `libvitaGL.a`，时间为 2021-06-06。

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

VitaArc 链接 `vitaGL`、`vitashark`、`SceShaccCg_stub`。

## vitaGL 构建

Makefile：

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

`make install`：

- 复制 `libvitaGL.a` 到 `$VITASDK/arm-vita-eabi/lib/`
- 复制 `source/vitaGL.h` 到 `$VITASDK/arm-vita-eabi/include/`

VitaArc 顶层 `target_link_libraries(vitaGL)` 使用的是 VitaSDK 安装目录中的 archive，除非工具链另行配置 library path。仅修改本地 vitaGL 源码不会自动影响 VitaArc 链接结果；需要重建并安装或让 VitaArc显式链接本地 archive。

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

实现受：

```c
#ifdef HAVE_CIRCULAR_VERTEX_POOL
```

保护。只有 vitaGL 以 `CIRCULAR_VERTEX_POOL=1` 编译时才会设置 `vertex_data_pool_size`；默认构建为空操作。

### `vglUseVram`

只执行：

```c
use_vram = usage;
```

影响后续 `vgl_malloc`/相关分配的内存类型。若要影响初始化阶段分配，应该在 `vglInitExtended` 前调用。

### `vglInitExtended`

转调：

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

非 system app 模式：

1. 先 `initGxm()`。
2. 查询 `sceKernelGetFreeMemorySize`。
3. RAM pool 使用 `free_user - ram_threshold`，不足时用全部 free_user。
4. CDRAM pool保留 256 KiB threshold。
5. phycont pool保留 1 MiB threshold。
6. 调用 `vglInitWithCustomSizes`。

`vglInitWithCustomSizes` 又调用一次 `initGxm()`。当前源码调用层级显示 `vglInitWithCustomThreshold` 和 `vglInitWithCustomSizes` 都含 `initGxm()` 调用，具体是否由内部状态防止重复需要结合 `initGxm` 实现。

它设置：

- display width/height/stride
- viewport
- SceGxm
- memory heaps
- purge lists
- scissor
- default texture 0
- texture slots
- texture matrix
- 可选 debugger

## Shader object

`glCreateShader`：

- 在固定 shader slot 数组找空项。
- handle 从 1 开始。
- 保存 `GL_FRAGMENT_SHADER` 或 `GL_VERTEX_SHADER`。
- 标记 valid。

`glGetShaderiv`：

- `GL_SHADER_TYPE` → slot type。
- `GL_COMPILE_STATUS` → `s->prog != NULL`。
- `GL_INFO_LOG_LENGTH` → `strlen(s->log)` 或 0。

它没有在可见实现中验证 handle 范围。

## `glShaderSource`

签名：

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

VitaArc当前 wrapper在调用 `glShaderSource` 后立刻释放 source，不符合此生命周期。

vitaGL 没有 `glGetShaderSource` 实现，也没有在 `vitaGL.h` 声明此函数。

## `glCompileShader`

1. 若 vitaShaRK 未在线，调用 `startShaderCompiler()`。
2. 失败则设置 `GL_INVALID_OPERATION`。
3. 使用：

```c
shark_compile_shader_extended(
    source,
    &size,
    fragment_or_vertex,
    compiler_opts,
    compiler_fastmath,
    compiler_fastprecision,
    compiler_fastint);
```

4. 编译成功后：
   - 使用 `vgl_malloc(size, VGL_MEM_EXTERNAL)` 申请长期内存。
   - 复制 GXP/GXM program。
   - 注册到 `sceGxmShaderPatcher`。
   - 把 `s->prog` 替换为 patcher 返回的 program。
5. 可选保存 shark log。
6. 清理 VitaShaRK output。

## Program attach/create/link

`glCreateProgram`：

- handle 从 1 开始。
- 状态设 `PROG_UNLINKED`。
- attribute/uniform/stream/shader fields 清空。
- 每个 `attr[j].regIndex = 0xDEAD`。

`glAttachShader`：

- program 必须仍是 `PROG_UNLINKED`。
- 根据 shader type 保存 `vshader` 或 `fshader` 指针。

`glLinkProgram`：

1. debug path 下检查 vertex/fragment shader `prog` 非空。
2. 状态设为 linked。
3. 扫描 fragment shader 参数：
   - sampler → 标记 texture unit
   - uniform → 分配本地 uniform 节点和数据
4. 扫描 vertex shader参数：
   - attribute → 增加 `attr_num`
   - uniform → 分配数据；同名同尺寸 fragment uniform 可 alias
5. 依据 stream configuration 决定立即 patch vertex/fragment program，或延后到 draw path。
6. 检查 attribute slot 是否存在 `0xDEAD`，设置 `has_unaligned_attrs`。

`glGetProgramiv` 支持：

- `GL_LINK_STATUS`
- `GL_INFO_LOG_LENGTH`
- `GL_ATTACHED_SHADERS`
- `GL_ACTIVE_ATTRIBUTES`
- `GL_ACTIVE_ATTRIBUTE_MAX_LENGTH`
- `GL_ACTIVE_UNIFORMS`
- `GL_ACTIVE_UNIFORM_MAX_LENGTH`

`glGetProgramInfoLog` 始终返回空 log。

## Attribute binding

`glBindAttribLocation(program, index, name)`：

1. 取得 `program.vshader->prog`。
2. `sceGxmProgramFindParameterByName`。
3. 要求 category 为 attribute。
4. 将 GXM resource index 写入 `p->attr[index].regIndex`。
5. 更新 `attr_highest_idx`。

它需要：

- program 已附加 vertex shader。
- vertex shader 已经被 vitaShaRK 编译，`vshader->prog` 已成为 GXM program。
- Cg 编译后的 parameter name 与 Cocos传入 name 一致。

`glGetAttribLocation`：

1. 在编译后的 vertex program 中按名字查 parameter。
2. 若已绑定，返回对应 attribute slot。
3. 否则在 `attr_num` 范围找第一个 `0xDEAD` slot，自动绑定并返回。

VitaArc当前：

- 把 `glBindAttribLocation` 替换为 no-op。
- 对 Cocos `bindPredefinedVertexAttribs` 打机器码补丁。
- 依赖后续 `glGetAttribLocation`/draw pipeline 的自动映射。

## `vglBindAttribLocation`

这是 vitaGL-specific API，与 OpenGL `glBindAttribLocation` 不同。额外接收：

- component count
- GL type

它直接填充 `SceGxmVertexAttribute` 和 `SceGxmVertexStream`，用于 GXM architecture 的明确 attribute layout。

VitaArc未使用它。

## `glVertexAttribPointer`

保存：

- pointer/offset
- 当前 VBO
- component count
- GXM attribute format
- stride

支持：

- `GL_HALF_FLOAT`
- `GL_FLOAT`
- `GL_SHORT`
- `GL_UNSIGNED_SHORT`
- `GL_BYTE`
- `GL_UNSIGNED_BYTE`

normalized 会选择相应 normalized GXM format。

## Buffer/renderbuffer/VAO兼容背景

VitaArc 自己声明 `_gpubuffer` 结构并用非空指针判断 `glIsBuffer`，但该结构是对 vitaGL 内部 handle 的假设，没有从本地 vitaGL public API 保证。

VitaArc renderbuffer 和 VAO OES 方法为空实现。目标 Cocos2d-x 若依赖实际 FBO depth/stencil renderbuffer 或 VAO state，将得到不完整行为。

