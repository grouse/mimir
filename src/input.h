#ifndef INPUT_H
#define INPUT_H

enum InputEventType {
    IE_MOUSE_WHEEL = 1,
    IE_MOUSE_PRESS,
    IE_MOUSE_RELEASE,
    IE_KEY_PRESS,
    IE_KEY_RELEASE,
    IE_TEXT
};

enum VirtualCode : u8 {
    VC_LEFT = 1,
    VC_RIGHT,
    VC_UP,
    VC_DOWN,
    VC_BACKSPACE,
    VC_DELETE,
    VC_ENTER,
    VC_TAB,
    VC_PAGE_UP,
    VC_PAGE_DOWN,
    VC_ESC,
    VC_SPACE,
    
    VC_OPEN_BRACKET,  // [{ on US/UK
    VC_CLOSE_BRACKET, // ]} on US/UK

    VC_F1,
    VC_F2,
    VC_F3,
    VC_F4,
    VC_F5,
    VC_F6,
    VC_F7,
    VC_F8,
    VC_F9,
    VC_F10,
    VC_F11,
    VC_F12,

    VC_A,
    VC_B,
    VC_C,
    VC_D,
    VC_E,
    VC_F,
    VC_G,
    VC_H,
    VC_I,
    VC_J,
    VC_K,
    VC_L,
    VC_M,
    VC_N,
    VC_O,
    VC_P,
    VC_Q,
    VC_R,
    VC_S,
    VC_T,
    VC_U,
    VC_V,
    VC_W,
    VC_X,
    VC_Y,
    VC_Z
};

enum ScanCode : u8 {
};

enum ModifierFlags : u8 {
    MF_NONE = 0,
    MF_ALT = 1 << 0,
    MF_CTRL = 1 << 1,
    MF_SHIFT = 1 << 2,
};


struct InputEvent {
    InputEventType type;
    union {
        struct {
            i32 delta;
        } mouse_wheel;
        struct {
            u8 button;
            i16 x, y;
        } mouse;
        struct {
            u32 virtual_code : 8;
            u32 scan_code    : 8;
            u32 modifiers    : 8;
            u32 prev_state   : 1;
            u32 unused       : 7;
        } key;
        struct {
            u8 c[4];
            u8 length;
            u8 modifiers;
        } text;
    };
};


#endif //INPUT_H
