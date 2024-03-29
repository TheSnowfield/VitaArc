#include <common/define.h>
#include <utils/patcher.h>
#include <logcat/logcat.h>

#include "opengl.h"
#include "impl/opengl.h"

static const BRIDGEFUNC BRIDGE_GL[] =
{
  {"glActiveTexture", (uintptr_t)&glActiveTexture},
  {"glAttachShader", (uintptr_t)&glAttachShader},
  {"glBindAttribLocation", (uintptr_t)&glBindAttribLocation},
  {"glBindBuffer", (uintptr_t)&glBindBuffer},
  {"glBindFramebuffer", (uintptr_t)&glBindFramebuffer},
  {"glBindRenderbuffer", (uintptr_t)&glBindRenderbuffer},
  {"glBindTexture", (uintptr_t)&glBindTexture},
  {"glBlendEquation", (uintptr_t)&glBlendEquation},
  {"glBlendFunc", (uintptr_t)&glBlendFunc},
  {"glBufferData", (uintptr_t)&glBufferData},
  {"glBufferSubData", (uintptr_t)&glBufferSubData},
  {"glCheckFramebufferStatus", (uintptr_t)&glCheckFramebufferStatus},
  {"glClear", (uintptr_t)&glClear},
  {"glClearColor", (uintptr_t)&glClearColor},
  {"glClearDepthf", (uintptr_t)&glClearDepthf},
  {"glClearStencil", (uintptr_t)&glClearStencil},
  {"glColorMask", (uintptr_t)&glColorMask},
  {"glCompileShader", (uintptr_t)&glCompileShader},
  {"glCompressedTexImage2D", (uintptr_t)&glCompressedTexImage2D},
  {"glCreateProgram", (uintptr_t)&glCreateProgram},
  {"glCreateShader", (uintptr_t)&glCreateShader},
  {"glCullFace", (uintptr_t)&glCullFace},
  {"glDeleteBuffers", (uintptr_t)&glDeleteBuffers},
  {"glDeleteFramebuffers", (uintptr_t)&glDeleteFramebuffers},
  {"glDeleteProgram", (uintptr_t)&glDeleteProgram},
  {"glDeleteRenderbuffers", (uintptr_t)&glDeleteRenderbuffers},
  {"glDeleteShader", (uintptr_t)&glDeleteShader},
  {"glDeleteTextures", (uintptr_t)&glDeleteTextures},
  {"glDepthFunc", (uintptr_t)&glDepthFunc},
  {"glDepthMask", (uintptr_t)&glDepthMask},
  {"glDisable", (uintptr_t)&glDisable},
  {"glDisableVertexAttribArray", (uintptr_t)&glDisableVertexAttribArray},
  {"glDrawArrays", (uintptr_t)&glDrawArrays},
  {"glDrawElements", (uintptr_t)&glDrawElements},
  {"glEnable", (uintptr_t)&glEnable},
  {"glEnableVertexAttribArray", (uintptr_t)&glEnableVertexAttribArray},
  {"glFramebufferRenderbuffer", (uintptr_t)&glFramebufferRenderbuffer},
  {"glFramebufferTexture2D", (uintptr_t)&glFramebufferTexture2D},
  {"glFrontFace", (uintptr_t)&glFrontFace},
  {"glGenBuffers", (uintptr_t)&glGenBuffers},
  {"glGenFramebuffers", (uintptr_t)&glGenFramebuffers},
  {"glGenRenderbuffers", (uintptr_t)&glGenRenderbuffers},
  {"glGenTextures", (uintptr_t)&glGenTextures},
  {"glGenerateMipmap", (uintptr_t)&glGenerateMipmap},
  {"glGetActiveAttrib", (uintptr_t)&glGetActiveAttrib},
  {"glGetActiveUniform", (uintptr_t)&glGetActiveUniform},
  {"glGetAttribLocation", (uintptr_t)&glGetAttribLocation},
  {"glGetError", (uintptr_t)&glGetError},
  {"glGetFloatv", (uintptr_t)&glGetFloatv},
  {"glGetIntegerv", (uintptr_t)&glGetIntegerv},
  {"glGetProgramInfoLog", (uintptr_t)&glGetProgramInfoLog},
  {"glGetProgramiv", (uintptr_t)&glGetProgramiv},
  {"glGetShaderInfoLog", (uintptr_t)&glGetShaderInfoLog},
  {"glShaderSource", (uintptr_t)&_glShaderSource},
  {"glGetShaderiv", (uintptr_t)&_glGetShaderiv},
  {"glGetString", (uintptr_t)&glGetString},
  {"glGetUniformLocation", (uintptr_t)&glGetUniformLocation},
  {"glIsBuffer", (uintptr_t)&glIsBuffer},
  {"glIsEnabled", (uintptr_t)&glIsEnabled},
  {"glIsRenderbuffer", (uintptr_t)&glIsRenderbuffer},
  {"glLineWidth", (uintptr_t)&glLineWidth},
  {"glLinkProgram", (uintptr_t)&glLinkProgram},
  {"glMapBufferOES", (uintptr_t)&glMapBuffer},
  {"glPixelStorei", (uintptr_t)&glPixelStorei},
  {"glReadPixels", (uintptr_t)&glReadPixels},
  {"glRenderbufferStorage", (uintptr_t)&glRenderbufferStorage},
  {"glScissor", (uintptr_t)&glScissor},
  // {"glGetShaderSource", (uintptr_t)&glGetShaderSource},
  {"glStencilFunc", (uintptr_t)&glStencilFunc},
  {"glStencilMask", (uintptr_t)&glStencilMask},
  {"glStencilOp", (uintptr_t)&glStencilOp},
  {"glTexImage2D", (uintptr_t)&glTexImage2D},
  {"glTexParameteri", (uintptr_t)&glTexParameteri},
  {"glTexSubImage2D", (uintptr_t)&glTexSubImage2D},
  {"glUniform1f", (uintptr_t)&glUniform1f},
  {"glUniform1fv", (uintptr_t)&glUniform1fv},
  {"glUniform1i", (uintptr_t)&glUniform1i},
  {"glUniform2f", (uintptr_t)&glUniform2f},
  {"glUniform2fv", (uintptr_t)&glUniform2fv},
  {"glUniform2i", (uintptr_t)&glUniform2i},
  {"glUniform2iv", (uintptr_t)&glUniform2iv},
  {"glUniform3f", (uintptr_t)&glUniform3f},
  {"glUniform3fv", (uintptr_t)&glUniform3fv},
  {"glUniform3i", (uintptr_t)&glUniform3i},
  {"glUniform3iv", (uintptr_t)&glUniform3iv},
  {"glUniform4f", (uintptr_t)&glUniform4f},
  {"glUniform4fv", (uintptr_t)&glUniform4fv},
  {"glUniform4i", (uintptr_t)&glUniform4i},
  {"glUniform4iv", (uintptr_t)&glUniform4iv},
  {"glUniformMatrix2fv", (uintptr_t)&glUniformMatrix2fv},
  {"glUniformMatrix3fv", (uintptr_t)&glUniformMatrix3fv},
  {"glUniformMatrix4fv", (uintptr_t)&glUniformMatrix4fv},
  {"glUnmapBufferOES", (uintptr_t)&glUnmapBuffer},
  {"glUseProgram", (uintptr_t)&glUseProgram},
  {"glVertexAttribPointer", (uintptr_t)&glVertexAttribPointer},
  {"glViewport", (uintptr_t)&glViewport},
  {"eglGetProcAddress", (uintptr_t)&eglGetProcAddress}
};

void bridgePatchGL(HSOLIB hSoLibrary)
{
  patchSymbols(hSoLibrary, BRIDGE_GL, sizeof(BRIDGE_GL) / sizeof(BRIDGEFUNC));
  
  // cocos2d::GLProgram::bindPredefinedVertexAttribs
  // calling unsupported function
  // patchARM(hSoLibrary, 0x6E6AD4, 0x00BF00BF);

  // cocos2d::GLProgram::link
  // calling unsupported function
  // patchARM(hSoLibrary, 0x6E6534, 0x00BF00BF);

}
