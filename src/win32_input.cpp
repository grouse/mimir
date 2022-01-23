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
    case VK_SPACE: return VC_SPACE;
    case VK_TAB: return VC_TAB;
    case VK_PRIOR: return VC_PAGE_UP;
    case VK_NEXT: return VC_PAGE_DOWN;
    case VK_ESCAPE: return VC_ESC;
    case VK_OEM_4: return VC_OPEN_BRACKET;
    case VK_OEM_6: return VC_CLOSE_BRACKET;
        
    case VK_F1: return VC_F1;
    case VK_F2: return VC_F2;
    case VK_F3: return VC_F3;
    case VK_F4: return VC_F4;
    case VK_F5: return VC_F5;
    case VK_F6: return VC_F6;
    case VK_F7: return VC_F7;
    case VK_F8: return VC_F8;
    case VK_F9: return VC_F9;
    case VK_F10: return VC_F10;
    case VK_F11: return VC_F11;
    case VK_F12: return VC_F12;

    case 'A': return VC_A;
    case 'B': return VC_B;
    case 'C': return VC_C;
    case 'D': return VC_D;
    case 'E': return VC_E;
    case 'F': return VC_F;
    case 'G': return VC_G;
    case 'H': return VC_H;
    case 'I': return VC_I;
    case 'J': return VC_J;
    case 'K': return VC_K;
    case 'L': return VC_L;
    case 'M': return VC_M;
    case 'N': return VC_N;
    case 'O': return VC_O;
    case 'P': return VC_P;
    case 'Q': return VC_Q;
    case 'R': return VC_R;
    case 'S': return VC_S;
    case 'T': return VC_T;
    case 'U': return VC_U;
    case 'V': return VC_V;
    case 'W': return VC_W;
    case 'X': return VC_X;
    case 'Y': return VC_Y;
    case 'Z': return VC_Z;
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
        event.key.prev_state = (lparam >> 30) & 0x1;
        
        LOG_INFO("virtual keycode: 0x%X (%c), scancode: 0x%X, prev_state: %d", wparam, (char)wparam, (lparam >> 16) & 0xFF, event.key.prev_state);
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
