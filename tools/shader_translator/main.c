#include "host.h"
#include "glsl_utils.h"

void vglAddSemanticBinding(const char* name, int index, GLenum type)
{
    for(int i = 0; i < glsl_custom_bindings_num; ++i) {
        if(strcmp(glsl_custom_bindings[i].name, name) == 0) {
            glsl_custom_bindings[i].ref_idx = glsl_current_ref_idx;
            return;
        }
    }

    if(glsl_custom_bindings_num >= MAX_CUSTOM_BINDINGS) {
        fputs("Too many shader semantic bindings\n", stderr);
        exit(5);
    }

    glsl_sema_bind* bind = &glsl_custom_bindings[glsl_custom_bindings_num++];
    snprintf(bind->name, sizeof(bind->name), "%s", name);
    bind->idx = index;
    bind->type = type;
    bind->ref_idx = glsl_current_ref_idx;
}

int main(int argc, char** argv)
{
    if(argc != 4) {
        return 1;
    }

    FILE* input = fopen(argv[1], "rb");
    if(!input) {
        return 2;
    }

    if(fseek(input, 0, SEEK_END)) {
        fclose(input);
        return 3;
    }

    long length = ftell(input);
    if(length < 0 || length > 1024 * 1024) {
        fclose(input);
        return 3;
    }

    size_t size = length;
    rewind(input);
    shader source = {0};
    source.source = malloc(size + 1);
    if(!source.source || fread(source.source, 1, size, input) != size) {
        free(source.source);
        fclose(input);
        return 3;
    }

    source.source[size] = 0;
    fclose(input);
    source.type = strcmp(argv[3], "vertex") == 0 ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
    glsl_sema_mode = VGL_MODE_GLOBAL;
    vglAddSemanticBinding("v_texCoord", 0, VGL_TYPE_TEXCOORD);
    vglAddSemanticBinding("v_fragmentColor", 0, VGL_TYPE_COLOR);
    glsl_translator_process(&source);
    FILE* output = fopen(argv[2], "wb");
    if(!output) {
        free(source.source);
        return 4;
    }

    size = strlen(source.source);
    int result = fwrite(source.source, 1, size, output) == size ? 0 : 4;
    if(fclose(output)) {
        result = 4;
    }

    free(source.source);
    return result;
}
