#include "win32_opengl.h"
#include "win32_gdi32.h"

#include "core.h"
#include "maths.h"

#include "win32_lite.h"
#include "win32_core.h"

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

void* gl_get_proc(const char *name, HMODULE gl_dll)
{
    void *proc = (void*)wglGetProcAddress(name);

    if (proc == nullptr) {
        proc = (void*)GetProcAddress(gl_dll, name);
    }

    return proc;
}

struct Win32Window {
    HWND hwnd;
    HDC hdc;
    HGLRC glrc;
};

void load_opengl_procs(HANDLE gl_dll)
{
    LOAD_GL_ARB_PROC(wglGetExtensionsString, gl_dll);

    LOAD_GL_PROC(glAttachShader, gl_dll);
    LOAD_GL_PROC(glBindAttribLocation, gl_dll);
    LOAD_GL_PROC(glCompileShader, gl_dll);
    LOAD_GL_PROC(glCreateProgram, gl_dll);
    LOAD_GL_PROC(glCreateShader, gl_dll);
    LOAD_GL_PROC(glLinkProgram, gl_dll);
    LOAD_GL_PROC(glShaderSource, gl_dll);
    LOAD_GL_PROC(glGenBuffers, gl_dll);
    LOAD_GL_PROC(glGenVertexArrays, gl_dll);
    LOAD_GL_PROC(glBindVertexArray, gl_dll);
    LOAD_GL_PROC(glBindBuffer, gl_dll);
    LOAD_GL_PROC(glBufferData, gl_dll);
    LOAD_GL_PROC(glVertexAttribPointer, gl_dll);
    LOAD_GL_PROC(glEnableVertexAttribArray, gl_dll);
    LOAD_GL_PROC(glDisableVertexAttribArray, gl_dll);
    LOAD_GL_PROC(glUseProgram, gl_dll);
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
    LOAD_GL_PROC(glBindTexture, gl_dll);
    LOAD_GL_PROC(glBlendFunc, gl_dll);
    LOAD_GL_PROC(glDebugMessageCallback, gl_dll);
    LOAD_GL_PROC(glPolygonMode, gl_dll);
    LOAD_GL_PROC(glBindBufferRange, gl_dll);
    LOAD_GL_PROC(glUniform1fv, gl_dll);
    LOAD_GL_PROC(glBufferSubData, gl_dll);
    LOAD_GL_PROC(glScissor, gl_dll);
    LOAD_GL_PROC(glViewport, gl_dll);
    
    LOAD_GL_PROC(glPixelStorei, gl_dll);

    LOAD_GL_PROC(glTexParameteri, gl_dll);
    LOAD_GL_PROC(glTexStorage2D, gl_dll);
    LOAD_GL_PROC(glTexImage2D, gl_dll);
    LOAD_GL_PROC(glTexSubImage2D, gl_dll);
    LOAD_GL_PROC(glTextureSubImage2D, gl_dll);
}

Win32Window create_opengl_window(
    const char *title, 
    Vector2 resolution, 
    HINSTANCE hInstance,
    WNDPROC event_proc,
    DWORD style = WS_OVERLAPPED)
{
    Win32Window wnd{};
    
    WNDCLASSA wc{};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = event_proc;
    wc.hInstance = hInstance;
    wc.lpszClassName = title;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);

    RegisterClassA(&wc);

    HWND dummy_hwnd = CreateWindowExA(
        0,
        wc.lpszClassName,
        title,
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        resolution.x, resolution.y,
        NULL, NULL,
        hInstance,
        NULL);

    PANIC_IF(dummy_hwnd == NULL, "unable to create dummy window: (%d) %s", WIN32_ERR_STR);

    HDC dummy_hdc = GetDC(dummy_hwnd);
    PANIC_IF(dummy_hdc == NULL, "unable to get dc from dummy window");

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof pfd;
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int format = ChoosePixelFormat(dummy_hdc, &pfd);
    if (!SetPixelFormat(dummy_hdc, format, &pfd)) PANIC("unable to set pixel format: (%d) %s", WIN32_ERR_STR);

    HGLRC dummy_glrc = wglCreateContext(dummy_hdc);
    PANIC_IF(dummy_glrc == NULL, "unable to create gl context: (%d) %s", WIN32_ERR_STR);

    wglMakeCurrent(dummy_hdc, dummy_glrc);
    
    HANDLE gl_dll = LoadLibraryA("opengl32.dll");
    LOAD_GL_ARB_PROC(wglChoosePixelFormat, gl_dll);
    LOAD_GL_ARB_PROC(wglCreateContextAttribs, gl_dll);
    
    wnd.hwnd = CreateWindowExA(
        0,
        wc.lpszClassName,
        title,
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        resolution.x, resolution.y,
        NULL, NULL,
        hInstance,
        NULL);
    PANIC_IF(wnd.hwnd == NULL, "unable to create window: (%d) %s", WIN32_ERR_STR);

    wnd.hdc = GetDC(wnd.hwnd);
    PANIC_IF(wnd.hdc == NULL, "unable to get dc from window");

    if (wglChoosePixelFormat) {
        i32 format_attribs[] = {
            WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
            WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
            WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
            WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
            WGL_COLOR_BITS_ARB, 32,
            WGL_DEPTH_BITS_ARB, 24,
            WGL_STENCIL_BITS_ARB, 8,
            WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, GL_TRUE,
            0
        };

        u32 formats_count;
        i32 format;
        wglChoosePixelFormat(wnd.hdc, format_attribs, NULL, 1, &format, &formats_count);

        pfd = {};
        DescribePixelFormat(wnd.hdc, format, sizeof pfd, &pfd);
        SetPixelFormat(wnd.hdc, format, &pfd);
    } else {
        PANIC("wglChoosePixelFormat proc not loaded, and no fallback available");
    }

    wnd.glrc = NULL;
    if (wglCreateContextAttribs) {
        i32 context_attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
            0
        };

        wnd.glrc = wglCreateContextAttribs(wnd.hdc, 0, context_attribs);
    } else {
        PANIC("wglCreateContextAttribs proc not loaded, and no fallback available");
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(dummy_glrc);
    ReleaseDC(dummy_hwnd, dummy_hdc);
    DestroyWindow(dummy_hwnd);

    if (!wglMakeCurrent(wnd.hdc, wnd.glrc)) {
        PANIC("wglMakeCurrent failed");
    }
    
    load_opengl_procs(gl_dll);
    
    ShowWindow(wnd.hwnd, SW_SHOW);
    return wnd;
}
