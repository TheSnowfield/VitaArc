# Graphics, shader, audio and Cocos2d-x bridges

## OpenGL bridge strategy

`bridgePatchGL` 将 Android `libcocos2dcpp.so` 的 OpenGL ES/EGL 导入重定向到：

- vitaGL 中已有的同名函数。
- VitaArc 自己的包装函数。
- VitaArc 的空实现。

vitaGL 是 OpenGL 子集到 SceGxm 的硬件加速 wrapper，不是完整 OpenGL ES 驱动。

## 直接映射到 vitaGL 的 OpenGL 函数

### Texture

- `glActiveTexture`
- `glBindTexture`
- `glCompressedTexImage2D`
- `glDeleteTextures`
- `glGenTextures`
- `glGenerateMipmap`
- `glTexImage2D`
- `glTexParameteri`
- `glTexSubImage2D`

### Buffer和vertex attribute

- `glBindBuffer`
- `glBufferData`
- `glBufferSubData`
- `glDeleteBuffers`
- `glGenBuffers`
- `glDisableVertexAttribArray`
- `glEnableVertexAttribArray`
- `glVertexAttribPointer`

`glMapBufferOES` 和 `glUnmapBufferOES` 不直接映射 vitaGL，而是映射 VitaArc 占位函数。

### Framebuffer

- `glBindFramebuffer`
- `glCheckFramebufferStatus`
- `glDeleteFramebuffers`
- `glFramebufferTexture2D`
- `glGenFramebuffers`

Renderbuffer 相关函数由 VitaArc 占位。

### Shader/program

- `glAttachShader`
- `glCompileShader`
- `glCreateProgram`
- `glCreateShader`
- `glDeleteProgram`
- `glDeleteShader`
- `glGetActiveAttrib`
- `glGetActiveUniform`
- `glGetAttribLocation`
- `glGetProgramInfoLog`
- `glGetProgramiv`
- `glGetShaderInfoLog`
- `glGetShaderiv`
- `glGetUniformLocation`
- `glLinkProgram`
- `glUseProgram`

包装：

- Android `glShaderSource` → VitaArc `_glShaderSource`
- Android `glBindAttribLocation` → VitaArc `_glBindAttribLocation`
- Android `glGetShaderSource` → VitaArc `_glGetShaderSource`

### State/draw

- `glBlendEquation`
- `glBlendFunc`
- `glClear`
- `glClearColor`
- `glClearDepthf`
- `glClearStencil`
- `glColorMask`
- `glCullFace`
- `glDepthFunc`
- `glDepthMask`
- `glDisable`
- `glDrawArrays`
- `glDrawElements`
- `glEnable`
- `glFrontFace`
- `glGetError`
- `glGetFloatv`
- `glGetIntegerv`
- `glGetString`
- `glIsEnabled`
- `glLineWidth`
- `glReadPixels`
- `glScissor`
- `glStencilFunc`
- `glStencilMask`
- `glStencilOp`
- `glViewport`

### Uniform

- `glUniform1f`
- `glUniform1fv`
- `glUniform1i`
- `glUniform2f`
- `glUniform2fv`
- `glUniform2i`
- `glUniform2iv`
- `glUniform3f`
- `glUniform3fv`
- `glUniform3i`
- `glUniform3iv`
- `glUniform4f`
- `glUniform4fv`
- `glUniform4i`
- `glUniform4iv`
- `glUniformMatrix2fv`
- `glUniformMatrix3fv`
- `glUniformMatrix4fv`

### EGL

- `eglGetProcAddress` → VitaArc wrapper

## OpenGL 占位/兼容函数

以下只记录 unsupported，不执行图形操作：

- `glBindRenderbuffer`
- `glDeleteRenderbuffers`
- `glGenRenderbuffers`
- `glFramebufferRenderbuffer`
- `glRenderbufferStorage`
- `glPixelStorei`
- `glGenVertexArrays`
- `glDeleteVertexArrays`
- `glBindVertexArray`

`glMapBuffer`：

- 记录 unsupported。
- 返回 `NULL`。

`glUnmapBuffer`：

- 记录 unsupported。
- 返回 `GL_FALSE`。

`glIsBuffer`：

- 将整数 handle 当 `_gpubuffer *`。
- 只检查是否非空。

`glIsRenderbuffer`：

- 将整数 handle 当指针。
- 只检查是否非空。

`eglGetProcAddress` 只识别：

- `glGenVertexArraysOES`
- `glDeleteVertexArraysOES`
- `glBindVertexArrayOES`

其他返回 `NULL`。

## vitaGL 对 shader source 的要求

历史读取的本地 vitaGL `source/vitaGL.h` 注释说明：

```c
glShaderSource(...); // NOTE: Uses CG shader sources
```

其 `glShaderSource` 实现：

- 只使用 `*string`，即第一段。
- 不复制源码。
- 暂时把 shader source 地址放入 `shader.prog`。
- `size = *length` 或 `strlen(*string)`。

后续 `glCompileShader` 调用 vitaShaRK，把 Cg 编译成 GXM program。

因此 Android/Cocos2d-x 提供的 GLSL ES 不能直接传入本地 vitaGL。

## `_glShaderSource`

当前 VitaArc 包装流程：

1. 忽略每段 `length[i]`。
2. 对每段调用 `strlen`，求总长度。
3. `malloc(total + 1)`。
4. 顺序拼接所有 shader source strings。
5. 写 NUL。
6. 使用 `logPrintf("%s", gl_shader)` 输出原始 GLSL。
7. 写入固定文件 `ux0:vitaarc/shader/1.glsl`。
8. 调用 vitaGL `glGetShaderiv(handle, GL_SHADER_TYPE, &type)`。
9. fragment → `translate_frag_shader`。
10. vertex → `translate_vert_shader`。
11. 使用 `logPrintf("%s", cg_shader)` 输出生成 Cg。
12. `glShaderSource(handle, 1, &cg_shader_source, NULL)`。
13. 立即 `free(cg_shader)` 和 `free(gl_shader)`。

当前已经避免了旧版 “shader 文本当 format string” 的日志问题，但仍有 source 生命周期问题：vitaGL 不复制 Cg，而 VitaArc 在返回前释放 `cg_shader`。之后 Cocos2d-x 再调用 `glCompileShader` 时，vitaGL 会读取已释放内存。

若 Android 调用者传入非 NUL 结尾 source 并依赖显式 `length`，当前 `strlen` 会越界。

固定输出文件 `1.glsl` 每次覆盖，不区分 shader handle/type，也没有检查 `fopen` 是否成功。

## `_glGetShaderSource`

当前为空实现：

- 不读取 vitaGL shader。
- 不写 `source`。
- 不写 `length`。

本地 vitaGL 没有提供 `glGetShaderSource`，VitaArc 增加此符号是为了满足目标 SO 导入。

## `_glBindAttribLocation`

当前只记录 program、index、name，不调用 vitaGL：

```c
logI(TAG, "_glBindAttribLocation(%d, %d, %s)", program, index, name);
```

当前日志调用参数顺序已正确。

同时 `bridgePatchCocos2DX` 修改 `cocos2d::GLProgram::bindPredefinedVertexAttribs`，用于阻止原代码调用 attribute binding。

本地 vitaGL 的真实 `glBindAttribLocation`：

- 取得 program 的已附加 vertex shader。
- 在编译后 GXM program 中按名称找 attribute parameter。
- 将其 resource index 写入指定 attribute slot。

标准 Cocos2d-x 通常在 link 前请求绑定固定 attribute index；vitaGL 的实现依赖已经存在的编译后 vertex program，调用时序和参数命名必须兼容。

## GLSL 到 Cg 转换器

文件：

- `src/bridges/opengl/impl/glsl2cg.c`
- `src/bridges/opengl/impl/glsl2cg.h`

当前这些文件已存在于 `src` 下，并由顶层 `GLOB_RECURSE` 自动编译。

入口：

- `perform_static_analysis(const char *string, int size)`
- `translate_frag_shader(const char *string, int size)`
- `translate_vert_shader(char *string, int size)`

### 静态替换

输出 buffer 固定分配 `0x8000` 字节。

预置：

```c
#define saturate(a) __saturate(a)
#define texture __texture
```

扫描并替换/移除：

- `lowp`：移除
- `mediump`：移除
- `highp`：移除
- `precision`：把语句改为注释形式
- `texture2D`：转换为 Cg 对应调用形式
- `fract`：转换
- `mix` → `lerp`
- `vec2` → `float2`
- `vec3` → `float3`
- `vec4` → `float4`
- `mat2` → `float2x2`
- `mat3` → `float3x3`
- `mat4` → `float4x4`
- `mod` → `fmod`
- `atan` → `atan2`
- `const` → `static`

扫描基于 `strstr` 和指针比较，不解析 token、注释、字符串、预处理器或嵌套语法。

### Fragment shader 重写

- 查找 `void main()`、`void main(void)` 或 `void main( void )`。
- 扫描 `varying` 声明。
- 为 varying 分配 Cg semantic：
  - float → `FOG`
  - float2 → `TEXCOORDn`
  - float3 → `TEXCOORDn`
  - float4 → `COLOR`
- 重写 main 参数和 fragment 输出。

### Vertex shader 重写

- 查找 main 的三种拼写。
- 扫描 `attribute`。
- 扫描 `varying`。
- 为 varying 输出分配 semantic：
  - float → `FOG`
  - float2/float3 → `TEXCOORDn`
  - float4 → `COLOR`
- 修改 main 签名。
- 尝试处理 `gm_Matrices` 的矩阵乘法并改写为 `mul` 形式。

转换器大量使用固定数组、`strstr`、`strcpy`、`sprintf` 和指针算术；未验证所有关键查找结果，遇到不匹配的 GLSL 格式可能空指针或越界。

## Cocos2d-x 固定偏移补丁

运行时地址统一是 `0x98000000 + offset`。

当前启用：

### `0x69A342`

- 注释：`cocos2d::Node::onEnter`
- 原因：read nullptr
- 写入 16-bit Thumb `0x00BF`，即 NOP。

### `0x50E30E`

- 注释：`AudioManager::internal_loadSFX`
- 原因：阻止调用 OpenSL ES。
- 写入 16-bit Thumb `0x00BF`。

### `0x6E6AD4`

- 注释：`cocos2d::GLProgram::bindPredefinedVertexAttribs`
- 原因：阻止调用 `glBindAttribLocation`
- 写入 32-bit ARM 值 `0x00F020E3`。

当前注释掉的实验补丁包括：

- `cocos2d::experimental::FrameBuffer::_frameBuffers`
- `cocos2d::JniHelper::deleteLocalRefs`
- `cocos2d::JniHelper::convert`
- `AudioManager::init`

`bridgePatchGL` 中也保留了两个注释掉的 OpenGL 相关补丁：

- `0x6E6AD4`
- `0x6E6534`

这些偏移依赖目标 `libcocos2dcpp.so` 的精确版本和链接布局。任何版本变化都可能修改错误指令。

## AudioProvider bridge

替换 C++ mangled symbols：

- `_ZN13AudioProvider4initEff`
- `_ZN13AudioProvider6resumeEv`
- `_ZN13AudioProvider12setBGMVolumeEf`
- `_ZN13AudioProvider12setSFXVolumeEf`
- `_ZN13AudioProvider7suspendEv`
- `_ZN13AudioProvider6updateEv`
- `_ZN13AudioProviderD2Ev`

RTTI `_ZTI13AudioProvider` 的替换被注释。

### 本地伪对象布局

`audioProviderConstruct` 分配：

```c
struct IAMCLASS {
    IAMETHODS *sMethods;
};

struct IAMETHODS {
    uintptr_t methods[6];
};
```

methods：

1. `audioProviderInit`
2. `audioProviderResume`
3. `audioProviderSetBGMVolume`
4. `audioProviderSetSFXVolume`
5. `audioProviderSupsend`
6. `audioProviderUpdate`

destructor 不在该 methods 数组中。

当前方法行为：

- `audioProviderInit` 返回 1。
- `audioProviderSetBGMVolume` 返回 1。
- `audioProviderSetSFXVolume` 返回 1。
- 其他方法只记录日志。

`audioProviderDestruct` 不释放对象或 methods 数组。

### AudioManager hook

从目标 SO 查找：

- `_ZN12AudioManagerC2Ev`
- `_ZN12AudioManager4initEP13AudioProvider`

然后在固定偏移 `0x38FD1C` 安装绝对跳转到 `audioManagerCreate`。

`audioManagerCreate`：

1. 忽略传入的 `audioManager` 和 `audioProvider`。
2. `malloc(0x268)`。
3. 清零对象。
4. 调用目标库 `AudioManager` constructor。
5. 创建本地伪 AudioProvider。
6. 调用目标库 `AudioManager::init`。
7. 初始化失败时，从对象内部指针按手写偏移取 destructor 并调用。
8. 返回新 AudioManager 地址或 `NULL`。

constructor/init 查找结果没有空检查。

失败路径调用 destructor 后没有 `free(lpAudioManagerThis)`。
