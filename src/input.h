#ifndef INPUT_H
#define INPUT_H

enum InputEventType {
    IE_MOUSE_WHEEL = 1,
    IE_KEY_PRESS,
    IE_KEY_RELEASE,
};

enum InputKey {
    IK_LEFT = 1,
    IK_RIGHT,
    IK_UP,
    IK_DOWN,
};

struct InputEvent {
    InputEventType type;
    union {
        struct {
            i32 delta;
        } mouse_wheel;
        struct {
            InputKey code;
        } key;
    };
};


#endif //INPUT_H
