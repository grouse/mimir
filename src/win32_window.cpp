#include "win32_opengl.h"
#include "window.h"
#include "hash_table.h"
#include "maths.h"
#include "array.h"

#include "win32_user32.h"
#include "win32_lite.h"
#include "win32_core.h"
#include "win32_gdi32.h"


struct {
    bool text_input_enabled;
    bool raw_mouse;
    u8 mouse_button_state;
} win32_input{};

#include "window.cpp"

struct AppWindow {
    HWND hwnd;
    HDC hdc;
    HGLRC glrc;

    Vector2 resolution;
};

HashTable<u64, DynamicArray<WindowEvent>> event_queues;

MouseState g_mouse{};
MouseCursor current_cursor;
HCURSOR cursors[MC_MAX];

KeyCode_ keycode_from_scancode(u8 scancode)
{
    switch (scancode) {
    case 0x01: return KC_ESC;
    case 0x02: return KC_1;
    case 0x03: return KC_2;
    case 0x04: return KC_3;
    case 0x05: return KC_4;
    case 0x06: return KC_5;
    case 0x07: return KC_6;
    case 0x08: return KC_7;
    case 0x09: return KC_8;
    case 0x0A: return KC_9;
    case 0x0B: return KC_0;
    case 0x0C: return KC_MINUS;
    case 0x0D: return KC_PLUS;
    case 0x0E: return KC_BACKSPACE;
    case 0x0F: return KC_TAB;
    case 0x10: return KC_Q;
    case 0x11: return KC_W;
    case 0x12: return KC_E;
    case 0x13: return KC_R;
    case 0x14: return KC_T;
    case 0x15: return KC_Y;
    case 0x16: return KC_U;
    case 0x17: return KC_I;
    case 0x18: return KC_O;
    case 0x19: return KC_P;
    case 0x1A: return KC_LBRACKET;
    case 0x1B: return KC_RBRACKET;
    case 0x1C: return KC_ENTER;
    case 0x1D: return KC_CTRL;
    case 0x1E: return KC_A;
    case 0x1F: return KC_S;
    case 0x20: return KC_D;
    case 0x21: return KC_F;
    case 0x22: return KC_G;
    case 0x23: return KC_H;
    case 0x24: return KC_J;
    case 0x25: return KC_K;
    case 0x26: return KC_L;
    case 0x27: return KC_COLON;
    case 0x28: return KC_TICK;
    case 0x29: return KC_GRAVE;
    case 0x2A: return KC_LSHIFT;
    case 0x2B: return KC_BACKSLASH;
    case 0x2C: return KC_Z;
    case 0x2D: return KC_X;
    case 0x2E: return KC_C;
    case 0x2F: return KC_V;
    case 0x30: return KC_B;
    case 0x31: return KC_N;
    case 0x32: return KC_M;
    case 0x33: return KC_COMMA;
    case 0x34: return KC_PERIOD;
    case 0x35: return KC_SLASH;
    case 0x36: return KC_RSHIFT;
    case 0x38: return KC_ALT;
    case 0x39: return KC_SPACE;
    case 0x3B: return KC_F1;
    case 0x3C: return KC_F2;
    case 0x3D: return KC_F3;
    case 0x3E: return KC_F4;
    case 0x3F: return KC_F5;
    case 0x40: return KC_F6;
    case 0x41: return KC_F7;
    case 0x42: return KC_F8;
    case 0x43: return KC_F9;
    case 0x44: return KC_F10;
    case 0x45: return KC_BREAK;
    case 0x46: return KC_SCLK;
    case 0x47: return KC_HOME;
    case 0x48: return KC_UP;
    case 0x4B: return KC_LEFT;
    case 0x49: return KC_PAGE_UP;
    case 0x4D: return KC_RIGHT;
    case 0x4F: return KC_END;
    case 0x50: return KC_DOWN;
    case 0x51: return KC_PAGE_DOWN;
    case 0x52: return KC_INSERT;
    case 0x53: return KC_DELETE;
    case 0x57: return KC_F11;
    case 0x58: return KC_F12;
    case 0x5B: return KC_LSUPER;
    case 0x5C: return KC_APP;
    case 0x5D: return KC_RSUPER;
    }

    return KC_UNKNOWN;
}

void capture_mouse(AppWindow *wnd)
{
    if (!win32_input.raw_mouse) {
        RAWINPUTDEVICE rid{
            .usUsagePage = 0x0001,
            .usUsage = 0x0002,
            .dwFlags = RIDEV_NOLEGACY | RIDEV_CAPTUREMOUSE,
            .hwndTarget = wnd->hwnd,
        };

        if (!RegisterRawInputDevices(&rid, 1, sizeof rid)) {
            LOG_ERROR("error calling RegisterRawInputDevices (%d): %s", WIN32_ERR_STR);
        }

        win32_input.raw_mouse = true;
    }

    while (ShowCursor(false) >= 0) {}
    g_mouse.captured = true;
}

void release_mouse(AppWindow */*wnd*/)
{
    if (win32_input.raw_mouse) {
        RAWINPUTDEVICE rid{
            .usUsagePage = 0x0001,
            .usUsage = 0x0002,
            .dwFlags = RIDEV_REMOVE,
            .hwndTarget = 0,
        };

        if (!RegisterRawInputDevices(&rid, 1, sizeof rid)) {
            LOG_ERROR("error calling RegisterRawInputDevices (%d): %s", WIN32_ERR_STR);
        }

        win32_input.raw_mouse = false;
        win32_input.mouse_button_state = 0;
    }

    while (ShowCursor(true) < 0) {}
    g_mouse.captured = false;
}

bool next_event(AppWindow *wnd, WindowEvent *dst)
{
    DynamicArray<WindowEvent> *queue = map_find(&event_queues, (u64)wnd->hwnd);

    MSG msg;
    while (!queue->count && PeekMessageA(&msg, wnd->hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    if (!queue->count) return false;
    *dst = queue->at(0);
    array_remove(queue, 0);

    if (translate_input_event(queue, *dst)) {
        if (!queue->count) return false;
        *dst = queue->at(0);
        array_remove(queue, 0);
    }

    return true;
}

void wait_for_next_event(AppWindow *wnd)
{
    DynamicArray<WindowEvent> *queue = map_find(&event_queues, (u64)wnd->hwnd);

    MSG msg;
    while (!queue->count && GetMessageA(&msg, wnd->hwnd, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

void push_cursor(MouseCursor c)
{
    current_cursor = c;
}

MouseButton mb_from_message(UINT message, WPARAM wparam)
{
    if (message == WM_LBUTTONUP || message == WM_LBUTTONDOWN) return MB_PRIMARY;
    else if (message == WM_RBUTTONUP || message == WM_RBUTTONDOWN) return MB_SECONDARY;
    else if (message == WM_MBUTTONUP || message == WM_MBUTTONDOWN) return MB_MIDDLE;
    else if (message == WM_XBUTTONUP || message == WM_XBUTTONDOWN) {
        if (wparam >> 16 == XBUTTON1) return MB_4;
        else if (wparam >> 16 == XBUTTON2) return MB_5;
    }

    return MB_UNKNOWN;
}

AppWindow* create_window(WindowCreateDesc desc)
{
    SArena scratch = tl_scratch_arena();

    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD ex_style = WS_EX_OVERLAPPEDWINDOW;

    HINSTANCE hInstance = GetModuleHandleA(NULL);

    if (!(desc.flags & WINDOW_OPENGL)) PANIC("unsupported render backend");

    AppWindow *wnd = ALLOC_T(mem_dynamic, AppWindow);

    WNDCLASSA wc{
        .style = CS_OWNDC,
        .lpfnWndProc = [](HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) -> LRESULT
        {
            static bool mouse_in_client = false;
            static u8 modifier_state = 0;

            DynamicArray<WindowEvent> *queue = map_find_emplace(&event_queues, (u64)hwnd);

            DWORD result = 0;
            WindowEvent event{};

            // TODO(jesper): read the current mouse and key state when window focus changes, mouse enter/leave, etc
            switch (message) {
            case WM_CLOSE:
            case WM_QUIT:
                event.type = WE_QUIT;
                break;
            case WM_SIZING:
                result = 1;
                break;
            case WM_SIZE:
                switch (wparam) {
                case SIZE_RESTORED:
                case SIZE_MINIMIZED:
                case SIZE_MAXIMIZED:
                    event.type = WE_RESIZE;
                    event.resize.width = lparam & 0xFFFF;
                    event.resize.height = lparam >> 16;
                    array_add(queue, event);
                    break;
                case SIZE_MAXSHOW: break;
                case SIZE_MAXHIDE: break;
                default: PANIC_UNREACHABLE();
                }
                break;
            case WM_INPUT: {
                HRAWINPUT hri = (HRAWINPUT)lparam;

                RAWINPUT ri{};

                UINT size = sizeof ri;
                UINT bytes = GetRawInputData(hri, RID_INPUT, &ri, &size, sizeof ri.header);
                PANIC_IF(size != bytes, "mismatch between expected and actual bytes read");

                switch (ri.header.dwType) {
                case RIM_TYPEMOUSE:
                    if (!win32_input.raw_mouse) break;
                    event.type = WE_MOUSE_MOVE;
                    event.mouse.modifiers = modifier_state;

                    if (ri.data.mouse.usFlags == RI_MOUSE_MOVE_RELATIVE) {
                        event.mouse.dx = ri.data.mouse.lLastX;
                        event.mouse.dy = ri.data.mouse.lLastY;
                        event.mouse.x = g_mouse.x + ri.data.mouse.lLastX;
                        event.mouse.y = g_mouse.y + ri.data.mouse.lLastY;
                    } else if (ri.data.mouse.usFlags & RI_MOUSE_MOVE_ABSOLUTE) {
                        LOG_ERROR("this path is untested and based purely on the msdn docs");
                        bool vdesktop = ri.data.mouse.usFlags & RI_MOUSE_VIRTUAL_DESKTOP;

                        // TODO(jesper): surely we can cache these, they're probably slow
                        int width = GetSystemMetrics(vdesktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
                        int height = GetSystemMetrics(vdesktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);

                        event.mouse.x = (i16)((ri.data.mouse.lLastX / 65535.0f) * width);
                        event.mouse.y = (i16)((ri.data.mouse.lLastY / 65535.0f) * height);
                        event.mouse.dx = event.mouse.x - g_mouse.x;
                        event.mouse.dy = event.mouse.y - g_mouse.y;
                    }

                    if (ri.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_DOWN) {
                        array_add(queue, WindowEvent{
                            .type = WE_MOUSE_PRESS,
                            .mouse.x = event.mouse.x,
                            .mouse.y = event.mouse.y,
                            .mouse.button = MB_PRIMARY
                        });
                        win32_input.mouse_button_state |= MB_PRIMARY;
                    } else if (ri.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_1_UP) {
                        array_add(queue, WindowEvent{
                            .type = WE_MOUSE_RELEASE,
                            .mouse.x = event.mouse.x,
                            .mouse.y = event.mouse.y,
                            .mouse.button = MB_PRIMARY
                        });
                        win32_input.mouse_button_state &= ~MB_PRIMARY;
                    }

                    if (ri.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_DOWN) {
                        array_add(queue, WindowEvent{
                            .type = WE_MOUSE_PRESS,
                            .mouse.x = event.mouse.x,
                            .mouse.y = event.mouse.y,
                            .mouse.button = MB_SECONDARY
                        });
                        win32_input.mouse_button_state |= MB_SECONDARY;
                    } else if (ri.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_2_UP) {
                        array_add(queue, WindowEvent{
                            .type = WE_MOUSE_RELEASE,
                            .mouse.x = event.mouse.x,
                            .mouse.y = event.mouse.y,
                            .mouse.button = MB_SECONDARY
                        });
                        win32_input.mouse_button_state &= ~MB_SECONDARY;
                    }

                    if (ri.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_DOWN) {
                        array_add(queue, WindowEvent{
                            .type = WE_MOUSE_PRESS,
                            .mouse.x = event.mouse.x,
                            .mouse.y = event.mouse.y,
                            .mouse.button = MB_MIDDLE,
                        });
                        win32_input.mouse_button_state |= MB_MIDDLE;
                    } else if (ri.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_3_UP) {
                        array_add(queue, WindowEvent{
                            .type = WE_MOUSE_RELEASE,
                            .mouse.x = event.mouse.x,
                            .mouse.y = event.mouse.y,
                            .mouse.button = MB_MIDDLE,
                        });
                        win32_input.mouse_button_state &= ~MB_MIDDLE;
                    }

                    if (ri.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN) {
                        LOG_INFO("xbutton down: 4");
                        array_add(queue, WindowEvent{
                            .type = WE_MOUSE_PRESS,
                            .mouse.x = event.mouse.x,
                            .mouse.y = event.mouse.y,
                            .mouse.button = MB_4,
                        });
                        win32_input.mouse_button_state |= MB_4;
                    } else if (ri.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP) {
                        LOG_INFO("xbutton up: 4");
                        array_add(queue, WindowEvent{
                            .type = WE_MOUSE_RELEASE,
                            .mouse.x = event.mouse.x,
                            .mouse.y = event.mouse.y,
                            .mouse.button = MB_4,
                        });
                        win32_input.mouse_button_state &= ~MB_4;
                    }

                    if (ri.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN) {
                        LOG_INFO("xbutton down: 5");
                        array_add(queue, WindowEvent{
                            .type = WE_MOUSE_PRESS,
                            .mouse.x = event.mouse.x,
                            .mouse.y = event.mouse.y,
                            .mouse.button = MB_5,
                        });
                        win32_input.mouse_button_state |= MB_5;
                    } else if (ri.data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP) {
                        LOG_INFO("xbutton up: 5");
                        array_add(queue, WindowEvent{
                            .type = WE_MOUSE_RELEASE,
                            .mouse.x = event.mouse.x,
                            .mouse.y = event.mouse.y,
                            .mouse.button = MB_5,
                        });
                        win32_input.mouse_button_state &= ~MB_5;
                    }

                    event.mouse.button = win32_input.mouse_button_state;

                    g_mouse.x = event.mouse.x;
                    g_mouse.y = event.mouse.y;
                    break;
                default: break;
                }

                if (wparam == RIM_INPUT) result = DefWindowProcA(hwnd, message, wparam, lparam);
            } break;
            case WM_MOUSEWHEEL: {
                i16 delta = (wparam >> 16) & 0xFFFF;
                event.type = WE_MOUSE_WHEEL;
                event.mouse_wheel.delta = delta / 120;
            } break;
            case WM_MOUSELEAVE: {
                mouse_in_client = false;
            } break;
            case WM_MOUSEMOVE:
                if (win32_input.raw_mouse) break;

                // NOTE(jesper): wparam contains the virtual button state for the mouse, but its behavior is inconsistent
                // if the mouse is grabbed as a rawinputdevice while the mouse button is down
                event.type = WE_MOUSE_MOVE;
                event.mouse = {
                    .modifiers = modifier_state,
                    .button = win32_input.mouse_button_state,
                    .x = (i16)(lparam & 0xFFFF),
                    .y = (i16)((lparam >> 16) & 0xFFFF),
                };

                event.mouse.dx = event.mouse.x - g_mouse.x;
                event.mouse.dy = event.mouse.y - g_mouse.y;
                g_mouse.x = event.mouse.x;
                g_mouse.y = event.mouse.y;

                if (!mouse_in_client) {
                    mouse_in_client = true;
                    event.mouse.dx = event.mouse.dy = 0;

                    TRACKMOUSEEVENT tme{
                        .cbSize = sizeof tme,
                        .dwFlags = TME_LEAVE,
                        .hwndTrack = hwnd,
                    };
                    TrackMouseEvent(&tme);
                }

                break;
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_XBUTTONDOWN:
                event.type = WE_MOUSE_PRESS;
                event.mouse = {
                    .modifiers = modifier_state,
                    .button = mb_from_message(message, wparam),
                    .x = (i16)((i16)(lparam & 0xFFFF)),
                    .y = (i16)((lparam >> 16) & 0xFFFF),
                };

                win32_input.mouse_button_state |= event.mouse.button;
                break;
            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
            case WM_XBUTTONUP:
                event.type = WE_MOUSE_RELEASE;
                event.mouse = {
                    .modifiers = modifier_state,
                    .button = mb_from_message(message, wparam),
                    .x = (i16)(lparam & 0xFFFF),
                    .y = (i16)((lparam >> 16) & 0xFFFF),
                };

                win32_input.mouse_button_state &= ~event.mouse.button;
                break;
            case WM_CHAR:
                if (text_input_enabled() && wparam >= 0x20) {
                    static u16 utf16_high;

                    u16 c = (u16)wparam;
                    if (c >= 0xD800 && c < 0xDC00) {
                        utf16_high = c;
                        break;
                    }

                    u32 utf32 = c;
                    if (c >= 0xDC00 && c <= 0xDFFF) {
                        utf32 = (((utf16_high - 0xD800) << 10) | c) + 0x1000000;
                    }

                    u8 utf8[4];
                    i32 length = utf8_from_utf32(utf8, utf32);

                    event.type = WE_TEXT;
                    event.text = { .modifiers = modifier_state };
                    u16 repeat_count = lparam & 0xff;

                    i32 count = 0;
                    for (i32 r = repeat_count; r > 0; ) {
                        for (; r > 0 && (event.text.length + length < (i32)sizeof event.text.c); r--) {
                            memcpy(&event.text.c[event.text.length], utf8, length);
                            event.text.length += length;
                            count++;
                        }
                        array_add(queue, event);
                        event.text.length = 0;
                    }
                    ASSERT(count == repeat_count);
                } break;
            case WM_SYSKEYDOWN:
                result = DefWindowProcA(hwnd, message, wparam, lparam);
            case WM_KEYDOWN: {
                if (wparam == VK_CONTROL) modifier_state |= MF_CTRL;
                else if (wparam == VK_SHIFT) modifier_state |= MF_SHIFT;
                else if (wparam == VK_MENU) modifier_state |= MF_ALT;

                u8 scancode = (i16)(lparam >> 16) & 0xff;
                u16 repeat_count = lparam & 0xff;

                event.key = {
                    .modifiers = modifier_state,
                    .keycode = keycode_from_scancode(scancode),
                    .prev_state = (u32)((lparam >> 30) & 0x1),
                };

                if (event.key.keycode != KC_UNKNOWN) {
                    event.type = WE_KEY_PRESS;
                    array_add(queue, event);

                    event.key.repeat = 1;
                    for (; repeat_count > 1; repeat_count--) {
                        event.type = WE_KEY_RELEASE;
                        array_add(queue, event);

                        event.type = WE_KEY_PRESS;
                        array_add(queue, event);
                    }

                    event.type = 0;
                }

                if (event.key.keycode == KC_UNKNOWN) LOG_ERROR("unknown keycode from scancode: 0x%X", scancode);
                else if (false) LOG_INFO("keycode: 0x%X (%.*s), scancode: 0x%X, prev_state: %d", event.key.keycode, STRFMT(string_from_enum((KeyCode_)event.key.keycode)), scancode, event.key.prev_state);
            } break;
            case WM_SYSKEYUP:
                result = DefWindowProcA(hwnd, message, wparam, lparam);
            case WM_KEYUP: {
                if (wparam == VK_CONTROL) modifier_state &= ~MF_CTRL;
                else if (wparam == VK_SHIFT) modifier_state &= ~MF_SHIFT;
                else if (wparam == VK_MENU) modifier_state &= ~MF_ALT;

                u8 scancode = (i16)(lparam >> 16) & 0xff;

                event.type = WE_KEY_RELEASE;
                event.key = {
                    .modifiers = modifier_state,
                    .keycode = keycode_from_scancode(scancode),
                    .prev_state = (u32)((lparam >> 30) & 0x1)
                };
            } break;
            case WM_SETCURSOR:
                if ((lparam & 0xFFFF) == HTCLIENT && current_cursor != MC_NORMAL) {
                    SetCursor(cursors[current_cursor]);
                    return 1;
                }
                return DefWindowProcA(hwnd, message, wparam, lparam);
            default:
                return DefWindowProcA(hwnd, message, wparam, lparam);
            }

            if (event.type != 0) array_add(queue, event);
            return result;
        },
        .hInstance = hInstance,
        .hCursor = LoadCursorA(NULL, IDC_ARROW),
        .lpszClassName = sz_string(desc.title, scratch),
    };

    RegisterClassA(&wc);

    HWND dummy_hwnd = CreateWindowExA(
        ex_style,
        wc.lpszClassName,
        "dummy",
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        desc.width, desc.height,
        NULL, NULL,
        hInstance,
        NULL);

    PANIC_IF(dummy_hwnd == NULL, "unable to create dummy window: (%d) %s", WIN32_ERR_STR);

    HDC dummy_hdc = GetDC(dummy_hwnd);
    PANIC_IF(dummy_hdc == NULL, "unable to get dc from dummy window");

    PIXELFORMATDESCRIPTOR pfd{
        .nSize = sizeof pfd,
        .nVersion = 1,
        .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        .iPixelType = PFD_TYPE_RGBA,
        .cColorBits = 32,
        .cDepthBits = 24,
        .cStencilBits = 8,
        .iLayerType = PFD_MAIN_PLANE,
    };

    int format = ChoosePixelFormat(dummy_hdc, &pfd);
    if (!SetPixelFormat(dummy_hdc, format, &pfd)) PANIC("unable to set pixel format: (%d) %s", WIN32_ERR_STR);

    HGLRC dummy_glrc = wglCreateContext(dummy_hdc);
    PANIC_IF(dummy_glrc == NULL, "unable to create gl context: (%d) %s", WIN32_ERR_STR);

    wglMakeCurrent(dummy_hdc, dummy_glrc);

    HANDLE gl_dll = LoadLibraryA("opengl32.dll");
    LOAD_GL_ARB_PROC(wglChoosePixelFormat, gl_dll);
    LOAD_GL_ARB_PROC(wglCreateContextAttribs, gl_dll);

    wnd->hwnd = CreateWindowExA(
        0,
        wc.lpszClassName,
        sz_string(desc.title, scratch),
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        desc.width, desc.height,
        NULL, NULL,
        hInstance,
        NULL);
    PANIC_IF(wnd->hwnd == NULL, "unable to create window: (%d) %s", WIN32_ERR_STR);

    wnd->hdc = GetDC(wnd->hwnd);
    PANIC_IF(wnd->hdc == NULL, "unable to get dc from window");

    PANIC_IF(!wglChoosePixelFormat, "wglChoosePixelFormat proc not loaded, and no fallback available");

    {
        i32 format_attribs[] = {
            WGL_DRAW_TO_WINDOW_ARB,           GL_TRUE,
            WGL_SUPPORT_OPENGL_ARB,           GL_TRUE,
            WGL_DOUBLE_BUFFER_ARB,            GL_TRUE,
            WGL_PIXEL_TYPE_ARB,               WGL_TYPE_RGBA_ARB,
            WGL_COLOR_BITS_ARB,               32,
            WGL_DEPTH_BITS_ARB,               24,
            WGL_STENCIL_BITS_ARB,             8,
            WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, GL_TRUE,
            0
        };

        u32 formats_count;
        i32 format;
        wglChoosePixelFormat(wnd->hdc, format_attribs, NULL, 1, &format, &formats_count);
    }

    pfd = {};
    DescribePixelFormat(wnd->hdc, format, sizeof pfd, &pfd);
    SetPixelFormat(wnd->hdc, format, &pfd);

    PANIC_IF(!wglCreateContextAttribs, "wglCreateContextAttribs proc not loaded, and no fallback available");
    {
        i32 context_attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            WGL_CONTEXT_FLAGS_ARB,         WGL_CONTEXT_DEBUG_BIT_ARB,
            0
        };

        wnd->glrc = wglCreateContextAttribs(wnd->hdc, 0, context_attribs);
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(dummy_glrc);
    ReleaseDC(dummy_hwnd, dummy_hdc);
    DestroyWindow(dummy_hwnd);

    if (!wglMakeCurrent(wnd->hdc, wnd->glrc)) {
        PANIC("wglMakeCurrent failed");
    }

    load_opengl_procs(gl_dll);

    ShowWindow(wnd->hwnd, SW_SHOW);

    if (!cursors[MC_NORMAL]) {
        cursors[MC_NORMAL] = LoadCursorA(NULL, IDC_ARROW);
        cursors[MC_SIZE_NW_SE] = LoadCursorA(NULL, IDC_SIZENWSE);
    }

    return wnd;
}

Vector2 get_client_resolution(AppWindow *wnd)
{
    Vector2 size{};

    RECT client_rect;
    if (GetClientRect(wnd->hwnd, &client_rect)) {
        size.x = (f32)(client_rect.right - client_rect.left);
        size.y = (f32)(client_rect.bottom - client_rect.top);
    }

    return size;
}

void present_window(AppWindow *wnd)
{
    SwapBuffers(wnd->hdc);
    if (current_cursor != MC_NORMAL) SetCursor(cursors[current_cursor]);
    current_cursor = MC_NORMAL;
}

HWND win32_window_handle(AppWindow *wnd)
{
    return wnd->hwnd;
}
