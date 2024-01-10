#include "win32_opengl.h"
#include "win32_gdi32.h"

#include "core.h"
#include "maths.h"

#include "win32_lite.h"
#include "win32_core.h"

void* gl_get_proc(const char *name, HMODULE gl_dll)
{
    void *proc = (void*)wglGetProcAddress(name);
    if (proc == nullptr) proc = (void*)GetProcAddress(gl_dll, name);
    return proc;
}

void load_opengl_procs(HANDLE gl_dll)
{
    LOAD_GL_ARB_PROC(wglGetExtensionsString, gl_dll);

    LOAD_GL_PROC(glBindAttribLocation, gl_dll);

    LOAD_GL_PROC(glCreateProgram, gl_dll);
    LOAD_GL_PROC(glCreateShader, gl_dll);
    LOAD_GL_PROC(glDeleteShader, gl_dll);
    LOAD_GL_PROC(glDeleteProgram, gl_dll);
    LOAD_GL_PROC(glShaderSource, gl_dll);
    LOAD_GL_PROC(glCompileShader, gl_dll);
    LOAD_GL_PROC(glAttachShader, gl_dll);
    LOAD_GL_PROC(glDetachShader, gl_dll);
    LOAD_GL_PROC(glLinkProgram, gl_dll);

    LOAD_GL_PROC(glGenVertexArrays, gl_dll);
    LOAD_GL_PROC(glBindVertexArray, gl_dll);

    LOAD_GL_PROC(glGenBuffers, gl_dll);
    LOAD_GL_PROC(glBindBuffer, gl_dll);
    LOAD_GL_PROC(glBindBufferBase, gl_dll);
    LOAD_GL_PROC(glBindBufferRange, gl_dll);
    LOAD_GL_PROC(glBufferData, gl_dll);
    LOAD_GL_PROC(glBufferSubData, gl_dll);
    LOAD_GL_PROC(glMapBufferRange, gl_dll);
    LOAD_GL_PROC(glMapNamedBufferRange, gl_dll);
    LOAD_GL_PROC(glUnmapBuffer, gl_dll);
    LOAD_GL_PROC(glUnmapNamedBuffer, gl_dll);

    LOAD_GL_PROC(glVertexAttribPointer, gl_dll);
    LOAD_GL_PROC(glEnableVertexAttribArray, gl_dll);
    LOAD_GL_PROC(glDisableVertexAttribArray, gl_dll);
    LOAD_GL_PROC(glUseProgram, gl_dll);
    LOAD_GL_PROC(glUniform1i, gl_dll);
    LOAD_GL_PROC(glUniform1f, gl_dll);
    LOAD_GL_PROC(glUniform2f, gl_dll);
    LOAD_GL_PROC(glUniform3f, gl_dll);
    LOAD_GL_PROC(glUniformMatrix3fv, gl_dll);
    LOAD_GL_PROC(glUniformMatrix4fv, gl_dll);
    LOAD_GL_PROC(glGetUniformLocation, gl_dll);
    LOAD_GL_PROC(glGetShaderiv, gl_dll);
    LOAD_GL_PROC(glGetShaderInfoLog, gl_dll);
    LOAD_GL_PROC(glGetProgramiv, gl_dll);
    LOAD_GL_PROC(glGetProgramInfoLog, gl_dll);
    LOAD_GL_PROC(glGenTextures, gl_dll);
    LOAD_GL_PROC(glDeleteTextures, gl_dll);
    LOAD_GL_PROC(glBindTexture, gl_dll);
    LOAD_GL_PROC(glBlendFunc, gl_dll);
    LOAD_GL_PROC(glDebugMessageCallback, gl_dll);
    LOAD_GL_PROC(glPolygonMode, gl_dll);
    LOAD_GL_PROC(glUniform1fv, gl_dll);
    LOAD_GL_PROC(glScissor, gl_dll);
    LOAD_GL_PROC(glViewport, gl_dll);

    LOAD_GL_PROC(glPixelStorei, gl_dll);

    LOAD_GL_PROC(glTexParameteri, gl_dll);
    LOAD_GL_PROC(glTexStorage2D, gl_dll);
    LOAD_GL_PROC(glTexImage2D, gl_dll);
    LOAD_GL_PROC(glTexSubImage2D, gl_dll);
    LOAD_GL_PROC(glTextureSubImage2D, gl_dll);

    LOAD_GL_PROC(glMemoryBarrier, gl_dll);

}
