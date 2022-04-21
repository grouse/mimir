#ifndef INPUT_H
#define INPUT_H

enum InputEventType : u8 {
    IE_MOUSE_WHEEL = 1,
    IE_MOUSE_PRESS,
    IE_MOUSE_RELEASE,
    IE_MOUSE_MOVE,
    IE_KEY_PRESS,
    IE_KEY_RELEASE,
    IE_TEXT,
};

enum KeyCode : u8 {
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
    
    KC_SPACE,
    
    KC_LSHIFT,
    KC_RSHIFT,
    KC_CTRL,
    KC_ALT,
    
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
    MF_NONE = 0,
    MF_ALT = 1 << 0,
    MF_CTRL = 1 << 1,
    MF_SHIFT = 1 << 2,
};

enum MouseButton : u8 {
    MB_PRIMARY = 1 << 0,
    MB_SECONDARY = 1 << 1,
    MB_MIDDLE = 1 << 2,
};

struct InputEvent {
    u8 type;
    union {
        struct {
            i32 delta;
        } mouse_wheel;
        struct {
            u8 button;
            i16 x, y;
        } mouse;
        struct {
            u32 keycode    : 8;
            u32 modifiers  : 8;
            u32 prev_state : 1;
            u32 unused     : 15;
        } key;
        struct {
            u8 c[4];
            u8 length;
            u8 modifiers;
        } text;
    };
};

String string_from_enum(KeyCode kc);

void enable_text_input();
void disable_text_input();

#endif //INPUT_H
