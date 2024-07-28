#include "gui.h"
#include "gen/gui.h"
#include "gen/internal/gui.h"
#include "gen/font.h"
#include "gen/gfx_opengl.h"
#include "gen/core/window.h"

#include "core/core.h"
#include "core/file.h"
#include "assets.h"
#include "core/array.h"
#include "core/memory.h"
#include "window.h"
#include "core/maths.h"

#define GUI_DEBUG_HOT 0
#define GUI_DEBUG_PRESSED 0
#define GUI_DEBUG_FOCUSED 0

extern Allocator mem_frame;

GuiContext gui;

constexpr Vector2 GIZMO_CORNER_SIZE{ 10, 10 };
constexpr Vector2 GIZMO_ARROW_SIZE{ 50, 6 };
constexpr Vector2 GIZMO_ARROW_TIP_SIZE{ 16, 16 };
constexpr Vector3 GIZMO_OUTLINE_COLOR{ 0.0f, 0.0f, 0.0f };

bool gui_is_parent(GuiId id) INTERNAL
{
    return array_find_index(gui.id_stack, id) != -1;
}

i32 gui_push_overlay(Rect rect, bool active) INTERNAL
{
    i32 window = gui.current_window;

    if (active) {
        gui_push_clip_rect(rect);
        gui_push_command_buffer();
        array_add(&gui.overlay_rects, rect);

        gui.current_window = GUI_OVERLAY;
    }

    return window;
}

f32 calc_center(f32 min, f32 max, f32 size) INTERNAL
{
    return min + 0.5f*(max-min - size);
}

Vector2 calc_center(Vector2 tl, Vector2 br, Vector2 size) INTERNAL
{
    return {
        calc_center(tl.x, br.x, size.x),
        calc_center(tl.y, br.y, size.y)
    };
}

void gui_push_command_buffer() INTERNAL
{
    array_add(&gui.draw_stack, gfx_command_buffer());
}

void gui_pop_command_buffer(GfxCommandBuffer *dst) INTERNAL
{
    GfxCommandBuffer buf = array_pop(&gui.draw_stack);
    if (dst) array_add(&dst->commands, buf.commands);
}

GfxCommand gui_command(GfxCommandType type) INTERNAL
{
    // TODO(jesper): this entire function essentially exists to initialise the vertex buffer
    // data for the command. This should be replaced with a better API that lets you push
    // some data onto a frame or persistent VBO and get back a structure that you pass
    // to the command stream and it does the right thing
    GfxCommand cmd{ .type = type };
    switch (type) {
    case GFX_COMMAND_COLORED_PRIM:
        cmd.colored_prim.vbo = gui.vbo;
        cmd.colored_prim.vbo_offset = gui.vertices.count;
        break;
    case GFX_COMMAND_GUI_TEXT:
        cmd.gui_text.vbo = gui.vbo;
        cmd.gui_text.vbo_offset = gui.vertices.count;
        break;
    case GFX_COMMAND_GUI_PRIM_TEXTURE:
        cmd.gui_prim_texture.vbo = gui.vbo;
        cmd.gui_prim_texture.vbo_offset = gui.vertices.count;
        break;
    case GFX_COMMAND_COLORED_LINE:
    case GFX_COMMAND_TEXTURED_PRIM:
    case GFX_COMMAND_MONO_TEXT:
        LOG_ERROR("don't use this API for these");
        break;
    }
    return cmd;
}

void gui_push_command(GfxCommandBuffer *cmdbuf, GfxCommand cmd) INTERNAL
{
    if (auto *existing = array_tail(cmdbuf->commands);
        existing && existing->type == cmd.type)
    {
        switch (cmd.type) {
        case GFX_COMMAND_GUI_TEXT:
            if (existing->gui_text.vbo_offset + existing->gui_text.vertex_count == cmd.gui_text.vbo_offset &&
                existing->gui_text.font_atlas == cmd.gui_text.font_atlas &&
                existing->gui_text.color == cmd.gui_text.color)
            {
                existing->gui_text.vertex_count += cmd.gui_text.vertex_count;
                return;
            }
            break;
        case GFX_COMMAND_GUI_PRIM_TEXTURE:
            if (existing->gui_prim_texture.vbo_offset + existing->gui_prim_texture.vertex_count == cmd.gui_prim_texture.vbo_offset &&
                existing->gui_prim_texture.texture == cmd.gui_prim_texture.texture)
            {
                existing->gui_prim_texture.vertex_count += cmd.gui_prim_texture.vertex_count;
                return;
            }
            break;
        case GFX_COMMAND_COLORED_PRIM:
        case GFX_COMMAND_COLORED_LINE:
        case GFX_COMMAND_TEXTURED_PRIM:
        case GFX_COMMAND_MONO_TEXT:
            PANIC("incorrect API usage; use gfx_push_command");
            break;
        }
    }

    array_add(&cmdbuf->commands, cmd);
}

void gui_push_layout(LayoutRect rect) EXPORT
{
    array_add(&gui.layout_stack, rect);
}

void gui_pop_layout() EXPORT
{
    ASSERT(gui.layout_stack.count > 0);

    LayoutRect *layout = gui_current_layout();

    if (LayoutRect *parent = gui_parent_layout();
        (layout->flags & LAYOUT_ROOT) == 0 &&
        layout->flags & EXPAND_XY &&
        parent && parent->flags & EXPAND_XY)
    {
        if (parent->flags & EXPAND_X) {
            parent->rem.tl.x = MAX(parent->rem.tl.x, layout->rem.tl.x);
            parent->rem.br.x = MAX(parent->rem.br.x, parent->rem.tl.x);
        }

        if (parent->flags & EXPAND_Y) {
            parent->rem.tl.y = MAX(parent->rem.tl.y, layout->rem.tl.y);
            parent->rem.br.y = MAX(parent->rem.br.y, parent->rem.tl.y);
        }
    }


    gui.layout_stack.count--;
}

LayoutRect* gui_current_layout() EXPORT
{
    return array_tail(gui.layout_stack);
}

LayoutRect* gui_parent_layout() EXPORT
{
    return gui.layout_stack.count > 1 ? &gui.layout_stack[gui.layout_stack.count-2] : nullptr;
}



Rect shrink_rect(Rect *rect, f32 amount) EXPORT  { return shrink_rect(rect, { amount, amount }); }
Rect expand_rect(Rect *rect, f32 amount) EXPORT  { return expand_rect(rect, { amount, amount }); }

Rect shrink_rect(Rect *rect, Vector2 amount) EXPORT
{
    Rect result = *rect;
    rect->tl += amount;
    rect->br -= amount;
    return result;
}

Rect expand_rect(Rect *rect, Vector2 amount) EXPORT
{
    Rect result = *rect;
    rect->tl -= amount;
    rect->br += amount;
    return result;
}

Rect split_left(Rect *rect, f32 w) EXPORT
{
    f32 x = rect->tl.x;
    rect->tl.x = MIN(rect->tl.x+w, rect->br.x);
    return { { x, rect->tl.y }, { rect->tl.x, rect->br.y } };
}

Rect split_right(Rect *rect, f32 w) EXPORT
{
    f32 x = rect->br.x;
    rect->br.x = MAX(rect->br.x-w, rect->tl.x);
    return { { rect->br.x, rect->tl.y }, { x, rect->br.y } };
}

Rect split_top(Rect *rect, f32 h) EXPORT
{
    f32 y = rect->tl.y;
    rect->tl.y = MIN(rect->tl.y+h, rect->br.y);
    return { { rect->tl.x, y }, { rect->br.x, rect->tl.y } };
}

Rect split_bottom(Rect *rect, f32 h) EXPORT
{
    f32 y = rect->br.y;
    rect->br.y = MAX(rect->br.y-h, rect->tl.y);
    return { { rect->tl.x, rect->br.y }, { rect->br.x, y } };
}

Rect shrink_rect(Rect rect, Vector2 amount) EXPORT  { return shrink_rect(&rect, amount); }
Rect expand_rect(Rect rect, Vector2 amount) EXPORT  { return expand_rect(&rect, amount); }

Rect shrink_rect(Rect rect, f32 amount) EXPORT  { return shrink_rect(&rect, amount); }
Rect expand_rect(Rect rect, f32 amount) EXPORT  { return expand_rect(&rect, amount); }

Rect split_left(Rect rect, f32 w) EXPORT { return split_left(&rect, w); }
Rect split_right(Rect rect, f32 w) EXPORT { return split_right(&rect, w); }
Rect split_top(Rect rect, f32 h) EXPORT { return split_top(&rect, h); }
Rect split_bottom(Rect rect, f32 h) EXPORT { return split_bottom(&rect, h); }


LayoutRect shrink_rect(LayoutRect *layout, f32 amount) EXPORT { return shrink_rect(layout, { amount, amount }); }
LayoutRect expand_rect(LayoutRect *layout, f32 amount) EXPORT { return expand_rect(layout, { amount, amount }); }

LayoutRect shrink_rect(LayoutRect *layout, Vector2 amount) EXPORT
{
    LayoutRect result = *layout;
    result.rem = shrink_rect(&layout->rem, amount);
    return result;
}

LayoutRect expand_rect(LayoutRect *layout, Vector2 amount) EXPORT
{
    if (layout->flags & EXPAND_X) {
        f32 diff = amount.x - (layout->rem.br.x - layout->rem.tl.x);
        if (diff > 0) layout->rem.br.x += diff;
    } else {
        amount.x = MIN(amount.x, layout->rem.br.x-layout->rem.tl.x);
    }

    if (layout->flags & EXPAND_Y) {
        f32 diff = amount.y - (layout->rem.br.y - layout->rem.tl.y);
        if (diff > 0) layout->rem.br.y += diff;
    } else {
        amount.y = MIN(amount.y, layout->rem.br.x-layout->rem.tl.x);
    }

    LayoutRect result = *layout;
    result.rem = shrink_rect(&layout->rem, amount);
    return result;
}


LayoutRect split_left(LayoutRect *layout, SplitDesc desc) EXPORT
{
    desc.flags &= ~SPLIT_RIGHT;
    return split_col_internal(layout, desc);
}

LayoutRect split_right(LayoutRect *layout, SplitDesc desc) EXPORT
{
    desc.flags |= SPLIT_RIGHT;
    return split_col_internal(layout, desc);
}

LayoutRect split_top(LayoutRect *layout, SplitDesc desc) EXPORT
{
    desc.flags &= ~SPLIT_BOTTOM;
    return split_row_internal(layout, desc);
}

LayoutRect split_bottom(LayoutRect *layout, SplitDesc desc) EXPORT
{
    desc.flags |= SPLIT_BOTTOM;
    return split_row_internal(layout, desc);
}


LayoutRect split_col(LayoutRect *layout, SplitDesc desc) EXPORT
{
    desc.flags &= ~SPLIT_RIGHT;
    desc.flags |= layout->flags & SPLIT_RIGHT;
    return split_col_internal(layout, desc);
}

LayoutRect split_row(LayoutRect *layout, SplitDesc desc) EXPORT
{
    desc.flags &= ~SPLIT_BOTTOM;
    desc.flags |= layout->flags & SPLIT_BOTTOM;
    return split_row_internal(layout, desc);
}

LayoutRect make_layout_rect(SplitDesc desc, Rect rect) INTERNAL
{
    LayoutRect result{ rect, desc.rflags };
    if (desc.margin) shrink_rect(&result, desc.margin);
    return result;
}

LayoutRect split_row_internal(LayoutRect *layout, SplitDesc desc) INTERNAL
{
    desc.rflags |= SPLIT_COL;

    f32 height = desc.factor > 0 ? MAX(desc.h, desc.factor*layout->bounds.size().y) : desc.h;
    if (height == 0) height = layout->size().y;

    f32 width = MAX(desc._w, layout->size().x);
    if (desc.flags & SPLIT_SQUARE) width = height = MAX(width, height);

    if (layout->flags & EXPAND_Y) {
        // TODO(jesper): expand_top/bottom(&layout->rect, diff) ?
        f32 diff = height - layout->size().y;
        if (diff > 0) layout->rem.br.y += diff;
    } else {
        height = MIN(height, layout->size().y);
    }

    if (layout->flags & EXPAND_X) {
        f32 diff = width - layout->size().x;
        if (diff > 0) layout->rem.br.x += diff;
    } else {
        width = MIN(width, layout->size().x);
    }

    Rect rect;
    if (desc.flags & SPLIT_BOTTOM) rect = split_bottom(&layout->rem, height);
    else rect = split_top(&layout->rem, height);
    // TODO(jesper): LAYOUT_ALIGN if width < layout->size().x
    rect.br.x = rect.tl.x + width;

    return make_layout_rect(desc, rect);
}

LayoutRect split_col_internal(LayoutRect *layout, SplitDesc desc) INTERNAL
{
    desc.rflags &= ~SPLIT_COL;

    f32 width = desc.factor > 0 ? MAX(desc.w, desc.factor*layout->bounds.size().x) : desc.w;
    if (width == 0) width = layout->size().x;

    f32 height = MAX(desc._h, layout->size().y);
    if (desc.flags & SPLIT_SQUARE) width = height = MAX(width, height);

    f32 diff = width - layout->size().x;
    if (layout->flags & EXPAND_X && diff > 0) {
        // TODO(jesper): expand_left/right(&layout->rect, diff) ?
        if (layout->flags & SPLIT_RIGHT) layout->rem.tl.x -= diff;
        else layout->rem.br.x += diff;
    } else {
        width = MIN(width, layout->size().x);
    }

    if (layout->flags & EXPAND_Y) {
        f32 diff = height - layout->size().y;
        if (diff > 0) layout->rem.br.y += diff;
    } else {
        height = MIN(height, layout->size().y);
    }

    Rect rect;
    if (desc.flags & SPLIT_RIGHT) rect = split_right(&layout->rem, width);
    else rect = split_left(&layout->rem, width);
    // TODO(jesper): LAYOUT_ALIGN if height < layout->size().y
    rect.br.y = rect.tl.y + height;

    return make_layout_rect(desc, rect);
}

LayoutRect split_rect(LayoutRect *layout, SplitDesc desc) EXPORT
{
    if (layout->flags & SPLIT_COL) {
        f32 width  = desc.w;
        f32 height = desc._h;
        desc.w = width; desc._h = height;
        return split_col(layout, desc);
    } else {
        f32 width = desc._w;
        f32 height = desc.h;
        desc.w = width; desc._h = height;
        return split_row(layout, desc);
    }
}

GuiMenu* gui_find_or_create_menu(GuiId id, Vector2 initial_size, Vector2 max_size /*= {}*/) INTERNAL
{
    GuiMenu *menu = map_find_emplace(
        &gui.menus, id, {
            .size = { MAX(initial_size.x, gui.style.menu.min_width), initial_size.y },
            .max_size = max_size,
        });

    return menu;
}


void init_gui() EXPORT
{
    PANIC_IF(!mem_frame, "mem_frame not initialised");
    gui.vertices = { .alloc = mem_frame };
    gui.layout_stack = { .alloc = mem_frame };
    gui.overlay_rects = { .alloc = mem_frame };

    array_add(&gui.id_stack, { 0xdeadbeef });

    glGenBuffers(1, &gui.vbo);

    array_add(&gui.windows, {
        .id = GUI_ID,
        .client_rect = Rect{ { 0.0f, 0.0f }, { gfx.resolution.x, gfx.resolution.y } },
        .command_buffer.commands.alloc = mem_frame,
        .size = gfx.resolution,
    });

    array_add(&gui.windows, {
        .id = GUI_ID,
        .client_rect = Rect{ { 0.0f, 0.0f }, { gfx.resolution.x, gfx.resolution.y } },
        .command_buffer.commands.alloc = mem_frame,
        .size = gfx.resolution,
    });

    array_add(&gui.layout_stack, { { { 0, 0 }, gfx.resolution } });

    gui.fonts.base  = create_font("fonts/Ubuntu/Ubuntu-Regular.ttf", 18);
    gui.fonts.icons = create_font("fonts/NerdFont/UbuntuNerdFont-Regular.ttf", 16);

    {
        SArena scratch = tl_scratch_arena();
        GlyphsData down = calc_glyphs_data(gui.icons.down, &gui.fonts.icons, scratch);
        gui.style.scrollbar.thickness = MAX3(gui.style.scrollbar.thickness, down.bounds.y, down.bounds.x);
    }

    init_input_map(gui.input.base, {
        { FOCUS_NEXT,  IKEY(KC_TAB) },
        { FOCUS_PREV,  IKEY(KC_TAB, MF_SHIFT) },
        { FOCUS_CLEAR, IKEY(KC_ESC) },
    });

    init_input_map(gui.input.scrollarea, {
        { SCROLL, IMOUSE(.type = AXIS, MB_WHEEL) },
    });

    init_input_map(gui.input.window, {
        { WINDOW_CLOSE, IKEY(KC_Q, MF_CTRL) },
        { WINDOW_CLEAR, IKEY(KC_ESC) },
    });

    init_input_map(gui.input.editbox, {
        { TEXT_INPUT,   .text = {}},
        { CURSOR_LEFT,  IKEY(KC_LEFT) },
        { CURSOR_RIGHT, IKEY(KC_RIGHT) },
        { SELECT_ALL,   IKEY(KC_A, MF_CTRL) },
        { COPY,         IKEY(KC_C, MF_CTRL) },
        { PASTE,        IKEY(KC_V, MF_CTRL) },
        { DELETE,       IKEY(KC_DELETE) },
        { DELETE_BACK,  IKEY(KC_BACKSPACE) },
        { CONFIRM,      IKEY(KC_ENTER) },
        { CONFIRM_CONT, IKEY(KC_TAB) },
        { CANCEL,       IKEY(KC_ESC) },
    });

    init_input_map(gui.input.lister, {
        { FORWARD, IKEY(KC_DOWN ) },
        { BACK,    IKEY(KC_UP   ) },
        { CONFIRM, IKEY(KC_ENTER) },
        { CANCEL,  IKEY(KC_ESC  ) },
    });

    init_input_map(gui.input.tree, {
        { FOCUS_NEXT, IKEY(KC_DOWN) },
        { FOCUS_PREV, IKEY(KC_UP) },
        { ACTIVATE,   IKEY(KC_RIGHT) },
        { DEACTIVATE, IKEY(KC_LEFT) },
        { TOGGLE,     IKEY(KC_SPACE) },
    });

    init_input_map(gui.input.checkbox, {
        { TOGGLE, IKEY(KC_SPACE) },
    });

    init_input_map(gui.input.button, {
        { CONFIRM, IKEY(KC_ENTER) },
    });

    init_input_map(gui.input.dropdown, {
        { TOGGLE, IKEY(KC_SPACE) },
        { CANCEL, IKEY(KC_ESC) },
    });
}

GuiWindow* gui_current_window() EXPORT
{
    ASSERT(gui.current_window < gui.windows.count);
    return &gui.windows[gui.current_window];
}

Vector2 gui_mouse() INTERNAL
{
    return { (f32)gui.mouse.x, (f32)gui.mouse.y };
}

bool gui_input_layer(GuiId id, InputMapId map_id) EXPORT
{
    if (gui.focused == id &&
        gui.focused_window == gui_current_window()->id)
    {
        push_input_layer(map_id);
        return true;
    }

    return false;
}

void gui_drag_start(Vector2 data0, Vector2 data1) EXPORT
{
    gui.drag_start_mouse = gui_mouse();
    gui.drag_start_data[0] = data0;
    gui.drag_start_data[1] = data1;
}

void gui_hot(GuiId id) EXPORT
{
    if (gui.pressed == id ||
        (gui.pressed == GUI_ID_INVALID &&
         (gui.hot_window == gui.windows[gui.current_window].id)))
    {
        gui.next_hot = id;
    }
}

void gui_focus(GuiId id) EXPORT
{
    if (gui.focused == id) return;
    if (GUI_DEBUG_FOCUSED) LOG_INFO("gui focusing: %u", id);

    gui.focused = id;
    //gui.focused_window = gui_current_window()->id;
}

void gui_focus_window(GuiId id) EXPORT
{
    if (gui.focused_window == id) return;
    if (GUI_DEBUG_FOCUSED) LOG_INFO("gui focusing window: %u", id);

    gui.focused_window = id;
    //gui_focus(GUI_ID_INVALID);
}

bool gui_hot_rect(GuiId id, Rect rect) EXPORT
{
    Vector2 mouse = gui_mouse();

    if (mouse.x >= rect.tl.x && mouse.x <= rect.br.x &&
        mouse.y >= rect.tl.y && mouse.y <= rect.br.y)
    {
        gui_hot(id);
        return true;
    }

    return false;
}

void gui_clear_hot() EXPORT
{
    if (GUI_DEBUG_HOT) LOG_INFO("clearing hot and next hot");
    gui.hot = gui.next_hot = GUI_ID_INVALID;
}

bool gui_pressed(GuiId id) EXPORT
{
    if (gui.pressed == id && !get_input_mouse(MB_PRIMARY, HOLD)) {
        if (GUI_DEBUG_PRESSED) LOG_INFO("clearing gui pressed");
        gui.pressed = GUI_ID_INVALID;
        return false;
    }

    if (gui.hot == id && get_input_mouse(MB_PRIMARY)) {
        if (GUI_DEBUG_PRESSED) LOG_INFO("gui pressed: %u", id);
        gui.pressed = id;
        gui_focus(id);
        return true;
    }

    return false;
}

bool gui_clicked(GuiId id, Rect rect) EXPORT
{
    bool was_pressed = gui.pressed == id;
    gui_hot_rect(id, rect);
    if (!gui_pressed(id) && gui.hot == id && gui.pressed != id && was_pressed) {
        return true;
    }

    return false;
}


bool gui_drag(
    GuiId id,
    Vector2 data0, Vector2 data1 = {},
    f32 min_drag = 0.0f) EXPORT
{
    if (gui_pressed(id)) gui_drag_start(data0, data1);
    if (gui.dragging == id && gui.pressed != id) gui.dragging = GUI_ID_INVALID;

    Vector2 d = gui.drag_start_mouse - gui_mouse();
    if (gui.pressed == id && length_sq(d) >= min_drag*min_drag) {
        gui.dragging = id;
    }

    if (gui.pressed == id) {
        gui_hot(id);
        gui_focus_window(gui_current_window()->id);
    }

    return gui.dragging == id;
}

void gui_handle_focus_grabbing(GuiId id) INTERNAL
{
    if (gui.focused_window == gui_current_window()->id) {
        if (gui.focused == GUI_ID_INVALID || gui.pressed == id) {
            gui_focus(id);
        }

        if (gui.focused == id) {
            push_input_layer(gui.input.base);
            if (get_input_edge(FOCUS_NEXT, INPUT_MAP_ANY)) gui_focus(GUI_ID_INVALID);
            if (get_input_edge(FOCUS_PREV, INPUT_MAP_ANY)) gui_focus(gui.prev_focusable_widget);
            if (get_input_edge(FOCUS_CLEAR, INPUT_MAP_ANY)) clear_focus();
        }

        gui.prev_focusable_widget = id;
    }
}

void clear_focus() INTERNAL
{
    gui_focus(gui.focused_window);
}


bool apply_clip_rect(GlyphRect *g, Rect *clip_rect) INTERNAL
{
    if (!clip_rect) return true;

    if (g->x0 > clip_rect->br.x) return false;
    if (g->y0 > clip_rect->br.y) return false;
    if (g->x1 < clip_rect->tl.x) return false;
    if (g->y1 < clip_rect->tl.y) return false;

    if (g->x1 > clip_rect->br.x) {
        f32 w0 = g->x1 - g->x0;
        g->x1 = clip_rect->br.x;
        f32 w1 = g->x1 - g->x0;
        g->s1 = g->s0 + (g->s1 - g->s0) * (w1/w0);
    }

    if (g->x0 < clip_rect->tl.x) {
        f32 w0 = g->x1 - g->x0;
        g->x0 = clip_rect->tl.x;
        f32 w1 = g->x1 - g->x0;
        g->s0 = g->s1 - (g->s1 - g->s0) * (w1/w0);
    }

    if (g->y1 > clip_rect->br.y) {
        f32 h0 = g->y1 - g->y0;
        g->y1 = clip_rect->br.y;
        f32 h1 = g->y1 - g->y0;
        g->t1 = g->t0 + (g->t1 - g->t0) * (h1/h0);
    }

    if (g->y0 < clip_rect->tl.y) {
        f32 h0 = g->y1 - g->y0;
        g->y0 = clip_rect->tl.y;
        f32 h1 = g->y1 - g->y0;
        g->t0 = g->t1 - (g->t1 - g->t0) * (h1/h0);
    }

    return true;
}

bool apply_clip_rect(Rect *rect, Rect *clip_rect) INTERNAL
{
    if (!clip_rect) return true;

    if (rect->tl.x > clip_rect->br.x) return false;
    if (rect->tl.y > clip_rect->br.y) return false;
    if (rect->br.x < clip_rect->tl.x) return false;
    if (rect->br.y < clip_rect->tl.y) return false;

    f32 left = rect->tl.x;
    f32 right = rect->br.x;
    f32 top = rect->tl.y;
    f32 bottom = rect->br.y;

    top = MAX(top, clip_rect->tl.y);
    left = MAX(left, clip_rect->tl.x);
    right = MIN(right, clip_rect->br.x);
    bottom = MIN(bottom, clip_rect->br.y);

    rect->tl = { left, top };
    rect->br = { right, bottom };
    return true;
}

void gui_begin_frame() EXPORT
{
    gui.mouse.x = g_mouse.x;
    gui.mouse.y = g_mouse.y;

    gui_push_command_buffer();

    for (GuiWindow &wnd : gui.windows) {
        array_reset(&wnd.command_buffer.commands, wnd.command_buffer.commands.capacity);
        wnd.state.was_active = wnd.state.active;
        wnd.state.active = false;
    }

    array_reset(&gui.vertices, gui.vertices.count);
    array_reset(&gui.layout_stack, gui.layout_stack.count);
    array_reset(&gui.overlay_rects, gui.overlay_rects.count);

    // NOTE(jesper): when resolution changes the root and overlay "windows" have to adjust their
    // clip rects as well
    gui.windows[GUI_BACKGROUND].client_rect.br = gfx.resolution;
    gui.windows[GUI_OVERLAY].client_rect.br = gfx.resolution;

    gui_push_layout({ { { 0, 0 }, gfx.resolution }});
}

void gui_end_frame() EXPORT
{
    for (i32 i = 0; i < gui.dialogs.count; i++) {
        GuiDialog it = gui.dialogs[i];
        GuiAction action = GUI_NONE;

        Vector2 dialog_size{ 100, 50 };
        GuiWindowDesc desc {
            .title = it.title,
            .position = calc_center({}, gfx.resolution, dialog_size),
            .size = dialog_size,
            .flags = GUI_DIALOG,
        };

        i32 index = gui_find_or_create_window_index(it.id, desc);
        gui.windows[index].state.hidden = false;

        if (gui_begin_window_internal(it.id, it.title, index)) {
            action = it.proc(it.id);
            gui_end_window();
        } else {
            action = GUI_END;
        }

        if (action == GUI_END) {
            FREE(mem_dynamic, it.title.data);
            array_remove(&gui.dialogs, i--);
        }
    }

    gui_pop_layout();

    if (!get_input_mouse(MB_PRIMARY, HOLD)) gui.pressed = GUI_ID_INVALID;
    gui.mouse.dx = gui.mouse.dy = 0;

    gui_pop_command_buffer(&gui.windows[GUI_BACKGROUND].command_buffer);

    ASSERT(gui.draw_stack.count == 0);
    ASSERT(gui.clip_stack.count == 0);
    ASSERT(gui.id_stack.count == 1);
    ASSERT(gui.layout_stack.count == 0);

    glBindBuffer(GL_ARRAY_BUFFER, gui.vbo);
    glBufferData(GL_ARRAY_BUFFER, gui.vertices.count * sizeof gui.vertices[0], gui.vertices.data, GL_STREAM_DRAW);

    if (GUI_DEBUG_HOT && gui.hot != gui.next_hot) {
        LOG_INFO("next hot id: %u", gui.next_hot);
    }

    gui.hot = gui.next_hot;
    gui.next_hot = GUI_ID_INVALID;
    // TODO(jesper): removing this means you can shift-tab circularly through a window's focusable widgets, but cycling forward doesn't work due to how the window grabs focus if nothing is, to avoid other awkward behaviours. Will want to revist this eventually
    gui.prev_focusable_widget = GUI_ID_INVALID;

    GuiId old_focused = gui.focused_window;
    GuiId old = gui.hot_window;

    for (GuiWindow &wnd : gui.windows) {
        if (!wnd.state.active && gui.focused_window == wnd.id) {
            gui_focus_window(GUI_ID_INVALID);
            break;
        }
    }

    gui.hot_window = GUI_ID_INVALID;
    for (Rect &r : gui.overlay_rects) {
        if (point_in_rect(gui_mouse(), r)) {
            gui.hot_window = gui.windows[GUI_OVERLAY].id;
            if (gui.focused_window == GUI_ID_INVALID) gui_focus_window(gui.windows[GUI_OVERLAY].id);
            gui.windows[GUI_OVERLAY].state.active = true;
            break;
        }
    }

    if (gui.hot_window == GUI_ID_INVALID) {
        for (auto wnd : reverse(gui.windows)) {
            if (wnd->state.active && point_in_rect(gui_mouse(), { wnd->pos, wnd->pos+wnd->size })) {
                gui.hot_window = wnd->id;
                break;
            }
        }
    }

    if (gui.hot == GUI_ID_INVALID &&
        gui.hot_window != GUI_ID_INVALID)
    {
        gui.hot = gui.hot_window;
        if (GUI_DEBUG_HOT) LOG_INFO("setting gui hot to the hot window: %u", gui.hot);
    }

    if (gui.hot_window == GUI_ID_INVALID) {
        gui.hot_window = gui.windows[GUI_BACKGROUND].id;
    }

    if (gui.focused_window == GUI_ID_INVALID) {
        gui_focus_window(gui.windows[GUI_BACKGROUND].id);
    }

    if (GUI_DEBUG_HOT && old != gui.hot_window) {
        LOG_INFO("hot window: %u", gui.hot_window);
    }

    GuiId old_focused_widget = gui.focused;
    if (GUI_DEBUG_FOCUSED && old_focused_widget != gui.focused) {
        LOG_INFO("focused widget: %u", gui.focused);
        LOG_INFO("old focused widget: %u", old_focused_widget);
        old_focused_widget = gui.focused;
    }

    if (GUI_DEBUG_FOCUSED && old_focused != gui.focused_window) {
        LOG_INFO("focused window: %u", gui.focused_window);
        LOG_INFO("old focused window: %u", old_focused);
    }
}

void gui_render() EXPORT
{
    Matrix3 view = mat3_orthographic(0, gfx.resolution.x, gfx.resolution.y, 0);

    gfx_submit_commands(gui.windows[GUI_BACKGROUND].command_buffer, view);
    for (auto &wnd : slice(gui.windows, 2)) gfx_submit_commands(wnd.command_buffer, view);
    gfx_submit_commands(gui.windows[GUI_OVERLAY].command_buffer, view);
}


void gui_textbox(GlyphsData text, Rect rect) EXPORT
{
    Vector2 pos{ rect.tl.x, calc_center(rect.tl.y, rect.br.y, text.font->line_height) };
    gui_draw_text(text, pos, gui.style.fg);
}


void gui_textbox(String str, Rect rect, FontAtlas *font /*= &gui.fonts.base */) EXPORT
{
    SArena scratch = tl_scratch_arena();
    GlyphsData td = calc_glyphs_data(str, font, scratch);

    Vector2 pos{ rect.tl.x, calc_center(rect.tl.y, rect.br.y, font->line_height) };
    gui_draw_text(td, pos, gui.style.fg);
}

void gui_textbox(String str, FontAtlas *font /*= &gui.fonts.base*/) EXPORT
{
    SArena scratch = tl_scratch_arena();
    GlyphsData td = calc_glyphs_data(str, font, scratch);

    // TODO(jesper): what I want here is some kind of flag/property on the parent layout that allows a child to query for appropriate row/column dimension to fit. E.g. a row-layout parent with a fixed-height per item, and only if the parent doesn't have such, then it uses the calculated height
    Rect rect = split_rect({ td.bounds.x, td.bounds.y, .margin = gui.style.margin }).rem;
    Vector2 pos{ rect.tl.x, calc_center(rect.tl.y, rect.br.y, font->line_height) };
    gui_draw_text(td, pos, gui.style.fg);
}

void gui_textbox(String str, u32 flags) EXPORT
{
    SArena scratch = tl_scratch_arena();
    GlyphsData td = calc_glyphs_data(str, &gui.fonts.base, scratch);

    Rect rect;
    if (flags & SPLIT_RIGHT) rect = split_right({ td.bounds.x, td.bounds.y, .margin = gui.style.margin });
    else if (flags & SPLIT_COL) rect = split_left({ td.bounds.x, td.bounds.y, .margin = gui.style.margin });
    else if (flags & SPLIT_BOTTOM) rect = split_bottom({ td.bounds.x, td.bounds.y, .margin = gui.style.margin });
    else rect = split_top({ td.bounds.x, td.bounds.y, .margin = gui.style.margin });

    return gui_textbox(td, rect);
}


GuiWindow* push_window_to_top(i32 index) INTERNAL
{
    if (index != gui.windows.count-1) {
        GuiWindow window = gui.windows[index];
        array_remove(&gui.windows, index);
        array_add(&gui.windows, window);
    }

    if (gui.current_window == index) gui.current_window = gui.windows.count-1;

    GuiWindow *wnd = gui_current_window();
    gui_focus(GUI_ID_INVALID);
    gui_focus_window(wnd->id);
    return wnd;
}


bool gui_button_id(GuiId id, Rect rect) EXPORT
{
    bool clicked = false;

    gui_handle_focus_grabbing(id);
    if (gui_input_layer(id, gui.input.button)) {
        if (get_input_edge(CONFIRM, gui.input.button)) clicked = true;
    }

    clicked = clicked || gui_clicked(id, rect);
    gui_draw_button(id, rect);
    return clicked;
}

bool gui_button_id(GuiId id, String text) EXPORT
{
    SArena scratch = tl_scratch_arena();
    GlyphsData td = calc_glyphs_data(text, &gui.fonts.base, scratch);
    Rect rect = split_rect({ td.bounds.x+6, td.bounds.y+16, .margin = gui.style.margin });
    return gui_button_id(id, td, rect);
}

bool gui_button_id(GuiId id, String text, Rect rect) EXPORT
{
    SArena scratch = tl_scratch_arena();
    GlyphsData td = calc_glyphs_data(text, &gui.fonts.base, scratch);
    return gui_button_id(id, td, rect);
}

bool gui_button_id(GuiId id, AssetHandle icon, Rect rect) EXPORT
{
    bool clicked = gui_clicked(id, rect);
    gui_draw_button(id, rect);
    gui_draw_rect(rect, icon);
    return clicked;
}

bool gui_button_id(GuiId id, AssetHandle icon, f32 icon_size) EXPORT
{
    Vector2 icon_s{ icon_size, icon_size };
    Rect rect = split_rect({
        icon_size+4, icon_size+4,
        .margin = gui.style.margin,
    });

    bool clicked = gui_clicked(id, rect);
    gui_draw_button(id, rect);
    gui_draw_rect({ calc_center(rect.tl, rect.br, icon_s), icon_s }, icon);

    return clicked;
}

bool gui_button_id(GuiId id, GlyphsData td, Rect rect) EXPORT
{
    bool clicked = gui_button_id(id, rect);
    gui_draw_text(td, calc_center(rect.tl, rect.br, { td.bounds.x, td.font->line_height }), gui.style.fg);
    return clicked;
}

bool gui_icon_button_id(
    GuiId id,
    String icon,
    u32 flags /*= LAYOUT_INVALID_FLAG*/) EXPORT
{
    SplitDesc desc{
        gui.icons.size, gui.icons.size,
        .margin = gui.style.margin,
        .flags = SPLIT_SQUARE,
    };

    Rect rect;
    if (flags == LAYOUT_INVALID_FLAG) rect = split_rect(desc);
    else if (flags & SPLIT_RIGHT) rect = split_right(desc);
    else if (flags & SPLIT_COL) rect = split_left(desc);
    else if (flags & SPLIT_BOTTOM) rect = split_bottom(desc);
    else rect = split_top(desc);

    bool clicked = gui_button_id(id, rect);
    gui_draw_icon(icon, rect);
    return clicked;
}

bool gui_icon_switch_id(
    GuiId id,
    String icon,
    bool checked) EXPORT
{
    bool initial = checked;

    gui_handle_focus_grabbing(id);
    if (gui_input_layer(id, gui.input.checkbox)) {
        if (get_input_edge(TOGGLE, gui.input.checkbox)) checked = !checked;
    }

    Rect rect = split_rect({ gui.icons.size, gui.icons.size, .margin = gui.style.margin, .flags = SPLIT_SQUARE });
    if (gui_clicked(id, rect)) checked = !checked;

    Vector3 btn_bg = gui.style.bg;
    if (checked) btn_bg = gui.pressed == id ? gui.style.accent_bg_press : gui.hot == id ? gui.style.accent_bg_hot : gui.style.accent_bg;
    else btn_bg = gui.pressed == id ? gui.style.bg_press : gui.hot == id ? gui.style.bg_hot : gui.style.bg;

    Vector3 btn_bg_acc0, btn_bg_acc1;
    btn_bg_acc0 = gui.pressed == id ? rgb_mul(btn_bg, 0.6f) : rgb_mul(btn_bg, 1.25f);
    btn_bg_acc1 = gui.pressed == id ? rgb_mul(btn_bg, 1.25f) : rgb_mul(btn_bg, 0.6f);

    gui_draw_button(rect, btn_bg, btn_bg_acc0, btn_bg_acc1);
    gui_draw_icon(icon, rect);

    return initial != checked;

}

bool gui_icon_button_id(GuiId id, String icon, Rect rect) EXPORT
{
    bool clicked = gui_button_id(id, rect);
    gui_draw_icon(icon, rect);
    return clicked;
}

bool gui_checkbox_id(GuiId id, bool *checked, Rect rect) EXPORT
{
    bool initial = *checked;

    gui_handle_focus_grabbing(id);

    bool clicked = false;
    if (gui_input_layer(id, gui.input.checkbox)) {
        if (get_input_edge(TOGGLE, gui.input.checkbox)) clicked = true;
    }

    if (clicked || gui_clicked(id, rect)) *checked = !(*checked);

    gui_draw_button(id, rect);
    if (*checked) gui_draw_icon(gui.icons.check, rect);

    return initial != *checked;;
}

bool gui_checkbox_id(GuiId id, bool *checked) EXPORT
{
    Rect rect = split_col({ gui.style.checkbox.size, .margin = gui.style.margin, .flags = SPLIT_SQUARE });
    return gui_checkbox_id(id, checked, rect);
}

bool gui_checkbox_id(GuiId id, String label, bool *checked) EXPORT
{
    SArena scratch = tl_scratch_arena();

    GlyphsData td = calc_glyphs_data(label, &gui.fonts.base, scratch);

    f32 height = MAX(gui.style.checkbox.size, td.bounds.y);
    LayoutRect layout = split_rect({ td.bounds.x+gui.style.checkbox.size+6, height, .margin = gui.style.margin });
    Rect rect = layout.rem;
    Rect c_rect = split_col(&layout, { gui.style.checkbox.size, .margin = gui.style.margin, .flags = SPLIT_SQUARE });
    Rect t_rect = split_col(&layout, { td.bounds.x+4*gui.style.margin, c_rect.size().y, .margin = gui.style.margin });

    gui_hot_rect(id, rect);
    bool toggled = gui_checkbox_id(id, checked, c_rect);

    Vector2 pos{ t_rect.tl.x+gui.style.margin, calc_center(rect.tl.y, rect.br.y, td.font->line_height) };
    gui_draw_text(td, pos, gui.style.fg);

    return toggled;
}

GuiAction gui_slider_id(GuiId id, String label, f32 *value, f32 min, f32 max, f32 step) EXPORT
{
    SArena scratch = tl_scratch_arena();
    GuiAction action = GUI_NONE;

    GlyphsData td = calc_glyphs_data(label, &gui.fonts.base, scratch);

    LayoutRect layout = split_rect({
        td.bounds.x+gui.style.slider.width+4,
        MAX(gui.style.slider.height, td.bounds.y)+4,
        .margin = gui.style.margin,
        .rflags = SPLIT_COL,
    });

    GUI_LAYOUT(layout) {
        Rect text = split_col({ td.bounds.x + 4 });
        Vector2 pos{ text.tl.x, calc_center(text.tl.y, text.br.y, td.font->line_height) };
        gui_draw_text(td, pos, gui.style.fg);
        action = gui_slider_id(id, value, min, max, step, split_col({ .margin = gui.style.margin }));
    }

    return action;
}

GuiAction gui_slider_id(
    GuiId id,
    f32 *value,
    f32 min, f32 max, f32 step,
    Rect rect) EXPORT
{
    gui_push_id(id);
    defer { gui_pop_id(); };

    GuiAction action = GUI_NONE;
    Rect border = shrink_rect(&rect, gui.style.slider.border);

    f32 factor = (*value-min)/(max-min);
    f32 value_x = factor*rect.size().x;

    Rect handle_rect{
        { rect.tl.x + value_x - gui.style.slider.handle, rect.tl.y },
        { rect.tl.x + value_x + gui.style.slider.handle, rect.br.y }
    };

    Vector3 border_col = gui.style.bg_light1;
    Vector3 bg_col = gui.style.bg;
    Vector3 bg_fill = gui.style.accent_bg;

    bool was_dragging = (gui.dragging == id);
    gui_hot_rect(id, handle_rect);
    if (gui_drag(id, { *value, 0 }) && gui.mouse.dx != 0) {
        f32 dx = gui.mouse.x - gui.drag_start_mouse.x;
        f32 v = gui.drag_start_data[0].x + (max-min)/rect.size().x*dx;
        v = CLAMP(v, min, max);
        v = round_to(v, step);

        if (v != *value) {
            *value = v;
            action = GUI_CHANGE;
        }
    } else if (was_dragging) {
        action = GUI_END;
    }


    gui_draw_rect(border, border_col);
    if (value_x > 0) gui_draw_rect(split_left(&rect, value_x), bg_fill);
    gui_draw_rect(rect, bg_col);
    gui_draw_button(id, handle_rect);
    return action;
}

GuiAction gui_slider_id(
    GuiId id,
    f32 *value,
    f32 min, f32 max, f32 step) EXPORT
{
    Rect rect = split_row({ gui.style.slider.height });
    return gui_slider_id(id, value, min, max, step, rect);
}

String gui_editbox_str() EXPORT  { return { gui.edit.buffer, gui.edit.length }; }


GuiAction gui_editbox_id(GuiId id, String initial_string, Rect rect) EXPORT
{
    SArena scratch = tl_scratch_arena();
    GuiAction action = GUI_NONE;

    bool was_focused = gui.focused == id;
    bool was_pressed = gui.pressed == id;

    gui_handle_focus_grabbing(id);

    gui_hot_rect(id, rect);
    if (gui_pressed(id)) gui_focus(id);
    else if (get_input_mouse(MB_PRIMARY) &&
             gui.hot != id && gui.focused == id)
    {
        clear_focus();
        action = GUI_CANCEL;
    }

    if (gui.focused == id && !was_focused) {
        action = GUI_BEGIN;

        ASSERT(initial_string.length < (i32)sizeof gui.edit.buffer);
        memcpy(gui.edit.buffer, initial_string.data, initial_string.length);
        gui.edit.length = initial_string.length;
        gui.edit.offset = gui.edit.cursor = gui.edit.selection = 0;

        gui.edit.cursor = gui.edit.length;
        gui.edit.selection = 0;
    } else if (gui.focused != id && was_focused) {
        action = GUI_CANCEL;
    }

    if (gui_input_layer(id, gui.input.editbox)) {
        if (get_input_edge(CURSOR_LEFT, gui.input.editbox)) {
            gui.edit.cursor = utf8_decr({ gui.edit.buffer, gui.edit.length }, gui.edit.cursor);
            gui.edit.cursor = MAX(0, gui.edit.cursor);
            gui.edit.selection = gui.edit.cursor;
        }

        if (get_input_edge(CURSOR_RIGHT, gui.input.editbox)) {
            gui.edit.cursor = utf8_incr({ gui.edit.buffer, gui.edit.length }, gui.edit.cursor);
            gui.edit.selection = gui.edit.cursor;
        }

        if (get_input_edge(SELECT_ALL, gui.input.editbox)) {
            gui.edit.selection = 0;
            gui.edit.cursor = gui.edit.length;
        }

        if (get_input_edge(COPY, gui.input.editbox)) {
            i32 start = MIN(gui.edit.cursor, gui.edit.selection);
            i32 end = MAX(gui.edit.cursor, gui.edit.selection);

            String selected = slice({ gui.edit.buffer, gui.edit.length }, start, end);
            set_clipboard_data(selected);
        }

        if (get_input_edge(PASTE, gui.input.editbox)) {
            i32 start = MIN(gui.edit.cursor, gui.edit.selection);
            i32 end = MAX(gui.edit.cursor, gui.edit.selection);

            i32 limit = sizeof gui.edit.buffer - (gui.edit.length - (end-start));

            String str = read_clipboard_str(scratch);
            str.length = utf8_truncate(str, limit);

            if (str.length > (end-start)) {
                memmove(
                    gui.edit.buffer+end+str.length-(end-start),
                    gui.edit.buffer+end,
                    gui.edit.length-end);
            }

            memcpy(gui.edit.buffer+start, str.data, str.length);

            if (str.length < (end-start)) {
                memmove(
                    gui.edit.buffer+start+str.length,
                    gui.edit.buffer+end,
                    gui.edit.length-end);
            }

            gui.edit.cursor = gui.edit.selection = start+str.length;
            gui.edit.length = gui.edit.length - (end-start) + str.length;
            action = GUI_CHANGE;
        }

        if (get_input_edge(DELETE, gui.input.editbox)) {
            if (gui.edit.cursor != gui.edit.selection || gui.edit.cursor < gui.edit.length) {
                i32 start = MIN(gui.edit.cursor, gui.edit.selection);
                i32 end = MAX(gui.edit.cursor, gui.edit.selection);
                if (start == end) end = utf8_incr({ gui.edit.buffer, gui.edit.length }, end);

                i32 new_length = gui.edit.length-(end-start);
                memmove(gui.edit.buffer+start, gui.edit.buffer+end, new_length-start);
                gui.edit.length = new_length;
                gui.edit.cursor = gui.edit.selection = start;

                i32 new_offset = codepoint_index_from_byte_index({ gui.edit.buffer, gui.edit.length}, gui.edit.cursor);
                gui.edit.offset = MIN(gui.edit.offset, new_offset);
                action = GUI_CHANGE;
            }
        }

        if (get_input_edge(DELETE_BACK, gui.input.editbox)) {
            if (gui.edit.cursor != gui.edit.selection || gui.edit.cursor > 0) {
                i32 start = MIN(gui.edit.cursor, gui.edit.selection);
                i32 end = MAX(gui.edit.cursor, gui.edit.selection);
                if (start == end) start = utf8_decr({ gui.edit.buffer, gui.edit.length }, start);

                memmove(gui.edit.buffer+start, gui.edit.buffer+end, gui.edit.length-(end-start));
                gui.edit.length = gui.edit.length-(end-start);
                gui.edit.cursor = gui.edit.selection = start;
                gui.edit.offset = MIN(gui.edit.offset, codepoint_index_from_byte_index({ gui.edit.buffer, gui.edit.length}, gui.edit.cursor));
                action = GUI_CHANGE;
            }
        }

        if (get_input_edge(CONFIRM, gui.input.editbox)) {
            action = GUI_END;
            gui_focus(gui.focused_window);
        }

        // NOTE(jesper): bit of a special case; we want to confirm the editbox value and focus the next widget in line. I'm not sure if there are other types where I want it to behave this way. If there is, should probably make this work cleaner in the input/focus grab handling
        if (get_input_edge(CONFIRM_CONT, gui.input.editbox)) {
            action = GUI_END;
            gui_focus(GUI_ID_INVALID);
        }

        if (get_input_edge(CANCEL, gui.input.editbox)) {
            action = GUI_CANCEL;
            clear_focus();
        }

        for (TextEvent text; get_input_text(TEXT_INPUT, &text, gui.input.editbox);) {
            if (gui.edit.cursor != gui.edit.selection) {
                i32 start = MIN(gui.edit.cursor, gui.edit.selection);
                i32 end = MAX(gui.edit.cursor, gui.edit.selection);
                if (start == end) end = utf8_incr({ gui.edit.buffer, gui.edit.length }, end);

                memmove(gui.edit.buffer+start, gui.edit.buffer+end, gui.edit.length-start);
                gui.edit.length -= end-start;
                gui.edit.cursor = gui.edit.selection = start;

                i32 new_offset = codepoint_index_from_byte_index({ gui.edit.buffer, gui.edit.length }, gui.edit.cursor);
                gui.edit.offset = MIN(gui.edit.offset, new_offset);
                action = GUI_CHANGE;
            }

            if (gui.edit.length+text.length <= (i32)sizeof gui.edit.buffer) {
                for (i32 i = gui.edit.length + text.length-1; i > gui.edit.cursor; i--) {
                    gui.edit.buffer[i] = gui.edit.buffer[i-text.length];
                }

                memcpy(gui.edit.buffer+gui.edit.cursor, text.c, text.length);
                gui.edit.length += text.length;
                gui.edit.cursor += text.length;
                gui.edit.selection = gui.edit.cursor;
                action = GUI_CHANGE;
            }

            if (gui.edit.length == sizeof gui.edit.buffer) break;
        }
    }

    String str = gui.focused == id ? String{ gui.edit.buffer, gui.edit.length } : initial_string;
    Array<GlyphRect> glyphs = calc_glyph_rects(str, &gui.fonts.base, scratch);

    Rect border = shrink_rect(&rect, gui.style.edit.border);
    Rect text_clip_rect = rect;
    Rect text_rect = rect; shrink_rect(&text_rect, gui.style.edit.margin);

    Vector2 text_pos{ text_rect.tl.x, calc_center(text_rect.tl.y, text_rect.br.y, gui.fonts.base.line_height) };

    f32 text_pan = 0;

    Vector3 selection_bg = gui.style.bg_light0;

    Vector3 bg = gui.style.bg_dark1;
    Vector3 border_col = gui.focused == id ? gui.style.accent_bg : gui.style.bg_light1;
    Vector3 fg = gui.style.fg;

    gui_draw_rect(border, border_col);
    gui_draw_rect(rect, bg);

    f32 cursor_pos = rect.tl.x;
    if (glyphs.count > 0) {
        i32 offset = gui.focused == id ? gui.edit.offset : 0;

        if (gui.focused == id) {
            text_pan = gui.edit.offset < glyphs.count ?
                -glyphs[gui.edit.offset].x0 :
                -glyphs[gui.edit.offset-1].x1;

            // TODO(jesper): this is incomplete. I need to figure out a good way to determine the difference between widget being pressed as a result of focusing on it, vs being pressed as a result of already being focused and wanting to drag the cursor around. The current get_mouse_input (used by gui_pressed with friends) only check current state of mouse, there's no consumption of decrementing like there are for other edges, and I haven't figured out how I want to handle these things just yet
            if (was_focused && was_pressed && gui.pressed == id) {
                i32 new_cursor;
                for (new_cursor = 0; new_cursor < glyphs.count; new_cursor++) {
                    GlyphRect g = glyphs.data[new_cursor];
                    Vector2 mouse = gui_mouse();
                    Vector2 rel = mouse - rect.tl;
                    if (rel.x < g.x0+text_pan || (rel.x >= g.x0+text_pan && rel.x <= g.x1+text_pan)) {
                        if (rel.x >= (g.x0 + g.x1)/2 + text_pan) new_cursor++;
                        break;
                    }
                }

                gui.edit.cursor = byte_index_from_codepoint_index(str, new_cursor);
                gui.edit.selection = gui.edit.cursor;
            }

            i32 cursor_ci = codepoint_index_from_byte_index(str, gui.edit.cursor);
            cursor_pos += cursor_ci < glyphs.count ? glyphs[cursor_ci].x0 : glyphs[cursor_ci-1].x1;
            while ((cursor_pos + text_pan) > text_clip_rect.br.x) {
                gui.edit.offset++;
                text_pan = gui.edit.offset < glyphs.count ? -glyphs[gui.edit.offset].x0 : 0.0f;
            }

            while ((cursor_pos + text_pan < text_clip_rect.tl.x)) {
                gui.edit.offset--;
                text_pan = gui.edit.offset < glyphs.count ? -glyphs[gui.edit.offset].x0 : 0.0f;
            }

            if (gui.edit.selection != gui.edit.cursor) {
                f32 selection_pos = rect.tl.x;
                i32 selection_ci = codepoint_index_from_byte_index(str, gui.edit.selection);
                selection_pos += selection_ci < glyphs.count ? glyphs[selection_ci].x0 : glyphs[selection_ci-1].x1;

                f32 x0 = MIN(cursor_pos, selection_pos);
                f32 x1 = MAX(cursor_pos, selection_pos);

                gui_draw_rect(
                    Vector2{ x0+text_pan, rect.tl.y },
                    Vector2{ x1-x0, rect.size().y },
                    selection_bg);
            }
        }

        if (apply_clip_rect(&text_clip_rect, array_tail(gui.clip_stack))) {
            GlyphsData subset{
                .font = &gui.fonts.base,
                .glyphs = slice(glyphs, offset),
            };
            gui_push_clip_rect(text_clip_rect);
            gui_draw_text(subset, text_pos + Vector2{ text_pan, text_pan }, fg);
            gui_pop_clip_rect();
        }
    }


    if (gui.focused == id) {
        gui_draw_rect(
            { cursor_pos+text_pan, rect.tl.y },
            Vector2{ 1.0f, rect.size().y },
            fg);
    }

    return action;
}

GuiAction gui_editbox_id(GuiId id, String initial_string) EXPORT
{
    f32 height = gui.fonts.base.line_height + gui.style.edit.padding.y;
    Rect rect = split_rect({ 20, height, .margin = gui.style.margin });
    return gui_editbox_id(id, initial_string, rect);
}


GuiAction gui_editbox_id(GuiId id, String initial_string, f32 width) EXPORT
{
    f32 height = gui.fonts.base.line_height + gui.style.edit.padding.y;
    Rect rect = split_rect({ MAX(width, 20), height, .margin = gui.style.margin });
    return gui_editbox_id(id, initial_string, rect);
}

GuiAction gui_editbox_id(GuiId id, f32 *value) EXPORT
{
    char buffer[100];

    String str = gui.focused == id ?
        String{ gui.edit.buffer, gui.edit.length } :
        stringf(buffer, sizeof buffer, "%.3f", *value);

    GuiAction action = gui_editbox_id(id, str);
    if (action == GUI_END) f32_from_string(gui_editbox_str(), value);
    return action;
}

GuiAction gui_editbox_id(GuiId id, i32 *value) EXPORT
{
    char buffer[100];

    String str = gui.focused == id ?
        String{ gui.edit.buffer, gui.edit.length } :
        stringf(buffer, sizeof buffer, "%d", *value);

    GuiAction action = gui_editbox_id(id, str);
    if (action == GUI_END) i32_from_string(gui_editbox_str(), value);
    return action;
}

GuiAction gui_editbox_id(GuiId id, u64 *value) EXPORT
{
    char buffer[100];

    String str = gui.focused == id ?
        String{ gui.edit.buffer, gui.edit.length } :
        stringf(buffer, sizeof buffer, "%llu", *value);

    GuiAction action = gui_editbox_id(id, str);
    if (action == GUI_END) u64_from_string(gui_editbox_str(), value);
    return action;
}


GuiAction gui_editbox_id(GuiId id, f32 *value, Rect rect) EXPORT
{
    char buffer[100];

    String str = gui.focused == id ?
        String{ gui.edit.buffer, gui.edit.length } :
        stringf(buffer, sizeof buffer, "%.3f", *value);

    GuiAction action = gui_editbox_id(id, str, rect);
    if (action == GUI_END) f32_from_string(gui_editbox_str(), value);

    return action;
}

GuiAction gui_editbox_id(GuiId id, i32 *value, Rect rect) EXPORT
{
    char buffer[100];

    String str = gui.focused == id ?
        String{ gui.edit.buffer, gui.edit.length } :
        stringf(buffer, sizeof buffer, "%d", *value);

    GuiAction action = gui_editbox_id(id, str, rect);
    if (action == GUI_END) i32_from_string(gui_editbox_str(), value);

    return action;
}

GuiAction gui_editbox_id(GuiId id, u64 *value, Rect rect) EXPORT
{
    char buffer[100];

    String str = gui.focused == id ?
        String{ gui.edit.buffer, gui.edit.length } :
        stringf(buffer, sizeof buffer, "%llu", *value);

    GuiAction action = gui_editbox_id(id, str, rect);
    if (action == GUI_END) u64_from_string(gui_editbox_str(), value);

    return action;
}

i32 gui_find_or_create_window_index(GuiId id, GuiWindowDesc desc) INTERNAL
{
    for (auto it : iterator(gui.windows)) if (it->id == id) return it.index;

    Rect rect = *gui_current_layout();

    GuiWindow wnd{
        .id = id,
        .command_buffer.commands.alloc = mem_frame,
        .pos = rect.tl + desc.position - desc.anchor*desc.size + desc.parent_anchor*rect.size(),
        .size = desc.size,
        .flags = desc.flags,
        .state = { .hidden = false },
    };
    return array_add(&gui.windows, wnd);
}

GuiId gui_create_window_id(GuiId id, GuiWindowDesc desc) EXPORT
{
    for (auto it : iterator(gui.windows)) {
        if (it->id == id) {
            LOG_ERROR("window with id %u already exists", id);
            return id;
        }
    }

    Rect rect = *gui_current_layout();

    GuiWindow wnd{
        .id = id,
        .command_buffer.commands.alloc = mem_frame,
        .title = duplicate_string(desc.title, mem_dynamic),
        .pos = rect.tl + desc.position - desc.anchor*desc.size + desc.parent_anchor*rect.size(),
        .size = desc.size,
        .flags = desc.flags,
    };

    array_add(&gui.windows, wnd);
    return id;
}

i32 gui_find_window_index(GuiId id) EXPORT
{
    for (auto it : iterator(gui.windows)) if (it->id == id) return it.index;
    return -1;
}

GuiWindow* gui_find_window(GuiId id) EXPORT
{
    for (auto &it : gui.windows) if (it.id == id) return &it;
    return nullptr;
}

void gui_window_toggle(GuiId id) EXPORT
{
    GuiWindow *wnd = gui_find_window(id);
    if (wnd) wnd->state.hidden = !wnd->state.hidden;
}

bool gui_begin_window_id(
    GuiId id,
    GuiWindowDesc desc,
    bool *visible) EXPORT
{
    desc.flags &= ~GUI_NO_CLOSE;
    i32 index = gui_find_or_create_window_index(id, desc);
    gui.windows[index].state.hidden = !(*visible);

    if (*visible) *visible = gui_begin_window_internal(id, desc.title, index);
    return *visible;
}

bool gui_begin_window_id(
    GuiId id,
    GuiWindowDesc desc,
    bool visible) EXPORT
{
    i32 index = gui_find_or_create_window_index(id, desc);
    gui.windows[index].state.hidden = !visible;

    if (visible) visible = gui_begin_window_internal(id, desc.title, index);
    return visible;
}

bool gui_begin_window_id(
    GuiId id,
    GuiWindowDesc desc) EXPORT
{
    return gui_begin_window_internal(id, desc.title, gui_find_or_create_window_index(id, desc));
}

bool gui_begin_window_id(GuiId id, String title) EXPORT
{
    return gui_begin_window_internal(id, title, gui_find_window_index(id));
}

bool gui_begin_window_id(GuiId id) EXPORT
{
    i32 index = gui_find_window_index(id);
    if (index == -1) {
        LOG_ERROR("window with id %u does not exist", id);
        return false;
    }

    GuiWindow *wnd = &gui.windows[index];
    return gui_begin_window_internal(id, wnd->title, index);
}

bool gui_begin_window_internal(
    GuiId id,
    String title,
    i32 wnd_index) INTERNAL
{
    GuiWindow *wnd = &gui.windows[wnd_index];
    if (wnd->state.hidden) return false;

    ASSERT(gui.current_window == GUI_BACKGROUND);
    gui.current_window = wnd_index;

    wnd->state.active = true;
    if (!wnd->state.was_active) {
        wnd = push_window_to_top(gui.current_window);
    } else if (gui.focused_window != id &&
               gui.hot_window == id &&
               get_input_mouse(MB_PRIMARY))
    {
        wnd = push_window_to_top(gui.current_window);
    }

    gui_push_id(id);
    gui_push_layout({
        { wnd->pos, wnd->pos+wnd->size },
        LAYOUT_ROOT | EXPAND_XY,
    });
    gui_push_command_buffer();

    defer {
        if (wnd->state.hidden) {
            if (gui.focused == wnd->id) gui_focus(GUI_ID_INVALID);
            if (gui.focused_window == wnd->id) gui_focus_window(GUI_ID_INVALID);

            wnd->command_buffer.commands.count = 0;
            gui_pop_command_buffer(nullptr);
            gui_pop_layout();
            gui_pop_id();

            gui.current_window = GUI_BACKGROUND;
        }
    };

    Rect border = shrink_rect(gui_current_layout(), gui.style.window.border);

    bool do_titlebar = !(wnd->flags & GUI_NO_TITLE);
    LayoutRect titlebar_rect = do_titlebar ? split_top({ gui.style.window.title_height }): LayoutRect{};

    LayoutRect title_rect = titlebar_rect;
    split_left(&title_rect, { 5 });

    f32 close_s = title_rect.size().y;
    Rect close_rect = !(wnd->flags & GUI_NO_CLOSE)
        ? split_right(&title_rect, { close_s, close_s-gui.style.window.margin }).bounds
        : Rect{};

    GuiId title_id = GUI_ID;
    GuiId close_id = GUI_ID;

    // TODO(jesper): alt-drag on any part of the window
    if (!(wnd->flags & GUI_NO_MOVE) && do_titlebar) {
        gui_hot_rect(title_id, titlebar_rect);
        if (gui_drag(title_id, wnd->pos) && (gui.mouse.dx != 0 || gui.mouse.dy != 0)) {
            Vector2 mouse = { (f32)gui.mouse.x, (f32)gui.mouse.y };
            wnd->pos = gui.drag_start_data[0] + mouse - gui.drag_start_mouse;
        }
    }

    if (gui.hot_window != id &&
        gui.focused_window == id &&
        get_input_mouse(MB_PRIMARY))
    {
        gui_focus_window(GUI_ID_INVALID);
    }

    // NOTE(jesper): the window keyboard nav is split between begin/end because children of the window
    // should be given a chance to respond to certain key events first
    // I need to implement something that lets widgets mark events as handled in appropriate hierarchy order
    if (gui.focused_window == id) {
        push_input_layer(gui.input.window);
        if (get_input_edge(WINDOW_CLEAR, gui.input.window))
            gui_focus_window(GUI_ID_INVALID);
        if (get_input_edge(WINDOW_CLOSE, gui.input.window)) {
            wnd->state.hidden = true;
            return false;
        }
    }

    Vector3 title_bg = gui.focused_window != id
        ? gui.style.bg_dark2
        : gui.hot == title_id ? gui.style.accent_bg_hot : gui.style.accent_bg;
    Vector3 border_bg = gui.focused_window != id ? gui.style.bg_dark3 : gui.style.accent_bg;

    gui_draw_rect(border, border_bg);
    gui_draw_rect(titlebar_rect, title_bg);
    gui_draw_rect(*gui_current_layout(), gui.style.bg_dark0);

    {
        gui_push_clip_rect(title_rect);
        gui_draw_text(title, title_rect.rem.tl, gui.style.window.title_fg, &gui.fonts.base);
        gui_pop_clip_rect();
    }

    if (!(wnd->flags & GUI_NO_CLOSE) && do_titlebar) {
        if (gui_clicked(close_id, close_rect)) {
            wnd->state.hidden = true;
            return false;
        }

        if (gui.hot == close_id) gui_draw_rect(close_rect, gui.style.window.close_bg_hot);
        gui_draw_icon(gui.icons.close, close_rect);
    }

    if (!(wnd->flags & GUI_NO_VOVERFLOW_SCROLL) && wnd->state.has_voverflow) split_right({ gui.style.scrollbar.thickness });
    if (!(wnd->flags & GUI_NO_HOVERFLOW_SCROLL) && wnd->state.has_hoverflow) split_bottom({ gui.style.scrollbar.thickness });

    shrink_rect(gui_current_layout(), gui.style.window.margin);
    wnd->client_rect = *gui_current_layout();
    gui_push_clip_rect(wnd->client_rect);

    if (!(wnd->flags & GUI_NO_VOVERFLOW_SCROLL) && wnd->state.has_voverflow) {
        gui_current_layout()->rem.tl.y -= wnd->scroll_offset.y;
        gui_current_layout()->rem.br.y -= wnd->scroll_offset.y;
    }

    return !wnd->state.hidden;
}

void gui_end_window() EXPORT
{
    ASSERT(gui.current_window > GUI_OVERLAY);
    GuiWindow *wnd = gui_current_window();
    ASSERT(!wnd->state.hidden);

    defer {
        gui_pop_command_buffer(&wnd->command_buffer);
        gui_pop_layout();
        gui_pop_id();

        gui.current_window = GUI_BACKGROUND;
        gui_pop_clip_rect();
    };

    // NOTE(jesper): scrollbar space is reserved on-demand, so if there is any overflow we need to expand the client rect to accomodate it
    f32 x_pad = 0, y_pad = 0;
    if (!(wnd->flags & GUI_NO_VOVERFLOW_SCROLL) && wnd->state.has_voverflow) x_pad = gui.style.scrollbar.thickness;
    if (!(wnd->flags & GUI_NO_HOVERFLOW_SCROLL) && wnd->state.has_hoverflow) y_pad = gui.style.scrollbar.thickness;

    // NOTE(jesper): re-create the window client and expand it right to the window border (ignoring margins) in order to position the window widgets right at the border of the window
    Rect client_rect{
        wnd->client_rect.tl,
        wnd->client_rect.br + Vector2{ x_pad, y_pad },
    };
    expand_rect(&client_rect, gui.style.window.margin);

    // NOTE(jesper): the content rect is the window rect minus margin and scrollbars (if there are any), we need to keep this around for scrollbar calculations
    Rect content_rect = wnd->client_rect;
    wnd->client_rect = client_rect;

    Rect vscroll_rect = split_right(&client_rect, gui.style.scrollbar.thickness);
    Rect hscroll_rect = split_bottom(&client_rect, gui.style.scrollbar.thickness);

    if (!(wnd->flags & GUI_NO_RESIZE)) {
        // NOTE(jesper): we handle the window resizing at the end to ensure it's always on
        // top and has the highest input priority in the window.
        GuiId resize_id = GUI_ID;
        f32 size = gui.style.window.resize_thickness;

        // NOTE(jesper): allocate space in the scrollbar widgets for the resize widget so they don't overlap
        split_right(&hscroll_rect, size);
        Vector2 resize_br = split_bottom(&vscroll_rect, size).br;
        Vector2 resize_tr = resize_br + Vector2{ 0, -size };
        Vector2 resize_bl = resize_br + Vector2{ -size, 0 };

        if (point_in_triangle(gui_mouse(), resize_tr, resize_br, resize_bl)) {
            gui_hot(resize_id);
        }

        if (gui.hot == resize_id) push_cursor(MC_SIZE_NW_SE);
        if (gui_drag(resize_id, wnd->size)) {
            Vector2 nsize = gui.drag_start_data[0] + gui_mouse() - gui.drag_start_mouse;
            wnd->size.x = MAX(wnd->min_size.x, nsize.x);
            wnd->size.y = MAX(wnd->min_size.y, nsize.y);
        }

        Vector3 resize_bg = gui.hot == resize_id ? gui.style.bg_light0 : gui.style.bg_light1;
        gfx_draw_triangle(resize_tr, resize_br, resize_bl, resize_bg, array_tail(gui.draw_stack));
    }

    if (!(wnd->flags & GUI_NO_VOVERFLOW_SCROLL)) {
        f32 total_height = gui_current_layout()->rem.br.y+wnd->scroll_offset.y - content_rect.tl.y;
        if (wnd->state.has_voverflow) {
            gui_push_clip_rect(vscroll_rect);

            gui_push_layout({ content_rect, LAYOUT_ROOT });
            gui_push_layout({ vscroll_rect });
            gui_vscrollbar(&wnd->scroll_offset.y, total_height, 10);
            gui_pop_layout();
            gui_pop_layout();

            gui_pop_clip_rect();
        }
        wnd->state.has_voverflow = total_height > content_rect.size().y;
    }

    if ((wnd->flags & (GUI_NO_RESIZE|GUI_NO_HOVERFLOW_SCROLL)) == (GUI_NO_RESIZE|GUI_NO_HOVERFLOW_SCROLL) &&
        gui_current_layout()->flags & EXPAND_X)
    {
        wnd->size.x = gui_current_layout()->rem.tl.x - gui_current_layout()->bounds.tl.x + gui.style.window.margin;
    }

    if ((wnd->flags & (GUI_NO_RESIZE |GUI_NO_VOVERFLOW_SCROLL)) == (GUI_NO_RESIZE|GUI_NO_VOVERFLOW_SCROLL) &&
        gui_current_layout()->flags & EXPAND_Y)
    {
        wnd->size.y = gui_current_layout()->rem.tl.y - gui_current_layout()->bounds.tl.y + gui.style.window.margin;
    }
}

GuiAction gui_2d_gizmo_translate_axis_id(
    GuiId id,
    Vector2 *position,
    Matrix3 ss_from_ws,
    Vector2 axis) EXPORT
{
    GuiAction action = GUI_NONE;

    i32 old_window = gui.current_window;
    gui.current_window = GUI_BACKGROUND;
    defer { gui.current_window = old_window; };

    // TODO(jesper): these wigets need to support a rotated and translated camera
    GfxCommandBuffer *cmdbuf = array_tail(gui.draw_stack);

    Vector2 n{ axis.y, -axis.x };
    Vector2 p = (ss_from_ws*Vector3{ .xy = *position, 1 }).xy;

    // base
    Vector2 size = GIZMO_ARROW_SIZE;
    Vector2 br = p + 0.5f*size.y*n + axis*size.x;
    Vector2 tr = p - 0.5f*size.y*n + axis*size.x;
    Vector2 tl = p - 0.5f*size.y*n;
    Vector2 bl = p + 0.5f*size.y*n;

    // tip
    Vector2 tsize = GIZMO_ARROW_TIP_SIZE;
    // NOTE(jesper): -axis to cause a 1px overlap in base and tip to hackily avoid
    // any wonkyness with hit detection between mouse and the edge of the tip and bar
    Vector2 toffset = -axis;
    Vector2 t0 = p + toffset + axis*size.x - 0.5f*tsize.y*n;
    Vector2 t1 = p + toffset + axis*size.x + axis*tsize.x;
    Vector2 t2 = p + toffset + axis*size.x + 0.5f*tsize.y*n;

    Vector2 mouse = gui_mouse();
    if (point_in_rect(mouse, tl, tr, br, tl) ||
        point_in_triangle(mouse, t0, t1, t2))
    {
        gui_hot(id);
    }

    bool was_dragging = (gui.dragging == id);
    if (gui_drag(id, *position)) {
        if (!was_dragging) {
            action = GUI_BEGIN;
        } else if (gui.mouse.dx != 0 || gui.mouse.dy != 0) {
            action = GUI_CHANGE;
            Matrix3 ws_from_ss = mat3_inverse(ss_from_ws);
            Vector2 d = (ws_from_ss*Vector3{ .xy = (gui_mouse()- gui.drag_start_mouse) }).xy;
            *position = gui.drag_start_data[0] + dot(axis, d) * axis;
        }
    } else if (was_dragging) {
        action = GUI_END;
    }

    Vector3 color{ .rg = abs(axis), ._b = 0 };
    if (gui.hot == id) color = 0.5f*(color + Vector3{ 1.0f, 1.0f, 1.0f });

    gfx_draw_square(tl, tr, br, bl, color, cmdbuf);
    gfx_draw_triangle(t0, t1, t2, color, cmdbuf);

    Vector2 points[] = { tl, tr, t0, t1, t2, br, bl };
    gfx_draw_line_loop(points, ARRAY_COUNT(points), GIZMO_OUTLINE_COLOR, cmdbuf);
    return action;
}

GuiAction gui_2d_gizmo_size_axis_id(
    GuiId id,
    Vector2 position, Vector2 *size,
    Matrix3 ss_from_ws,
    Vector2 axis,
    f32 multiple /*= 0*/) EXPORT
{
    GuiAction action = GUI_NONE;

    i32 old_window = gui.current_window;
    gui.current_window = GUI_BACKGROUND;
    defer { gui.current_window = old_window; };

    // TODO(jesper): these wigets need to support a rotated camera
    GfxCommandBuffer *cmdbuf = array_tail(gui.draw_stack);

    Vector2 n{ axis.y, -axis.x };
    Vector2 p = (ss_from_ws*Vector3{ .xy = position, 1 }).xy;

    // TODO(jesper): this is a bit ugly to do a yflip on the visual representation of the axis, needed because
    // of the y-flipped mouse vs gl-coordinates and the way I do the size calculations
    Vector2 vaxis{ axis.x, -axis.y };

    // base
    Vector2 bsize = GIZMO_ARROW_SIZE;
    Vector2 br = p + 0.5f*bsize.y*n + vaxis*bsize.x;
    Vector2 tr = p - 0.5f*bsize.y*n + vaxis*bsize.x;
    Vector2 tl = p - 0.5f*bsize.y*n;
    Vector2 bl = p + 0.5f*bsize.y*n;

    // tip
    Vector2 tsize = GIZMO_ARROW_TIP_SIZE;
    // NOTE(jesper): -1 axis to cause a 1px overlap in base and tip to not cause
    // any wonkyness with mouse dragging across the arrow
    Vector2 toffset = -vaxis;
    Vector2 t0 = p + toffset + vaxis*bsize.x - 0.5f*tsize.y*n;
    Vector2 t1 = p + toffset + vaxis*bsize.x + 0.5f*tsize.y*n;
    Vector2 t2 = t1 + vaxis*tsize.x;
    Vector2 t3 = t0 + vaxis*tsize.x;

    Vector2 mouse = gui_mouse();
    if (point_in_rect(mouse, tl, tr, br, tl) ||
        point_in_rect(mouse, t0, t1, t2, t3))
    {
        gui_hot(id);
    }

    bool was_dragging = (gui.dragging == id);
    if (gui_drag(id, *size)) {
        if (!was_dragging) action = GUI_BEGIN;
        else if (gui.mouse.dx || gui.mouse.dy) {
            action = GUI_CHANGE;

            Matrix3 ws_from_ss = mat3_inverse(ss_from_ws);
            Vector2 delta = (ws_from_ss*Vector3{ .xy = axis*dot(mouse, axis) - axis*dot(gui.drag_start_mouse, axis) }).xy;
            delta.y = -delta.y;
            if (multiple) delta = round_to(delta, multiple);
            *size = gui.drag_start_data[0] + delta;

            size->x = MAX(size->x, 0);
            size->y = MAX(size->y, 0);
        }
    } else if (was_dragging) {
        action = gui.drag_start_data[0] != *size ? GUI_END : GUI_CANCEL;
    }

    Vector3 color{ .rg = abs(axis), ._b = 0 };
    if (gui.hot == id) color = 0.5f*(color + Vector3{ 1.0f, 1.0f, 1.0f });

    gfx_draw_square(tl, tr, br, bl, color, cmdbuf);
    gfx_draw_square(t0, t1, t2, t3, color, cmdbuf);

    Vector2 points[] = { tl, tr, t0, t3, t2, t1, br, bl };
    gfx_draw_line_loop(points, ARRAY_COUNT(points), GIZMO_OUTLINE_COLOR, cmdbuf);

    return action;
}

GuiAction gui_2d_gizmo_translate_plane_id(
    GuiId id,
    Vector2 *position,
    Matrix3 ss_from_ws) EXPORT
{
    // TODO(jesper): these wigets need to support a rotated camera
    GuiAction action = GUI_NONE;

    i32 old_window = gui.current_window;
    gui.current_window = GUI_BACKGROUND;
    defer { gui.current_window = old_window; };

    GfxCommandBuffer *cmdbuf = array_tail(gui.draw_stack);

    Vector2 size = { 20, 20 };
    Vector2 p = (ss_from_ws*Vector3{ .xy = *position, 1 }).xy;

    Vector2 offset = { 10, -10 };
    Vector2 br = p + offset + Vector2{ size.x, 0.0f };
    Vector2 tr = p + offset + Vector2{ size.x, -size.y };
    Vector2 tl = p + offset + Vector2{ 0.0f, -size.y };
    Vector2 bl = p + offset;

    Vector2 mouse = gui_mouse();
    if (point_in_rect(mouse, tl, tr, br, bl)) {
        gui_hot(id);
    }

    bool was_dragging = (gui.dragging == id);
    if (gui_drag(id, *position)) {
        if (!was_dragging) action = GUI_BEGIN;
        else if (gui.mouse.dx != 0 || gui.mouse.dy != 0) {
            action = GUI_CHANGE;
            Matrix3 ws_from_ss = mat3_inverse(ss_from_ws);
            Vector2 d = (ws_from_ss*Vector3{ .xy = mouse - gui.drag_start_mouse }).xy;
            *position = gui.drag_start_data[0] + d;
        }
    } else if (was_dragging) {
        action = GUI_END;
    }

    Vector3 color{ 0.5f, 0.5f, 0.0f };
    if (gui.hot == id) color = { 1.0f, 1.0f, 0.0f };

    Vector2 points[] = { tl, tr, br, bl };
    gfx_draw_square(tl, tr, br, bl, color, cmdbuf);
    gfx_draw_line_loop(points, ARRAY_COUNT(points), GIZMO_OUTLINE_COLOR, cmdbuf);
    return action;
}

GuiAction gui_2d_gizmo_translate_id(
    GuiId id,
    Vector2 *position,
    Matrix3 ss_from_ws) EXPORT
{
    gui_push_id(id);
    defer { gui_pop_id(); };

    GuiAction a0 = gui_2d_gizmo_translate_axis(position, ss_from_ws, { 1.0f, 0.0f });
    GuiAction a1 = gui_2d_gizmo_translate_axis(position, ss_from_ws, { 0.0f, -1.0f });
    GuiAction a2 = gui_2d_gizmo_translate_plane(position, ss_from_ws);
    return (GuiAction)MAX(a0, MAX(a1, a2));
}

GuiAction gui_2d_gizmo_translate_id(
    GuiId id,
    Vector2 *position,
    Matrix3 ss_from_ws,
    f32 multiple) EXPORT
{
    gui_push_id(id);
    defer { gui_pop_id(); };

    GuiAction a0 = gui_2d_gizmo_translate_axis(position, ss_from_ws, { 1.0f, 0.0f });
    GuiAction a1 = gui_2d_gizmo_translate_axis(position, ss_from_ws, { 0.0f, -1.0f });
    GuiAction a2 = gui_2d_gizmo_translate_plane(position, ss_from_ws);

    position->x = round_to(position->x, multiple);
    position->y = round_to(position->y, multiple);

    return (GuiAction)MAX(a0, MAX(a1, a2));
}


GuiAction gui_2d_gizmo_size_id(
    GuiId id,
    Vector2 position, Vector2 *size,
    Matrix3 ss_from_ws) EXPORT
{
    gui_push_id(id);
    defer { gui_pop_id(); };

    GuiAction a0 = gui_2d_gizmo_size_axis(position, size, ss_from_ws, { 1.0f, 0.0f });
    GuiAction a1 = gui_2d_gizmo_size_axis(position, size, ss_from_ws, { 0.0f, 1.0f });
    return (GuiAction)MAX(a0, a1);
}

GuiAction gui_2d_gizmo_size_id(
    GuiId id,
    Vector2 position, Vector2 *size,
    Matrix3 ss_from_ws,
    f32 multiple) EXPORT
{
    gui_push_id(id);
    defer { gui_pop_id(); };

    GuiAction a0 = gui_2d_gizmo_size_axis(position, size, ss_from_ws, { 1.0f, 0.0f }, multiple);
    GuiAction a1 = gui_2d_gizmo_size_axis(position, size, ss_from_ws, { 0.0f, 1.0f }, multiple);
    return (GuiAction)MAX(a0, a1);
}

GuiAction gui_2d_gizmo_size_square_id(
    GuiId id,
    Vector2 *center, Vector2 *size,
    Matrix3 ss_from_ws,
    f32 multiple /*= 0*/) EXPORT
{
    gui_push_id(id);
    defer { gui_pop_id(); };

    GuiAction action = GUI_NONE;

    i32 old_window = gui.current_window;
    gui.current_window = GUI_BACKGROUND;
    defer { gui.current_window = old_window; };

    GfxCommandBuffer *cmdbuf = array_tail(gui.draw_stack);

    Vector2 rsize = multiple ? round_to(*size, multiple) : *size;
    Vector2 rcenter = multiple ? round_to(*center, multiple) : *center;

    Vector2 p = (ss_from_ws*Vector3{ .xy = rcenter, 1 }).xy;
    Vector2 half_size = 0.5f * (ss_from_ws*Vector3{ .xy = rsize }).xy;
    Vector2 tl = p - half_size;
    Vector2 br = p + half_size;
    Vector2 tr{ br.x, tl.y };
    Vector2 bl{ tl.x, br.y };

    Vector2 mouse = gui_mouse();
    Vector2 corners[] = { tl, br, tr, bl };

    GuiId corner_ids[] = {
        GUI_ID,
        GUI_ID,
        GUI_ID,
        GUI_ID,
    };


    Vector2 bsize = GIZMO_CORNER_SIZE;
    for (i32 i = 0; i < ARRAY_COUNT(corners); i++) {
        GuiId id = corner_ids[i];

        if (point_in_rect(mouse, corners[i], bsize)) {
            gui_hot(id);
        }

        bool was_dragging = (gui.dragging == id);
        if (gui_drag(id, rsize, rcenter)) {
            if (!was_dragging) action = GUI_BEGIN;
            else if (gui.mouse.dx || gui.mouse.dy) action = GUI_CHANGE;
        } else if (was_dragging) {
            action = gui.drag_start_data[0] != *size || gui.drag_start_data[1] != *center ? GUI_END : GUI_CANCEL;
        }

        Vector3 color{ 0.5f, 0.5f, 0.0f };
        if (gui.hot == id) color = { 1.0f, 1.0f, 0.0f };

        gfx_draw_square(corners[i], bsize, Vector4{ .rgb = color, ._a = 1.0f }, cmdbuf);
        gfx_draw_line_square(corners[i], bsize, GIZMO_OUTLINE_COLOR, cmdbuf);
    }

    if (action == GUI_CHANGE) {
        Matrix3 ws_from_ss = mat3_inverse(ss_from_ws);
        Vector2 delta = (ws_from_ss*Vector3{ .xy = mouse - gui.drag_start_mouse }).xy;
        if (multiple) delta = round_to(delta, multiple);

        if (gui.pressed == corner_ids[1] || gui.pressed == corner_ids[2]) {
            size->x = gui.drag_start_data[0].x + delta.x;
            center->x = gui.drag_start_data[1].x - 0.5f*gui.drag_start_data[0].x + 0.5f*size->x;
        } else {
            size->x = gui.drag_start_data[0].x - delta.x;
            center->x = gui.drag_start_data[1].x + 0.5f*gui.drag_start_data[0].x - 0.5f*size->x;
        }

        if (gui.pressed == corner_ids[0] || gui.pressed == corner_ids[2]) {
            size->y = gui.drag_start_data[0].y - delta.y;
            center->y = gui.drag_start_data[1].y + 0.5f*gui.drag_start_data[0].y - 0.5f*size->y;
        } else {
            size->y = gui.drag_start_data[0].y + delta.y;
            center->y = gui.drag_start_data[1].y - 0.5f*gui.drag_start_data[0].y + 0.5f*size->y;
        }
    }

    GuiAction a0 = gui_2d_gizmo_size_axis(*center, size, ss_from_ws, { 1.0f, 0.0f }, multiple);
    GuiAction a1 = gui_2d_gizmo_size_axis(*center, size, ss_from_ws, { 0.0f, 1.0f }, multiple);
    action = MAX(action, MAX(a0, a1));

    return action;
}

i32 gui_dropdown_id(GuiId id, Array<String> labels, i32 current_index) EXPORT
{
    SArena scratch = tl_scratch_arena();

    FontAtlas *font = &gui.fonts.base;
    GlyphsData td = calc_glyphs_data(labels[current_index >= 0 ? current_index : 0], font, scratch);
    Vector2 total_size = { td.bounds.x+20, font->line_height+6 };
    Rect rect = split_rect({ total_size.x, .margin = gui.style.margin });
    return gui_dropdown_id(id, labels, current_index, rect);
}

i32 gui_dropdown_id(GuiId id, Array<String> labels, i32 current_index, Rect rect) EXPORT
{
    SArena scratch = tl_scratch_arena();
    FontAtlas *font = &gui.fonts.base;
    GlyphsData td = calc_glyphs_data(labels[current_index >= 0 ? current_index : 0], font, scratch);

    shrink_rect(&rect, 1);
    Rect icon_r = split_right(rect, rect.size().y);
    Rect expand = shrink_rect(&icon_r, 3);
    shrink_rect(&expand, 1);

    GuiMenu *menu = gui_find_or_create_menu(id, rect.size(), { 0, 100 });

    gui_handle_focus_grabbing(id);
    if (gui_input_layer(id, gui.input.dropdown)) {
        if (get_input_edge(TOGGLE, gui.input.dropdown)) menu->active = !menu->active;
        if (get_input_edge(CANCEL, gui.input.dropdown)) {
            menu->active = false;
            clear_focus();
        }
    }

    gui_begin_menu_id(id, menu, rect, 0u);
    if (menu->active) gui_focus(id);

    Vector3 border_col = gui.focused == id ? gui.style.accent_bg : gui.style.bg_light1;
    gui_draw_rect(expand.tl, { 1, expand.size().y }, border_col);
    gui_draw_icon(gui.icons.down, icon_r);

    String filter = "";
    i32 selected_index = current_index;

    bool close_menu = false;
    if (!menu->active) {
        Vector2 text_p{
            rect.tl.x+gui.style.dropdown.margin,
            calc_center(rect.tl.y, rect.br.y, font->line_height)
        };

        if (current_index >= 0) gui_draw_text(td, text_p, gui.style.fg);
    } else {
        String curr = current_index >= 0 ? labels[current_index] : "";
        auto action = gui_editbox_id(id, curr, rect);

        filter = gui_editbox_str();
        if (action == GUI_CHANGE) current_index = -1;
        else if (action == GUI_CANCEL) close_menu = true;
    }

    menu->parent_wnd = gui_push_overlay({ menu->pos, menu->pos+menu->size }, menu->active );

    // TODO(jesper): pretty hacky, having to deal with a lot of weird push/pop state for menus right now that I need to investigate and clean up. id-stack, clip-rect stack, draw-stack, overlay, layout. Some happening early in gui_begin_menu, some happening in push_overlay, all of them closed in gui_end_menu which assumes they all were opened and not cancelled in between
    if (close_menu) {
        menu->active = false;
        gui_end_menu();
    }

    const f32 item_height = font->line_height+4;

    // NOTE(jesper): this is starting to look a lot like a lister. TODO?
    if (menu->active) {
        GuiScrollArea *scroll_area = nullptr;
        LayoutRect vscroll_r{};
        Rect view_r = { menu->pos, menu->pos+menu->size };

        if (menu->has_overflow_y) {
            scroll_area = map_find_emplace(&gui.scroll_areas, id);
            vscroll_r = split_right({ gui.style.scrollbar.thickness });
            gui_current_layout()->rem.tl.y -= scroll_area->offset.y;
        }

        for (auto it : iterator(labels)) {
            SArena scratch = tl_scratch_arena();
            GlyphsData td = calc_glyphs_data(it, &gui.fonts.base, scratch);

            if (filter && !string_contains(it, filter)) continue;
            if (current_index == -1) current_index = it.index;

            GuiId row_id = gui_gen_id(it.index);
            Rect row = split_row({item_height});

            if (gui_clicked(row_id, row)) {
                selected_index = it.index;
                menu->active = false;
                gui_focus(GUI_ID_INVALID);
                break;
            }

            gui_draw_button(row_id, row);

            Vector2 text_p{ row.tl.x+4, calc_center(row.tl.y, row.br.y, td.font->line_height) };
            gui_draw_text(td, text_p, gui.style.fg);
        }

        if (menu->has_overflow_y) {
            f32 total_height = gui_current_layout()->rem.br.y+scroll_area->offset.y - view_r.tl.y;
            gui_vscrollbar_id(id, &scroll_area->offset.y, total_height, item_height, vscroll_r, view_r);
        }

        gui_end_menu();
    }

    return selected_index;
}

void gui_push_clip_rect(Rect rect) INTERNAL
{
    array_add(&gui.clip_stack, rect);
}

void gui_pop_clip_rect() INTERNAL
{
    ASSERT(gui.clip_stack.count > 0);
    gui.clip_stack.count--;
}

GuiId gui_push_id(GuiId id) EXPORT
{
    array_add(&gui.id_stack, id);
    return id;
}

GuiId gui_gen_id(u32 src) EXPORT
{
    ASSERT(gui.id_stack.count > 0);
    GuiId *seed = array_tail(gui.id_stack);
    return hash32(&src, sizeof src, *seed);
}

GuiId gui_pop_id() EXPORT
{
    ASSERT(gui.id_stack.count > 1);
    return gui.id_stack.data[--gui.id_stack.count];
}

bool gui_begin_menu_id(GuiId id) EXPORT
{
    gui_push_id(id);


    LayoutRect layout = split_rect({
        0, gui.fonts.base.line_height+10,
        .flags = EXPAND_X,
        .rflags = LAYOUT_ROOT|EXPAND_X|SPLIT_COL
    });

    GuiMenu *menu = gui_find_or_create_menu(id, layout.size());
    menu->active = true;

    gui_push_layout(layout);

    menu->parent_wnd = gui_push_overlay(layout, menu->active);
    return true;
}

bool gui_begin_menu_id(GuiId id, GuiMenu *menu, Rect rect, u32 flags) EXPORT
{
    if (gui_clicked(id, rect)) {
        menu->active = !menu->active;

        if (menu->active) {
            for (i32 i = 0; i < gui.menus.capacity; i++) {
                if (!gui.menus.slots[i].occupied) continue;
                if (gui.menus.slots[i].key == id) continue;

                if (!gui_is_parent(gui.menus.slots[i].key)) {
                    gui.menus.slots[i].value.active = false;
                }
            }
        }
    }

    gui_draw_button(id, rect);
    if (menu->active) {
        gui_push_id(id);

        Rect sub_rect;
        if (!(flags & SPLIT_COL)) {
            sub_rect = {
                { rect.tl.x, rect.br.y },
                { rect.tl.x + menu->size.x, rect.br.y + menu->size.y },
            };
        } else {
            sub_rect = {
                { rect.br.x, rect.tl.y },
                { rect.br.x + menu->size.x, rect.tl.y + menu->size.y },
            };
        }

        gui_push_layout({ sub_rect, LAYOUT_ROOT|EXPAND_XY });
        menu->pos = sub_rect.tl;
    }

    return menu->active;
}

bool gui_begin_menu_id(GuiId id, GlyphsData text, Rect rect) EXPORT
{
    GuiMenu *menu = gui_find_or_create_menu(id, rect.size());
    u32 flags = gui_current_layout()->flags & SPLIT_COL ? 0 : SPLIT_COL;
    gui_begin_menu_id(id, menu, rect, flags);

    Vector2 label_pos = calc_center(rect.tl, rect.br, { text.bounds.x, text.font->line_height });
    gui_draw_text(text, label_pos, gui.style.fg);

    menu->parent_wnd = gui_push_overlay({ menu->pos, menu->pos+menu->size }, menu->active );
    return menu->active;
}

bool gui_begin_menu_id(GuiId id, String label, LayoutRect layout) EXPORT
{
    SArena scratch = tl_scratch_arena();
    GlyphsData td = calc_glyphs_data(label, &gui.fonts.base, scratch);
    return gui_begin_menu_id(id, td, layout);
}

bool gui_begin_menu_id(GuiId id, String label) EXPORT
{
    SArena scratch = tl_scratch_arena();
    GlyphsData td = calc_glyphs_data(label, &gui.fonts.base, scratch);
    Vector2 bounds{
        td.bounds.x+gui.style.menu.margin+6,
        MAX(td.bounds.y, gui.style.menu.height)+gui.style.menu.margin
    };

    LayoutRect layout = split_rect({ bounds.x, bounds.y, .margin = gui.style.margin });
    return gui_begin_menu_id(id, td, layout);
}

void gui_end_menu(GuiId id /*= GUI_ID_INVALID */) EXPORT
{
    if (id == GUI_ID_INVALID) id = gui_pop_id();

    GuiMenu *menu = map_find(&gui.menus, id);
    ASSERT(menu);

    LayoutRect *layout = gui_current_layout();
    if (layout->flags & EXPAND_XY) {
        menu->size.x = MAX(menu->size.x, layout->rem.br.x-layout->bounds.tl.x);
        menu->size.y = MAX(menu->size.y, layout->rem.br.y-layout->bounds.tl.y);
    }

    if (menu->size.x == 0 || menu->size.y == 0) {
        menu->active = false;
    }

    if (menu->max_size.x > 0 && menu->size.x > menu->max_size.x) {
        menu->size.x = menu->max_size.x;
        menu->has_overflow_x = true;
    }

    if (menu->max_size.y > 0 && menu->size.y > menu->max_size.y) {
        menu->size.y = menu->max_size.y;
        menu->has_overflow_y = true;
    }

    if (menu->active) {
        Vector3 bg = bgr_unpack(0xFF212121);
        gui_draw_rect(menu->pos, menu->size, bg, &gui_current_window()->command_buffer);
        gui_pop_command_buffer(&gui_current_window()->command_buffer);
    } else {
        gui_pop_command_buffer(nullptr);
    }

    gui_pop_clip_rect();
    gui_pop_layout();

    gui.current_window = menu->parent_wnd;
}

bool gui_begin_context_menu_id(GuiId id, GuiId widget) EXPORT
{
    GuiMenu *menu = gui_find_or_create_menu(id, { 0, 0 });

    if (gui.hot == widget && get_input_mouse(MB_SECONDARY)) {
        menu->active = !menu->active;
        if (menu->active) menu->pos = gui_mouse();
    }

    if (menu->active) {
        gui_push_id(id);

        gui_push_layout({
            { menu->pos, menu->pos + menu->size },
            EXPAND_XY | LAYOUT_ROOT,
        });

        menu->parent_wnd = gui_push_overlay({ menu->pos, menu->pos+menu->size }, menu->active);
        return true;
    }

    return false;
}

void gui_end_context_menu() EXPORT
{
    GuiId id = gui_pop_id();
    gui_end_menu(id);

    // TODO(jesper): something like this should be happening for all menus, but I don't have a great way of checking if the current hot widget is part of the menu's children, which can be outside of this menu's rect.
    //        Current thinking is that a menu should probably calculate the containing rect of all its sub-menus, which is a seperate rect of its own, and then we can check if the hot widget is inside that rect.
    GuiMenu *menu = map_find(&gui.menus, id);
    if (menu->active &&
        get_input_mouse(MB_ANY) &&
        !point_in_rect(gui_mouse(), { menu->pos, menu->pos+menu->size }))
    {
        menu->active = false;
    }
}

void gui_close_menu(GuiId id) EXPORT
{
    GuiMenu *menu = map_find(&gui.menus, id);
    if (menu) menu->active = false;
}

void gui_close_menu() EXPORT
{
    for (auto id : reverse(gui.id_stack)) {
        GuiMenu *menu = map_find(&gui.menus, id.elem());
        if (menu) {
            menu->active = false;
            return;
        }
    }
}

bool gui_begin_section(GuiId id, LayoutRect layout, u32 initial_state) EXPORT
{
    gui_push_id(id);
    gui_hot_rect(id, layout);

    u32 &flags = *map_find_emplace(&gui.flags, id, initial_state);
    if (gui_clicked(id, layout)) flags ^= GUI_ACTIVE;

    gui_push_layout(layout);

    String icon = flags & GUI_ACTIVE ? gui.icons.down : gui.icons.right;
    LayoutRect icon_r = split_left({ gui.icons.size, .flags = SPLIT_SQUARE });
    gui_draw_dark_button(id, layout);
    gui_draw_icon(icon, icon_r);

    return flags & GUI_ACTIVE;
}

bool gui_begin_section(GuiId id, u32 initial_state) EXPORT
{
    LayoutRect layout = split_row({ gui.fonts.base.line_height+4 });
    return gui_begin_section(id, layout, initial_state);
}

void gui_end_section() EXPORT
{
    gui_pop_layout();
    gui_pop_id();
}


void gui_vscrollbar_id(
    GuiId id,
    f32 *foffset,
    i32 *line_current,
    f32 line_height,
    i32 lines_total,
    i32 lines_num_visible,
    Rect rect,
    Rect view_r) EXPORT
{
    gui_push_id(id);
    defer { gui_pop_id(); };

    if (gui.hot_window == gui_current_window()->id &&
        point_in_rect(gui_mouse(), view_r))
    {
        push_input_layer(gui.input.scrollarea);
        if (f32 delta; get_input_axis(SCROLL, &delta, gui.input.scrollarea)) {
            *foffset += line_height*delta;
        }
    }

    Rect up_r   = split_top(&rect, gui.style.scrollbar.thickness);
    Rect down_r = split_bottom(&rect, gui.style.scrollbar.thickness);

    gui_draw_rect(rect, gui.style.bg_dark0);
    if (gui_icon_button(gui.icons.up,   up_r))   *foffset += line_height;
    if (gui_icon_button(gui.icons.down, down_r)) *foffset -= line_height;

    while (*foffset >= line_height) {
        f32 div        = *foffset / line_height;
        *line_current  = MAX(0, *line_current-1*div);
        *foffset      -= line_height;
    }

    while (*foffset <= -line_height) {
        f32 div        = *foffset / -line_height;
        *line_current += 1*div;
        *foffset      += line_height*div;
    }

    // TODO(jesper): +3?!
    *line_current = MIN(*line_current, lines_total-lines_num_visible+3);
    *line_current = MAX(*line_current, 0);

    f32 y0 = 0, y1 = 0;
    if (lines_num_visible >= lines_total) {
        y0 = 0;
        y1 = rect.size().y;
    } else {
        f32 start_factor = (f32)*line_current / lines_total;
        f32 end_factor   = ((f32)*line_current + lines_num_visible) / lines_total;

        y0 = start_factor * rect.size().y;
        y1 = end_factor * rect.size().y;
    }

    Rect handle_r {
        { rect.tl.x, MAX(rect.tl.y + y0, up_r.br.y) },
        { rect.br.x, MIN(rect.tl.y + y1, down_r.tl.y) }
    };

    gui_hot_rect(id, handle_r);
    if (gui_drag(id, { *foffset, transmute(f32, *line_current) }) && gui.mouse.dy != 0) {
        // TODO(jesper): +3?!
        f32 dy = gui.mouse.y - gui.drag_start_mouse.y;
        f32 sdy = dy*(f32(lines_total) / (lines_num_visible-3));

        //f32 drag_offset = gui.drag_start_data[0][0];
        i32 drag_line_start = transmute(i32, gui.drag_start_data[0][1]);

        i32 lines_dragged = floorf(sdy / line_height);
        if (lines_dragged) {
            // TODO(jesper): +3?!
            *line_current = drag_line_start + lines_dragged;
            *line_current = CLAMP(*line_current, 0, lines_total-lines_num_visible+3);
        }
    }

    gui_draw_button(id, handle_r);
}

void gui_vscrollbar_id(
    GuiId id,
    f32 *offset,
    f32 total_height,
    f32 step_size) EXPORT
{
    LayoutRect parent = *gui_parent_layout();
    Rect view_r { parent.bounds.tl, parent.bounds.br };
    gui_vscrollbar_id(id, offset, total_height, step_size, view_r);
}

void gui_vscrollbar_id(
    GuiId id,
    f32 *foffset,
    i32 *line_current,
    f32 line_height,
    i32 lines_total,
    i32 lines_num_visible,
    Rect rect) EXPORT
{
    LayoutRect parent = *gui_parent_layout();
    Rect view_r { parent.bounds.tl, parent.bounds.br };
    gui_vscrollbar_id(id, foffset, line_current, line_height, lines_total, lines_num_visible, rect, view_r);
}

void gui_vscrollbar_id(
    GuiId id,
    f32 *foffset,
    i32 *line_current,
    f32 line_height,
    i32 lines_total,
    i32 lines_num_visible) EXPORT
{
    Rect rect = split_right({ gui.style.scrollbar.thickness });
    gui_vscrollbar_id(id, foffset, line_current, line_height, lines_total, lines_num_visible, rect);
}


void gui_vscrollbar_id(
    GuiId id,
    f32 *offset,
    f32 total_height,
    f32 step_size,
    Rect view_r) EXPORT
{
    Rect rect = split_right({ gui.style.scrollbar.thickness });
    gui_vscrollbar_id(id, offset, total_height, step_size, rect, view_r);
}

void gui_vscrollbar_id(
    GuiId id,
    f32 *offset,
    f32 total_height,
    f32 step_size,
    Rect rect,
    Rect view_r) EXPORT
{
    gui_push_id(id);
    defer { gui_pop_id(); };

    GuiId h_id = GUI_ID;

    f32 max_offset = total_height > view_r.size().y ? total_height-view_r.size().y : 0;

    if (gui.hot_window == gui_current_window()->id &&
        point_in_rect(gui_mouse(), view_r))
    {
        push_input_layer(gui.input.scrollarea);
        if (f32 delta; get_input_axis(SCROLL, &delta, gui.input.scrollarea)) {
            *offset -= step_size*delta;
        }
    }

    Rect up_r   = split_top(&rect, gui.style.scrollbar.thickness);
    Rect down_r = split_bottom(&rect, gui.style.scrollbar.thickness);

    gui_draw_rect(rect, gui.style.bg_dark0);
    if (gui_icon_button(gui.icons.up, up_r)) *offset -= step_size;
    if (gui_icon_button(gui.icons.down, down_r)) *offset += step_size;
    *offset = MAX(*offset, 0);
    *offset = MIN(*offset, max_offset);

    f32 min_h = 8;
    f32 y0 =  *offset * (rect.size().y / total_height);
    f32 y1 = (*offset + view_r.size().y) * (rect.size().y / total_height);

    if (y1-y0 < min_h) {
        y0 = y1-0.5f*min_h;
        y1 = MIN(rect.br.y, y0+min_h);
        y0 = MIN(y0, y1-min_h);
    }

    Rect handle_r {
        { rect.tl.x, MAX(rect.tl.y + y0, up_r.br.y) },
        { rect.br.x, MIN(rect.tl.y + y1, down_r.tl.y) }
    };

    gui_hot_rect(h_id, handle_r);
    if (gui_drag(h_id, { 0, *offset }) && gui.mouse.dy != 0) {
        f32 dy = gui.mouse.y - gui.drag_start_mouse.y;

        *offset = gui.drag_start_data[0].y + (dy * (total_height / rect.size().y));
        *offset = MIN(*offset, max_offset);
        *offset = MAX(*offset, 0);
    }

    gui_draw_button(h_id, handle_r);
}

GuiAction gui_lister_id(GuiId id, Array<String> items, i32 *selected_item) EXPORT
{
    GuiAction result = GUI_NONE;
    *selected_item = CLAMP(*selected_item, 0, items.count-1);

    gui_handle_focus_grabbing(id);
    if (gui_input_layer(id, gui.input.lister)) {
        if (get_input_edge(CANCEL, gui.input.lister)) result = GUI_CANCEL;
        if (get_input_edge(CONFIRM, gui.input.lister)) result = GUI_END;
        if (get_input_edge(FORWARD, gui.input.lister)) *selected_item = MIN(*selected_item+1, items.count-1);
        if (get_input_edge(BACK, gui.input.lister)) *selected_item = MAX(*selected_item-1, 0);
    }

    FontAtlas *font = &gui.fonts.base;
    f32 item_height = font->line_height;

    GuiScrollArea *scroll_area = map_find_emplace(&gui.scroll_areas, id, {});

    Rect view_r = split_rect({ .margin = gui.style.margin });
    Rect border = shrink_rect(&view_r, 1);
    Rect vscroll_r = split_right(&view_r, gui.style.scrollbar.thickness);

    Rect rect = view_r;
    rect.tl.y -= scroll_area->offset.y;

    Vector3 bg = gui.style.bg_dark1;
    Vector3 border_col = gui.focused == id ? gui.style.accent_bg : gui.style.bg_light1;
    Vector3 selected_bg = gui.style.accent_bg;
    Vector3 selected_hot_bg = gui.style.accent_bg_hot;
    Vector3 hot_bg = gui.style.bg_hot;

    gui_draw_rect(border, border_col);
    gui_draw_rect(view_r, bg);

    gui_push_id(id);
    gui_push_layout({ rect, LAYOUT_ROOT|EXPAND_Y });

    defer {
        gui_pop_layout();
        gui_pop_id();
    };

    {
        gui_push_clip_rect(view_r);
        defer { gui_pop_clip_rect(); };

        i32 start = 0, end = items.count;
        for (i32 i = start; i < end; i++) {
            Rect item_r = split_row({ item_height });
            GuiId item_id = gui_gen_id(i);

            if (gui_clicked(item_id, item_r)) {
                *selected_item = i;
                result = GUI_CHANGE;
            }

            // TODO(jesper): border
            if (i == *selected_item && gui.hot == item_id) gui_draw_rect(item_r, selected_hot_bg);
            else if (i == *selected_item) gui_draw_rect(item_r, selected_bg);
            else if (gui.hot == item_id) gui_draw_rect(item_r, hot_bg);

            gui_textbox(items[i], item_r);
        }
    }

    f32 total_height = gui_current_layout()->rem.br.y+scroll_area->offset.y - view_r.tl.y;
    gui_vscrollbar(&scroll_area->offset.y, total_height, item_height, vscroll_r, view_r);
    return result;
}

bool gui_input(WindowEvent event) EXPORT
{
    switch (event.type) {
    case WE_MOUSE_MOVE:
        gui.mouse.dx += event.mouse.dx;
        gui.mouse.dy += event.mouse.dy;
        break;
    }

    return false;
}

bool gui_begin_tree_id(GuiId id, String label) EXPORT { return gui_begin_tree_id(id, label, false); }
void gui_tree_leaf_id(GuiId id, String label) EXPORT { gui_begin_tree_id(id, label, true); }

bool gui_begin_tree_id(GuiId id, String label, bool is_leaf) EXPORT
{
    SArena scratch = tl_scratch_arena();
    u32 &flags = *map_find_emplace(&gui.flags, id, 0u);

    gui_push_id(id);
    defer { if (!(flags & GUI_ACTIVE)) gui_pop_id(); };

    GlyphsData text = calc_glyphs_data(label, &gui.fonts.base, scratch);
    LayoutRect rect = split_row({ text.font->line_height+4 });
    LayoutRect text_rect = rect;


    gui_hot_rect(id, rect);
    gui_pressed(id);
    gui_handle_focus_grabbing(id);

    GuiId btn_id = GUI_ID;
    LayoutRect btn_rect;

    if (!is_leaf) {
        btn_rect = split_left(&text_rect, { rect.size().y, .margin = gui.style.margin, .flags = SPLIT_SQUARE });

        if (gui_clicked(btn_id, btn_rect)) {
            flags ^= GUI_ACTIVE;
            gui_focus(id);
        }
    }

    if (gui.focused == id) flags |= GUI_SELECTED;

    if (gui_input_layer(id, gui.input.tree)) {
        if (!is_leaf) {
            if (get_input_edge(TOGGLE, gui.input.tree))     flags ^= GUI_ACTIVE;
            if (get_input_edge(ACTIVATE, gui.input.tree))   flags |= GUI_ACTIVE;
            if (get_input_edge(DEACTIVATE, gui.input.tree)) flags &= ~GUI_ACTIVE;
        }
    }

    if (flags & GUI_SELECTED) gui_draw_rect(rect, gui.style.accent_bg);
    else if (gui.hot == id) gui_draw_rect(rect, gui.style.bg_hot);

    if (!is_leaf) {
        String icon = flags & GUI_ACTIVE ? gui.icons.down : gui.icons.right;
        gui_draw_icon_button(btn_id, icon, btn_rect);
    }

    gui_textbox(text, text_rect);

    if (flags & GUI_ACTIVE) gui_current_layout()->rem.tl.x += 16;
    return flags & GUI_ACTIVE;
}

void gui_end_tree() EXPORT
{
    gui_current_layout()->rem.tl.x -= 16;
    gui_pop_id();
}

void gui_divider(f32 thickness = 1, Vector3 color = gui.style.accent_bg) EXPORT
{
    Rect divider = split_rect({ thickness, thickness });
    gui_draw_rect(divider, color);
}

void gui_dialog_id(GuiId id, String title, GuiBodyProc proc) EXPORT
{
    for (auto it : gui.dialogs) if (it.id == id) return;
    array_add(&gui.dialogs, GuiDialog{ id, duplicate_string(title, mem_dynamic), proc });
}

// ------------------------------------------------------------------
// GUI_DRAW
// ------------------------------------------------------------------
void gui_draw_button(
    Rect rect,
    Vector3 btn_bg,
    Vector3 btn_bg_acc0,
    Vector3 btn_bg_acc1) INTERNAL
{
    Rect border = shrink_rect(&rect, 1);
    gui_draw_rect(border.tl, { border.size().x - 1.0f, 1.0f }, btn_bg_acc0);
    gui_draw_rect(border.tl, { 1.0f, border.size().y - 1.0f }, btn_bg_acc0);

    gui_draw_rect(border.tl + Vector2{ border.size().x - 1.0f, 0.0f }, { 1.0f, border.size().y }, btn_bg_acc1);
    gui_draw_rect(border.tl + Vector2{ 0.0f, border.size().y - 1.0f }, { border.size().x, 1.0f }, btn_bg_acc1);

    gui_draw_rect(rect, btn_bg);
}

void gui_draw_icon_button(GuiId id, String icon, Rect rect) INTERNAL
{
    gui_draw_button(id, rect);
    gui_draw_icon(icon, rect);
}

void gui_draw_icon(String icon, Rect rect) INTERNAL
{
    SArena scratch = tl_scratch_arena();

    GlyphsData td = calc_glyphs_data(icon, &gui.fonts.icons, scratch);
    // NOTE(jesper): this _should_ be `- 0.5f*td.offset`, but gui_draw_text currently always adds the baseline y-offset
    Vector2 pos = calc_center(rect.tl, rect.br, td.bounds) - Vector2{ td.offset.x*0.5f, 0 };

    gui_draw_text(td, pos, gui.style.fg);
}

void gui_draw_rect(Rect rect, AssetHandle handle) EXPORT
{
    if (!apply_clip_rect(&rect, array_tail(gui.clip_stack))) return;

    GfxCommandBuffer *cmdbuf = array_tail(gui.draw_stack);
    auto *texture = get_asset<TextureAsset>(handle);

    f32 vertices[] = {
        rect.br.x, rect.tl.y, 1.0f, 0.0f,
        rect.tl.x, rect.tl.y, 0.0f, 0.0f,
        rect.tl.x, rect.br.y, 0.0f, 1.0f,

        rect.tl.x, rect.br.y, 0.0f, 1.0f,
        rect.br.x, rect.br.y, 1.0f, 1.0f,
        rect.br.x, rect.tl.y, 1.0f, 0.0f,
    };

    GfxCommand cmd = gui_command(GFX_COMMAND_GUI_PRIM_TEXTURE);
    cmd.gui_prim_texture.texture = texture->texture_handle;
    cmd.gui_prim_texture.vertex_count = ARRAY_COUNT(vertices);
    gui_push_command(cmdbuf, cmd);

    array_add(&gui.vertices, vertices, ARRAY_COUNT(vertices));
}

void gui_draw_rect(
    Rect rect,
    Vector3 color,
    GfxCommandBuffer *cmdbuf) EXPORT
{
    if (!apply_clip_rect(&rect, array_tail(gui.clip_stack))) return;

    color = linear_from_sRGB(color);

    f32 vertices[] = {
        rect.br.x, rect.tl.y, color.r, color.g, color.b, 1.0f,
        rect.tl.x, rect.tl.y, color.r, color.g, color.b, 1.0f,
        rect.tl.x, rect.br.y, color.r, color.g, color.b, 1.0f,

        rect.tl.x, rect.br.y, color.r, color.g, color.b, 1.0f,
        rect.br.x, rect.br.y, color.r, color.g, color.b, 1.0f,
        rect.br.x, rect.tl.y, color.r, color.g, color.b, 1.0f,
    };

    GfxCommand cmd = gui_command(GFX_COMMAND_COLORED_PRIM);
    cmd.colored_prim.vertex_count = ARRAY_COUNT(vertices) / 6;
    array_add(&gui.vertices, vertices, ARRAY_COUNT(vertices));

    gfx_push_command(cmd, cmdbuf);
}


void gui_draw_rect(Rect rect, Vector3 color) EXPORT
{
    return gui_draw_rect(rect, color, array_tail(gui.draw_stack));
}
void gui_draw_rect(Vector2 pos, Vector2 size, Vector3 color) EXPORT
{
    return gui_draw_rect({ pos, pos+size }, color, array_tail(gui.draw_stack));
}
void gui_draw_rect(Vector2 pos, Vector2 size, Vector3 color, GfxCommandBuffer *cmdbuf) EXPORT
{
    return gui_draw_rect({ pos, pos+size }, color, cmdbuf);
}


Vector2 gui_draw_text(
    GlyphsData data,
    Vector2 pos,
    Vector3 color) EXPORT
{
    pos.x = floorf(pos.x);
    pos.y = floorf(pos.y);

    Vector2 cursor = pos;
    cursor.y += data.font->baseline;

    GfxCommand cmd = gui_command(GFX_COMMAND_GUI_TEXT);
    cmd.gui_text.font_atlas = data.font->texture;
    cmd.gui_text.color = color;

    Rect *clip_rect = array_tail(gui.clip_stack);

    for (GlyphRect g : data.glyphs) {
        g.x0 += pos.x;
        g.x1 += pos.x;
        g.y0 += pos.y;
        g.y1 += pos.y;

        if (!apply_clip_rect(&g, clip_rect)) continue;

        f32 verts[] = {
            g.x0, g.y0, g.s0, g.t0,
            g.x0, g.y1, g.s0, g.t1,
            g.x1, g.y1, g.s1, g.t1,

            g.x1, g.y1, g.s1, g.t1,
            g.x1, g.y0, g.s1, g.t0,
            g.x0, g.y0, g.s0, g.t0,
        };

        array_add(&gui.vertices, verts, ARRAY_COUNT(verts));
        cmd.gui_text.vertex_count += ARRAY_COUNT(verts);
    }

    gui_push_command(array_tail(gui.draw_stack), cmd);
    return cursor;
}

Vector2 gui_draw_text(
    String text,
    Vector2 pos,
    Vector3 color,
    FontAtlas *font) EXPORT
{
    pos.x = floorf(pos.x);
    pos.y = floorf(pos.y);

    Vector2 cursor = pos;
    cursor.y += font->baseline;

    GfxCommand cmd = gui_command(GFX_COMMAND_GUI_TEXT);
    cmd.gui_text.font_atlas = font->texture;
    cmd.gui_text.color = color;

    Rect *clip_rect = array_tail(gui.clip_stack);

    char *p = text.data;
    char *end = text.data+text.length;
    while (p < end) {
        i32 c = utf32_it_next(&p, end);
        if (c == 0) return cursor;

        GlyphRect g = get_glyph_rect(font, c, &cursor);
        if (!apply_clip_rect(&g, clip_rect)) continue;

        f32 vertices[] = {
            g.x0, g.y0, g.s0, g.t0,
            g.x0, g.y1, g.s0, g.t1,
            g.x1, g.y1, g.s1, g.t1,

            g.x1, g.y1, g.s1, g.t1,
            g.x1, g.y0, g.s1, g.t0,
            g.x0, g.y0, g.s0, g.t0,
        };

        array_add(&gui.vertices, vertices, ARRAY_COUNT(vertices));
        cmd.gui_text.vertex_count += ARRAY_COUNT(vertices);
    }

    GfxCommandBuffer *cmdbuf = array_tail(gui.draw_stack);
    gui_push_command(cmdbuf, cmd);

    return cursor;
}

void gui_draw_button(GuiId id, Rect rect) EXPORT
{
    Vector3 btn_bg = gui.pressed == id ? gui.style.bg_press : gui.hot == id ? gui.style.bg_hot : gui.style.bg;

    Vector3 btn_bg_acc0, btn_bg_acc1;
    if (gui.focused == id) {
        btn_bg_acc0 = gui.style.accent_bg;
        btn_bg_acc1 = gui.style.accent_bg_hot;
    } else {
        btn_bg_acc0 = gui.pressed == id ? rgb_mul(btn_bg, 0.6f) : rgb_mul(btn_bg, 1.25f);
        btn_bg_acc1 = gui.pressed == id ? rgb_mul(btn_bg, 1.25f) : rgb_mul(btn_bg, 0.6f);
    }
    gui_draw_button(rect, btn_bg, btn_bg_acc0, btn_bg_acc1);
}

void gui_draw_accent_button(GuiId id, Rect rect) EXPORT
{
    Vector3 btn_bg = gui.pressed == id ? gui.style.accent_bg_press : gui.hot == id ? gui.style.accent_bg_hot : gui.style.accent_bg;
    Vector3 btn_bg_acc0 = gui.pressed == id ? rgb_mul(btn_bg, 0.6f) : rgb_mul(btn_bg, 1.25f);
    Vector3 btn_bg_acc1 = gui.pressed == id ? rgb_mul(btn_bg, 1.25f) : rgb_mul(btn_bg, 0.6f);
    gui_draw_button(rect, btn_bg, btn_bg_acc0, btn_bg_acc1);
}

void gui_draw_dark_button(GuiId id, Rect rect) EXPORT
{
    Vector3 btn_bg = rgb_mul(gui.pressed == id ? gui.style.bg_press : gui.hot == id ? gui.style.bg_hot : gui.style.bg, 0.4f);
    Vector3 btn_bg_acc0 = gui.pressed == id ? rgb_mul(btn_bg, 0.8f) : rgb_mul(btn_bg, 1.5f);
    Vector3 btn_bg_acc1 = gui.pressed == id ? rgb_mul(btn_bg, 1.5f) : rgb_mul(btn_bg, 0.8f);
    gui_draw_button(rect, btn_bg, btn_bg_acc0, btn_bg_acc1);
}
