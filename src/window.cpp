#include "window.h"
#include "core/string.h"
#include "core/array.h"

struct InputMap {
    String name;

    DynamicArray<InputDesc> by_device[ID_MAX][IT_MAX];
    DynamicArray<InputDesc> by_type[IT_MAX][ID_MAX];

    HashTable<InputId, i32> edges;
    HashTable<InputId, bool> held;
    HashTable<InputId, f32[2]> axes;
    HashTable<InputId, DynamicArray<TextEvent>> text;
};

struct {
    HashTable<InputType, u8> mouse;

    DynamicArray<InputMap> maps;
    DynamicArray<InputMapId> layers;
    DynamicArray<InputMapId> queued_layers;
    InputMapId active_map = -1;
} input{};

String string_from_enum(WindowEventType type) EXPORT
{
    switch (type) {
    case WE_MOUSE_PRESS: return "WE_MOUSE_PRESS";
    case WE_MOUSE_RELEASE: return "WE_MOUSE_RELEASE";
    case WE_MOUSE_MOVE: return "WE_MOUSE_MOVE";
    case WE_MOUSE_WHEEL: return "WE_MOUSE_WHEEL";
    case WE_KEY_PRESS: return "WE_KEY_PRESS";
    case WE_KEY_RELEASE: return "WE_KEY_RELEASE";
    case WE_TEXT: return "WE_TEXT";
    case WE_RESIZE: return "WE_RESIZE";
    case WE_QUIT: return "WE_QUIT";
    case WE_INPUT: return "WE_INPUT";
    case WE_MAX: return "WE_MAX";
    }

    return "UNKNOWN";
}

String string_from_enum(KeyCode_ kc)
{
    switch (kc) {
    case KC_LSUPER: return "KC_LSUPER";
    case KC_RSUPER: return "KC_RSUPER";
    case KC_APP: return "KC_APP";
    case KC_SCLK: return "KC_SCLK";
    case KC_BREAK: return "KC_BREAK";
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
    case KC_SHIFT: return "KC_SHIFT";
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

void init_input_map_(InputMapId *dst, String name, std::initializer_list<InputDesc> descriptors) EXPORT
{
    InputMap map{ .name = name };
    for (auto it : descriptors) {
        array_add(&map.by_device[it.device][0], it);
        array_add(&map.by_type[it.type][0], it);

        if (it.type) array_add(&map.by_device[it.device][it.type], it);
        if (it.device) array_add(&map.by_type[it.type][it.device], it);
    }

    *dst = array_add(&input.maps, map);
}

void reset_input_map(InputMapId map_id) INTERNAL
{
    if (map_id < 0) return;

    auto *map = &input.maps[map_id];
    for (i32 i = 0; i < map->edges.capacity; i++) map->edges.slots[i].value = 0;

    for (i32 i = AXIS; i <= AXIS_2D; i++) {
        for (auto it : map->by_device[MOUSE][i]) {
            auto *axis = map_find_emplace(&map->axes, it.id);
            axis[0] = axis[1] = 0;
        }
    }
}

void input_begin_frame() EXPORT
{
    if (false) LOG_INFO("beginning input frame");
    if (auto *it = map_find(&input.mouse, EDGE_DOWN); it) *it = 0;
    if (auto *it = map_find(&input.mouse, EDGE_UP); it) *it = 0;

    reset_input_map(input.active_map);
    for (auto it : input.layers) reset_input_map(it);

    SWAP(input.layers, input.queued_layers);
    input.queued_layers.count = 0;
}

void set_input_map(InputMapId id) EXPORT
{
    LOG_INFO("switching input map: [%d] %.*s", id, STRFMT(input.maps[id].name));
    reset_input_map(input.active_map);
    reset_input_map(id);
    input.active_map = id;
    input.layers.count = 0;
}

void push_input_layer(InputMapId layer) EXPORT
{
    PANIC_IF(layer < 0, "Invalid input layer id: %d", layer);
    if (false) LOG_INFO("pushing input layer: [%d] %.*s", layer, STRFMT(input.maps[layer].name));
    array_add(&input.queued_layers, layer);
}

InputMapId get_input_map() EXPORT
{
    return input.active_map;
}

bool translate_input_event(
    DynamicArray<WindowEvent> *queue,
    InputMapId map_id,
    WindowEvent event) INTERNAL
{
    bool handled = false;

    constexpr auto insert_axis_event = [](
        DynamicArray<WindowEvent> *queue,
        InputMapId map,
        InputId id,
        InputType type,
        f32 axis)
    {
        array_insert(queue, 0, { WE_INPUT, .input = { map, id, type, .axis = axis } });
    };

    constexpr auto insert_axis2d_event = [](
        DynamicArray<WindowEvent> *queue,
        InputMapId map,
        InputId id,
        InputType type,
        f32 axis[2])
    {
        array_insert(queue, 0, { WE_INPUT, .input = { map, id, type, .axis2d = { axis[0], axis[1] } } });
    };

    constexpr auto insert_input_event = [](
        DynamicArray<WindowEvent> *queue,
        InputMapId map,
        InputId id,
        InputType type)
    {
        array_insert(queue, 0, { WE_INPUT, .input = { map, id, type } });
    };

    switch (event.type) {
    case WE_MOUSE_PRESS:
        (*map_find_emplace(&input.mouse, EDGE_DOWN, u8(0))) |= event.mouse.button;
        (*map_find_emplace(&input.mouse, HOLD, u8(0))) |= event.mouse.button;
        break;
    case WE_MOUSE_RELEASE:
        (*map_find_emplace(&input.mouse, EDGE_UP, u8(0))) |= event.mouse.button;
        (*map_find_emplace(&input.mouse, HOLD, u8(0))) &= ~event.mouse.button;
        break;
    default: break;
    }

    if (event.type != WE_INPUT && map_id != -1) {
        InputMap *map = &input.maps[map_id];

        switch (event.type) {
        case WE_TEXT:
            for (InputDesc it : map->by_type[TEXT][0]) {
                array_insert(queue, 0, { WE_INPUT, .input = { map_id, it.id, it.type, .text = event.text } });
                handled = handled || !(it.flags & FALLTHROUGH);

                auto *text = map_find_emplace(&map->text, it.id);
                array_add(text, event.text);
            }
            break;
        case WE_MOUSE_WHEEL:
            for (InputDesc it : map->by_device[MOUSE][AXIS]) {
                if (event.mouse_wheel.modifiers == it.mouse.modifiers ||
                    it.mouse.modifiers == MF_ANY)
                {
                    f32 *axis = map_find_emplace(&map->axes, it.id);
                    axis[0] += event.mouse_wheel.delta;
                    insert_axis_event(queue, map_id, it.id, it.type, (f32)event.mouse_wheel.delta);
                    handled = handled || !(it.flags & FALLTHROUGH);
                }
            }
            break;
        case WE_MOUSE_MOVE:
            for (InputDesc it : map->by_device[MOUSE][AXIS_2D]) {
                if ((event.mouse.button == it.mouse.button ||
                     it.mouse.button == MB_ANY) &&
                    (event.mouse.modifiers == it.mouse.modifiers ||
                     it.mouse.modifiers == MF_ANY))
                {
                    f32 *axis = map_find_emplace(&map->axes, it.id);
                    axis[0] += event.mouse.dx;
                    axis[1] += event.mouse.dy;
                    insert_axis2d_event(queue, map_id, it.id, it.type, (f32[2]){ (f32)event.mouse.dx, (f32)event.mouse.dy });
                    handled = handled || !(it.flags & FALLTHROUGH);
                }
            }
            break;
        case WE_MOUSE_PRESS:
            for (InputDesc it : map->by_device[MOUSE][EDGE_DOWN]) {
                if ((event.mouse.button == it.mouse.button ||
                     it.mouse.button == MB_ANY) &&
                    (event.mouse.modifiers == it.mouse.modifiers ||
                     it.mouse.button == MF_ANY))
                {
                    (*map_find_emplace(&map->edges, it.id, 0))++;
                    insert_input_event(queue, map_id, it.id, it.type);
                    handled = handled || !(it.flags & FALLTHROUGH);

                    LOG_INFO(
                        "edge down [%s]: map: [%d] %.*s, input id: %d, handled: 0x%x",
                        string_from_enum((KeyCode_)event.key.keycode).data,
                        map_id, STRFMT(map->name),
                        it.id, handled);
                }
            }

            for (InputDesc it : map->by_device[MOUSE][HOLD]) {
                if ((event.mouse.button == it.mouse.button ||
                     it.mouse.button == MB_ANY) &&
                    (event.mouse.modifiers == it.mouse.modifiers ||
                     it.mouse.button == MF_ANY))
                {
                    (*map_find_emplace(&map->held, it.id, false)) = true;
                    insert_input_event(queue, map_id, it.id, it.type);
                    handled = handled || !(it.flags & FALLTHROUGH);

                    LOG_INFO(
                        "hold begin [%s]: map: [%d] %.*s, input id: %d, handled: 0x%x",
                        string_from_enum((KeyCode_)event.key.keycode).data,
                        map_id, STRFMT(map->name),
                        it.id, handled);
                }
            }
            break;
        case WE_MOUSE_RELEASE:
            for (InputDesc it : map->by_device[MOUSE][EDGE_UP]) {
                if ((event.mouse.button == it.mouse.button ||
                     it.mouse.button == MB_ANY) &&
                    (event.mouse.modifiers == it.mouse.modifiers ||
                     it.mouse.modifiers == MF_ANY))
                {
                    (*map_find_emplace(&map->edges, it.id, 0))++;
                    insert_input_event(queue, map_id, it.id, it.type);
                    handled = handled || !(it.flags & FALLTHROUGH);

                    LOG_INFO(
                        "edge up [%s]: map: [%d] %.*s, input id: %d, handled: 0x%x",
                        string_from_enum((KeyCode_)event.key.keycode).data,
                        map_id, STRFMT(map->name),
                        it.id, handled);
                }
            }

            for (InputDesc it : map->by_device[MOUSE][HOLD]) {
                if ((event.mouse.button == it.mouse.button ||
                     it.mouse.button == MB_ANY) &&
                    (event.mouse.modifiers == it.mouse.modifiers ||
                     it.mouse.button == MF_ANY))
                {
                    (*map_find_emplace(&map->held, it.id, false)) = false;
                    insert_input_event(queue, map_id, it.id, it.type);
                    handled = handled || !(it.flags & FALLTHROUGH);

                    LOG_INFO(
                        "hold end [%s]: map: [%d] %.*s, input id: %d, handled: 0x%x",
                        string_from_enum((KeyCode_)event.key.keycode).data,
                        map_id, STRFMT(map->name),
                        it.id, handled);
                }
            }
            break;
        case WE_KEY_PRESS:
            if (event.key.prev_state) break;

            for (InputDesc it : map->by_device[KEYBOARD][EDGE_DOWN]) {
                if (event.key.keycode == it.keyboard.keycode &&
                    (event.key.modifiers == it.keyboard.modifiers ||
                     it.keyboard.modifiers == MF_ANY))
                {
                    (*map_find_emplace(&map->edges, it.id, 0))++;
                    insert_input_event(queue, map_id, it.id, it.type);
                    handled = handled || !(it.flags & FALLTHROUGH);

                    LOG_INFO(
                        "edge down [%s]: map: [%d] %.*s, input id: %d, handled: 0x%x",
                        string_from_enum((KeyCode_)event.key.keycode).data,
                        map_id, STRFMT(map->name),
                        it.id, handled);
                }
            }

            for (InputDesc it : map->by_device[KEYBOARD][HOLD]) {
                if (event.key.keycode == it.keyboard.keycode &&
                    (event.key.modifiers == it.keyboard.modifiers ||
                     it.keyboard.modifiers == MF_ANY))
                {
                    (*map_find_emplace(&map->held, it.id, false)) = true;
                    insert_input_event(queue, map_id, it.id, it.type);
                    handled = handled || !(it.flags & FALLTHROUGH);

                    LOG_INFO(
                        "hold begin [%s]: map: [%d] %.*s, input id: %d, handled: 0x%x",
                        string_from_enum((KeyCode_)event.key.keycode).data,
                        map_id, STRFMT(map->name),
                        it.id, handled);
                }
            }

            for (i32 i = AXIS; i <= AXIS_2D; i++) {
                for (InputDesc it : map->by_device[KEYBOARD][i]) {
                    if(event.key.keycode == it.keyboard.keycode &&
                       (event.key.modifiers == it.keyboard.modifiers ||
                        it.keyboard.modifiers == MF_ANY))
                    {
                        f32 *axis = map_find_emplace(&map->axes, it.id);
                        axis[it.keyboard.axis] += it.keyboard.faxis;
                        insert_axis2d_event(queue, map_id, it.id, it.type, (f32[2]){ axis[0], axis[1] });
                        handled = handled || !(it.flags & FALLTHROUGH);
                    }
                }
            }
            break;
        case WE_KEY_RELEASE:
            for (InputDesc it : map->by_device[KEYBOARD][EDGE_UP]) {
                if (event.key.keycode == it.keyboard.keycode &&
                    (event.key.modifiers == it.keyboard.modifiers ||
                     it.keyboard.modifiers == MF_ANY))
                {
                    (*map_find_emplace(&map->edges, it.id, 0))++;
                    insert_input_event(queue, map_id, it.id, it.type);
                    handled = handled || !(it.flags & FALLTHROUGH);

                    LOG_INFO(
                        "edge up [%s]: map: [%d] %.*s, input id: %d, handled: 0x%x",
                        string_from_enum((KeyCode_)event.key.keycode).data,
                        map_id, STRFMT(map->name),
                        it.id, handled);
                }
            }

            for (InputDesc it : map->by_device[KEYBOARD][HOLD]) {
                if (event.key.keycode == it.keyboard.keycode &&
                    (event.key.modifiers == it.keyboard.modifiers ||
                     it.keyboard.modifiers == MF_ANY))
                {
                    (*map_find_emplace(&map->held, it.id, false)) = false;
                    insert_input_event(queue, map_id, it.id, it.type);
                    handled = handled || !(it.flags & FALLTHROUGH);

                    LOG_INFO(
                        "hold end [%s]: map: [%d] %.*s, input id: %d, handled: 0x%x",
                        string_from_enum((KeyCode_)event.key.keycode).data,
                        map_id, STRFMT(map->name),
                        it.id, handled);
                }
            }

            for (i32 i = AXIS; i <= AXIS_2D; i++) {
                for (InputDesc it : map->by_device[KEYBOARD][i]) {
                    if (event.key.keycode == it.keyboard.keycode &&
                        (event.key.modifiers == it.keyboard.modifiers ||
                         it.keyboard.modifiers == MF_ANY))
                    {
                        f32 *axis = map_find_emplace(&map->axes, it.id);
                        axis[it.keyboard.axis] -= it.keyboard.faxis;
                        insert_axis2d_event(queue, map_id, it.id, it.type, (f32[2]){ axis[0], axis[1] });
                        handled = handled || !(it.flags & FALLTHROUGH);
                    }
                }
            }
            break;
        }

    }

    if (false && handled) LOG_INFO("translated input event: %.*s", STRFMT(string_from_enum((WindowEventType)event.type)));
    return handled;
}

bool translate_input_event(
    DynamicArray<WindowEvent> *queue,
    WindowEvent event) INTERNAL
{
    if (false) switch (event.type) {
    case WE_KEY_PRESS:
        LOG_INFO("KEY_PRESS: %.*s [0x%X]; modifiers: [0x%X]; prev_state: %d",
                 STRFMT(string_from_enum((KeyCode_)event.key.keycode)), event.key.keycode,
                 event.key.modifiers,
                 event.key.prev_state);
        break;
    case WE_KEY_RELEASE:
        LOG_INFO("KEY_RELEASE: %.*s [0x%X]; modifiers: [0x%X]; prev_state: %d",
                 STRFMT(string_from_enum((KeyCode_)event.key.keycode)), event.key.keycode,
                 event.key.modifiers,
                 event.key.prev_state);
        break;
    }

    for (auto it : reverse(input.layers)) if (translate_input_event(queue, it, event)) return true;
    return translate_input_event(queue, input.active_map, event);
}

bool text_input_enabled() EXPORT
{
    if (input.active_map != -1 && input.maps[input.active_map].by_type[TEXT][0].count) return true;
    for (auto it : input.layers) if (input.maps[it].by_type[TEXT][0].count) return true;
    return false;
}

bool get_input_text(InputId id, TextEvent *dst, InputMapId map_id) EXPORT
{
    if (map_id == INPUT_MAP_ANY) {
        for (auto it : reverse(input.layers)) if (get_input_text(id, dst, it)) return true;
        map_id = input.active_map;
    }

    if (map_id == -1) return false;
    InputMap *map = &input.maps[map_id];
    auto *text = map_find(&map->text, id);
    if (!text || text->count == 0) return false;

    *dst = text->at(0);
    array_remove(text, 0);
    return true;
}

bool get_input_axis(InputId id, f32 dst[1], InputMapId map_id) EXPORT
{
    if (map_id == INPUT_MAP_ANY) {
        for (auto it : reverse(input.layers)) if (get_input_axis(id, dst, it)) return true;
        map_id = input.active_map;
    }

    if (map_id == -1) return false;
    InputMap *map = &input.maps[map_id];
    auto *axis = map_find(&map->axes, id);
    if (!axis) return false;

    dst[0] = axis[0];
    return true;
}

bool get_input_axis2d(InputId id, f32 dst[2], InputMapId map_id) EXPORT
{
    if (map_id == INPUT_MAP_ANY) {
        for (auto it : reverse(input.layers)) if (get_input_axis2d(id, dst, it)) return true;
        map_id = input.active_map;
    }

    if (map_id == -1) return false;
    InputMap *map = &input.maps[map_id];
    auto *axis = map_find(&map->axes, id);
    if (!axis) return false;

    dst[0] = axis[0];
    dst[1] = axis[1];
    return true;
}

bool get_input_edge(InputId id, InputMapId map_id) EXPORT
{
    if (map_id == INPUT_MAP_ANY) {
        for (auto it : reverse(input.layers)) if (get_input_edge(id, it)) return true;
        map_id = input.active_map;
    }

    if (map_id == -1) return false;
    InputMap *map = &input.maps[map_id];
    i32 *value = map_find(&map->edges, id);
    if (!value) return false;

    return (*value)-- > 0;
}

bool get_input_held(InputId id, InputMapId map_id) EXPORT
{
    if (map_id == INPUT_MAP_ANY) {
        for (auto it : reverse(input.layers)) if (get_input_held(id, it)) return true;
        map_id = input.active_map;
    }

    if (map_id == -1) return false;
    InputMap *map = &input.maps[map_id];
    bool *value = map_find(&map->held, id);
    if (!value) return false;

    return *value;
}

bool get_input_mouse(MouseButton btn, InputType type /*= EDGE_DOWN*/) EXPORT
{
    auto *it = map_find(&input.mouse, type);
    if (!it) return false;
    return (*it & btn) == btn || (btn == MB_ANY && *it);
}
