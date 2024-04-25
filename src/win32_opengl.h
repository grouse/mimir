#ifndef WIN32_OPENGL_H
#define WIN32_OPENGL_H

#include "core/platform.h"
#include "core/win32_lite.h"

#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB             0x20A9

#define WGL_CONTEXT_MAJOR_VERSION_ARB           0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB           0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB             0x2093
#define WGL_CONTEXT_FLAGS_ARB                   0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB            0x9126
#define WGL_CONTEXT_DEBUG_BIT_ARB               0x0001
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB        0x00000001

#define WGL_DRAW_TO_WINDOW_ARB                  0x2001
#define WGL_SUPPORT_OPENGL_ARB                  0x2010
#define WGL_DOUBLE_BUFFER_ARB                   0x2011
#define WGL_PIXEL_TYPE_ARB                      0x2013
#define WGL_COLOR_BITS_ARB                      0x2014
#define WGL_DEPTH_BITS_ARB                      0x2022
#define WGL_STENCIL_BITS_ARB                    0x2023
#define WGL_TYPE_RGBA_ARB                       0x202B


#define GL_TRUE 1
#define GL_FALSE 0

#define GL_FRAGMENT_SHADER                0x8B30
#define GL_VERTEX_SHADER                  0x8B31
#define GL_ARRAY_BUFFER                   0x8892
#define GL_SHADER_STORAGE_BUFFER          0x90D2

#define GL_STREAM_DRAW                    0x88E0
#define GL_STREAM_READ                    0x88E1
#define GL_STREAM_COPY                    0x88E2
#define GL_STATIC_DRAW                    0x88E4
#define GL_STATIC_READ                    0x88E5
#define GL_STATIC_COPY                    0x88E6
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_DYNAMIC_READ                   0x88E9
#define GL_DYNAMIC_COPY                   0x88EA

#define GL_MAP_READ_BIT                   0x0001
#define GL_MAP_WRITE_BIT                  0x0002
#define GL_MAP_INVALIDATE_RANGE_BIT       0x0004
#define GL_MAP_INVALIDATE_BUFFER_BIT      0x0008
#define GL_MAP_FLUSH_EXPLICIT_BIT         0x0010
#define GL_MAP_UNSYNCHRONIZED_BIT         0x0020

#define GL_PIXEL_UNPACK_BUFFER            0x88EC

#define GL_UNPACK_ALIGNMENT               0x0CF5
#define GL_PACK_ALIGNMENT                 0x0D05

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

#define GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT 0x00000001
#define GL_ELEMENT_ARRAY_BARRIER_BIT      0x00000002
#define GL_UNIFORM_BARRIER_BIT            0x00000004
#define GL_TEXTURE_FETCH_BARRIER_BIT      0x00000008
#define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT 0x00000020
#define GL_COMMAND_BARRIER_BIT            0x00000040
#define GL_PIXEL_BUFFER_BARRIER_BIT       0x00000080
#define GL_TEXTURE_UPDATE_BARRIER_BIT     0x00000100
#define GL_BUFFER_UPDATE_BARRIER_BIT      0x00000200
#define GL_FRAMEBUFFER_BARRIER_BIT        0x00000400
#define GL_TRANSFORM_FEEDBACK_BARRIER_BIT 0x00000800
#define GL_ATOMIC_COUNTER_BARRIER_BIT     0x00001000
#define GL_ALL_BARRIER_BITS               0xFFFFFFFF

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
    PROC wglGetProcAddress(LPCSTR);

    HGLRC wglCreateContext(HDC);
    BOOL wglDeleteContext(HGLRC);
    BOOL wglMakeCurrent(HDC, HGLRC);

    const GLubyte* glGetString(GLenum name);
    void glEnable(GLenum cap);
    void glDisable(GLenum cap);

    void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
    void glClear(GLbitfield mask);
    void glDrawArrays(GLenum mode, GLint first, GLsizei count);

    typedef void (*GLDEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam);
}

void* gl_get_proc(const char *name, HMODULE gl_dll);
void load_opengl_procs(HMODULE gl_dll);

#define LOAD_GL_ARB_PROC(proc, dll) proc = (proc##ARB_t*)gl_get_proc(#proc "ARB", dll)
#define LOAD_GL_PROC(proc, dll) proc = (proc##_t*)gl_get_proc(#proc, dll)

#ifdef WIN32_OPENGL_IMPL
#define DECL_GL_PROC(ret, proc, ...) typedef ret proc##_t(__VA_ARGS__); proc##_t *proc = nullptr
#define DECL_GL_ARB_PROC(ret, proc, ...) typedef ret proc##ARB_t(__VA_ARGS__); proc##ARB_t *proc = nullptr
#else
#define DECL_GL_PROC(ret, proc, ...) typedef ret proc##_t(__VA_ARGS__); extern proc##_t *proc
#define DECL_GL_ARB_PROC(ret, proc, ...) typedef ret proc##ARB_t(__VA_ARGS__); extern proc##ARB_t *proc
#endif

DECL_GL_PROC(BOOL, wglSwapIntervalEXT, int interval);
DECL_GL_PROC(int, wglGetSwapIntervalEXT, void);

DECL_GL_ARB_PROC(char*, wglGetExtensionsString, HDC hdc);
DECL_GL_ARB_PROC(BOOL, wglChoosePixelFormat, HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
DECL_GL_ARB_PROC(HGLRC, wglCreateContextAttribs, HDC hDC, HGLRC hShareContext, const int *attribList);

#include "procs_opengl.h"

#endif // WIN32_OPENGL_H
