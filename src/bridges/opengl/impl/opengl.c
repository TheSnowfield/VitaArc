#include <stdlib.h>
#include <vitaGL.h>

#include "../../../common/define.h"
#include "../../../logcat/logcat.h"
#include "../../../utils/fs.h"
#include "opengl.h"

void glBindRenderbuffer(GLenum target, GLuint renderbuffer)
{
  logV(TAG, "Unsupported 'glBindRenderbuffer' called.");
}

void glDeleteRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
  logV(TAG, "Unsupported 'glDeleteRenderbuffers' called.");
}

void glGenRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
  logV(TAG, "Unsupported 'glGenRenderbuffers' called.");
}

void glFramebufferRenderbuffer(GLenum target, GLenum attachment,
                               GLenum renderbuffertarget, GLuint renderbuffer)
{
  logV(TAG, "Unsupported 'glFramebufferRenderbuffer' called.");
}

void glRenderbufferStorage(GLenum target, GLenum internalformat,
                           GLsizei width, GLsizei height)
{
  logV(TAG, "Unsupported 'glRenderbufferStorage' called.");
}

void glPixelStorei(GLenum pname, GLint param)
{
  logV(TAG, "Unsupported 'glPixelStorei' called.");
}

typedef struct
{
  void *ptr;
  int32_t size;
  vglMemType type;
  GLboolean used;
} _gpubuffer;

GLboolean glIsBuffer(GLuint buffer)
{
  logV(TAG, "Called glIsBuffer(%d)", buffer);
  _gpubuffer *p = (_gpubuffer *)buffer;
  return (p != NULL);
}

GLboolean glIsRenderbuffer(GLuint renderbuffer)
{
  logV(TAG, "Called glIsRenderbuffer(%d)", renderbuffer);
  void *p = (void *)renderbuffer;
  return (p != NULL);
}

void *glMapBuffer(GLenum target, GLenum access)
{
  logV(TAG, "Unsupported 'glMapBuffer' called.");
  return NULL;
}

GLboolean glUnmapBuffer(GLenum target)
{
  logV(TAG, "Unsupported 'glUnmapBuffer' called.");
  return GL_FALSE;
}

void glGenVertexArrays(GLsizei n, GLuint *arrays)
{
  logV(TAG, "Unsupported 'glGenVertexArrays' called.");
}

void glDeleteVertexArrays(GLsizei n, const GLuint *arrays)
{
  logV(TAG, "Unsupported 'glDeleteVertexArrays' called.");
}

void glBindVertexArray(GLuint array)
{
  logV(TAG, "Unsupported 'glBindVertexArray' called.");
}

void *eglGetProcAddress(char const *procname)
{
  logV(TAG, "eglGetProcAddress(\"%s\")", procname);

  if (strcmp("glGenVertexArraysOES", procname) == 0)
    return glGenVertexArrays;
  if (strcmp("glDeleteVertexArraysOES", procname) == 0)
    return glDeleteVertexArrays;
  if (strcmp("glBindVertexArrayOES", procname) == 0)
    return glBindVertexArray;

  return NULL;
}

const char *SHADER_VERTEX_CC[] =
{
"uniform CC_PMatrix { float4x4 self; } CC_PMatrixRef;\n"
"uniform CC_MVMatrix { float4x4 self; } CC_MVMatrixRef;\n"
"uniform CC_MVPMatrix { float4x4 self; } CC_MVPMatrixRef;\n"
"uniform CC_NormalMatrix { float3x3 self; } CC_NormalMatrixRef;\n"
"uniform CC_Time { float4 self; } CC_TimeRef;\n"
"uniform CC_SinTime { float4 self; } CC_SinTimeRef;\n"
"uniform CC_CosTime { float4 self; } CC_CosTimeRef;\n"
"uniform CC_Random01 { float4 self; } CC_Random01Ref;\n"
"uniform sampler2D CC_Texture0;\n"
"uniform sampler2D CC_Texture1;\n"
"uniform sampler2D CC_Texture2;\n"
"uniform sampler2D CC_Texture3;\n"
"//CC INCLUDES END\n\n"

"float4 a_position;\n"
"float2 a_texCoord;\n"
"float4 a_color;\n\n"

"#define guy_empty\n\n"

"varying float4 v_fragmentColor;\n"
"varying float2 v_texCoord;\n"

"void main()\n"
"{\n"
//"  vPosition = mul(CC_MVPMatrixRef.self, a_position);\n" float4 out vPosition : POSITION
"  v_fragmentColor = a_color;\n"
"  v_texCoord = a_texCoord;\n"
"}\n",
"\0"
};

void _glShaderSource(GLuint handle, GLsizei count,
                     const GLchar *const *string, const GLint *length)
{
  logV(TAG, "%d", count);

  // replace shader
  if (count == 4)
  {
    logV(TAG, "%s", string[2]);
    // if (strcmp(string[2], "\n#define guy_empty\n") == 0)
    //   return glShaderSource(handle, 1, SHADER_VERTEX_CC, NULL);
    if (strcmp(string[2], "\n#define guy_empty\n") == 0)
    {
      uint32_t nFileSize = utilGetFileSize("ux0:vitaarc/shader/cc.gxp");
      void *lpFile = malloc(nFileSize);
      utilsReadFileAll("ux0:vitaarc/shader/cc.gxp", lpFile, nFileSize);
      logV(TAG, "Loading shader cache %d %d %s", lpFile, nFileSize, "ux0:vitaarc/shader/cc.gxp");
      glShaderBinary(1, &handle, NULL, lpFile, nFileSize);
      logV(TAG, "Loaded %s", lpFile);
      free(lpFile);

      return;
    }
  }

  logV(TAG, "nothing matched");
}

void _glGetShaderiv(GLuint handle, GLenum pname, GLint *params)
{
  logV(TAG, "called _glGetShaderiv(%d, %d, %08X)", handle, pname, params);

  glGetShaderiv(handle, pname, params);
  
  if(pname == GL_COMPILE_STATUS)
    logV(TAG, "compile result %d", *params);
}
