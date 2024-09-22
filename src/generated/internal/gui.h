#ifndef GUI_INTERNAL_H
#define GUI_INTERNAL_H

static bool gui_is_parent(GuiId id);
static i32 gui_push_overlay(Rect rect, bool active);
static f32 calc_center(f32 min, f32 max, f32 size);
static Vector2 calc_center(Vector2 tl, Vector2 br, Vector2 size);
static void gui_push_command_buffer();
static void gui_pop_command_buffer(GfxCommandBuffer *dst);
static GfxCommand gui_command(GfxCommandType type);
static void gui_push_command(GfxCommandBuffer *cmdbuf, GfxCommand cmd);
static LayoutRect make_layout_rect(SplitDesc desc, Rect rect);
static LayoutRect split_row_internal(LayoutRect *layout, SplitDesc desc);
static LayoutRect split_col_internal(LayoutRect *layout, SplitDesc desc);
static GuiMenu *gui_find_or_create_menu(GuiId id, Vector2 initial_size, Vector2 max_size = {});
static Vector2 gui_mouse();
static void gui_handle_focus_grabbing(GuiId id);
static void clear_focus();
static bool apply_clip_rect(GlyphRect *g, Rect *clip_rect);
static bool apply_clip_rect(Rect *rect, Rect *clip_rect);
static GuiWindow *push_window_to_top(i32 index);
static i32 gui_find_or_create_window_index(GuiId id, GuiWindowDesc desc);
static bool gui_begin_window_internal(GuiId id, String title, i32 wnd_index);
static void gui_push_clip_rect(Rect rect);
static void gui_pop_clip_rect();
static void gui_draw_button(Rect rect, Vector3 btn_bg, Vector3 btn_bg_acc0, Vector3 btn_bg_acc1);
static void gui_draw_icon_button(GuiId id, String icon, Rect rect);
static void gui_draw_icon(String icon, Rect rect);

#endif // GUI_INTERNAL_H
