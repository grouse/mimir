#include "input.h"

#if defined(_WIN32)
#include "win32_input.cpp"
#elif defined(__linux__)
#include "linux_input.cpp"
#endif

String string_from_enum(KeyCode kc)
{
    switch (kc) {
    case KC_UNKNOWN: return "KC_UNKNOWN";
    case KC_BACKSPACE: return "KC_BACKSPACE";
    case KC_ENTER: return "KC_ENTER";
    case KC_TAB: return "KC_TAB";
    case KC_INSERT: return "KC_INSERT";
    case KC_DELETE: return "KC_DELETE";
    case KC_HOME: return "KC_HOME";
    case KC_END: return "KC_END";
    case KC_PAGE_UP: return "KC_PAGE_UP";
    case KC_PAGE_DOWN: return "KC_PAGE_DOWN";
    case KC_SPACE: return "KC_SPACE";
    case KC_LSHIFT: return "KC_LSHIFT";
    case KC_RSHIFT: return "KC_RSHIFT";
    case KC_CTRL: return "KC_CTRL";
    case KC_ALT: return "KC_ALT";
    case KC_LBRACKET: return "KC_LBRACKET";
    case KC_RBRACKET: return "KC_RBRACKET";
    case KC_BACKSLASH: return "KC_BACKSLASH";
    case KC_COLON: return "KC_COLON";
    case KC_TICK: return "KC_TICK";
    case KC_COMMA: return "KC_COMMA";
    case KC_PERIOD: return "KC_PERIOD";
    case KC_SLASH: return "KC_SLASH";
    case KC_1: return "KC_1";
    case KC_2: return "KC_2";
    case KC_3: return "KC_3";
    case KC_4: return "KC_4";
    case KC_5: return "KC_5";
    case KC_6: return "KC_6";
    case KC_7: return "KC_7";
    case KC_8: return "KC_8";
    case KC_9: return "KC_9";
    case KC_0: return "KC_0";
    case KC_GRAVE: return "KC_GRAVE";
    case KC_MINUS: return "KC_MINUS";
    case KC_PLUS: return "KC_PLUS";
    case KC_ESC: return "KC_ESC";
    case KC_F1: return "KC_F1";
    case KC_F2: return "KC_F2";
    case KC_F3: return "KC_F3";
    case KC_F4: return "KC_F4";
    case KC_F5: return "KC_F5";
    case KC_F6: return "KC_F6";
    case KC_F7: return "KC_F7";
    case KC_F8: return "KC_F8";
    case KC_F9: return "KC_F9";
    case KC_F10: return "KC_F10";
    case KC_F11: return "KC_F11";
    case KC_F12: return "KC_F12";
    case KC_A: return "KC_A";
    case KC_B: return "KC_B";
    case KC_C: return "KC_C";
    case KC_D: return "KC_D";
    case KC_E: return "KC_E";
    case KC_F: return "KC_F";
    case KC_G: return "KC_G";
    case KC_H: return "KC_H";
    case KC_I: return "KC_I";
    case KC_J: return "KC_J";
    case KC_K: return "KC_K";
    case KC_L: return "KC_L";
    case KC_M: return "KC_M";
    case KC_N: return "KC_N";
    case KC_O: return "KC_O";
    case KC_P: return "KC_P";
    case KC_Q: return "KC_Q";
    case KC_R: return "KC_R";
    case KC_S: return "KC_S";
    case KC_T: return "KC_T";
    case KC_U: return "KC_U";
    case KC_V: return "KC_V";
    case KC_W: return "KC_W";
    case KC_X: return "KC_X";
    case KC_Y: return "KC_Y";
    case KC_Z: return "KC_Z";
    case KC_LEFT: return "KC_LEFT";
    case KC_UP: return "KC_UP";
    case KC_DOWN: return "KC_DOWN";
    case KC_RIGHT: return "KC_RIGHT";
    }

    return "KC_UNKNOWN";
}

