"""Use production symbol definitions/search with sentinel platform callbacks.

Only the platform/shader/float callees absent from this test are substituted.
Buffer/VAO/proc wrappers and the symbol search execute production C sources.
"""

import importlib.util
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location("symbols", root / "tools/generate_symbols.py")
symbols = importlib.util.module_from_spec(spec)
spec.loader.exec_module(symbols)
output = Path(sys.argv[1])
output.mkdir(parents=True, exist_ok=True)
entries = [entry for entry in symbols.parse_definition(root / "src/bridges/symbol.def")
           if entry[0].startswith(("gl", "egl"))]
callbacks = sorted({callback for _, callback in entries})
real = {
    "bridge_gl_gen_buffers", "bridge_gl_bind_buffer", "bridge_gl_delete_buffers",
    "bridge_gl_is_buffer", "bridge_gl_get_error", "bridge_gl_get_integer_v",
    "bridge_gl_get_float_v", "bridge_gl_get_boolean_v", "bridge_gl_get_vertex_attrib_iv",
    "bridge_gl_get_vertex_attrib_fv", "bridge_gl_vertex_attrib_pointer",
    "bridge_gl_gen_vertex_arrays", "bridge_gl_bind_vertex_array",
    "bridge_gl_delete_vertex_arrays", "bridge_egl_get_proc_address",
    "glGetVertexAttribPointerv",
    "bridge_gl_pixel_store_i", "bridge_gl_tex_image_2d", "bridge_gl_tex_sub_image_2d",
}
declarations = []
stubs = []
for callback in callbacks:
    if callback not in real:
        # Separate, distinguishable sentinel addresses for omitted callees.
        declarations.append(f"extern void fixture_{callback}(void);")
        declarations.append(f"#define {callback} fixture_{callback}")
        stubs.append(f"void fixture_{callback}(void) {{ ++fixture_calls; }}")

rows = "\n".join(f'  {{"{name}", (uintptr_t)&{callback}}},'
                 for name, callback in sorted(entries))
(output / "bridge_symbols.h").write_text(
    '#include <patcher.h>\n#include <opengl/gl.h>\n' + "\n".join(declarations) +
    '\nstatic const patch_func_t __bridge_symbols[] = {\n' + rows + '\n};\n',
    encoding="utf-8", newline="\n")

checks = "\n".join(
    f'  assert(bridge_egl_get_proc_address("{name}") == '
    f'(void (*)(void))loader_get_bridge_proc_address("{name}"));'
    for name, _ in entries)
(output / "gl_proc_fixture.c").write_text(
    '#include <assert.h>\n#include <loader.h>\n#include <opengl/gl.h>\n'
    'static unsigned fixture_calls;\n' + "\n".join(stubs) + '\n'
    'void test_gl_proc_addresses(void) {\n' + checks + '\n'
    '  assert(!bridge_egl_get_proc_address(NULL));\n'
    '  assert(!bridge_egl_get_proc_address(""));\n'
    '  assert(!bridge_egl_get_proc_address("malloc"));\n'
    '  assert(!bridge_egl_get_proc_address("glDoesNotExist"));\n'
    '  assert(!bridge_egl_get_proc_address("glDrawArraysInstancedUnimplemented"));\n'
    '  assert(!loader_get_bridge_proc_address(NULL));\n'
    '  assert(!loader_get_bridge_proc_address("!"));\n'
    '  assert(!loader_get_bridge_proc_address("zzzz"));\n'
    '  assert(bridge_egl_get_proc_address("glGenBuffers") == (void (*)(void))bridge_gl_gen_buffers);\n'
    '  assert(bridge_egl_get_proc_address("glBindBuffer") == (void (*)(void))bridge_gl_bind_buffer);\n'
    '  assert(bridge_egl_get_proc_address("glGenVertexArraysOES") == (void (*)(void))bridge_gl_gen_vertex_arrays);\n'
    '  assert(bridge_egl_get_proc_address("glBindVertexArrayOES") == (void (*)(void))bridge_gl_bind_vertex_array);\n'
    '  assert(bridge_egl_get_proc_address("glDeleteVertexArraysOES") == (void (*)(void))bridge_gl_delete_vertex_arrays);\n'
    '  assert(bridge_egl_get_proc_address("glClearColor") == fixture_bridge_softfp_gl_clear_color);\n'
    '  assert(bridge_egl_get_proc_address("glUniform4f") == fixture_bridge_softfp_gl_uniform4f);\n'
    '  assert(bridge_egl_get_proc_address("glShaderSource") == fixture__gl_shader_source);\n'
    '  assert(bridge_egl_get_proc_address("glCompileShader") == fixture__gl_compile_shader);\n'
    '  assert(bridge_egl_get_proc_address("glMapBuffer") == bridge_egl_get_proc_address("glMapBufferOES"));\n'
    '  assert(bridge_egl_get_proc_address("glBindFramebufferOES") == bridge_egl_get_proc_address("glBindFramebuffer"));\n'
    '  GLuint name = 0;\n'
    '  void (*gen)(GLsizei, GLuint *) = (void (*)(GLsizei, GLuint *))bridge_egl_get_proc_address("glGenBuffers");\n'
    '  void (*bind)(GLenum, GLuint) = (void (*)(GLenum, GLuint))bridge_egl_get_proc_address("glBindBuffer");\n'
    '  GLboolean (*is_buffer)(GLuint) = (GLboolean (*)(GLuint))bridge_egl_get_proc_address("glIsBuffer");\n'
    '  void (*remove)(GLsizei, const GLuint *) = (void (*)(GLsizei, const GLuint *))bridge_egl_get_proc_address("glDeleteBuffers");\n'
    '  gen(1, &name);\n  assert(name && !is_buffer(name));\n'
    '  bind(GL_ARRAY_BUFFER, name);\n  assert(is_buffer(name));\n'
    '  remove(1, &name);\n  assert(!is_buffer(name));\n'
    '  assert(!fixture_calls);\n}\n', encoding="utf-8", newline="\n")
