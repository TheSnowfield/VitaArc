#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psp2/kernel/clib.h>
#include <psp2/kernel/processmgr.h>
#include <vitaGL.h>
#include <opengl/pixel_store.h>
#include <opengl/buffers.h>

/* A real GXM/vitaGL probe. The two Cg sources implement the first shader pair
 * dumped by the supported game's production bridge. No runtime compiler is
 * involved: glShaderBinary accepts vitaGL's matrix-index header plus GXP. */
static GLuint load_shader(const char* path, GLenum type)
{
    FILE* file = fopen(path, "rb");
    if(!file) {
        sceClibPrintf("VitaArc shader probe: cannot read %s\n", path);
        return 0;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    if(size < 64 || size > 1024 * 1024) {
        fclose(file);
        return 0;
    }

    uint32_t* binary = malloc((size_t)size + 8);
    if(!binary || fread(binary + 2, 1, (size_t)size, file) != (size_t)size) {
        free(binary);
        fclose(file);
        return 0;
    }

    fclose(file);
    const SceGxmProgram* program = (const SceGxmProgram*)(binary + 2);
    if(binary[2] != UINT32_C(0x00505847)) {
        free(binary);
        return 0;
    }

    const SceGxmProgramParameter* matrix = sceGxmProgramFindParameterByName(program, "CC_MVPMatrix");
    binary[0] = matrix ? 1 : 0;
    if(matrix) {
        binary[1] = sceGxmProgramParameterGetIndex(program, matrix);
    } else {
        memmove(binary + 1, binary + 2, (size_t)size);
    }

    GLuint shader = glCreateShader(type);
    glShaderBinary(1, &shader, 0, binary, (GLsizei)size + (matrix ? 8 : 4));
    free(binary);
    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    sceClibPrintf("VitaArc shader probe: binary type=%x shader=%u status=%d\n", type, shader, status);
    return status ? shader : 0;
}

static int run_probe(void)
{
    vglInitExtended(0, 960, 544, 24 * 1024 * 1024, SCE_GXM_MULTISAMPLE_NONE);
    GLuint vertex = load_shader("app0:probe.vert.gxp", GL_CG_VERTEX_SHADER_EXT);
    GLuint fragment = load_shader("app0:probe.frag.gxp", GL_CG_FRAGMENT_SHADER_EXT);
    if(!vertex || !fragment) {
        return 1;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glBindAttribLocation(program, 0, "a_position");
    glBindAttribLocation(program, 1, "a_color");
    glBindAttribLocation(program, 2, "a_texCoord");
    glLinkProgram(program);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    GLint matrix = glGetUniformLocation(program, "CC_MVPMatrix");
    GLint sampler = glGetUniformLocation(program, "CC_Texture0");
    sceClibPrintf("VitaArc shader probe: linked=%d matrix=%d sampler=%d\n", linked, matrix, sampler);
    if(!linked || matrix < 0 || sampler < 0) {
        return 2;
    }

    glUseProgram(program);
    const GLfloat transform[16] = {0.5f, 0, 0, 0, 0, 0.5f, 0, 0, 0, 0, 1, 0, 0.75f, 0, 0, 1};

    glUniformMatrix4fv(matrix, 1, GL_FALSE, transform);
    glUniform1i(sampler, 0);
    GLuint texture;
    /* Two RGB rows with GLES's default four-byte alignment. Ignoring alignment
     * corrupts the second row even though the first row appears correct. */
    const GLubyte texel[] = {200, 100, 50, 11, 200, 100, 50};

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    bridge_gl_pixel_store_i(GL_UNPACK_ALIGNMENT, 4);
    bridge_gl_tex_image_2d(GL_TEXTURE_2D, 0, GL_RGB, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, texel);
    const GLfloat positions[] = {-1, -1, 0, 1, 1, -1, 0, 1, -1, 1, 0, 1, 1, 1, 0, 1};

    const GLfloat colors[] = {0.5f, 1, 1, 1, 0.5f, 1, 1, 1, 0.5f, 1, 1, 1, 0.5f, 1, 1, 1};

    const GLfloat coords[] = {0, 0, 1, 0, 0, 1, 1, 1};

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, positions);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, colors);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, coords);
    for(GLuint index = 0; index < 3; ++index) {
        glEnableVertexAttribArray(index);
    }

    glViewport(0, 0, 960, 544);
    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glFinish();
    /* Use a GPU transfer, which also requests emulator surface readback. */
    GLubyte* pixels = vglMalloc(960 * 544 * 4);
    if(!pixels) {
        return 4;
    }

    memset(pixels, 0, 960 * 544 * 4);
    vglReadPixels(0, 0, 960, 544, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    GLubyte* center = pixels + (272 * 960 + 720) * 4;
    GLubyte* corner = pixels + (40 * 960 + 40) * 4;
    /* A translated square excludes screen center. A missing matrix transpose
     * keeps this pixel covered, so a diagonal-only transform cannot pass. */
    GLubyte* outside = pixels + (272 * 960 + 480) * 4;
    GLubyte* upper = pixels + (200 * 960 + 720) * 4;
    GLubyte* lower = pixels + (344 * 960 + 720) * 4;
    GLenum error = bridge_gl_get_error();
    sceClibPrintf("VitaArc shader probe: pixels center=%u,%u,%u,%u corner=%u,%u,%u,%u error=%x\n",
        center[0],
        center[1],
        center[2],
        center[3],
        corner[0],
        corner[1],
        corner[2],
        corner[3],
        error);
    sceClibPrintf("VitaArc shader probe: matrix outside=%u,%u,%u,%u\n", outside[0], outside[1], outside[2], outside[3]);
    sceClibPrintf("VitaArc shader probe: RGB unpack rows=%u,%u,%u / %u,%u,%u\n",
        upper[0],
        upper[1],
        upper[2],
        lower[0],
        lower[1],
        lower[2]);
    vglSwapBuffers(GL_TRUE);
    glDeleteTextures(1, &texture);
    glDeleteProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    int result = error == GL_NO_ERROR && abs((int)center[0] - 100) <= 3 && abs((int)center[1] - 100) <= 3 &&
            abs((int)center[2] - 50) <= 3 && corner[0] == 0 && corner[1] == 0 && corner[2] == 255 && outside[0] == 0 &&
            outside[1] == 0 && outside[2] == 255 && abs((int)upper[0] - 100) <= 3 && abs((int)upper[1] - 100) <= 3 &&
            abs((int)upper[2] - 50) <= 3 && abs((int)lower[0] - 100) <= 3 && abs((int)lower[1] - 100) <= 3 &&
            abs((int)lower[2] - 50) <= 3
        ? 0
        : 3;
    vglFree(pixels);
    return result;
}

int main(void)
{
    int result = run_probe();
    sceClibPrintf("VitaArc shader probe: exit=%d\n", result);
    sceKernelExitProcess(result);
    return result;
}
