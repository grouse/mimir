#include "input.h"

String string_from_enum(KeyCode kc)
{
    switch (kc) {
    case KC_UNKNOWN: return "KC_UNKNOWN";
    case KC_BACKSPACE: return "KC_BACKSPACE";
    case KC_ENTER: return "KC_ENTER";
    case KC_TAB: return "KC_TAB";
    case KC_INSERT: return "KC_INSERT";
    case KC_DELETE: return "KC_DELETE";
    case KC_HOME: return "KC_HOME";
    case KC_END: return "KC_END";
    case KC_PAGE_UP: return "KC_PAGE_UP";
    case KC_PAGE_DOWN: return "KC_PAGE_DOWN";
    case KC_SPACE: return "KC_SPACE";
    case KC_LSHIFT: return "KC_LSHIFT";
    case KC_RSHIFT: return "KC_RSHIFT";
    case KC_CTRL: return "KC_CTRL";
    case KC_ALT: return "KC_ALT";
    case KC_LBRACKET: return "KC_LBRACKET";
    case KC_RBRACKET: return "KC_RBRACKET";
    case KC_BACKSLASH: return "KC_BACKSLASH";
    case KC_COLON: return "KC_COLON";
    case KC_TICK: return "KC_TICK";
    case KC_COMMA: return "KC_COMMA";
    case KC_PERIOD: return "KC_PERIOD";
    case KC_SLASH: return "KC_SLASH";
    case KC_1: return "KC_1";
    case KC_2: return "KC_2";
    case KC_3: return "KC_3";
    case KC_4: return "KC_4";
    case KC_5: return "KC_5";
    case KC_6: return "KC_6";
    case KC_7: return "KC_7";
    case KC_8: return "KC_8";
    case KC_9: return "KC_9";
    case KC_0: return "KC_0";
    case KC_GRAVE: return "KC_GRAVE";
    case KC_MINUS: return "KC_MINUS";
    case KC_PLUS: return "KC_PLUS";
    case KC_ESC: return "KC_ESC";
    case KC_F1: return "KC_F1";
    case KC_F2: return "KC_F2";
    case KC_F3: return "KC_F3";
    case KC_F4: return "KC_F4";
    case KC_F5: return "KC_F5";
    case KC_F6: return "KC_F6";
    case KC_F7: return "KC_F7";
    case KC_F8: return "KC_F8";
    case KC_F9: return "KC_F9";
    case KC_F10: return "KC_F10";
    case KC_F11: return "KC_F11";
    case KC_F12: return "KC_F12";
    case KC_A: return "KC_A";
    case KC_B: return "KC_B";
    case KC_C: return "KC_C";
    case KC_D: return "KC_D";
    case KC_E: return "KC_E";
    case KC_F: return "KC_F";
    case KC_G: return "KC_G";
    case KC_H: return "KC_H";
    case KC_I: return "KC_I";
    case KC_J: return "KC_J";
    case KC_K: return "KC_K";
    case KC_L: return "KC_L";
    case KC_M: return "KC_M";
    case KC_N: return "KC_N";
    case KC_O: return "KC_O";
    case KC_P: return "KC_P";
    case KC_Q: return "KC_Q";
    case KC_R: return "KC_R";
    case KC_S: return "KC_S";
    case KC_T: return "KC_T";
    case KC_U: return "KC_U";
    case KC_V: return "KC_V";
    case KC_W: return "KC_W";
    case KC_X: return "KC_X";
    case KC_Y: return "KC_Y";
    case KC_Z: return "KC_Z";
    case KC_LEFT: return "KC_LEFT";
    case KC_UP: return "KC_UP";
    case KC_DOWN: return "KC_DOWN";
    case KC_RIGHT: return "KC_RIGHT";
    }
    
    return "KC_UNKNOWN";
}

KeyCode keycode_from_scancode(u8 scancode)
{
    switch (scancode) {
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
    case 0x1D: return KC_CTRL;
    case 0x38: return KC_ALT;
    case 0x39: return KC_SPACE;
    case 0x01: return KC_ESC;
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
    case 0x52: return KC_INSERT;
    case 0x53: return KC_DELETE;
    case 0x4B: return KC_LEFT;
    case 0x47: return KC_HOME;
    case 0x4F: return KC_END;
    case 0x48: return KC_UP;
    case 0x50: return KC_DOWN;
    case 0x49: return KC_PAGE_UP;
    case 0x51: return KC_PAGE_DOWN;
    case 0x4D: return KC_RIGHT;
    case 0x57: return KC_F11;
    case 0x58: return KC_F12;
    }
    
    return KC_UNKNOWN;
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
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN: {
            if (wparam == VK_CONTROL) modifier_state |= MF_CTRL;
            else if (wparam == VK_SHIFT) modifier_state |= MF_SHIFT;

            u8 scancode = (lparam >> 16) & 0xff;

            event.type = IE_KEY_PRESS;
            event.key.keycode = keycode_from_scancode(scancode);
            event.key.modifiers = modifier_state;
            event.key.prev_state = (lparam >> 30) & 0x1;
            
            if (event.key.keycode == KC_UNKNOWN) LOG_ERROR("unknown keycode from scancode: 0x%X", scancode);
            else if (false) LOG_INFO("keycode: 0x%X (%.*s), scancode: 0x%X, prev_state: %d", event.key.keycode, STRFMT(string_from_enum((KeyCode)event.key.keycode)), scancode, event.key.prev_state);
        } break;
    case WM_SYSKEYUP:
    case WM_KEYUP: {
            if (wparam == VK_CONTROL) modifier_state &= ~MF_CTRL;
            else if (wparam == VK_SHIFT) modifier_state &= ~MF_SHIFT;

            u8 scancode = (lparam >> 16) & 0xff;

            event.type = IE_KEY_RELEASE;
            event.key.keycode = keycode_from_scancode(scancode);
            event.key.modifiers = modifier_state;
            event.key.prev_state = (lparam >> 30) & 0x1;
        } break;
    }
    
    return event;
}
