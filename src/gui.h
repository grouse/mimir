#ifndef GUI_H
#define GUI_H

#include "maths.h"
#include "array.h"
#include "string.h"
#include "window.h"
#include "assets.h"
#include "font.h"
#include "core.h"

#include <initializer_list>

enum {
    FOCUS_NEXT = GUI_INPUT_ID_START,
    FOCUS_PREV,
    FOCUS_CLEAR,

    WINDOW_CLOSE,
    WINDOW_CLEAR,

    SCROLL,

    ACTIVATE,
    DEACTIVATE,
    TOGGLE,

    CURSOR_LEFT,
    CURSOR_RIGHT,
    DELETE_BACK,
};

#define GUI_ID gui_gen_id(__COUNTER__*1000 + __LINE__)
#define GUI_ID_INVALID GuiId{ 0 }

#define GUI_ID_FMT(var) (var)
#define GUI_ID_S "%d"

#define gui_button(...) gui_button_id(GUI_ID, __VA_ARGS__)
#define gui_icon_button(...) gui_icon_button_id(GUI_ID, __VA_ARGS__)
#define gui_icon_switch(...) gui_icon_switch_id(GUI_ID, __VA_ARGS__)
#define gui_dropdown(...) gui_dropdown_id(GUI_ID, __VA_ARGS__)
#define gui_checkbox(...) gui_checkbox_id(GUI_ID, __VA_ARGS__)
#define gui_slider(...) gui_slider_id(GUI_ID, __VA_ARGS__)
#define gui_editbox(...) gui_editbox_id(GUI_ID, __VA_ARGS__)
#define gui_2d_gizmo_translate(...) gui_2d_gizmo_translate_id(GUI_ID, __VA_ARGS__)
#define gui_2d_gizmo_translate_axis(...) gui_2d_gizmo_translate_axis_id(GUI_ID, __VA_ARGS__)
#define gui_2d_gizmo_translate_plane(...) gui_2d_gizmo_translate_plane_id(GUI_ID, __VA_ARGS__)
#define gui_2d_gizmo_size(...) gui_2d_gizmo_size_id(GUI_ID, __VA_ARGS__)
#define gui_2d_gizmo_size_square(...) gui_2d_gizmo_size_square_id(GUI_ID, __VA_ARGS__)
#define gui_2d_gizmo_size_axis(...) gui_2d_gizmo_size_axis_id(GUI_ID, __VA_ARGS__)
#define gui_scrollbar(...) gui_vscrollbar_id(GUI_ID, __VA_ARGS__)
#define gui_vscrollbar(...) gui_vscrollbar_id(GUI_ID, __VA_ARGS__)
#define gui_create_window(...) gui_create_window_id(GUI_ID, __VA_ARGS__)

#define gui_begin_window(...) gui_begin_window_id(GUI_ID, __VA_ARGS__)
#define gui_begin_menu(...) gui_begin_menu_id(GUI_ID ARGS(__VA_ARGS__))

// TODO(jesper): this needs to be just gui_button by making the styling powerful enough
#define gui_menu_button(...) gui_menu_button_id(GUI_ID, __VA_ARGS__)

#define gui_menu(...) SCOPE_CEXPR(gui_begin_menu(__VA_ARGS__), gui_end_menu())
#define gui_window(...) SCOPE_CEXPR(gui_begin_window(__VA_ARGS__), gui_end_window())
#define gui_window_id(...) SCOPE_CEXPR(gui_begin_window_id(__VA_ARGS__), gui_end_window())

#define gui_tree(...) SCOPE_CEXPR(gui_begin_tree_id(GUI_ID, __VA_ARGS__), gui_end_tree())
#define gui_tree_id(...) SCOPE_CEXPR(gui_begin_tree_id(__VA_ARGS__), gui_end_tree())

#define gui_tree_leaf(...) gui_tree_leaf_id(GUI_ID, __VA_ARGS__)

#define gui_context_menu(...) SCOPE_CEXPR(gui_begin_context_menu_id(GUI_ID, __VA_ARGS__), gui_end_context_menu())
#define gui_context_menu_id(...) SCOPE_CEXPR(gui_begin_context_menu_id(__VA_ARGS__), gui_end_context_menu())

#define gui_section(...) SCOPE_EXPR(gui_begin_section(GUI_ID, __VA_ARGS__), gui_end_section())
#define gui_section_id(...) SCOPE_EXPR(gui_begin_section(__VA_ARGS__), gui_end_section())

#define gui_dialog(title, body) gui_dialog_id(GUI_ID, title, [](GuiId) body);

#define GUI_ROW(...) SCOPE_EXPR(gui_push_layout(split_row(__VA_ARGS__)), gui_pop_layout())

#define GUI_COL(...) SCOPE_EXPR(gui_push_layout(split_col(__VA_ARGS__)), gui_pop_layout())
#define GUI_LAYOUT(...) SCOPE_EXPR(gui_push_layout(__VA_ARGS__), gui_pop_layout())

#define GUI_BACKGROUND 0
#define GUI_OVERLAY 1

enum GuiAction {
    GUI_NONE = 0,
    GUI_BEGIN,
    GUI_CHANGE,
    GUI_END,
    GUI_CANCEL,
};

enum GuiAnchor {
    GUI_ANCHOR_TOP,
    GUI_ANCHOR_RIGHT,
    GUI_ANCHOR_BOTTOM = GUI_ANCHOR_RIGHT,
    GUI_ANCHOR_LEFT = GUI_ANCHOR_TOP,
};

enum GuiWindowFlags : u8 {
    GUI_NO_CLOSE            = 1 << 0,
    GUI_NO_MOVE             = 1 << 1,
    GUI_NO_RESIZE           = 1 << 2,
    GUI_NO_TITLE            = 1 << 3,
    GUI_NO_VOVERFLOW_SCROLL = 1 << 4,
    GUI_NO_HOVERFLOW_SCROLL = 1 << 5,

    GUI_NO_OVERFLOW_SCROLL = GUI_NO_VOVERFLOW_SCROLL | GUI_NO_HOVERFLOW_SCROLL,
    GUI_DIALOG             = GUI_NO_RESIZE | GUI_NO_OVERFLOW_SCROLL,
    GUI_STATIC             = GUI_NO_MOVE | GUI_NO_RESIZE,
};

enum LayoutFlags : u8 {
    EXPAND_X      = 1 << 0,
    EXPAND_Y      = 1 << 1,
    SPLIT_RIGHT   = 1 << 2, // split_col left/right split direction | 0 - split_left
    SPLIT_BOTTOM  = 1 << 3, // split_row top/bottom split direction | 0 - split_top
    SPLIT_COL     = 1 << 4, // split_rect col/row split type | 0 - split_row
    LAYOUT_ROOT   = 1 << 5,
    SPLIT_SQUARE  = 1 << 6,

    EXPAND_XY = EXPAND_X | EXPAND_Y,
    SPLIT_CR  = SPLIT_COL | SPLIT_RIGHT,

    LAYOUT_INVALID_FLAG = 0xFF,
};

typedef u32 GuiId;
typedef GuiAction (*GuiBodyProc)(GuiId id);

struct LayoutRect {
    Rect rect;
    Vector2 r[2] = { rect.tl, rect.br };
    u32 flags;

    constexpr operator Rect() const { return rect; }
    constexpr Vector2 size() const { return rect.size(); }
    constexpr Vector2 bsize() const { return r[1]-r[0]; }
};

struct SplitDesc {
    union {
        struct { f32 w, _h; };
        struct { f32 h, _w; };
    };
    f32 factor;
    f32 margin;
    u32 flags;
    u32 rflags = flags;
};


struct GuiWindowStateFlags {
    u32 active:        1 = false;
    u32 was_active:    1 = false;
    u32 hidden:        1 = true;
    u32 has_voverflow: 1 = false;
    u32 has_hoverflow: 1 = false;
};

struct GuiWindow {
    GuiId id;

    Rect client_rect;
    Vector2 scroll_offset;

    GfxCommandBuffer command_buffer;

    String title;
    Vector2 pos;
    Vector2 size;

    Vector2 min_size;

    u8 flags;
    GuiWindowStateFlags state;
};

struct GuiWindowDesc {
    String title;
    Vector2 position;
    Vector2 size;
    u8 flags;
    GuiWindowStateFlags state;
    Vector2 anchor;
    Vector2 parent_anchor;
};

struct GuiMenu {
    Vector2 pos;
    Vector2 size;
    i32 parent_wnd;
    bool active;
};

enum WidgetFlags : u32 {
    GUI_ACTIVE   = 1 << 0,
    GUI_SELECTED = 1 << 1
};

struct GuiDialog {
    GuiId id;
    String title;
    GuiBodyProc proc;
};

struct GuiContext {
    DynamicArray<f32> vertices;
    GLuint vbo;
    GLuint text_vao;

    GuiId hot;
    GuiId next_hot;
    GuiId pressed;

    GuiId focused;
    GuiId prev_focusable_widget;

    GuiId dragging;

    GuiId hot_window;
    GuiId focused_window;

    i32 current_window;
    DynamicArray<GuiId> id_stack;
    DynamicArray<Rect> clip_stack;
    DynamicArray<LayoutRect> layout_stack;
    DynamicArray<GfxCommandBuffer> draw_stack;
    DynamicArray<Rect> overlay_rects;
    DynamicArray<GuiDialog> dialogs;

    HashTable<GuiId, u32> flags;
    HashTable<GuiId, GuiMenu> menus;
    DynamicArray<GuiWindow> windows;

    DynamicArray<WindowEvent> events;
    bool capture_text[2];

    Vector2 drag_start_mouse;
    Vector2 drag_start_data[2];

    struct {
        InputMapId base;
        InputMapId scrollarea;
        InputMapId window;
        InputMapId editbox;
        InputMapId tree;
        InputMapId button;
        InputMapId checkbox;
        InputMapId dropdown;
    } input;

    struct {
        i32 cursor = 0;
        i32 selection = 0;
        i32 offset = 0;

        char buffer[200];
        i32 length = 0;
    } edit;

    struct {
        i16 x, y;
        i16 dx, dy;
    } mouse;

    struct {
        Vector3 fg = bgr_unpack(0xFFFFFFFF);

        Vector3 bg = bgr_unpack(0x333333u);
        Vector3 bg_hot = bgr_unpack(0x3F3F3Fu);
        Vector3 bg_press = bgr_unpack(0x2A2A2Au);

        Vector3 bg_dark0 = bgr_unpack(0x212121u);
        Vector3 bg_dark1 = bgr_unpack(0x282828u);
        Vector3 bg_dark2 = bgr_unpack(0x131313u);
        Vector3 bg_dark3 = bgr_unpack(0x000000u);

        Vector3 bg_light0 = bgr_unpack(0x828282u);
        Vector3 bg_light1 = bgr_unpack(0x5C5C5Cu);

        Vector3 accent_bg = bgr_unpack(0xCA5100u);
        Vector3 accent_bg_hot = bgr_unpack(0xE16817u);
        Vector3 accent_bg_press = bgr_unpack(0xDA4100u);

        struct {
            f32 margin = 2;
            f32 border = 1;
            f32 title_height = 20;
            f32 resize_thickness = 10;
            Vector3 title_fg = { 1, 1, 1 };
            Vector3 close_bg_hot = bgr_unpack(0xFFFF0000);
        } window;

        struct {
            struct {
                Vector2 size{ 20, 20 };
                Vector2 icon{ 16, 16 };
            } close;
        } button;

        struct {
            f32 border = 1;
            f32 margin = 1;
            f32 size = 20;
        } checkbox;

        struct {
            f32 height = 20;
            f32 width = 100;
            f32 border = 1;
            f32 handle = 4;
        } slider;

        struct {
            Vector2 padding = { 2.0f, 8.0f };
            f32 border = 1;
            f32 margin = 3;
        } edit;

        struct {
            f32 margin = 3;
        } dropdown;

        struct {
            f32 height = 20;
            f32 margin = 1;
            f32 min_width = 200;
        } menu;

        struct {
            f32 thickness = 10;
        } scrollbar;

        f32 margin = 1;
    } style;

    struct {
        FontAtlas base;
        FontAtlas icons;
    } fonts;

    struct {
        String close = "";
        String up    = "";
        String down  = "";
        String right = "";
        String check = "󰄬";

        f32 size = 18;
    } icons;
};

extern GuiContext gui;


extern LayoutRect split_col(LayoutRect *layout, SplitDesc desc);
extern LayoutRect split_row(LayoutRect *layout, SplitDesc desc);
extern LayoutRect split_rect(LayoutRect *layout, SplitDesc desc);
extern LayoutRect split_left(LayoutRect *layout, SplitDesc desc);
extern LayoutRect split_right(LayoutRect *layout, SplitDesc desc);
extern LayoutRect split_top(LayoutRect *layout, SplitDesc desc);
extern LayoutRect split_bottom(LayoutRect *layout, SplitDesc desc);

extern LayoutRect* gui_current_layout();
inline LayoutRect split_col(SplitDesc desc)    { return split_col(gui_current_layout(), desc); }
inline LayoutRect split_row(SplitDesc desc)    { return split_row(gui_current_layout(), desc); }
inline LayoutRect split_rect(SplitDesc desc)   { return split_rect(gui_current_layout(), desc); }
inline LayoutRect split_left(SplitDesc desc)   { return split_left(gui_current_layout(), desc); }
inline LayoutRect split_right(SplitDesc desc)  { return split_right(gui_current_layout(), desc); }
inline LayoutRect split_top(SplitDesc desc)    { return split_top(gui_current_layout(), desc); }
inline LayoutRect split_bottom(SplitDesc desc) { return split_bottom(gui_current_layout(), desc); }


extern i32 gui_dropdown_id(GuiId id, Array<String> labels, i32 current_index, Rect rect);
extern i32 gui_dropdown_id(GuiId id, Array<String> labels, i32 current_index);

template<typename T>
i32 gui_dropdown_id(GuiId id, Array<String> labels, Array<T> values, T *value)
{
    i32 current_index = array_find_index(values, *value);
    i32 index = gui_dropdown_id(id, labels, current_index);

    if (index != current_index && value != nullptr) *value = values[index];
    return index;
}

template<typename T>
i32 gui_dropdown_id(GuiId id, String *labels, T *values, i32 count, T *value)
{
    Array<String> labels_arr = Array<String>{ labels, count };
    Array<T> values_arr = Array<T>{ values, count };

    i32 current_index = array_find_index(values_arr, *value);
    i32 index = gui_dropdown_id(id, labels_arr, values_arr, current_index);

    if (index != current_index && value != nullptr) *value = values_arr[index];
    return index;
}

template<typename T>
T gui_dropdown_id(GuiId id, Array<String> labels, Array<T> values, T value)
{
    i32 current_index = array_find_index(values, value);
    i32 index = gui_dropdown_id(id, labels, current_index);

    return index >= 0 ? values[index] : T{};
}

template<typename T>
T gui_dropdown_id(GuiId id, String *labels, T *values, i32 count, T value)
{
    Array<String> labels_arr{ .data = labels, .count = count };
    Array<T> values_arr{ .data = values, .count = count };

    i32 current_index = array_find_index(values_arr, value);
    i32 index = gui_dropdown_id(id, labels_arr, current_index);

    return index >= 0 ? values_arr[index] : T{};
}

template<typename T>
T gui_dropdown_id(GuiId id, std::initializer_list<String> labels, std::initializer_list<T> values, T value)
{
    Array<String> labels_arr{ .data = (String*)labels.begin(), .count = (i32)labels.size() };
    Array<T> values_arr{ .data = (T*)values.begin(), .count = (i32)values.size() };

    i32 current_index = array_find_index(values_arr, value);
    i32 index = gui_dropdown_id(id, labels_arr, current_index);

    return index >= 0 ? values_arr[index] : T{};
}

#endif // GUI_H
