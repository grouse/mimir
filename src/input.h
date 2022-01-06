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

enum InputKey {
    IK_LEFT = 1,
    IK_RIGHT,
    IK_UP,
    IK_DOWN,
    IK_BACKSPACE,
    IK_DELETE,
    IK_ENTER,
    IK_TAB,
    IK_PAGE_UP,
    IK_PAGE_DOWN,
    IK_ESC,
    
    IK_Z,
    IK_R,
    IK_E,
    IK_B,
    IK_S,
    IK_W,
    IK_I,
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
            InputKey code;
            ModifierFlags modifiers;
        } key;
        struct {
            u8 c[4];
            i32 length;
        } text;
    };
};


#endif //INPUT_H
