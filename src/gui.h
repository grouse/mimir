#ifndef GUI_H
#define GUI_H

#include "external/stb/stb_truetype.h"

#include "maths.h"
#include "array.h"
#include "string.h"
#include "input.h"
#include "gfx_opengl.h"

#define GUI_ID(index) GuiId{ __COUNTER__, index, -1 }
#define GUI_ID_INVALID GuiId{ -1, -1, -1 }
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
    
    GUI_WINDOW_DEFAULT = GUI_WINDOW_MOVABLE | GUI_WINDOW_RESIZABLE
};

struct GuiId {
    i32 owner;
    i32 index;
    i32 internal;

    bool operator==(GuiId rhs)
    {
        return owner == rhs.owner && index == rhs.index && internal == rhs.internal;
    }

    bool operator!=(GuiId rhs)
    {
        return owner != rhs.owner || index != rhs.index || internal != rhs.internal;
    }
};

typedef stbtt_aligned_quad TextQuad;

struct TextQuadsAndBounds {
    DynamicArray<TextQuad> quads;
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

enum GuiLayoutType {
    GUI_LAYOUT_ABSOLUTE,
    GUI_LAYOUT_ROW,
    GUI_LAYOUT_COLUMN,
};

struct GuiLayout {
    GuiLayoutType type;
    
    
    union {
        struct {
            Vector2 pos;
            Vector2 size;
        };
        
        Rect rect;
    };
    
    Vector2 current;
    Vector2 available_space;

    union {
        struct {
            f32 margin;
        } row, column;
    };
};

// TODO(jesper0): this is really more like scrollarea state, I just need to do a couple
// passes on the API before it gets there
struct GuiLister {
    GuiId id;
    f32 offset;
};

struct Font {
    stbtt_bakedchar atlas[256];
    i32 ascent;
    i32 descent;
    i32 line_gap;
    i32 baseline;
    f32 scale;
    f32 line_height;
    f32 space_width;
    
    GLuint texture;
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
    GuiId active = GUI_ID_INVALID;
    GuiId active_press = GUI_ID_INVALID;
    GuiId last_active = GUI_ID_INVALID;
    GuiId active_window = GUI_ID_INVALID;
    
    i32 hot_z = 0;

    i32 current_window;
    DynamicArray<GuiWindow> windows;
    DynamicArray<GuiLister> listers;
    DynamicArray<GuiLayout> layout_stack;
    
    GuiId current_id;
    DynamicArray<GuiId> id_stack;

    DynamicArray<f32> vertices;
    
    bool capture_text[2];
    bool capture_keyboard[2];
    bool capture_mouse_wheel[2];
    
    DynamicArray<InputEvent> events;

    Vector2 drag_start_mouse;
    Vector2 drag_start_data;
    Vector2 drag_start_data1;
    
    Camera camera;
    
    struct {
        i16 x, y;
        i16 dx, dy;
        f32 dwheel;
        bool left_pressed;
        bool left_was_pressed;
        bool middle_pressed;
    } mouse;
    
    struct {
        struct {
            Font font;
            
            Vector3 color = Vector3{ 1.0f, 1.0f, 1.0f };
        } text;
        
        struct {
            Vector2 margin = { 5.0f, 5.0f };
            Vector2 border = { 1, 1 };

            Vector2 close_size = { 16.0f, 16.0f };
            
            Vector3 bg = rgb_unpack(0xFF282828);
            
            f32 title_height = 21;
            
            Vector3 title_fg = { 1, 1, 1 };
            
            Vector3 title_bg = rgb_unpack(0xFF212121);
            Vector3 title_bg_hot = rgb_unpack(0xFFFD8433);
            Vector3 title_bg_active = rgb_unpack(0xFFCA5100);
            
            Vector3 close_bg_hot = rgb_unpack(0xFFFF0000);
        } window;
        
        struct {
            Font font;
            
            Vector3 bg = rgb_unpack(0xFF333333);
            Vector3 bg_active = rgb_unpack(0xFF2C2C2C);
            Vector3 bg_hot = rgb_unpack(0xFF3A3A3A);
            
            Vector3 accent0 = rgb_unpack(0xFF404040);
            Vector3 accent0_active = rgb_unpack(0xFF000000);
            Vector3 accent1 = rgb_unpack(0xFF000000);
            Vector3 accent1_active = rgb_unpack(0xFF404040);
            Vector3 accent2 = rgb_unpack(0xFF1D2021);
        } button;
        
        struct {
            GLuint close;
        } icons;
        
        struct {
            Vector3 bg = rgb_unpack(0xFFCCCCCC);
            
            Vector3 scroll_btn = rgb_unpack(0xFF333333);
            Vector3 scroll_btn_hot = rgb_unpack(0xFF3A3A3A);
            Vector3 scroll_btn_active = rgb_unpack(0xFF2C2C2C);
            
            f32 thickness = 15.0f;
        } scrollbar;
        
        struct {
            Vector3 bg = rgb_unpack(0xFF383838);
            Vector3 border = rgb_unpack(0xFFAAAAAA);
            Vector3 fg = rgb_unpack(0xFFFFFFFF);
            Vector3 selected_bg = rgb_unpack(0xFF282828);
            Vector3 hot_bg = rgb_unpack(0xFF2F2F2F);
        } lister;
    } style;

};

extern GuiContext gui;

void init_gui();
void gui_begin_frame();
bool gui_input(InputEvent event);
void gui_render(Camera camera);

void gui_begin_layout(GuiLayout layout);
void gui_end_layout();

bool gui_begin_window_id(GuiId id, String title, Vector2 pos, Vector2 size, bool *visible, u32 flags = GUI_WINDOW_DEFAULT);
bool gui_begin_window_id(GuiId id, String title, Vector2 pos, Vector2 size, u32 flags = GUI_WINDOW_DEFAULT);
bool gui_begin_window_id(GuiId id, String title, GuiWindowState *state, u32 flags = GUI_WINDOW_DEFAULT);
void gui_end_window();

GuiEditboxAction gui_editbox_id(GuiId id, String in_str);
GuiEditboxAction gui_editbox_id(GuiId id, String in_str, Vector2 size);
GuiEditboxAction gui_editbox_id(GuiId id, f32 *value, Vector2 size);

Vector2 gui_layout_widget(Vector2 preferred_size, GuiAnchor anchor = GUI_ANCHOR_TOP);
Vector2 gui_layout_widget(Vector2 *required_size);
Rect gui_layout_widget_fill();

bool gui_button_id(GuiId id, String text);
bool gui_button_id(GuiId id, String text, Vector2 size);
bool gui_button_id(GuiId id, Font *font, TextQuadsAndBounds td, Vector2 size);

bool gui_checkbox_id(GuiId id, String label, bool *checked);

template<typename T> void gui_dropdown_id(GuiId id, Array<String> labels, Array<T> values, T *value);
template<typename T> void gui_dropdown_id(GuiId id, String *labels, T *values, i32 count, T *value);
template<typename T> T gui_dropdown_id(GuiId id, Array<String> labels, Array<T> values, T value);
template<typename T> T gui_dropdown_id(GuiId id, String *labels, T *values, i32 count, T value);

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
