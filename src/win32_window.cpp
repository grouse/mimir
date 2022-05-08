#include "window.h"

struct {
    bool text_input_enabled;
} input;

void enable_text_input()
{
    input.text_input_enabled = true;
}

void disable_text_input()
{
    input.text_input_enabled = false;
}

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

void win32_input_event(DynamicArray<WindowEvent> *queue, HWND /*hwnd*/, UINT message, WPARAM wparam, LPARAM lparam)
{
    static u8 modifier_state = MF_NONE;
    WindowEvent event{};
    
    switch (message) {
    case WM_CLOSE:
    case WM_QUIT:
        event.type = WE_QUIT;
        break;
    case WM_MOUSEWHEEL: {
            i16 delta = (wparam >> 16) & 0xFFFFF;
            event.type = WE_MOUSE_WHEEL;
            event.mouse_wheel.delta = delta;
        } break;
    case WM_MOUSEMOVE:
        event.type = WE_MOUSE_MOVE;
        event.mouse = { .x = (i16)(lparam & 0xFFFF), .y = (i16)((lparam >> 16) & 0xFFFF) };
        if (wparam & MK_LBUTTON) event.mouse.button |= MB_PRIMARY;
        if (wparam & MK_RBUTTON) event.mouse.button |= MB_SECONDARY;
        if (wparam & MK_MBUTTON) event.mouse.button |= MB_MIDDLE;
        break;
    case WM_LBUTTONDOWN:
        event.type = WE_MOUSE_PRESS;
        event.mouse = { 
            .x = (i16)((i16)(lparam & 0xFFFF)), .y = (i16)((lparam >> 16) & 0xFFFF),
            .button = MB_PRIMARY
        };
        break;
    case WM_LBUTTONUP:
        event.type = WE_MOUSE_RELEASE;
        event.mouse = { 
            .x = (i16)(lparam & 0xFFFF), .y = (i16)((lparam >> 16) & 0xFFFF),
            .button = MB_PRIMARY
        };
        break;
    case WM_RBUTTONDOWN:
        event.type = WE_MOUSE_PRESS;
        event.mouse = { 
            .x = (i16)(lparam & 0xFFFF), .y = (i16)((lparam >> 16) & 0xFFFF),
            .button = MB_SECONDARY
        };
        break;
    case WM_RBUTTONUP:
        event.type = WE_MOUSE_RELEASE;
        event.mouse = { 
            .x = (i16)(lparam & 0xFFFF), .y = (i16)((lparam >> 16) & 0xFFFF),
            .button = MB_SECONDARY
        };
        break;
    case WM_CHAR:
        if (input.text_input_enabled && wparam >= 0x20) {
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
    case WM_KEYDOWN: {
            if (wparam == VK_CONTROL) modifier_state |= MF_CTRL;
            else if (wparam == VK_SHIFT) modifier_state |= MF_SHIFT;

            u8 scancode = (i16)(lparam >> 16) & 0xff;
            u16 repeat_count = lparam & 0xff;
            
            event.key = {
                .keycode = keycode_from_scancode(scancode),
                .modifiers = modifier_state,
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
    case WM_KEYUP: {
            if (wparam == VK_CONTROL) modifier_state &= ~MF_CTRL;
            else if (wparam == VK_SHIFT) modifier_state &= ~MF_SHIFT;

            u8 scancode = (i16)(lparam >> 16) & 0xff;

            event.type = WE_KEY_RELEASE;
            event.key = { 
                .keycode = keycode_from_scancode(scancode),
                .modifiers = modifier_state,
                .prev_state = (u32)((lparam >> 30) & 0x1)
            };
        } break;
    }
    
    if (event.type != 0) array_add(queue, event);
}
                       