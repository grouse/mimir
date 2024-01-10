#ifndef WINDOW_H
#define WINDOW_H

#include "string.h"
#include "maths.h"

#include <initializer_list>

#define init_input_map(id, ...) init_input_map_(&id, #id, __VA_ARGS__)

enum WindowEventType : u8 {
    WE_INPUT = 1,

    WE_MOUSE_WHEEL,
    WE_MOUSE_PRESS,
    WE_MOUSE_RELEASE,
    WE_MOUSE_MOVE,

    WE_KEY_PRESS,
    WE_KEY_RELEASE,

    WE_TEXT,

    WE_RESIZE,
    WE_QUIT,

    WE_MAX,
};

enum KeyCode_ : u8 {
    KC_UNKNOWN = 0,

    KC_BACKSPACE,
    KC_ENTER,
    KC_TAB,

    KC_INSERT,
    KC_DELETE,
    KC_HOME,
    KC_END,
    KC_PAGE_UP,
    KC_PAGE_DOWN,
    KC_SCLK,
    KC_BREAK,

    KC_SPACE,

    KC_LSHIFT,
    KC_RSHIFT,
    KC_CTRL,
    KC_ALT,
    KC_LSUPER,
    KC_RSUPER,
    KC_APP,

    KC_LBRACKET,  // [{ on US/UK
    KC_RBRACKET, // ]} on US/UK
    KC_BACKSLASH,
    KC_COLON,
    KC_TICK,
    KC_COMMA,
    KC_PERIOD,
    KC_SLASH,

    KC_1,
    KC_2,
    KC_3,
    KC_4,
    KC_5,
    KC_6,
    KC_7,
    KC_8,
    KC_9,
    KC_0,

    KC_GRAVE,
    KC_MINUS,
    KC_PLUS,

    KC_ESC,

    KC_F1,
    KC_F2,
    KC_F3,
    KC_F4,
    KC_F5,
    KC_F6,
    KC_F7,
    KC_F8,
    KC_F9,
    KC_F10,
    KC_F11,
    KC_F12,

    KC_A,
    KC_B,
    KC_C,
    KC_D,
    KC_E,
    KC_F,
    KC_G,
    KC_H,
    KC_I,
    KC_J,
    KC_K,
    KC_L,
    KC_M,
    KC_N,
    KC_O,
    KC_P,
    KC_Q,
    KC_R,
    KC_S,
    KC_T,
    KC_U,
    KC_V,
    KC_W,
    KC_X,
    KC_Y,
    KC_Z,

    KC_LEFT,
    KC_UP,
    KC_DOWN,
    KC_RIGHT,
};

enum ModifierFlags : u8 {
    MF_ALT   = 1 << 0,
    MF_CTRL  = 1 << 1,
    MF_SHIFT = 1 << 2,

    MF_ANY   = 0xFF,
};

enum MouseButton : u8 {
    MB_UNKNOWN   = 0,

    MB_PRIMARY   = 1 << 0,
    MB_SECONDARY = 1 << 1,
    MB_MIDDLE    = 1 << 2,
    MB_WHEEL     = 1 << 3,

    MB_4         = 1 << 4,
    MB_5         = 1 << 5,

    MB_ANY       = 0xFF,
};

enum InputType {
    AXIS = 1,
    AXIS_2D,
    EDGE_DOWN,
    EDGE_UP,
    HOLD,
    TEXT,

    IT_MAX,
};

enum InputDevice {
    KEYBOARD = 1,
    MOUSE,

    ID_MAX,
};

enum InputFlags {
    FALLTHROUGH = 1 << 0,
};

enum {
    CONFIRM = 0xF000,
    CANCEL,

    SAVE,

    TEXT_INPUT,

    UNDO,
    REDO,

    DUPLICATE,
    COPY,
    PASTE,
    CUT,

    DELETE,

    SELECT,
    SELECT_ALL,
    DESELECT,

    FORWARD,
    BACK
};

typedef u32 InputId;
typedef i32 InputMapId;

constexpr i32 INPUT_MAP_INVALID = -1;
constexpr i32 INPUT_MAP_ANY = -2;

struct AppWindow;

enum WindowFlags : u32 {
    WINDOW_OPENGL = 1 << 0,
};

struct WindowCreateDesc {
    String title;
    i32 width, height;
    u32 flags;
};

struct MouseState {
    i16 x, y;
    bool captured;
};

extern MouseState g_mouse;

struct MouseEvent {
    u8 modifiers;
    u8 button;
    i16 x, y;
    i16 dx, dy;
};

struct InputDesc {
    InputId id;
    InputType type;
    InputDevice device;

    union {
        struct {
            u8 keycode;
            u8 modifiers;
            u8 axis;
            i8 faxis;
        } keyboard;
        struct {
            u8 button;
            u8 modifiers;
        } mouse;
    };

    u32 flags;
};

struct TextEvent {
    // NOTE(jesper): key-repeat fills this buffer with repeated utf8 sequences
    u8 modifiers;
    u8 c[12];
    u8 length;
};

struct WindowEvent {
    u8 type;
    union {
        struct {
            u8 modifiers;
            i16 delta;
        } mouse_wheel;
        MouseEvent mouse;
        struct {
            u32 modifiers  : 8;
            u32 keycode    : 8;
            u32 prev_state : 1;
            u32 repeat     : 1;
            u32 unused     : 14;
        } key;
        TextEvent text;
        struct {
            InputMapId map;
            InputId id;
            InputType type;
            union {
                f32 axis;
                f32 axis2d[2];
                TextEvent text;
            };
        } input;
        struct {
            i16 width, height;
        } resize;
    };
};

enum MouseCursor {
    MC_NORMAL = 0,
    MC_SIZE_NW_SE,
    MC_HIDDEN,
    MC_MAX,
};

AppWindow* create_window(WindowCreateDesc desc);
void present_window(AppWindow *wnd);
Vector2 get_client_resolution(AppWindow *wnd);

void wait_for_next_event(AppWindow *wnd);
bool next_event(AppWindow *wnd, WindowEvent *dst);

void capture_mouse(AppWindow *wnd);
void release_mouse(AppWindow *wnd);

void push_cursor(MouseCursor c);

#endif // WINDOW_H
