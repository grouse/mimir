struct {
	bool text_input_enabled;
} input;

struct MouseState {
    i16 x, y;
    i16 dx, dy;
    bool left_pressed;
    bool left_was_pressed;
};

MouseState g_mouse{};

#include "linux_clipboard.cpp"

void enable_text_input()
{
	input.text_input_enabled = true;
}

void disable_text_input()
{
	input.text_input_enabled = false;
}

u8 keycode_from_x11_keycode(KeyCode x11_keycode)
{
	switch (x11_keycode) {
	case 0x09: return KC_ESC;
	case 0x43: return KC_F1;
	case 0x44: return KC_F2;
	case 0x45: return KC_F3;
	case 0x46: return KC_F4;
	case 0x47: return KC_F5;
	case 0x48: return KC_F6;
	case 0x49: return KC_F7;
	case 0x4A: return KC_F8;
	case 0x4B: return KC_F9;
	case 0x4C: return KC_F10;
	case 0x5F: return KC_F11;
	case 0x60: return KC_F12;
	case 0x31: return KC_GRAVE;
	case 0x0A: return KC_1;
	case 0x0B: return KC_2;
	case 0x0C: return KC_3;
	case 0x0D: return KC_4;
	case 0x0E: return KC_5;
	case 0x0F: return KC_6;
	case 0x10: return KC_7;
	case 0x11: return KC_8;
	case 0x12: return KC_9;
	case 0x13: return KC_0;
	case 0x14: return KC_MINUS;
	case 0x15: return KC_PLUS;
	case 0x16: return KC_BACKSPACE;
	case 0x17: return KC_TAB;
	case 0x18: return KC_Q;
	case 0x19: return KC_W;
	case 0x1A: return KC_E;
	case 0x1B: return KC_R;
	case 0x1C: return KC_T;
	case 0x1D: return KC_Y;
	case 0x1E: return KC_U;
	case 0x1F: return KC_I;
	case 0x20: return KC_O;
	case 0x21: return KC_P;
	case 0x22: return KC_LBRACKET;
	case 0x23: return KC_RBRACKET;
	case 0x24: return KC_ENTER;
	case 0x26: return KC_A;
	case 0x27: return KC_S;
	case 0x28: return KC_D;
	case 0x29: return KC_F;
	case 0x2A: return KC_G;
	case 0x2B: return KC_H;
	case 0x2C: return KC_J;
	case 0x2D: return KC_K;
	case 0x2E: return KC_L;
	case 0x2F: return KC_COLON;
	case 0x30: return KC_TICK;
	case 0x33: return KC_BACKSLASH;
	case 0x34: return KC_Z;
	case 0x35: return KC_X;
	case 0x36: return KC_C;
	case 0x37: return KC_V;
	case 0x38: return KC_B;
	case 0x39: return KC_N;
	case 0x3A: return KC_M;
	case 0x3B: return KC_COMMA;
	case 0x3C: return KC_PERIOD;
	case 0x3D: return KC_SLASH;
	case 0x3E: return KC_RSHIFT;
	case 0x32: return KC_LSHIFT;
	case 0x76: return KC_INSERT;
	case 0x77: return KC_DELETE;
	case 0x25: return KC_CTRL;
	case 0x40: return KC_ALT;
	case 0x41: return KC_SPACE;
	case 0x71: return KC_LEFT;
	case 0x72: return KC_RIGHT;
	case 0x6F: return KC_UP;
	case 0x74: return KC_DOWN;
	}

	return KC_UNKNOWN;
}


void log_key_event(XKeyEvent event)
{
    LOG_INFO(
        "keycode: 0x%x, shift: %d, ctrl: %d, lock: %d, alt: %d, mod2: %d, mod3: %d, mod4: %d, mod5: %d",
        event.keycode,
        event.state & ShiftMask ? 1 : 0,
        event.state & ControlMask ? 1 : 0,
        event.state & LockMask ? 1 : 0,
        event.state & Mod1Mask ? 1 : 0, // alt
        event.state & Mod2Mask ? 1 : 0, // no idea
        event.state & Mod3Mask ? 1 : 0, // no idea
        event.state & Mod4Mask ? 1 : 0, // no idea
        event.state & Mod5Mask ? 1 : 0); // no idea
}

void linux_input_event(DynamicArray<InputEvent> *stream, XIC ic, XEvent xevent)
{
	static u16 key_state[256] = {0};

	WindowEvent event{};

	switch (xevent.type) {
	case ButtonPress:
		event.type = WE_MOUSE_PRESS;
		event.mouse.button = xevent.xbutton.button;
		event.mouse.x = g_mouse.x;
		event.mouse.y = g_mouse.y;
		break;
	case ButtonRelease:
		event.type = WE_MOUSE_RELEASE;
		event.mouse.button = xevent.xbutton.button;
		event.mouse.x = g_mouse.x;
		event.mouse.y = g_mouse.y;
		break;
	case SelectionClear:
	case SelectionRequest:
    case SelectionNotify:
        handle_clipboard_events(xevent);
        break;
	case KeyPress:
		if (false) log_key_event(xevent.xkey);
		if (input.text_input_enabled) {
			WindowEvent t{ .type = WE_TEXT };
			t.text.length = Xutf8LookupString(
				ic, &xevent.xkey,
				(char*)t.text.c, sizeof t.text.c,
				nullptr, nullptr);

			if (t.text.length > 0) {
				array_add(stream, t);
				String s{(char*)t.text.c, t.text.length};
				LOG_INFO("text input: '%.*s'", STRFMT(s));
			}
		}

		event.type = IE_KEY_PRESS;
		event.key.keycode = keycode_from_x11_keycode(xevent.xkey.keycode);
		event.key.prev_state = key_state[event.key.keycode];
		if (xevent.xkey.state & ShiftMask) event.key.modifiers |= MF_SHIFT;
		if (xevent.xkey.state & ControlMask) event.key.modifiers |= MF_CTRL;
		if (xevent.xkey.state & Mod1Mask) event.key.modifiers |= MF_ALT;
		key_state[event.key.keycode] = 1;

		if (false && event.key.keycode == KC_UNKNOWN) LOG_INFO("unknown x11 keycode: 0x%X", xevent.xkey.keycode);

		break;

    case KeyRelease:
        if (false) log_key_event(xevent.xkey);

		event.type = IE_KEY_RELEASE;
		event.key.keycode = keycode_from_x11_keycode(xevent.xkey.keycode);
		event.key.prev_state = key_state[event.key.keycode];
		if (xevent.xkey.state & ShiftMask) event.key.modifiers |= MF_SHIFT;
		if (xevent.xkey.state & ControlMask) event.key.modifiers |= MF_CTRL;
		if (xevent.xkey.state & Mod1Mask) event.key.modifiers |= MF_ALT;
		key_state[event.key.keycode] = 1;
        break;
    default:
        if (false) LOG_INFO("Unhandled event %d", xevent.type);
        break;
	}

	if (event.type) array_add(stream, event);
}
