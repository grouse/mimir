#ifndef WINDOW_INTERNAL_H
#define WINDOW_INTERNAL_H

static void reset_input_map(InputMapId map_id);
static bool translate_input_event(DynamicArray<WindowEvent> *queue, InputMapId map_id, WindowEvent event);
static bool translate_input_event(DynamicArray<WindowEvent> *queue, WindowEvent event);

#endif // WINDOW_INTERNAL_H
