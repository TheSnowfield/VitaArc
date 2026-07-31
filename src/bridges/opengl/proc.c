#include <stddef.h>
#include <string.h>

#include <loader.h>
#include "gl.h"

typedef struct {
    const char* ext;
    const char* name;
} gl_alias_t;

static const gl_alias_t __aliases[] = {
    {"glGenVertexArraysOES", "glGenVertexArrays"},
    {"glDeleteVertexArraysOES", "glDeleteVertexArrays"},
    {"glBindVertexArrayOES", "glBindVertexArray"},
    {"glMapBuffer", "glMapBufferOES"},
    {"glUnmapBuffer", "glUnmapBufferOES"},
    {"glBindFramebufferOES", "glBindFramebuffer"},
    {"glDeleteFramebuffersOES", "glDeleteFramebuffers"},
    {"glGenFramebuffersOES", "glGenFramebuffers"},
    {"glCheckFramebufferStatusOES", "glCheckFramebufferStatus"},
    {"glFramebufferTexture2DOES", "glFramebufferTexture2D"},
    {"glBindRenderbufferOES", "glBindRenderbuffer"},
    {"glDeleteRenderbuffersOES", "glDeleteRenderbuffers"},
    {"glGenRenderbuffersOES", "glGenRenderbuffers"},
    {"glRenderbufferStorageOES", "glRenderbufferStorage"},
    {"glFramebufferRenderbufferOES", "glFramebufferRenderbuffer"},
    {"glIsRenderbufferOES", "glIsRenderbuffer"},
    {"glGenerateMipmapOES", "glGenerateMipmap"},
    {"glBlendEquationOES", "glBlendEquation"},
};

/* Native vitaGL floats use hard-float; Android needs base PCS adapters. */
void (*bridge_egl_get_proc_address(const char* name))(void)
{
    if(!name) {
        return NULL;
    }

    if(strncmp(name, "gl", 2) != 0 && strncmp(name, "egl", 3) != 0) {
        return NULL;
    }

    for(size_t index = 0; index < sizeof(__aliases) / sizeof(__aliases[0]); ++index) {
        if(strcmp(name, __aliases[index].ext) == 0) {
            name = __aliases[index].name;
            break;
        }
    }

    return (void (*)(void))loader_get_bridge_proc_address(name);
}
