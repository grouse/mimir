#include "input.h"

VirtualCode virtual_code_from_wparam(WPARAM wparam)
{
    switch (wparam) {
    case VK_LEFT: return VC_LEFT;
    case VK_RIGHT: return VC_RIGHT;
    case VK_UP: return VC_UP;
    case VK_DOWN: return VC_DOWN;
    case VK_BACK: return VC_BACKSPACE;
    case VK_DELETE: return VC_DELETE;
    case VK_RETURN: return VC_ENTER;
    case VK_TAB: return VC_TAB;
    case VK_PRIOR: return VC_PAGE_UP;
    case VK_NEXT: return VC_PAGE_DOWN;
    case VK_ESCAPE: return VC_ESC;
    case VK_OEM_4: return VC_OPEN_BRACKET;
    case VK_OEM_6: return VC_CLOSE_BRACKET;
        
    case 0x5a: return VC_Z;
    case 0x52: return VC_R;
    case 0x53: return VC_S;
    case 0x42: return VC_B;
    case 0x45: return VC_E;
    case 0x57: return VC_W;
    case 0x4A: return VC_J;
    case 0x4B: return VC_K;
    case 0x44: return VC_D;
    case 0x55: return VC_U;
    case 0x49: return VC_I;
    case 0x51: return VC_Q;
    case 0x58: return VC_X;
    case 0x59: return VC_Y;
    case 0x50: return VC_P;
    }
    
    return (VirtualCode)0;
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
            event.text.modifiers = (ModifierFlags)modifier_state;
        } break;
    case WM_KEYDOWN:
        if (wparam == VK_CONTROL) modifier_state |= MF_CTRL;
        else if (wparam == VK_SHIFT) modifier_state |= MF_SHIFT;
        
        event.type = IE_KEY_PRESS;
        event.key.virtual_code = virtual_code_from_wparam(wparam);
        event.key.scan_code = (ScanCode)((lparam >> 16) & 0xFF);
        event.key.modifiers = (ModifierFlags)modifier_state;
        
        LOG_INFO("virtual keycode: 0x%X, scancode: 0x%X", wparam, (lparam >> 16) & 0xFF);
        break;
    case WM_KEYUP:
        if (wparam == VK_CONTROL) modifier_state &= ~MF_CTRL;
        else if (wparam == VK_SHIFT) modifier_state &= ~MF_SHIFT;
        
        event.type = IE_KEY_RELEASE;
        event.key.virtual_code = virtual_code_from_wparam(wparam);
        event.key.scan_code = (ScanCode)((lparam >> 16) & 0xFF);
        event.key.modifiers = (ModifierFlags)modifier_state;
        break;
    }
    
    return event;
}