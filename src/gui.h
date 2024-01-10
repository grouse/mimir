#ifndef GUI_H
#define GUI_H

#include "maths.h"
#include "array.h"
#include "string.h"
#include "window.h"
#include "gfx_opengl.h"
#include "font.h"

#include <initializer_list>

#define GUI_ID(index) GuiId{ __COUNTER__+1, index, -1}
#define GUI_ID_INVALID GuiId{ 0, 0, 0 }
#define GUI_ID_INTERNAL(id, internal) GuiId{ id.owner, id.index, internal }
#define GUI_ID_INDEX(id, index) GuiId{ id.owner, index, id.internal }

#define gui_button(...) gui_button_id(GUI_ID(0), __VA_ARGS__)
#define gui_dropdown(...) gui_dropdown_id(GUI_ID(0), __VA_ARGS__)
#define gui_checkbox(...) gui_checkbox_id(GUI_ID(0), __VA_ARGS__)
#define gui_checkbox(...) gui_checkbox_id(GUI_ID(0), __VA_ARGS__)
#define gui_editbox(...) gui_editbox_id(GUI_ID(0), __VA_ARGS__)
#define gui_2d_gizmo_translate(...) gui_2d_gizmo_translate_id(GUI_ID(0), __VA_ARGS__)
#define gui_2d_gizmo_translate_axis(...) gui_2d_gizmo_translate_axis_id(GUI_ID(0), __VA_ARGS__)
#define gui_2d_gizmo_size(...) gui_2d_gizmo_size_id(GUI_ID(0), __VA_ARGS__)
#define gui_2d_gizmo_size_square(...) gui_2d_gizmo_size_square_id(GUI_ID(0), __VA_ARGS__)
#define gui_2d_gizmo_size_axis(...) gui_2d_gizmo_size_axis_id(GUI_ID(0), __VA_ARGS__)
#define gui_scrollbar(...) gui_vscrollbar_id(GUI_ID(0), __VA_ARGS__)
#define gui_vscrollbar(...) gui_vscrollbar_id(GUI_ID(0), __VA_ARGS__)
#define gui_lister(...) gui_lister_id(GUI_ID(0), __VA_ARGS__)

#define gui_begin_window(...) gui_begin_window_id(GUI_ID(0), __VA_ARGS__)
#define gui_begin_menu(...) gui_begin_menu_id(GUI_ID(0) ARGS(__VA_ARGS__))

// TODO(jesper): this needs to be just gui_button by making the styling powerful enough
#define gui_menu_button(...) gui_menu_button_id(GUI_ID(0), __VA_ARGS__)


#define gui_menu(...) for (i32 i_##__LINE__ = 0; i_##__LINE__ == 0 && gui_begin_menu(__VA_ARGS__); i_##__LINE__ = (gui_end_menu(), 1))
#define gui_window(...) for (i32 i_##__LINE__ = 0; i_##__LINE__ == 0 && gui_begin_window(__VA_ARGS__); i_##__LINE__ = (gui_end_window(), 1))

#define GUI_BACKGROUND 0
#define GUI_OVERLAY 1

enum GuiEditboxAction {
    GUI_EDITBOX_NONE = 0,
    GUI_EDITBOX_CANCEL = 1 << 0,
    GUI_EDITBOX_CHANGE = 1 << 1,
    GUI_EDITBOX_FINISH = 1 << 2,
};

enum GuiListerAction {
    GUI_LISTER_NONE,
    GUI_LISTER_SELECT,
    GUI_LISTER_FINISH,
    GUI_LISTER_CANCEL,
};

enum GuiGizmoAction {
    GUI_GIZMO_NONE,
    GUI_GIZMO_BEGIN,
    GUI_GIZMO_CANCEL,
    GUI_GIZMO_END,
};

enum GuiAnchor {
    GUI_ANCHOR_TOP,
    GUI_ANCHOR_RIGHT,
    GUI_ANCHOR_BOTTOM = GUI_ANCHOR_RIGHT,
    GUI_ANCHOR_LEFT = GUI_ANCHOR_TOP,
};

enum GuiWindowFlags : u32 {
    GUI_WINDOW_CLOSE = 1 << 0,
    GUI_WINDOW_MOVABLE = 1 << 1,
    GUI_WINDOW_RESIZABLE = 1 << 2,

    GUI_WINDOW_NO_TITLE = 1 << 3,

    GUI_WINDOW_DEFAULT = GUI_WINDOW_MOVABLE | GUI_WINDOW_RESIZABLE
};

enum GuiLayoutFlags : u32 {
    GUI_LAYOUT_EXPAND_X = 1 << 0,
    GUI_LAYOUT_EXPAND_Y = 1 << 1,


    GUI_LAYOUT_EXPAND = GUI_LAYOUT_EXPAND_X | GUI_LAYOUT_EXPAND_Y,
};

struct GuiId {
    i32 owner;
    i32 index;
    i32 internal;

    bool operator==(GuiId rhs) const
    {
        return owner == rhs.owner && index == rhs.index && internal == rhs.internal;
    }

    bool operator!=(GuiId rhs) const
    {
        return owner != rhs.owner || index != rhs.index || internal != rhs.internal;
    }
};


struct TextQuadsAndBounds {
    DynamicArray<GlyphRect> glyphs;
    Rect bounds;
};

struct GuiWindow {
    GuiId id;

    Rect clip_rect;

    GfxCommandBuffer command_buffer;

    u32 flags;
    Vector2 pos;
    Vector2 size;
    bool active, last_active;
};

struct GuiWindowState {
    Vector2 pos = { 10.0f, 30.0f };
    Vector2 size = { 300.0f, 200.0f };
    bool active;
};

struct GuiMenu {
    GuiId id;
    i32 parent_wnd;
    Vector2 size;
    i32 draw_index = -1;
    bool active;
};

enum GuiLayoutType {
    GUI_LAYOUT_ABSOLUTE,
    GUI_LAYOUT_ROW,
    GUI_LAYOUT_COLUMN,
};

struct GuiLayout {
    GuiLayoutType type;
    u32 flags;

    union {
        struct {
            Vector2 pos;
            Vector2 size;
        };

        Rect rect;
    };

    Vector2 current;
    Vector2 available_space;
    Vector2 margin;

    union {
        struct {
            f32 spacing;
        } row, column;
    };
};

// TODO(jesper): this is really more like scrollarea state, I just need to do a couple
// passes on the API before it gets there
struct GuiLister {
    GuiId id;
    f32 offset;
};

struct GuiContext {
    GLuint vbo;
    GLuint text_vao;

    struct {
        i32 cursor = 0;
        i32 selection = 0;
        i32 offset = 0;

        char buffer[200];
        i32 length = 0;
    } edit;

    GuiId hot = GUI_ID_INVALID;
    GuiId focused = GUI_ID_INVALID;
    GuiId pressed = GUI_ID_INVALID;

    GuiId next_hot = GUI_ID_INVALID;

    GuiId focused_window = GUI_ID_INVALID;
    GuiId hot_window = GUI_ID_INVALID;

    GuiId active_menu = GUI_ID_INVALID;

    i32 current_window = GUI_BACKGROUND;
    DynamicArray<GuiWindow> windows;
    DynamicArray<GuiLister> listers;
    DynamicArray<GuiMenu> menus;
    DynamicArray<GuiLayout> layout_stack;

    DynamicArray<Rect> overlay_rects;

    GuiId current_id;
    DynamicArray<GuiId> id_stack;

    DynamicArray<f32> vertices;

    bool capture_text[2];
    bool capture_keyboard[2];
    bool capture_mouse_wheel[2];

    DynamicArray<WindowEvent> events;

    Vector2 drag_start_mouse;
    Vector2 drag_start_data;
    Vector2 drag_start_data1;

    struct {
        i16 x, y;
        i16 dx, dy;
        bool left_pressed;
        bool left_was_pressed;
    } mouse;

    struct {
        Vector3 fg = bgr_unpack(0xFFFFFFFF);

        Vector3 bg = bgr_unpack(0x333333u);
        Vector3 bg_hot = bgr_unpack(0x3F3F3Fu);
        Vector3 bg_press = bgr_unpack(0x2A2A2Au);

        Vector3 bg_dark0 = bgr_unpack(0x212121u);
        Vector3 bg_dark1 = bgr_unpack(0x282828u);
        Vector3 bg_dark2 = bgr_unpack(0x131313u);

        Vector3 bg_light0 = bgr_unpack(0x828282u);
        Vector3 bg_light1 = bgr_unpack(0x5C5C5Cu);

        Vector3 accent_bg = bgr_unpack(0xCA5100u);
        Vector3 accent_bg_hot = bgr_unpack(0xE16817u);
        Vector3 accent_bg_press = bgr_unpack(0xDA4100u);

        struct {
            FontAtlas font;

            Vector3 color = Vector3{ 1.0f, 1.0f, 1.0f };
        } text;

        struct {
            Vector2 margin = { 5.0f, 5.0f };
            Vector2 border = { 1, 1 };

            Vector2 close_size = { 16.0f, 16.0f };

            f32 title_height = 21;

            Vector3 title_fg = { 1, 1, 1 };

            Vector3 close_bg_hot = bgr_unpack(0xFFFF0000);
        } window;

        struct {
            FontAtlas font;
        } button;

        struct {
            Vector2 padding = { 2.0f, 4.0f };
        } edit;

        struct {
            f32 thickness = 12.0f;
        } scrollbar;
    } style;

    struct {
        GLuint close;
        GLuint check;
        GLuint down;
        GLuint up;
    } icons;
};

extern GuiContext gui;

void init_gui();
void gui_begin_frame();
bool gui_input(WindowEvent event);
void gui_render();

void gui_begin_layout(GuiLayout layout);
void gui_end_layout();
Vector2 gui_layout_widget(Vector2 preferred_size, GuiAnchor anchor = GUI_ANCHOR_TOP);
Vector2 gui_layout_widget(Vector2 *required_size, GuiAnchor anchor = GUI_ANCHOR_TOP);
Rect gui_layout_widget_fill();

bool gui_begin_window_id(GuiId id, String title, Vector2 pos, Vector2 size, bool visible, u32 flags = GUI_WINDOW_DEFAULT);
bool gui_begin_window_id(GuiId id, String title, Vector2 pos, Vector2 size, bool *visible, u32 flags = GUI_WINDOW_DEFAULT);
bool gui_begin_window_id(GuiId id, String title, Vector2 pos, Vector2 size, Vector2 anchor, bool *visible, u32 flags = GUI_WINDOW_DEFAULT);
bool gui_begin_window_id(GuiId id, String title, Vector2 pos, Vector2 size, Vector2 anchor, bool visible, u32 flags = GUI_WINDOW_DEFAULT);
bool gui_begin_window_id(GuiId id, String title, Vector2 pos, Vector2 size, u32 flags = GUI_WINDOW_DEFAULT);
bool gui_begin_window_id(GuiId id, String title, GuiWindowState *state, u32 flags = GUI_WINDOW_DEFAULT);
void gui_end_window();

GuiEditboxAction gui_editbox_id(GuiId id, String initial_str);
GuiEditboxAction gui_editbox_id(GuiId id, String initial_str, f32 width);
GuiEditboxAction gui_editbox_id(GuiId id, f32 *value, f32 width);

bool gui_button_id(GuiId id, String text);
bool gui_button_id(GuiId id, String text, Vector2 size);
bool gui_button_id(GuiId id, FontAtlas *font, TextQuadsAndBounds td, Vector2 size);
bool gui_button_id(GuiId id, f32 icon_width, GLuint icon);
bool gui_button_id(GuiId id, Vector2 pos, Vector2 size);
bool gui_button_id(GuiId id, Vector2 pos, Vector2 size, GLuint icon);

bool gui_checkbox_id(GuiId id, String label, bool *checked);

i32 gui_dropdown_id(GuiId id, Array<String> labels, i32 current_index);
i32 gui_dropdown_id(GuiId id, String* labels, i32 count, i32 current_index);
template<typename T> i32 gui_dropdown_id(GuiId id, Array<String> labels, Array<T> values, T *value = nullptr);
template<typename T> i32 gui_dropdown_id(GuiId id, String *labels, T *values, i32 count, T *value = nullptr);
template<typename T> T gui_dropdown_id(GuiId id, Array<String> labels, Array<T> values, T value);
template<typename T> T gui_dropdown_id(GuiId id, String *labels, T *values, i32 count, T value);
template<typename T> T gui_dropdown_id(GuiId id, std::initializer_list<String> labels, std::initializer_list<T> values, T value);

void gui_vscrollbar_id(GuiId id, f32 line_height, i32 *current, i32 max, i32 num_visible, f32 *offset, GuiAnchor = GUI_ANCHOR_RIGHT);
void gui_vscrollbar_id(GuiId id, f32 *current, f32 total_height, f32 step_size, GuiAnchor anchor = GUI_ANCHOR_RIGHT);

GuiListerAction gui_lister_id(GuiId id, Array<String> items);

GuiGizmoAction gui_2d_gizmo_translate_id(GuiId id, Camera camera, Vector2 *position);
GuiGizmoAction gui_2d_gizmo_translate_id(GuiId id, Camera camera, Vector2 *position, f32 multiple);
GuiGizmoAction gui_2d_gizmo_translate_axis_id(GuiId id, Camera camera, Vector2 *position, Vector2 axis);

GuiGizmoAction gui_2d_gizmo_size_id(GuiId id, Camera camera, Vector2 position, Vector2 *size);
GuiGizmoAction gui_2d_gizmo_size_id(GuiId id, Camera camera, Vector2 position, Vector2 *size, f32 multiple);

GuiGizmoAction gui_2d_gizmo_size_square_id(GuiId id, Camera camera, Vector2 *position, Vector2 *size);
GuiGizmoAction gui_2d_gizmo_size_square_id(GuiId id, Camera camera, Vector2 *position, Vector2 *size, f32 multiple);

#endif // GUI_H
