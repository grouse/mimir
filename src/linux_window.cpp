#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/extensions/XInput2.h>

#include "window.h"

#include "core/memory.h"
#include "core/hash_table.h"

#include "linux_glx.h"
#include "linux_opengl.h"

#include "gen/internal/window.h"

#define LOG_KEY_PRESS 0
#define LOG_KEY_RELEASE 0

struct {
	int xi_opcode;

    Atom WM_PROTOCOLS;
    Atom WM_DELETE_WINDOW;
} x11{};

#define X11_GET_ATOM(dsp, name) x11.name = XInternAtom(dsp, #name, False)

extern Cursor cursors[MC_MAX];

struct AppWindow {
    Display *dsp;
    Window handle;
    Vector2 client_resolution;

    XIM im;
    XIC ic;

    DynamicArray<WindowEvent> events;

};

MouseState g_mouse{};

#include "linux_clipboard.cpp"

u8 modifiers_from_x11(u32 x11_mask)
{
	u8 modifiers = 0;
	if (x11_mask & ShiftMask) modifiers |= MF_SHIFT;
	if (x11_mask & ControlMask) modifiers |= MF_CTRL;
	if (x11_mask & Mod1Mask) modifiers |= MF_ALT;
	return modifiers;
}

u8 keycode_from_x11_keycode(KeyCode x11_keycode, KeySym sym)
{
	switch (sym) {
	case XK_Escape: return KC_ESC;
	case XK_Delete: return KC_DELETE;
	case XK_BackSpace: return KC_BACKSPACE;
	case XK_KP_Enter:
	case XK_ISO_Enter:
		return KC_ENTER;
	case XK_Tab: return KC_TAB;
	}

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
	case 0x3E: return KC_SHIFT;
	case 0x32: return KC_SHIFT;
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

void linux_input_event(DynamicArray<WindowEvent> *stream, AppWindow *wnd, XEvent xevent)
{
	// TODO(jesper): I need to reset a bunch of input state with these events occur to ensure correct tracking of e.g. modifier key state
	static u8 button_state = 0;
	static u8 modifier_state = 0;
	static u16 key_state[256] = {0};

	WindowEvent event{};

	switch (xevent.type) {
    case ConfigureNotify: {
        if (wnd->client_resolution.x != xevent.xconfigure.width ||
            wnd->client_resolution.y != xevent.xconfigure.height)
        {
            LOG_INFO("window size change");
            event.type = WE_RESIZE;
            event.resize = {
                .width = (i16)xevent.xconfigure.width,
                .height = (i16)xevent.xconfigure.height,
            };

            wnd->client_resolution = {
                (f32)xevent.xconfigure.width,
                (f32)xevent.xconfigure.height
            };
        }
    } break;
	case ClientMessage:
        if (xevent.xclient.message_type == x11.WM_PROTOCOLS &&
            xevent.xclient.format == 32 &&
            (u64)xevent.xclient.data.l[0] == x11.WM_DELETE_WINDOW)
        {
            event.type = WE_QUIT;
        }
	    break;

	case GenericEvent:
		if (g_mouse.captured &&
		    xevent.xcookie.extension == x11.xi_opcode &&
		    XGetEventData(xevent.xany.display, &xevent.xcookie))
		{
			switch (xevent.xcookie.evtype) {
			case XI_RawMotion: {
				// see http://who-t.blogspot.com/2009/07/xi2-recipes-part-4.html
				XIRawEvent *revent = (XIRawEvent*)xevent.xcookie.data;

				f64 *values = revent->valuators.values;

				static f64 accum_axes[2] = { 0, 0 };
				for (i32 i = 0, j = 0; j < 2 && i < revent->valuators.mask_len*8; i++) {
					if (XIMaskIsSet(revent->valuators.mask, i)) {
						accum_axes[j++] += *values;
						values++;
					}
				}

                i16 dx = i16(accum_axes[0]);
                i16 dy = i16(accum_axes[1]);

                accum_axes[0] -= dx;
                accum_axes[1] -= dy;

                if (dx == 0 && dy == 0) break;

				g_mouse.x += dx;
				g_mouse.y += dy;

				event.type = WE_MOUSE_MOVE;
				event.mouse = {
					.modifiers = modifier_state,
					.button = button_state,
					.x = g_mouse.x,
					.y = g_mouse.y,
					.dx = dx,
					.dy = dy,
				};

				} break;
			}
		} break;

	case FocusIn:
		if (g_mouse.captured) {
			XGrabPointer(
				xevent.xany.display, xevent.xany.window,
				true,
				PointerMotionMask | ButtonPressMask,
				GrabModeAsync, GrabModeAsync,
				xevent.xany.window,
				cursors[MC_HIDDEN],
				CurrentTime);
		} break;

	case FocusOut:
		if (g_mouse.captured) {
			XUngrabPointer(xevent.xany.display, CurrentTime);
		} break;

	case EnterNotify:
		g_mouse.x = xevent.xcrossing.x;
		g_mouse.y = xevent.xcrossing.y;
		break;

	case MotionNotify:
		if (g_mouse.captured) break;

		event.type = WE_MOUSE_MOVE;
		event.mouse = {
			.modifiers = modifiers_from_x11(xevent.xmotion.state),
			.button = button_state,
			.x = (i16)xevent.xmotion.x,
			.y = (i16)xevent.xmotion.y,
			.dx = i16(xevent.xmotion.x - g_mouse.x),
			.dy = i16(xevent.xmotion.y - g_mouse.y),
		};
		g_mouse.x = event.mouse.x;
		g_mouse.y = event.mouse.y;

		if (false) {
			LOG_INFO(
				"mouse move x: %d, y: %d, primary: %d, secondary: %d, middle: %d, ctrl: %d, shift: %d, alt: %d",
				event.mouse.x,
				event.mouse.y,
				(event.mouse.button & MB_PRIMARY) != 0,
				(event.mouse.button & MB_SECONDARY) != 0,
				(event.mouse.button & MB_MIDDLE) != 0,
				(event.mouse.modifiers & MF_CTRL) != 0,
				(event.mouse.modifiers & MF_SHIFT) != 0,
				(event.mouse.modifiers & MF_ALT) != 0);
		}
		break;

	case ButtonPress:
		if (xevent.xbutton.button == 4 || xevent.xbutton.button == 5) {
			event.type = WE_MOUSE_WHEEL;
			event.mouse_wheel = {
				.modifiers = modifiers_from_x11(xevent.xbutton.state),
				.delta = xevent.xbutton.button == 4 ? (i16)1 : (i16)-1,
			};
		} else if (xevent.xbutton.button == 6 || xevent.xbutton.button == 7) {
			// TODO(jesper): mouse wheel left/right
		} else {
			event.type = WE_MOUSE_PRESS;
			event.mouse = MouseEvent{
				.modifiers = modifiers_from_x11(xevent.xbutton.state),
				.x = (i16)xevent.xbutton.x,
				.y = (i16)xevent.xbutton.y,
			};

			switch (xevent.xbutton.button) {
			case 1: event.mouse.button = MB_PRIMARY; break;
			case 2: event.mouse.button = MB_MIDDLE; break;
			case 3: event.mouse.button = MB_SECONDARY; break;
			}

			button_state |= event.mouse.button;
		}
		break;

	case ButtonRelease:
		if (xevent.xbutton.button == 4 || xevent.xbutton.button == 5) {
			event.type = WE_MOUSE_WHEEL;
			event.mouse_wheel = {
				.modifiers = modifiers_from_x11(xevent.xbutton.state),
				.delta = xevent.xbutton.button == 4 ? (i16)1 : (i16)-1,
			};
		} else if (xevent.xbutton.button == 6 || xevent.xbutton.button == 7) {
			// TODO(jesper): mouse wheel left/right
		} else {
			event.type = WE_MOUSE_RELEASE;
			event.mouse = MouseEvent{
				.modifiers = modifiers_from_x11(xevent.xbutton.state),
				.x = (i16)xevent.xbutton.x,
				.y = (i16)xevent.xbutton.y,
			};

			switch (xevent.xbutton.button) {
			case 1: event.mouse.button = MB_PRIMARY; break;
			case 2: event.mouse.button = MB_MIDDLE; break;
			case 3: event.mouse.button = MB_SECONDARY; break;
			}

			button_state &= ~event.mouse.button;
		}
		break;

	case SelectionClear:
	case SelectionRequest:
    case SelectionNotify:
        handle_clipboard_events(xevent);
        break;

	case KeyPress: {
			if (LOG_KEY_PRESS) log_key_event(xevent.xkey);

			KeySym sym = XLookupKeysym(&xevent.xkey, xevent.xkey.keycode);
			u8 keycode = keycode_from_x11_keycode(xevent.xkey.keycode, sym);

			if (keycode == KC_UNKNOWN) {
			    LOG_INFO("unknown xkey.keycode: 0x%x", xevent.xkey.keycode);
			    break;
            }

			event.type = WE_KEY_PRESS;
			event.key = {
				.modifiers = modifiers_from_x11(xevent.xkey.state),
				.keycode = keycode,
				.prev_state = key_state[event.key.keycode],
			};

			key_state[event.key.keycode] = 1;

			if (text_input_enabled() &&
				event.key.keycode != KC_BACKSPACE &&
				event.key.keycode != KC_ESC &&
				event.key.keycode != KC_DELETE &&
				event.key.keycode != KC_ENTER &&
				event.key.keycode != KC_TAB)
			{
				WindowEvent t{ .type = WE_TEXT };

				t.text.length = Xutf8LookupString(
					wnd->ic, &xevent.xkey,
					(char*)t.text.c, sizeof t.text.c,
					nullptr, nullptr);

				if (t.text.length > 0) {
					array_add(stream, t);
					String s{(char*)t.text.c, t.text.length};
				}
			}

			if (LOG_KEY_PRESS) {
				String ks = string_from_enum((KeyCode_)event.key.keycode);
				LOG_INFO(
					"sending key press: %.*s, prev: %d, ctrl: %d, shift: %d, alt: %d",
					STRFMT(ks),
					event.key.prev_state,
					(event.key.modifiers & MF_CTRL) != 0,
					(event.key.modifiers & MF_SHIFT) != 0,
					(event.key.modifiers & MF_ALT) != 0);
			}

		} break;

    case KeyRelease: {
			if (LOG_KEY_RELEASE) log_key_event(xevent.xkey);

			KeySym sym = XLookupKeysym(&xevent.xkey, xevent.xkey.keycode);
			u8 keycode = keycode_from_x11_keycode(xevent.xkey.keycode, sym);

			event.type = WE_KEY_RELEASE;
			event.key = {
				.modifiers = modifiers_from_x11(xevent.xkey.state),
				.keycode = keycode,
				.prev_state = key_state[event.key.keycode],
			};

			if (LOG_KEY_RELEASE) {
				String ks = string_from_enum((KeyCode_)event.key.keycode);
				LOG_INFO(
					"sending key release: %.*s, ctrl: %d, shift: %d, alt: %d",
					STRFMT(ks),
					(event.key.modifiers & MF_CTRL) != 0,
					(event.key.modifiers & MF_SHIFT) != 0,
					(event.key.modifiers & MF_ALT) != 0);
			}
		} break;

    default:
        if (false) LOG_INFO("Unhandled event %d", xevent.type);
        break;
	}

	if (event.type) array_add(stream, event);
}

void capture_mouse(AppWindow *wnd)
{
	if (g_mouse.captured) return;
	return;

	int result = XGrabPointer(
		wnd->dsp, wnd->handle,
		true,
		PointerMotionMask | ButtonPressMask,
		GrabModeAsync, GrabModeAsync,
		wnd->handle,
		cursors[MC_HIDDEN],
		CurrentTime);

	if (result != GrabSuccess) {
		LOG_ERROR("failed grabbing pointer");
		return;
	}

	g_mouse.captured = true;
}

void release_mouse(AppWindow *wnd)
{
	if (!g_mouse.captured) return;
	XUngrabPointer(wnd->dsp, CurrentTime);
	g_mouse.captured = false;
}

AppWindow* create_window(WindowCreateDesc desc)
{
    SArena scratch = tl_scratch_arena();

    if (!(desc.flags & WINDOW_OPENGL)) PANIC("unsupported render backend");

    AppWindow *wnd = ALLOC_T(mem_dynamic, AppWindow) {
        .dsp = XOpenDisplay(0)
    };
    PANIC_IF(!wnd->dsp, "XOpenDisplay fail");

    int glx_major, glx_minor;
    PANIC_IF(!glXQueryVersion(wnd->dsp, &glx_major, &glx_minor), "failed querying glx version");
    PANIC_IF(glx_major == 1 && glx_minor < 4, "unsupported version");
    LOG_INFO("GLX version: %d.%d", glx_major, glx_minor);

    i32 fb_attribs[] = {
        GLX_X_RENDERABLE    , 1,
        GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
        GLX_RENDER_TYPE     , GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
        GLX_RED_SIZE        , 8,
        GLX_GREEN_SIZE      , 8,
        GLX_BLUE_SIZE       , 8,
        GLX_ALPHA_SIZE      , 8,
        GLX_DEPTH_SIZE      , 24,
        GLX_STENCIL_SIZE    , 8,
        GLX_DOUBLEBUFFER    , 1,
        0
    };

    int fb_count;
    GLXFBConfig *fbs = glXChooseFBConfig(wnd->dsp, DefaultScreen(wnd->dsp), fb_attribs, &fb_count);
    PANIC_IF(!fbs, "failed finding matching framebuffer configurations");

    i32 chosen_fb = -1, chosen_samples = -1;
    for (i32 i = 0; i < fb_count; i++) {
        i32 sample_buffers, samples;
        glXGetFBConfigAttrib(wnd->dsp, fbs[i], GLX_SAMPLE_BUFFERS, &sample_buffers);
        glXGetFBConfigAttrib(wnd->dsp, fbs[i], GLX_SAMPLES, &samples);

        if (sample_buffers && samples > chosen_samples) {
            chosen_fb = i;
            chosen_samples = samples;
        }
    }

    GLXFBConfig fbc = fbs[chosen_fb];
    XVisualInfo *vi = glXGetVisualFromFBConfig(wnd->dsp, fbc);

    XSetWindowAttributes swa{};
    swa.colormap = XCreateColormap(wnd->dsp, RootWindow(wnd->dsp, vi->screen), vi->visual, AllocNone);

    wnd->handle = XCreateWindow(
        wnd->dsp,
        RootWindow(wnd->dsp, vi->screen),
        0, 0,
        desc.width, desc.height,
        0,
        vi->depth,
        InputOutput,
        vi->visual,
        CWOverrideRedirect | CWBackPixmap | CWBorderPixel | CWColormap,
        &swa);

    PANIC_IF(!wnd->handle, "XCreateWindow failed");
    LOG_INFO("created wnd: %p", wnd);

    wnd->client_resolution = { (f32)desc.width, (f32)desc.height };

    XStoreName(wnd->dsp, wnd->handle, sz_string(desc.title, scratch));

    XSelectInput(
        wnd->dsp, wnd->handle,
        KeyPressMask | KeyReleaseMask |
        StructureNotifyMask |
        FocusChangeMask |
        PointerMotionMask | ButtonPressMask |
        LeaveWindowMask | EnterWindowMask |
        ButtonReleaseMask | EnterWindowMask);

    XMapWindow(wnd->dsp, wnd->handle);

    LOAD_GL_PROC(glXCreateContextAttribsARB);

    i32 context_attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    GLXContext ctx = glXCreateContextAttribsARB(wnd->dsp, fbc, 0, true, context_attribs);
    XSync(wnd->dsp, false);

    glXMakeCurrent(wnd->dsp, wnd->handle, ctx);

    load_opengl_procs();

    wnd->im = XOpenIM(wnd->dsp, NULL, NULL, NULL);
    wnd->ic = XCreateIC(wnd->im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, wnd->handle, NULL);

	if (!x11.xi_opcode) {
		// see http://who-t.blogspot.com/2009/05/xi2-recipes-part-1.html
		int event, error;
		PANIC_IF(!XQueryExtension(wnd->dsp, "XInputExtension", &x11.xi_opcode, &event, &error), "missing xi2 extension");

		int major = 2, minor = 0;
		PANIC_IF(XIQueryVersion(wnd->dsp, &major, &minor) == BadRequest, "failed querying XInput version");
		PANIC_IF(major < 2, "unsupported XInput extension version: %d.%d", major, minor);
		LOG_INFO("discovered xinput extension version %d.%d", major, minor);

		XIEventMask ximask{};
		unsigned char mask[XIMaskLen(XI_LASTEVENT)];
		memset(mask, 0, sizeof mask);

		ximask.deviceid = XIAllDevices;
		ximask.mask_len = sizeof mask;
		ximask.mask = mask;

		XISetMask(mask, XI_RawMotion);

		// Since raw events do not have target windows they are delivered exclusively to all root windows. Thus, a client that registers for raw events on a standard client window will receive a BadValue from XISelectEvents(). Like normal events however, if a client has a grab on the device, then the event is delivered only to the grabbing client.
		// see http://who-t.blogspot.com/2009/07/xi2-recipes-part-4.html
		XISelectEvents(wnd->dsp, DefaultRootWindow(wnd->dsp), &ximask, 1);
	}

	if (!x11.WM_PROTOCOLS) {
        X11_GET_ATOM(wnd->dsp, WM_PROTOCOLS);
        X11_GET_ATOM(wnd->dsp, WM_DELETE_WINDOW);

        Atom protocols[] = { x11.WM_DELETE_WINDOW };
        XSetWMProtocols(wnd->dsp, wnd->handle, protocols, ARRAY_COUNT(protocols));
    }

    if (!cursors[MC_NORMAL]) {
        cursors[MC_NORMAL] = XCreateFontCursor(wnd->dsp, XC_left_ptr);
        cursors[MC_SIZE_NW_SE] = XCreateFontCursor(wnd->dsp, XC_bottom_right_corner);

        XColor xcolor;
        char csr_bits[] = { 0x00 };

        Pixmap csr = XCreateBitmapFromData(wnd->dsp, wnd->handle, csr_bits, 1, 1);
        cursors[MC_HIDDEN] = XCreatePixmapCursor(wnd->dsp, csr, csr, &xcolor, &xcolor, 1, 1);
    }

    if (!clipboard.dsp) {
        clipboard.dsp = wnd->dsp;
        clipboard.wnd = XCreateSimpleWindow(
            clipboard.dsp,
            RootWindow(clipboard.dsp, DefaultScreen(clipboard.dsp)),
            -10, -10,
            1, 1,
            0, 0,
            0);
        PANIC_IF(!clipboard.wnd, "failed creading clipboard window");
        LOG_INFO("created clipboard wnd: %p", clipboard.wnd);

        clipboard.target_atom = XInternAtom(
            clipboard.dsp,
            "RAY_CLIPBOARD",
            False);
        clipboard.init = true;
    }

    return wnd;
}

Vector2 get_client_resolution(AppWindow *wnd)
{
    return wnd->client_resolution;
}

bool next_event(AppWindow *wnd, WindowEvent *dst)
{
    XEvent event;
    while (!wnd->events.count && XPending(wnd->dsp)) {
        XNextEvent(wnd->dsp, &event);
        linux_input_event(&wnd->events, wnd, event);
    }

    if (!wnd->events.count) return false;
    *dst = wnd->events[0];
    array_remove(&wnd->events, 0);

    if (translate_input_event(&wnd->events, *dst)) {
        if (!wnd->events.count) return false;
        *dst = wnd->events[0];
        array_remove(&wnd->events, 0);
    }

    return true;
}

void wait_for_next_event(AppWindow *wnd)
{
    while (!wnd->events.count) {
        XEvent event;
        if (XNextEvent(wnd->dsp, &event))
            linux_input_event(&wnd->events, wnd, event);
    }
}

void present_window(AppWindow *wnd)
{
    extern MouseCursor current_cursor;

    glXSwapBuffers(wnd->dsp, wnd->handle);
    XDefineCursor(wnd->dsp, wnd->handle, cursors[current_cursor]);
    current_cursor = MC_NORMAL;
}
