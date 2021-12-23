#ifndef WIN32_OPENGL_H
#define WIN32_OPENGL_H

#include "platform.h"
#include "win32_lite.h"

#define GL_TRUE 1
#define GL_FALSE 0

#define GL_FRAGMENT_SHADER                0x8B30
#define GL_VERTEX_SHADER                  0x8B31
#define GL_ARRAY_BUFFER                   0x8892

#define GL_STREAM_DRAW                    0x88E0
#define GL_STREAM_READ                    0x88E1
#define GL_STREAM_COPY                    0x88E2
#define GL_STATIC_DRAW                    0x88E4
#define GL_STATIC_READ                    0x88E5
#define GL_STATIC_COPY                    0x88E6
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_DYNAMIC_READ                   0x88E9
#define GL_DYNAMIC_COPY                   0x88EA

#define GL_FRAMEBUFFER_SRGB               0x8DB9
#define GL_BLEND                          0x0BE2
#define GL_VERSION                        0x1F02
#define GL_COLOR_BUFFER_BIT               0x00004000
#define GL_TRIANGLES                      0x0004
#define GL_LINE_STRIP                     0x0003
#define GL_FLOAT                          0x1406
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_TEXTURE_2D                     0x0DE1
#define GL_RED                            0x1903
#define GL_R8                             0x8229
#define GL_UNSIGNED_BYTE                  0x1401
#define GL_TEXTURE_MAG_FILTER             0x2800
#define GL_TEXTURE_MIN_FILTER             0x2801
#define GL_NEAREST                        0x2600
#define GL_LINEAR                         0x2601
#define GL_SRC_ALPHA                      0x0302
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_FRONT_AND_BACK                 0x0408
#define GL_LINE                           0x1B01
#define GL_LINES                          0x0001
#define GL_FILL                           0x1B02

#define GL_RGB                            0x1907
#define GL_RGBA                           0x1908
#define GL_RGBA8                          0x8058

#define GL_DEBUG_SEVERITY_HIGH            0x9146
#define GL_DEBUG_SEVERITY_MEDIUM          0x9147
#define GL_DEBUG_SEVERITY_LOW             0x9148
#define GL_DEBUG_SEVERITY_NOTIFICATION    0x826B
#define GL_MAX_DEBUG_GROUP_STACK_DEPTH    0x826C
#define GL_DEBUG_GROUP_STACK_DEPTH        0x826D
#define GL_DEBUG_OUTPUT_SYNCHRONOUS       0x8242
#define GL_DEBUG_OUTPUT                   0x92E0
#define GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH 0x8243
#define GL_DEBUG_CALLBACK_FUNCTION        0x8244
#define GL_DEBUG_CALLBACK_USER_PARAM      0x8245
#define GL_DEBUG_SOURCE_API               0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM     0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER   0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY       0x8249
#define GL_DEBUG_SOURCE_APPLICATION       0x824A
#define GL_DEBUG_SOURCE_OTHER             0x824B
#define GL_DEBUG_TYPE_ERROR               0x824C
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR 0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR  0x824E
#define GL_DEBUG_TYPE_PORTABILITY         0x824F
#define GL_DEBUG_TYPE_PERFORMANCE         0x8250
#define GL_DEBUG_TYPE_OTHER               0x8251
#define GL_DEBUG_TYPE_MARKER              0x8268
#define GL_DEBUG_TYPE_PUSH_GROUP          0x8269
#define GL_DEBUG_TYPE_POP_GROUP           0x826A
#define GL_MAX_DEBUG_MESSAGE_LENGTH       0x9143
#define GL_MAX_DEBUG_LOGGED_MESSAGES      0x9144
#define GL_DEBUG_LOGGED_MESSAGES          0x9145
#define GL_SCISSOR_TEST                   0x0C11

typedef char GLchar;
typedef int GLsizei;
typedef float GLclampf;
typedef unsigned int GLuint;
typedef unsigned char GLubyte;
typedef signed int GLint;
typedef i64 GLintptr;
typedef float GLfloat;
typedef unsigned char GLboolean;
typedef unsigned int GLenum;
typedef i64 GLsizeiptr;
typedef unsigned int GLbitfield;
typedef void GLvoid;

extern "C" {
    const GLubyte* glGetString(GLenum name);
    void glEnable(GLenum cap);
    void glDisable(GLenum cap);

    void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
    void glClear(GLbitfield mask);
    void glDrawArrays(GLenum mode, GLint first, GLsizei count);

    typedef void (*GLDEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam);
}


#define LOAD_GL_ARB_PROC(proc, dll) proc = (proc##ARB_t*)gl_get_proc(#proc "ARB", dll)
#define LOAD_GL_PROC(proc, dll) proc = (proc##_t*)gl_get_proc(#proc, dll)

#ifdef WIN32_OPENGL_IMPL
#define DECL_GL_PROC(ret, proc, ...) typedef ret proc##_t(__VA_ARGS__); proc##_t *proc = nullptr
#define DECL_GL_ARB_PROC(ret, proc, ...) typedef ret proc##ARB_t(__VA_ARGS__); proc##ARB_t *proc = nullptr
#else
#define DECL_GL_PROC(ret, proc, ...) typedef ret proc##_t(__VA_ARGS__); extern proc##_t *proc
#define DECL_GL_ARB_PROC(ret, proc, ...) typedef ret proc##ARB_t(__VA_ARGS__); extern proc##ARB_t *proc
#endif

DECL_GL_ARB_PROC(char*, wglGetExtensionsString, HDC hdc);
DECL_GL_ARB_PROC(BOOL, wglChoosePixelFormat, HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
DECL_GL_ARB_PROC(HGLRC, wglCreateContextAttribs, HDC hDC, HGLRC hShareContext, const int *attribList);

DECL_GL_PROC(void, glViewport, GLint x, GLint y, GLsizei width, GLsizei height);
DECL_GL_PROC(void, glAttachShader, GLuint program, GLuint shader);
DECL_GL_PROC(void, glBindAttribLocation, GLuint program, GLuint index, const GLchar *name);
DECL_GL_PROC(void, glCompileShader, GLuint shader);
DECL_GL_PROC(GLuint, glCreateProgram, void);
DECL_GL_PROC(GLuint, glCreateShader, GLenum type);
DECL_GL_PROC(void, glLinkProgram, GLuint program);
DECL_GL_PROC(void, glShaderSource, GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
DECL_GL_PROC(void, glGenBuffers, GLsizei n, GLuint *buffers);
DECL_GL_PROC(void, glGenVertexArrays, GLsizei n, GLuint *arrays);
DECL_GL_PROC(void, glBindVertexArray, GLuint array);
DECL_GL_PROC(void, glBindBuffer, GLenum target, GLuint buffer);
DECL_GL_PROC(void, glBufferData, GLenum target, GLsizeiptr size, const void *data, GLenum usage);
DECL_GL_PROC(void, glBufferSubData, GLenum target, GLintptr offset, GLsizeiptr size, const void * data);
DECL_GL_PROC(void, glVertexAttribPointer, GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
DECL_GL_PROC(void, glEnableVertexAttribArray, GLuint index);
DECL_GL_PROC(void, glDisableVertexAttribArray, GLuint index);
DECL_GL_PROC(void, glUseProgram, GLuint program);
DECL_GL_PROC(void, glUniform1f, GLint location, GLfloat v0);
DECL_GL_PROC(void, glUniform2f, GLint location, GLfloat v0, GLfloat v1);
DECL_GL_PROC(void, glUniform3f, GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
DECL_GL_PROC(void, glUniform1fv, GLint location, GLsizei count, const GLfloat *value);
DECL_GL_PROC(void, glUniformMatrix3fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
DECL_GL_PROC(void, glUniformMatrix4fv, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
DECL_GL_PROC(GLint, glGetUniformLocation, GLuint program, const GLchar *name);
DECL_GL_PROC(void, glGetShaderiv, GLuint shader, GLenum pname, GLint *params); 
DECL_GL_PROC(void, glGetShaderInfoLog, GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
DECL_GL_PROC(void, glGetProgramiv, GLuint program, GLenum pname, GLint *params);
DECL_GL_PROC(void, glGetProgramInfoLog, GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog);
DECL_GL_PROC(void, glTexImage2D, GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid * data);
DECL_GL_PROC(void, glGenTextures, GLsizei n, GLuint * textures);
DECL_GL_PROC(void, glBindTexture, GLenum target, GLuint texture);
DECL_GL_PROC(void, glTexParameteri, GLenum target, GLenum pname, GLint param);
DECL_GL_PROC(void, glBlendFunc, GLenum sfactor, GLenum dfactor);
DECL_GL_PROC(void, glDebugMessageCallback, GLDEBUGPROC callback, void * userParam);
DECL_GL_PROC(void, glPolygonMode, GLenum face, GLenum mode);
DECL_GL_PROC(void, glBindBufferRange, GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
DECL_GL_PROC(void, glScissor, GLint x, GLint y, GLsizei width, GLsizei height);

#endif // WIN32_OPENGL_H