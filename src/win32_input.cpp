#include "input.h"

InputEvent win32_input_event(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    InputEvent event{};
    switch (message) {
    case WM_MOUSEWHEEL: {
            i16 delta = (wparam >> 16) & 0xFFFFF;
            event.type = IE_MOUSE_WHEEL;
            event.mouse_wheel.delta = delta;
        } break;
    case WM_KEYDOWN:
        event.type = IE_KEY_PRESS;
        switch (wparam) {
        case VK_LEFT: event.key.code = IK_LEFT; break;
        case VK_RIGHT: event.key.code = IK_RIGHT; break;
        case VK_UP: event.key.code = IK_UP; break;
        case VK_DOWN: event.key.code = IK_DOWN; break;
        }
        break;
    case WM_KEYUP:
        event.type = IE_KEY_RELEASE;
        switch (wparam) {
        case VK_LEFT: event.key.code = IK_LEFT; break;
        case VK_RIGHT: event.key.code = IK_RIGHT; break;
        case VK_UP: event.key.code = IK_UP; break;
        case VK_DOWN: event.key.code = IK_DOWN; break;
        }
        break;
    }
    
    return event;
}