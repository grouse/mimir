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
    
    VC_Z,
    VC_R,
    VC_E,
    VC_B,
    VC_S,
    VC_W,
    VC_I,
    VC_J,
    VC_K,
    VC_D,
    VC_U,
};

enum ScanCode : u8 {
};

enum ModifierFlags : u32 {
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
            i8 button;
            i16 x, y;
        } mouse;
        struct {
            VirtualCode virtual_code;
            ScanCode scan_code;
            ModifierFlags modifiers;
        } key;
        struct {
            u8 c[4];
            i32 length;
        } text;
    };
};


#endif //INPUT_H
