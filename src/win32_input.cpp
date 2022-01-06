#include "input.h"

InputKey input_key_from_wparam(WPARAM wparam)
{
    switch (wparam) {
    case VK_LEFT: return IK_LEFT;
    case VK_RIGHT: return IK_RIGHT;
    case VK_UP: return IK_UP;
    case VK_DOWN: return IK_DOWN;
    case VK_BACK: return IK_BACKSPACE;
    case VK_DELETE: return IK_DELETE;
    case VK_RETURN: return IK_ENTER;
    case VK_TAB: return IK_TAB;
    case VK_PRIOR: return IK_PAGE_UP;
    case VK_NEXT: return IK_PAGE_DOWN;
        
    case 0x5a: return IK_Z;
    case 0x52: return IK_R;
    case 0x53: return IK_S;
    case 0x42: return IK_B;
    case 0x45: return IK_E;
    case 0x57: return IK_W;
    }
    
    LOG_INFO("unhandled virtual key: 0x%X", wparam);
    return (InputKey)0;
}

InputEvent win32_input_event(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    static u32 modifier_state = MF_NONE;
    
    InputEvent event{};
    
    switch (message) {
    case WM_MOUSEWHEEL: {
            i16 delta = (wparam >> 16) & 0xFFFFF;
            event.type = IE_MOUSE_WHEEL;
            event.mouse_wheel.delta = delta;
        } break;
    case WM_LBUTTONDOWN:
        event.type = IE_MOUSE_PRESS;
        event.mouse.button = 1;
        event.mouse.x = lparam & 0xFFFF;
        event.mouse.y = (lparam >> 16) & 0xFFFF;
        break;
    case WM_LBUTTONUP:
        event.type = IE_MOUSE_RELEASE;
        event.mouse.button = 1;
        event.mouse.x = lparam & 0xFFFF;
        event.mouse.y = (lparam >> 16) & 0xFFFF;
        break;
    case WM_RBUTTONDOWN:
        event.type = IE_MOUSE_PRESS;
        event.mouse.button = 2;
        event.mouse.x = lparam & 0xFFFF;
        event.mouse.y = (lparam >> 16) & 0xFFFF;
        break;
    case WM_RBUTTONUP:
        event.type = IE_MOUSE_RELEASE;
        event.mouse.button = 2;
        event.mouse.x = lparam & 0xFFFF;
        event.mouse.y = (lparam >> 16) & 0xFFFF;
        break;
    case WM_CHAR:
        if (wparam >= 0x20) {
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

            event.type = IE_TEXT;
            event.text.length = utf8_from_utf32(event.text.c, utf32);
        } break;
    case WM_KEYDOWN:
        if (wparam == VK_CONTROL) modifier_state |= MF_CTRL;
        event.type = IE_KEY_PRESS;
        event.key.code = input_key_from_wparam(wparam);
        event.key.modifiers = (ModifierFlags)modifier_state;
        break;
    case WM_KEYUP:
        if (wparam == VK_CONTROL) modifier_state &= ~MF_CTRL;
        event.type = IE_KEY_RELEASE;
        event.key.code = input_key_from_wparam(wparam);
        event.key.modifiers = (ModifierFlags)modifier_state;
        break;
    }
    
    return event;
}