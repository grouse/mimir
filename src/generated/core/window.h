#ifndef WINDOW_PUBLIC_H
#define WINDOW_PUBLIC_H

namespace PUBLIC {}
using namespace PUBLIC;

extern String string_from_enum(GamepadButton button);
extern String string_from_enum(GamepadAxis axis);
extern String string_from_enum(WindowEventType type);
extern String string_from_enum(InputType type);
extern String string_from_enum(KeyCode_ kc);
extern void init_input_map_(InputMapId *dst, String name, std::initializer_list<InputDesc> descriptors);
extern void input_begin_frame();
extern void set_input_map(InputMapId id);
extern void push_input_layer(InputMapId layer);
extern InputMapId get_input_map();
extern bool input_map_active(InputMapId id);
extern bool text_input_enabled();
extern bool get_input_text(InputId id, TextEvent *dst, InputMapId map_id);
extern bool get_input_axis(InputId id, f32 dst[1], InputMapId map_id = INPUT_MAP_ANY);
extern bool get_input_axis2d(InputId id, f32 dst[2], InputMapId map_id = INPUT_MAP_ANY);
extern bool get_input_edge(InputId id, InputMapId map_id = INPUT_MAP_ANY);
extern bool get_input_held(InputId id, InputMapId map_id);
extern bool get_input_mouse(MouseButton btn, InputType type = EDGE_DOWN);

#endif // WINDOW_PUBLIC_H
