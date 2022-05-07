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

void linux_input_event(DynamicArray<WindowEvent> *stream, XIC ic, XEvent xevent)
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
		if (true) log_key_event(xevent.xkey);

		if (input.text_input_enabled) {
			WindowEvent t{ .type = WE_TEXT };
			t.text.length = Xutf8LookupString(
				ic, &xevent.xkey,
				(char*)t.text.c, sizeof t.text.c,
				nullptr, nullptr);

			if (t.text.length > 0) array_add(stream, t);
		}

        PANIC_IF(xevent.xkey.keycode >= ARRAY_COUNT(key_state), "keycode out of bounds");
        key_state[xevent.xkey.keycode] = 1;
		break;

    case KeyRelease:
        if (false) log_key_event(xevent.xkey);

        PANIC_IF(xevent.xkey.keycode >= ARRAY_COUNT(key_state), "keycode out of bounds");
        key_state[xevent.xkey.keycode] = 0;
        break;
    default:
        if (false) LOG_INFO("Unhandled event %d", xevent.type);
        break;
	}

	if (event.type) array_add(stream, event);
}
