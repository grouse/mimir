#define LINUX_OPENGL_IMPL
#include "linux_opengl.h"
#include "linux_glx.h"

#include "core/core.h"
#include "core/maths.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

void load_opengl_procs()
{
    LOAD_GL_PROC(glXSwapIntervalEXT);
    LOAD_GL_PROC(glXCreateContextAttribsARB);

    LOAD_GL_PROC(glGetString);
    LOAD_GL_PROC(glEnable);
    LOAD_GL_PROC(glDisable);
    LOAD_GL_PROC(glClearColor);
    LOAD_GL_PROC(glClear);
    LOAD_GL_PROC(glDrawArrays);

    LOAD_GL_PROC(glAttachShader);
    LOAD_GL_PROC(glDetachShader);
    LOAD_GL_PROC(glBindAttribLocation);
    LOAD_GL_PROC(glCompileShader);
    LOAD_GL_PROC(glCreateProgram);
    LOAD_GL_PROC(glCreateShader);
    LOAD_GL_PROC(glDeleteShader);
    LOAD_GL_PROC(glDeleteProgram);
    LOAD_GL_PROC(glLinkProgram);
    LOAD_GL_PROC(glShaderSource);
    LOAD_GL_PROC(glGenBuffers);
    LOAD_GL_PROC(glGenVertexArrays);
    LOAD_GL_PROC(glBindVertexArray);
    LOAD_GL_PROC(glBindBuffer);
    LOAD_GL_PROC(glBufferData);
    LOAD_GL_PROC(glBufferSubData);
    LOAD_GL_PROC(glVertexAttribPointer);
    LOAD_GL_PROC(glMapBufferRange);
    LOAD_GL_PROC(glMapNamedBufferRange);
    LOAD_GL_PROC(glUnmapBuffer);
    LOAD_GL_PROC(glUnmapNamedBuffer);
    LOAD_GL_PROC(glEnableVertexAttribArray);
    LOAD_GL_PROC(glDisableVertexAttribArray);
    LOAD_GL_PROC(glUseProgram);
    LOAD_GL_PROC(glUniform1i);
    LOAD_GL_PROC(glUniform1f);
    LOAD_GL_PROC(glUniform2f);
    LOAD_GL_PROC(glUniform3f);
    LOAD_GL_PROC(glUniform1fv);
    LOAD_GL_PROC(glUniformMatrix3fv);
    LOAD_GL_PROC(glUniformMatrix4fv);
    LOAD_GL_PROC(glGetUniformLocation);
    LOAD_GL_PROC(glGetShaderiv);
    LOAD_GL_PROC(glGetShaderInfoLog);
    LOAD_GL_PROC(glGetProgramiv);
    LOAD_GL_PROC(glGetProgramInfoLog);
    LOAD_GL_PROC(glTexImage2D);
    LOAD_GL_PROC(glTexStorage2D);
    LOAD_GL_PROC(glGenTextures);
    LOAD_GL_PROC(glDeleteTextures);
    LOAD_GL_PROC(glBindTexture);
    LOAD_GL_PROC(glTexParameteri);
    LOAD_GL_PROC(glBlendFunc);
    LOAD_GL_PROC(glDebugMessageCallback);
    LOAD_GL_PROC(glPolygonMode);
    LOAD_GL_PROC(glBindBufferRange);
    LOAD_GL_PROC(glBindBufferBase);
    LOAD_GL_PROC(glScissor);
    LOAD_GL_PROC(glViewport);
    LOAD_GL_PROC(glTextureSubImage2D);
}

void opengl_enable_vsync(Display *dsp, Window wnd)
{
    u32 interval;
    glXQueryDrawable(dsp, wnd, GLX_MAX_SWAP_INTERVAL_EXT, &interval);
    glXSwapIntervalEXT(dsp, wnd, interval);
}
