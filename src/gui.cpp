#include "gui.h"

#include "core.h"
#include "file.h"
#include "assets.h"

extern Allocator mem_frame;

GuiContext gui;

#define GIZMO_BAR_SIZE Vector2{ 48.0f, 4.0f }
#define GIZMO_TIP_SIZE Vector2{ 10.0f, 10.0f }
#define GIZMO_OUTLINE_COLOR Vector3{ 0.0f, 0.0f, 0.0f }


void init_gui()
{
    glGenBuffers(1, &gui.vbo);
    
    GuiWindow background{
        .id = GUI_ID(0),
        .clip_rect = Rect{ { 0.0f, 0.0f }, { gfx.resolution.x, gfx.resolution.y } },
        .command_buffer.commands.alloc = mem_frame,
        .size = gfx.resolution,
    };
    array_add(&gui.windows, background);
    
    GuiWindow overlay{
        .id = GUI_ID(0),
        .clip_rect = Rect{ { 0.0f, 0.0f }, { gfx.resolution.x, gfx.resolution.y } },
        .command_buffer.commands.alloc = mem_frame,
        .size = gfx.resolution,
    };
    array_add(&gui.windows, overlay);
    
    gui.current_window = GUI_BACKGROUND;

    gui.camera.projection = matrix4_identity();
    gui.camera.projection[0][0] = 2.0f / gfx.resolution.x;
    gui.camera.projection[1][1] = 2.0f / gfx.resolution.y;
    gui.camera.projection[3][1] = -1.0f;
    gui.camera.projection[3][0] = -1.0f;
    
    if (auto a = find_asset("textures/close.png"); a) gui.icons.close = gfx_load_texture(a->data, a->size);
    if (auto a = find_asset("textures/check.png"); a) gui.icons.check = gfx_load_texture(a->data, a->size);
    if (auto a = find_asset("textures/down.png"); a) gui.icons.down = gfx_load_texture(a->data, a->size);
    if (auto a = find_asset("textures/up.png"); a) gui.icons.up = gfx_load_texture(a->data, a->size);

    gui.style.text.font = create_font("fonts/Ubuntu/Ubuntu-Regular.ttf", 18);
    gui.style.button.font = create_font("fonts/Ubuntu/Ubuntu-Regular.ttf", 18);
}

GuiLayout *gui_current_layout()
{
    ASSERT(gui.layout_stack.count > 0);
    return &gui.layout_stack[gui.layout_stack.count-1];
}

GuiWindow* gui_current_window()
{
    ASSERT(gui.current_window < gui.windows.count);
    return &gui.windows[gui.current_window];
}

bool gui_capture(bool capture_var[2]) 
{
    // TODO(jesper): this GUI capture strategy is rather coarse. I'm thinking the widgets
    // need an API to signal exactly which events they want to grab, which can then be
    // used to funnel the right events to the right widgets appropriately according
    // to hierarchy, and pass any unhandled events back to the application for
    // a fall-through type thing
    capture_var[1] = true;
    
    GuiWindow *wnd = &gui.windows[gui.current_window];
    return capture_var[0] && gui.focused_window == wnd->id;
}

void gui_drag_start(Vector2 data)
{
    gui.drag_start_mouse = { (f32)gui.mouse.x, (f32)gui.mouse.y };
    gui.drag_start_data = data;
}

void gui_drag_start(Vector2 data, Vector2 data1)
{
    gui.drag_start_mouse = { (f32)gui.mouse.x, (f32)gui.mouse.y };
    gui.drag_start_data = data;
    gui.drag_start_data1 = data1;
}

void gui_hot(GuiId id)
{
    if (gui.pressed == id || 
        (gui.pressed == GUI_ID_INVALID && 
         gui.hot_window == gui.windows[gui.current_window].id)) 
    {
        gui.next_hot = id;
    }
}

bool gui_hot_rect(GuiId id, Vector2 pos, Vector2 size)
{
    Vector2 mouse = { (f32)gui.mouse.x, (f32)gui.mouse.y };
    Vector2 rel = mouse - pos;

    if ((rel.x >= 0.0f && rel.x < size.x &&
         rel.y >= 0.0f && rel.y < size.y))
    {
        gui_hot(id);
    } 

    return gui.hot == id;
}

void gui_clear_hot()
{
    gui.hot = gui.next_hot = GUI_ID_INVALID;
}

bool gui_clicked(GuiId id)
{
    if (gui.hot == id && gui.mouse.left_pressed && !gui.mouse.left_was_pressed) {
        gui.pressed = id;
    } else if (gui.pressed == id && !gui.mouse.left_pressed) {
        gui.pressed = GUI_ID_INVALID;
        return true;
    }

    return false;
}

bool gui_pressed(GuiId id)
{
    if (gui.hot == id && gui.mouse.left_pressed && !gui.mouse.left_was_pressed) {
        gui.pressed = id;
        return true;
    } 
    return false;
}

bool gui_drag(GuiId id, Vector2 data)
{
    if (gui.hot == id || gui.pressed == id) {
        if (gui.pressed != id && gui.mouse.left_pressed && !gui.mouse.left_was_pressed)  {
            gui.pressed = id;
            gui_drag_start(data);
        } else if (gui.pressed == id && !gui.mouse.left_pressed) {
            gui.pressed = GUI_ID_INVALID;
        }
    }
    
    if (gui.pressed == id) {
        gui_hot(id);
        gui.focused_window = gui_current_window()->id;
    }
    return gui.pressed == id;
}

bool gui_focused_parent(GuiId id)
{
    return gui.focused == id || gui.current_id.owner == id.owner;
}

void gui_handle_focus_grabbing(GuiId id)
{
    // TODO(jesper): intercept tabs if currently held focus and set
    // to invalid id to make the next widget grab focus. Keep track of
    // previous widget which wanted to grab focus for prev-tab
    if (gui.focused_window == gui_current_window()->id &&
        gui.focused == GUI_ID_INVALID)
    {
        gui.focused = id;
    }
}

bool apply_clip_rect(GlyphRect *g, Rect clip_rect)
{
    if (g->x0 > clip_rect.pos.x + clip_rect.size.x) return false;
    if (g->y0 > clip_rect.pos.y + clip_rect.size.y) return false;
    if (g->x1 < clip_rect.pos.x) return false;
    if (g->y1 < clip_rect.pos.y) return false;

    if (g->x1 > clip_rect.pos.x + clip_rect.size.x) {
        f32 w0 = g->x1 - g->x0;
        g->x1 = clip_rect.pos.x + clip_rect.size.x;
        f32 w1 = g->x1 - g->x0;
        g->s1 = g->s0 + (g->s1 - g->s0) * (w1/w0);
    }

    if (g->x0 < clip_rect.pos.x) {
        f32 w0 = g->x1 - g->x0;
        g->x0 = clip_rect.pos.x;
        f32 w1 = g->x1 - g->x0;
        g->s0 = g->s1 - (g->s1 - g->s0) * (w1/w0);
    }

    if (g->y1 > clip_rect.pos.y + clip_rect.size.y) {
        f32 h0 = g->y1 - g->y0;
        g->y1 = clip_rect.pos.y + clip_rect.size.y;
        f32 h1 = g->y1 - g->y0;
        g->t1 = g->t0 + (g->t1 - g->t0) * (h1/h0);
    }

    if (g->y0 < clip_rect.pos.y) {
        f32 h0 = g->y1 - g->y0;
        g->y0 = clip_rect.pos.y;
        f32 h1 = g->y1 - g->y0;
        g->t0 = g->t1 - (g->t1 - g->t0) * (h1/h0);
    }

    return true;
}

bool apply_clip_rect(Vector2 *pos, Vector2 *size, Rect clip_rect)
{
    if (pos->x > clip_rect.pos.x + clip_rect.size.x) return false;
    if (pos->y > clip_rect.pos.y + clip_rect.size.y) return false;
    if (pos->x + size->x < clip_rect.pos.x) return false;
    if (pos->y + size->y < clip_rect.pos.y) return false;

    f32 left = pos->x;
    f32 right = pos->x + size->x;
    f32 top = pos->y;
    f32 bottom = pos->y + size->y;

    top = MAX(top, clip_rect.pos.y);
    left = MAX(left, clip_rect.pos.x);
    right = MIN(right, clip_rect.pos.x + clip_rect.size.x);
    bottom = MIN(bottom, clip_rect.pos.y + clip_rect.size.y);

    pos->x = left;
    pos->y = top;
    size->x = right-left;
    size->y = bottom-top;

    return true;
}

bool gui_mouse_over_rect(Vector2 pos, Vector2 size)
{
    Vector2 mouse = { (f32)gui.mouse.x, (f32)gui.mouse.y };
    Vector2 rel = mouse - pos;

    if ((rel.x >= 0.0f && rel.x < size.x &&
         rel.y >= 0.0f && rel.y < size.y))
    {
        return true;
    }

    return false;
}

Rect gui_layout_rect(GuiLayout *cl)
{
    ASSERT(cl);

    Rect r;
    r.pos = cl->pos+cl->current;
    r.size = cl->available_space;
    return r;
}

Rect gui_layout_rect()
{
    return gui_layout_rect(gui_current_layout());
}

void gui_begin_frame()
{
    for (GuiWindow &wnd : gui.windows) {
        array_reset(&wnd.command_buffer.commands, mem_frame, wnd.command_buffer.commands.capacity);

        wnd.last_active = wnd.active;
        wnd.active = false;
    }

    array_reset(&gui.vertices, mem_frame, gui.vertices.count);
    array_reset(&gui.layout_stack, mem_frame, gui.layout_stack.count);

    // NOTE(jesper): when resolution changes the root and overlay "windows" have to adjust their
    // clip rects as well
    gui.windows[GUI_BACKGROUND].clip_rect.size = gfx.resolution;
    gui.windows[GUI_OVERLAY].clip_rect.size = gfx.resolution;

    GuiLayout root_layout{
        .type = GUI_LAYOUT_ABSOLUTE,
        .pos = { 0.0f, 0.0f },
        .size = gfx.resolution
    };
    gui_begin_layout(root_layout);

}

void gui_end_frame()
{
    ASSERT(gui.id_stack.count == 0);
    ASSERT(gui.layout_stack.count == 1);

    glBindBuffer(GL_ARRAY_BUFFER, gui.vbo);
    glBufferData(GL_ARRAY_BUFFER, gui.vertices.count * sizeof gui.vertices[0], gui.vertices.data, GL_STREAM_DRAW);

    if (!gui.mouse.left_pressed) gui.pressed = GUI_ID_INVALID;
    gui.last_focused = gui.focused;

    if (false && gui.hot != gui.next_hot) {
        LOG_INFO(
            "next hot id: { %d, %d, %d }",
            gui.next_hot.owner, gui.next_hot.index, gui.next_hot.internal);
    }

    gui.hot = gui.next_hot;
    gui.next_hot = GUI_ID_INVALID;

    gui.events.count = 0;
    gui.capture_text[0] = gui.capture_text[1];
    gui.capture_text[1] = false;
    
    gui.capture_keyboard[0] = gui.capture_keyboard[1];
    gui.capture_keyboard[1] = false;

    gui.capture_mouse_wheel[0] = gui.capture_mouse_wheel[1];
    gui.capture_mouse_wheel[1] = false;

    for (GuiWindow &wnd : gui.windows) {
        if (!wnd.active && gui.focused_window == wnd.id) {
            gui.focused_window = GUI_ID_INVALID;
        }
    }

    GuiId old = gui.hot_window;
    gui.hot_window = GUI_ID_INVALID;

    for (Rect &r : gui.overlay_rects) {
        if (gui_mouse_over_rect(r.pos, r.size)) {
            gui.hot_window = gui.windows[GUI_OVERLAY].id;
            break;
        }
    }
    gui.overlay_rects.count = 0;

    if (gui.hot_window == GUI_ID_INVALID) {
        for (i32 i = gui.windows.count-1; i > GUI_OVERLAY; i--) {
            GuiWindow &wnd = gui.windows[i];
            if (wnd.active && gui_mouse_over_rect(wnd.pos, wnd.size)) {
                gui.hot_window = wnd.id;
                break;
            }
        }
    }

    if (gui.hot_window == GUI_ID_INVALID) gui.hot_window = gui.windows[GUI_BACKGROUND].id;

    if (false && old != gui.hot_window) {
        LOG_INFO("hot window: %d, %d, %d", gui.hot_window.owner, gui.hot_window.index, gui.hot_window.internal);
    }
}

void gui_render()
{
    gfx_submit_commands(gui.windows[GUI_BACKGROUND].command_buffer);
    for (i32 i = 2; i < gui.windows.count; i++) {
        gfx_submit_commands(gui.windows[i].command_buffer);
    }
    gfx_submit_commands(gui.windows[GUI_OVERLAY].command_buffer);
}

GfxCommand gui_command(GfxCommandType type)
{
    // TODO(jesper): this entire function essentially exists to initialise the vertex buffer
    // data for the command. This should be replaced with a better API that lets you push
    // some data onto a frame or persistent VBO and get back a structure that you pass
    // to the command stream and it does the right thing
    GfxCommand cmd{ .type = type };
    switch (type) {
    case GFX_COMMAND_GUI_PRIM_COLOR:
        cmd.gui_prim.vbo = gui.vbo;
        cmd.gui_prim.vbo_offset = gui.vertices.count;
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
    case GFX_COMMAND_COLORED_PRIM:
    case GFX_COMMAND_TEXTURED_PRIM:
        LOG_ERROR("don't use this API for these");
        break;
    }
    return cmd;
}

void gui_push_command(GfxCommandBuffer *cmdbuf, GfxCommand cmd, i32 draw_index = -1)
{
    i32 index = draw_index != -1 ? draw_index : cmdbuf->commands.count-1;
    if (index >= 0 && cmdbuf->commands.count > index) {
        GfxCommand *existing = &cmdbuf->commands[index];

        if (existing->type == cmd.type) {
            switch (cmd.type) {
            case GFX_COMMAND_GUI_PRIM_COLOR:
                if (existing->gui_prim.vbo_offset + existing->gui_prim.vertex_count == cmd.gui_prim.vbo_offset) {
                    existing->gui_prim.vertex_count += cmd.gui_prim.vertex_count;
                    return;
                }
                break;
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
            case GFX_COMMAND_COLORED_LINE:
            case GFX_COMMAND_COLORED_PRIM:
            case GFX_COMMAND_TEXTURED_PRIM:
                break;
            }
        }
    }

    if (draw_index >= 0) array_insert(&cmdbuf->commands, draw_index, cmd);
    else array_add(&cmdbuf->commands, cmd);
}

void gui_draw_rect(
    Vector2 pos,
    Vector2 size,
    GLuint texture,
    GfxCommandBuffer *cmdbuf = nullptr,
    i32 draw_index = -1)
{
    if (!cmdbuf) cmdbuf = &gui_current_window()->command_buffer;

    f32 left = pos.x;
    f32 right = pos.x + size.x;
    f32 top = pos.y;
    f32 bottom = pos.y + size.y;

    f32 vertices[] = {
        right, top, 1.0f, 0.0f,
        left, top, 0.0f, 0.0f,
        left, bottom, 0.0f, 1.0f,

        left, bottom, 0.0f, 1.0f,
        right, bottom, 1.0f, 1.0f,
        right, top, 1.0f, 0.0f,
    };

    GfxCommand cmd = gui_command(GFX_COMMAND_GUI_PRIM_TEXTURE);
    cmd.gui_prim_texture.texture = texture;
    cmd.gui_prim_texture.vertex_count = ARRAY_COUNT(vertices);
    gui_push_command(cmdbuf, cmd, draw_index);

    array_add(&gui.vertices, vertices, ARRAY_COUNT(vertices));
}

void gui_draw_rect(
    Vector2 pos,
    Vector2 size,
    Rect clip_rect,
    GLuint texture,
    GfxCommandBuffer *cmdbuf = nullptr,
    i32 draw_index = -1)
{
    if (!apply_clip_rect(&pos, &size, clip_rect)) return;
    gui_draw_rect(pos, size, texture, cmdbuf, draw_index);
}

void gui_draw_rect(
    Vector2 pos,
    Vector2 size,
    Vector3 color,
    GfxCommandBuffer *cmdbuf = nullptr,
    i32 draw_index = -1)
{
    if (!cmdbuf) cmdbuf = &gui_current_window()->command_buffer;

    f32 left = pos.x;
    f32 right = pos.x + size.x;
    f32 top = pos.y;
    f32 bottom = pos.y + size.y;

    color = linear_from_sRGB(color);

    f32 vertices[] = {
        right, top, color.r, color.g, color.b,
        left, top, color.r, color.g, color.b,
        left, bottom, color.r, color.g, color.b,

        left, bottom, color.r, color.g, color.b,
        right, bottom, color.r, color.g, color.b,
        right, top, color.r, color.g, color.b,
    };

    GfxCommand cmd = gui_command(GFX_COMMAND_GUI_PRIM_COLOR);
    cmd.gui_prim.vertex_count = ARRAY_COUNT(vertices);
    gui_push_command(cmdbuf, cmd, draw_index);

    array_add(&gui.vertices, vertices, ARRAY_COUNT(vertices));
}

void gui_draw_rect(
    Vector2 pos,
    Vector2 size,
    Rect clip_rect,
    Vector3 color,
    GfxCommandBuffer *cmdbuf = nullptr,
    i32 draw_index = -1)
{
    if (!apply_clip_rect(&pos, &size, clip_rect)) return;
    gui_draw_rect(pos, size, color, cmdbuf, draw_index);
}

TextQuadsAndBounds calc_text_quads_and_bounds(String text, FontAtlas *font, Allocator alloc = mem_tmp)
{
    TextQuadsAndBounds r{};
    r.bounds.size.y = font->line_height;

    array_reset(&r.glyphs, alloc, text.length);

    Vector2 cursor{ 0.0f, (f32)font->baseline };

    char *p = text.data;
    char *end = text.data+text.length;
    while (p < end) {
        i32 c = utf32_it_next(&p, end);
        if (c == 0) return r;

        GlyphRect g = get_glyph_rect(font, c, &cursor);
        array_add(&r.glyphs, g);

        r.bounds.pos.x = MIN(r.bounds.pos.x, MIN(g.x0, g.x1));
        r.bounds.pos.y = MIN(r.bounds.pos.y, MIN(g.y0, g.y1));

        r.bounds.size.x = MAX(r.bounds.size.x, MAX(g.x0, g.x1));
        r.bounds.size.y = MAX(r.bounds.size.y, MAX(g.y0, g.y1));
    }

    return r;
}

DynamicArray<GlyphRect> calc_text_quads(String text, FontAtlas *font, Allocator alloc = mem_tmp)
{
    DynamicArray<GlyphRect> glyphs{};
    array_reset(&glyphs, alloc, text.length);

    Vector2 cursor{ 0.0f, (f32)font->baseline };

    char *p = text.data;
    char *end = text.data+text.length;
    while (p < end) {
        i32 c = utf32_it_next(&p, end);
        if (c == 0) return glyphs;

        GlyphRect g = get_glyph_rect(font, c, &cursor);
        array_add(&glyphs, g);
    }

    return glyphs;
}

Vector2 gui_draw_text(
    Array<GlyphRect> glyphs,
    Vector2 pos,
    Rect clip_rect,
    Vector3 color,
    FontAtlas *font,
    GfxCommandBuffer *cmdbuf = nullptr)
{
    if (!cmdbuf) cmdbuf = &gui_current_window()->command_buffer;

    Vector2 cursor = pos;
    cursor.y += font->baseline;

    GfxCommand cmd = gui_command(GFX_COMMAND_GUI_TEXT);
    cmd.gui_text.font_atlas = font->texture;
    cmd.gui_text.color = color;

    for (GlyphRect g : glyphs) {
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

    gui_push_command(cmdbuf, cmd);
    return cursor;
}

Vector2 gui_draw_text(
    String text,
    Vector2 pos,
    Rect clip_rect,
    Vector3 color,
    FontAtlas *font,
    GfxCommandBuffer *cmdbuf = nullptr)
{
    if (!cmdbuf) cmdbuf = &gui_current_window()->command_buffer;

    Vector2 cursor = pos;
    cursor.y += gui.style.text.font.baseline;

    GfxCommand cmd = gui_command(GFX_COMMAND_GUI_TEXT);
    cmd.gui_text.font_atlas = font->texture;
    cmd.gui_text.color = color;

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
    gui_push_command(cmdbuf, cmd);

    return cursor;
}

void gui_textbox(String str, FontAtlas *font = &gui.style.text.font)
{
    GuiWindow *wnd = &gui.windows[gui.current_window];

    TextQuadsAndBounds td = calc_text_quads_and_bounds(str, font);
    Vector2 pos = gui_layout_widget({ td.bounds.size.x, MAX(td.bounds.size.y, font->line_height) });

    gui_draw_text(td.glyphs, pos, wnd->clip_rect, gui.style.text.color, font);
}

void gui_textbox(String str, Vector2 pos, FontAtlas *font = &gui.style.text.font)
{
    GuiWindow *wnd = &gui.windows[gui.current_window];

    TextQuadsAndBounds td = calc_text_quads_and_bounds(str, font);
    gui_draw_text(td.glyphs, pos, wnd->clip_rect, gui.style.text.color, font);
}

void gui_textbox(String str, Vector2 pos, Rect clip_rect, FontAtlas *font = &gui.style.text.font)
{
    TextQuadsAndBounds td = calc_text_quads_and_bounds(str, font);
    gui_draw_text(td.glyphs, pos, clip_rect, gui.style.text.color, font);
}


GuiWindow* push_window_to_top(i32 index)
{
    if (index != gui.windows.count-1) {
        GuiWindow window = gui.windows[index];
        array_remove_sorted(&gui.windows, index);
        array_add(&gui.windows, window);
    }
    gui.current_window = gui.windows.count-1;
    return &gui.windows[gui.windows.count-1];
}

Vector2 gui_layout_widget(Vector2 size, GuiAnchor anchor)
{
    GuiLayout *cl = gui_current_layout();
    Vector2 pos = cl->pos;

    switch (cl->type) {
    case GUI_LAYOUT_ROW:
        pos += cl->current;

        switch (anchor) {
        case GUI_ANCHOR_TOP:
            cl->current.y += size.y + cl->row.spacing;
            cl->available_space.y -=  size.y + cl->row.spacing;
            break;
        case GUI_ANCHOR_BOTTOM:
            pos.y += cl->available_space.y - size.y;
            cl->available_space.y -= size.y + cl->row.spacing;
            break;
        }
        break;
    case GUI_LAYOUT_COLUMN:
        pos += cl->current;

        switch (anchor) {
        case GUI_ANCHOR_LEFT:
            cl->current.x += size.x + cl->column.spacing;
            cl->available_space.x -= size.x + cl->column.spacing;
            break;
        case GUI_ANCHOR_RIGHT:
            pos.x += cl->available_space.x - size.x;
            cl->available_space.x -= size.x + cl->column.spacing;
            break;
        }
        break;
    case GUI_LAYOUT_ABSOLUTE:
        break;
    }

    if (cl->flags & GUI_LAYOUT_EXPAND_X) {
        f32 diff = (pos.x + size.x) - (cl->pos.x + cl->size.x - cl->margin.x);
        if (diff > 0) {
            cl->size.x += diff;
            cl->available_space.x += diff;
        }
    }

    if (cl->flags & GUI_LAYOUT_EXPAND_Y) {
        f32 diff = (pos.y + size.y) - (cl->pos.y + cl->size.y - cl->margin.y);
        if (diff > 0) {
            cl->size.y += diff;
            cl->available_space.y += diff;
        }
    }

    return pos;
}

Vector2 gui_layout_widget(Vector2 *required_size, GuiAnchor anchor)
{
    GuiLayout *layout = gui_current_layout();
    Vector2 pos = gui_layout_widget(*required_size, anchor);

    switch (layout->type) {
    case GUI_LAYOUT_ROW:
        required_size->x = layout->available_space.x;
        break;
    case GUI_LAYOUT_COLUMN:
        required_size->y = layout->available_space.y;
        break;
    case GUI_LAYOUT_ABSOLUTE:
        break;
    }

    return pos;
}

void gui_draw_button(GuiId id, Vector2 pos, Vector2 size)
{
    GuiWindow *wnd = gui_current_window();

    Vector3 btn_bg = gui.pressed == id ? gui.style.bg_press : gui.hot == id ? gui.style.bg_hot : gui.style.bg;
    Vector3 btn_bg_acc0 = gui.pressed == id ? rgb_mul(btn_bg, 0.6f) : rgb_mul(btn_bg, 1.25f);
    Vector3 btn_bg_acc1 = gui.pressed == id ? rgb_mul(btn_bg, 1.25f) : rgb_mul(btn_bg, 0.6f);

    gui_draw_rect(pos, { size.x - 1.0f, 1.0f }, wnd->clip_rect, btn_bg_acc0);
    gui_draw_rect(pos, { 1.0f, size.y - 1.0f }, wnd->clip_rect, btn_bg_acc0);

    gui_draw_rect(pos + Vector2{ size.x - 1.0f, 0.0f }, { 1.0f, size.y }, wnd->clip_rect, btn_bg_acc1);
    gui_draw_rect(pos + Vector2{ 0.0f, size.y - 1.0f }, { size.x, 1.0f }, wnd->clip_rect, btn_bg_acc1);

    gui_draw_rect(pos + Vector2{ 1.0f, 1.0f }, size - Vector2{ 2.0f, 2.0f }, wnd->clip_rect, btn_bg);
}

void gui_draw_accent_button(GuiId id, Vector2 pos, Vector2 size)
{
    GuiWindow *wnd = gui_current_window();

    Vector3 btn_bg = gui.pressed == id ? gui.style.accent_bg_press : gui.hot == id ? gui.style.accent_bg_hot : gui.style.accent_bg;
    Vector3 btn_bg_acc0 = gui.pressed == id ? rgb_mul(btn_bg, 0.6f) : rgb_mul(btn_bg, 1.25f);
    Vector3 btn_bg_acc1 = gui.pressed == id ? rgb_mul(btn_bg, 1.25f) : rgb_mul(btn_bg, 0.6f);

    gui_draw_rect(pos, { size.x - 1.0f, 1.0f }, wnd->clip_rect, btn_bg_acc0);
    gui_draw_rect(pos, { 1.0f, size.y - 1.0f }, wnd->clip_rect, btn_bg_acc0);

    gui_draw_rect(pos + Vector2{ size.x - 1.0f, 0.0f }, { 1.0f, size.y }, wnd->clip_rect, btn_bg_acc1);
    gui_draw_rect(pos + Vector2{ 0.0f, size.y - 1.0f }, { size.x, 1.0f }, wnd->clip_rect, btn_bg_acc1);

    gui_draw_rect(pos + Vector2{ 1.0f, 1.0f }, size - Vector2{ 2.0f, 2.0f }, wnd->clip_rect, btn_bg);
}


bool gui_button_id(GuiId id, Vector2 pos, Vector2 size)
{
    gui_hot_rect(id, pos, size);
    bool clicked = gui_clicked(id);
    gui_draw_button(id, pos, size);
    return clicked;
}

bool gui_button_id(GuiId id, Vector2 pos, Vector2 size, GLuint icon)
{
    bool clicked = gui_button_id(id, pos, size);

    Vector2 margin{ 4, 4 };
    Vector2 icon_s = size - margin;
    Vector2 icon_p = pos + 0.5f*(size-icon_s);
    gui_draw_rect(icon_p, icon_s, icon);
    return clicked;
}


bool gui_button_id(GuiId id, FontAtlas *font, TextQuadsAndBounds td, Vector2 size)
{
    GuiWindow *wnd = gui_current_window();
    Vector2 pos = gui_layout_widget(size);

    bool clicked = gui_button_id(id, pos, size);

    Rect clip_rect{ wnd->clip_rect };
    Vector2 text_center{ td.bounds.size.x * 0.5f, font->line_height*0.5f };
    Vector2 btn_center = size * 0.5f;
    Vector2 text_offset = btn_center - text_center;

    gui_draw_text(td.glyphs, pos + text_offset, clip_rect, gui.style.fg, font);
    return clicked;
}

bool gui_button_id(GuiId id, String text, Vector2 size)
{
    TextQuadsAndBounds td = calc_text_quads_and_bounds(text, &gui.style.button.font);
    return gui_button_id(id, &gui.style.button.font, td, size);
}

bool gui_button_id(GuiId id, String text)
{
    TextQuadsAndBounds td = calc_text_quads_and_bounds(text, &gui.style.button.font);
    return gui_button_id(id, &gui.style.button.font, td, td.bounds.size + Vector2{ 24.0f, 12.0f });
}

bool gui_checkbox_id(GuiId id, String label, bool *checked)
{
    bool toggled = false;

    id.internal = id.owner;
    id.owner = gui.current_id.owner;

    GuiWindow *wnd = &gui.windows[gui.current_window];

    TextQuadsAndBounds td = calc_text_quads_and_bounds(label, &gui.style.text.font);

    Vector2 btn_margin{ 5.0f, 0.0f };
    Vector2 btn_size{ 16.0f, 16.0f };
    Vector2 inner_size{ 16.0f, 16.0f };
    Vector2 border_size{ 1.0f, 1.0f };
    Vector2 inner_margin = 0.5f*(btn_size - inner_size);
    Vector2 btn_offset{ 0.0f, floorf(0.5f*(gui.style.text.font.line_height - btn_size.y)) };

    Vector2 size{ td.bounds.size.x + btn_size.x + btn_margin.x, MAX(btn_size.y, td.bounds.size.y) };
    Vector2 pos = gui_layout_widget(&size);

    Vector2 btn_pos = pos + btn_offset;

    gui_hot_rect(id, pos, size);
    if (gui_clicked(id)) {
        *checked = !(*checked);
        toggled = true;
    }

    Vector3 border_col = rgb_unpack(0xFFCCCCCC);
    Vector3 bg_col = gui.pressed == id ? rgb_unpack(0xFF2C2C2C) : gui.hot == id ? rgb_unpack(0xFF3A3A3A) : rgb_unpack(0xFF1d2021);
    Vector3 checked_col = rgb_unpack(0xFFCCCCCC); (void)checked_col;
    Vector3 label_col = rgb_unpack(0xFFFFFFFF);

    gui_draw_rect(btn_pos, btn_size, wnd->clip_rect, border_col);
    gui_draw_rect(btn_pos + border_size, btn_size - 2.0f*border_size, wnd->clip_rect, bg_col);
    if (*checked) gui_draw_rect(btn_pos + inner_margin, inner_size, gui.icons.check);
    gui_draw_text(td.glyphs, pos + Vector2{ btn_size.x + btn_margin.x, 0.0 }, wnd->clip_rect, label_col, &gui.style.text.font);

    return toggled;
}

GuiEditboxAction gui_editbox_id(GuiId id, String initial_string, Vector2 pos, Vector2 size)
{
    u32 action = GUI_EDITBOX_NONE;

    GuiWindow *wnd = &gui.windows[gui.current_window];

    bool was_focused = gui.focused == id;
    gui_handle_focus_grabbing(id);

    gui_hot_rect(id, pos, size);
    if (gui_pressed(id)) gui.focused = id;

    if (gui.focused == id && !was_focused) {
        ASSERT(initial_string.length < (i32)sizeof gui.edit.buffer);
        memcpy(gui.edit.buffer, initial_string.data, initial_string.length);
        gui.edit.length = initial_string.length;

        gui.edit.offset = gui.edit.cursor = gui.edit.selection = 0;
    }
    
    if (gui.focused == id && gui_capture(gui.capture_text) && gui_capture(gui.capture_keyboard)) {
        for (InputEvent e : gui.events) {
            switch (e.type) {
            case IE_KEY_PRESS:
                switch (e.key.keycode) {
                case KC_LEFT:
                    gui.edit.cursor = utf8_decr({ gui.edit.buffer, gui.edit.length }, gui.edit.cursor);
                    gui.edit.cursor = MAX(0, gui.edit.cursor);
                    gui.edit.selection = gui.edit.cursor;
                    break;
                case KC_RIGHT:
                    gui.edit.cursor = utf8_incr({ gui.edit.buffer, gui.edit.length }, gui.edit.cursor);
                    gui.edit.selection = gui.edit.cursor;
                    break;
                case KC_A:
                    if (e.key.modifiers == MF_CTRL) {
                        gui.edit.selection = 0;
                        gui.edit.cursor = gui.edit.length;
                    } break;
                case KC_C:
                    if (e.key.modifiers == MF_CTRL &&
                        gui.edit.cursor != gui.edit.selection)
                    {
                        i32 start = MIN(gui.edit.cursor, gui.edit.selection);
                        i32 end = MAX(gui.edit.cursor, gui.edit.selection);

                        String selected = slice({ gui.edit.buffer, gui.edit.length }, start, end);
                        set_clipboard_data(selected);
                    } break;
                case KC_V:
                    if (e.key.modifiers == MF_CTRL) {
                        i32 start = MIN(gui.edit.cursor, gui.edit.selection);
                        i32 end = MAX(gui.edit.cursor, gui.edit.selection);

                        i32 limit = sizeof gui.edit.buffer - (gui.edit.length - (end-start));

                        String str = read_clipboard_str();
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
                        action |= GUI_EDITBOX_CHANGE;
                    } break;
                case KC_DELETE:
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
                        action |= GUI_EDITBOX_CHANGE;
                    } break;
                case KC_BACKSPACE:
                    if (gui.edit.cursor != gui.edit.selection || gui.edit.cursor > 0) {
                        i32 start = MIN(gui.edit.cursor, gui.edit.selection);
                        i32 end = MAX(gui.edit.cursor, gui.edit.selection);
                        if (start == end) start = utf8_decr({ gui.edit.buffer, gui.edit.length }, start);

                        memmove(gui.edit.buffer+start, gui.edit.buffer+end, gui.edit.length-(end-start));
                        gui.edit.length = gui.edit.length-(end-start);
                        gui.edit.cursor = gui.edit.selection = start;
                        gui.edit.offset = MIN(gui.edit.offset, codepoint_index_from_byte_index({ gui.edit.buffer, gui.edit.length}, gui.edit.cursor));
                        action |= GUI_EDITBOX_CHANGE;
                    }  break;
                case KC_ENTER:
                    action |= GUI_EDITBOX_FINISH;
                    gui.focused = GUI_ID_INVALID;
                    break;
                case KC_ESC:
                    action |= GUI_EDITBOX_CANCEL;
                    gui.focused = GUI_ID_INVALID;
                    break;

                default: break;
                }
                break;
            case IE_TEXT:
                if (gui.edit.cursor != gui.edit.selection) {
                    i32 start = MIN(gui.edit.cursor, gui.edit.selection);
                    i32 end = MAX(gui.edit.cursor, gui.edit.selection);
                    if (start == end) end = utf8_incr({ gui.edit.buffer, gui.edit.length }, end);

                    memmove(gui.edit.buffer+start, gui.edit.buffer+end, gui.edit.length-start);
                    gui.edit.length -= end-start;
                    gui.edit.cursor = gui.edit.selection = start;

                    i32 new_offset = codepoint_index_from_byte_index({ gui.edit.buffer, gui.edit.length }, gui.edit.cursor);
                    gui.edit.offset = MIN(gui.edit.offset, new_offset);
                    action |= GUI_EDITBOX_CHANGE;
                }

                if (gui.edit.length+e.text.length <= (i32)sizeof gui.edit.buffer) {
                    for (i32 i = gui.edit.length + e.text.length-1; i > gui.edit.cursor; i--) {
                        gui.edit.buffer[i] = gui.edit.buffer[i-e.text.length];
                    }

                    memcpy(gui.edit.buffer+gui.edit.cursor, e.text.c, e.text.length);
                    gui.edit.length += e.text.length;
                    gui.edit.cursor += e.text.length;
                    gui.edit.selection = gui.edit.cursor;
                    action |= GUI_EDITBOX_CHANGE;
                }
                break;

            default: break;
            }

            if (gui.edit.length == sizeof gui.edit.buffer) break;
        }
    }

    Vector2 border{ 1, 1 };

    Vector3 selection_bg = gui.style.bg_light0;

    Vector3 bg = gui.style.bg_dark0;
    Vector3 border_col = gui.focused == id ? gui.style.accent_bg : gui.style.bg_light0;
    Vector3 fg = gui.style.fg;

    Vector2 text_pos = pos+border+gui.style.edit.padding;

    Rect text_clip_rect;
#if 1
    text_clip_rect.pos = text_pos;
    text_clip_rect.size = size-2*border-2*gui.style.edit.padding;
#else
    text_clip_rect.pos.x = MAX(wnd->clip_rect.pos.x, text_pos.x);
    text_clip_rect.pos.y = MAX(wnd->clip_rect.pos.y, text_pos.y);
    text_clip_rect.size.x = MIN(text_pos.x + text_pos.x, wnd->clip_rect.pos.x + wnd->clip_rect.size.x) - text_clip_rect.pos.x;
    text_clip_rect.size.y = MIN(text_pos.y + text_pos.y, wnd->clip_rect.pos.y + wnd->clip_rect.size.y) - text_clip_rect.pos.y;
#endif

    String str = gui.focused == id ? String{ gui.edit.buffer, gui.edit.length } : initial_string;
    Array<GlyphRect> glyphs = calc_text_quads(str, &gui.style.text.font);

    gui_draw_rect(pos, size, wnd->clip_rect, border_col);
    gui_draw_rect(pos+border, size-2*border, wnd->clip_rect, bg);

    Vector2 cursor_pos = text_pos;
    Vector2 text_pan{};

    if (glyphs.count > 0) {
        i32 offset = gui.focused == id ? gui.edit.offset : 0;

        if (gui.focused == id) {
            text_pan.x = gui.edit.offset < glyphs.count ?
                -glyphs[gui.edit.offset].x0 :
                -glyphs[gui.edit.offset-1].x1;

            if (gui_pressed(id)) {
                i32 new_cursor;
                for (new_cursor = 0; new_cursor < glyphs.count; new_cursor++) {
                    GlyphRect g = glyphs.data[new_cursor];
                    Vector2 mouse = { (f32)gui.mouse.x, (f32)gui.mouse.y };
                    Vector2 rel = mouse - pos;
                    if (rel.x >= g.x0 + text_pan.x && rel.x <= g.x1 + text_pan.x) {
                        break;
                    }
                }
                gui.edit.cursor = byte_index_from_codepoint_index(str, new_cursor);
                gui.edit.selection = gui.edit.cursor;
            } else if (gui.pressed == id) {
                i32 new_selection;
                for (new_selection = 0; new_selection < glyphs.count; new_selection++) {
                    GlyphRect g = glyphs.data[new_selection];
                    Vector2 mouse = { (f32)gui.mouse.x, (f32)gui.mouse.y };
                    Vector2 rel = mouse - pos;
                    if (rel.x >= g.x0 + text_pan.x && rel.x <= g.x1 + text_pan.x) {
                        break;
                    }
                }
                gui.edit.selection = byte_index_from_codepoint_index(str, new_selection);
            }

            i32 cursor_ci = codepoint_index_from_byte_index(str, gui.edit.cursor);
            cursor_pos.x += cursor_ci < glyphs.count ? glyphs[cursor_ci].x0 : glyphs[cursor_ci-1].x1;
            while ((cursor_pos.x + text_pan.x) > (text_clip_rect.pos.x + text_clip_rect.size.x)) {
                gui.edit.offset++;
                text_pan = { gui.edit.offset < glyphs.count ? -glyphs[gui.edit.offset].x0 : 0.0f, 0.0f };
            }

            while ((cursor_pos.x + text_pan.x < text_clip_rect.pos.x)) {
                gui.edit.offset--;
                text_pan = { gui.edit.offset < glyphs.count ? -glyphs[gui.edit.offset].x0 : 0.0f, 0.0f };
            }

            // TODO(jesper): this completely broken, fix it
            if (gui.edit.selection != gui.edit.cursor) {
                f32 x0 = cursor_pos.x;
                f32 x1 = pos.x + 1.0f;

                i32 selection_ci = codepoint_index_from_byte_index(str, gui.edit.selection);
                x1 += selection_ci < glyphs.count ? glyphs[selection_ci].x0 : glyphs[selection_ci-1].x1;

                if (x0 > x1) SWAP(x0, x1);

                gui_draw_rect(
                    Vector2{ x0, cursor_pos.y } + text_pan,
                    Vector2{ x1-x0, gui.style.text.font.line_height },
                    wnd->clip_rect,
                    selection_bg);
            }
        }

        gui_draw_text(slice(glyphs, offset), text_pos + text_pan, text_clip_rect, fg, &gui.style.text.font);
    }

    if (gui.focused == id) {
        gui_draw_rect(
            cursor_pos + text_pan,
            Vector2{ 1.0f, gui.style.text.font.line_height },
            wnd->clip_rect, fg);
    }

    return (GuiEditboxAction)action;
}

GuiEditboxAction gui_editbox_id(GuiId id, String initial_string)
{
    Vector2 size{ 20.0f, gui.style.text.font.line_height+2*gui.style.edit.padding.y };
    Vector2 pos = gui_layout_widget(&size);
    return gui_editbox_id(id, initial_string, pos, size);
}


GuiEditboxAction gui_editbox_id(GuiId id, String initial_string, f32 width)
{
    Vector2 size{ width, gui.style.text.font.line_height+gui.style.edit.padding.y };
    Vector2 pos = gui_layout_widget(size);
    return gui_editbox_id(id, initial_string, pos, size);
}

#if 0
GuiEditboxAction gui_editbox_id(GuiId id, String label, String initial_string, f32 width)
{
    f32 margin = 5.0f;

    GuiWindow *wnd = &gui.windows[gui.current_window];

    Vector2 padding = gui.style.edit.padding;
    Vector2 size = Vector2{ width, gui.style.text.font.line_height + padding.y }

    TextQuadsAndBounds td = calc_text_quads_and_bounds(label, &gui.style.text.font);
    Vector2 pos = gui_layout_widget({ td.bounds.size.x + size.x + margin, MAX3(td.bounds.size.y, size.y, gui.style.text.font.line_height) });
    gui_draw_text(td.glyphs, pos, wnd->clip_rect, { 1.0f, 1.0f, 1.0f }, &gui.style.text.font);
    return gui_editbox_id(id, initial_string, { pos.x + td.bounds.size.x + margin, pos.y }, size);
}

GuiEditboxAction gui_editbox_id(GuiId id, f32 *value, f32 width)
{
    char buffer[100];

    String str = gui.focused == id ?
        String{ gui.edit.buffer, gui.edit.length } :
        stringf(buffer, sizeof buffer, "%f", *value);

    GuiEditboxAction action = gui_editbox_id(id, str, width);
    if (action & GUI_EDITBOX_FINISH) f32_from_string({ gui.edit.buffer, gui.edit.length }, value);

    return action;
}

GuiEditboxAction gui_editbox_id(GuiId id, f32 *value, Vector2 pos, f32 width)
{
    char buffer[100];

    String str = gui.focused == id ?
        String{ gui.edit.buffer, gui.edit.length } :
        stringf(buffer, sizeof buffer, "%f", *value);

    GuiEditboxAction action = gui_editbox_id(id, str, pos, width);
    if (action & GUI_EDITBOX_FINISH) f32_from_string({ gui.edit.buffer, gui.edit.length }, value);

    return action;
}


GuiEditboxAction gui_editbox_id(GuiId id, String label, f32 *value, f32 width)
{
    f32 margin = 5.0f;

    GuiWindow *wnd = &gui.windows[gui.current_window];

    TextQuadsAndBounds td = calc_text_quads_and_bounds(label, &gui.style.text.font);
    Vector2 pos = gui_layout_widget({ td.bounds.size.x + size.x + margin, MAX3(td.bounds.size.y, size.y, gui.style.text.font.line_height) });
    gui_draw_text(td.glyphs, pos, wnd->clip_rect, { 1.0f, 1.0f, 1.0f }, &gui.style.text.font);
    return gui_editbox_id(id, value, Vector2{ pos.x + td.bounds.size.x + margin, pos.y }, size);
}
#endif

GuiLister* gui_find_or_create_lister(GuiId id)
{
    for (auto &l : gui.listers) if (l.id == id) return &l;
    i32 i = array_add(&gui.listers, GuiLister{ .id = id });
    return &gui.listers[i];
}

GuiMenu* gui_find_or_create_menu(GuiId id, Vector2 initial_size)
{
    for (auto &it : gui.menus) {
        if (it.id == id) return &it;
    }
    i32 i = array_add(&gui.menus, GuiMenu{ .id = id, .parent_wnd = gui.current_window, .size = initial_size, .active = false });
    return &gui.menus[i];
}

GuiMenu* gui_find_menu(GuiId id)
{
    for (auto &it : gui.menus) if (it.id == id) return &it;
    return nullptr;
}

i32 gui_create_window(GuiId id, u32 flags, Vector2 pos, Vector2 size)
{
    for (i32 i = 0; i < gui.windows.count; i++) {
        if (gui.windows[i].id == id) {
            if (!(flags & GUI_WINDOW_MOVABLE)) gui.windows[i].pos = pos;
            if (!(flags & GUI_WINDOW_RESIZABLE)) gui.windows[i].size = size;

            return i;
        }
    }

    GuiWindow wnd{};
    wnd.id = id;
    wnd.flags = flags;
    wnd.pos = pos;
    wnd.size = size;
    wnd.command_buffer.commands.alloc = mem_frame;

    if (gui.focused_window == GUI_ID_INVALID) gui.focused_window = id;

    return array_add(&gui.windows, wnd);
}

GuiWindow *gui_get_window(GuiId id)
{
    for (i32 i = 0; i < gui.windows.count; i++) {
        if (gui.windows[i].id == id) return &gui.windows[i];
    }

    return nullptr;
}

bool gui_window_close_button(GuiId id, Vector2 wnd_pos, Vector2 wnd_size)
{
    GuiWindow *wnd = gui_current_window();
    ASSERT(wnd);

    Vector2 close_size = gui.style.window.close_size + Vector2{ 4, 4 };
    Vector2 close_pos{ wnd_pos.x + wnd_size.x - close_size.x - gui.style.window.border.x, wnd_pos.y + gui.style.window.border.y };

    Vector2 icon_size = gui.style.window.close_size;
    Vector2 icon_pos = close_pos + (close_size - icon_size) * 0.5f;

    if (gui_hot_rect(id, close_pos, close_size)) {
        gui_draw_rect(close_pos, close_size, gui.style.window.close_bg_hot, &wnd->command_buffer);
    }
    bool clicked = gui_clicked(id);

    gui_draw_rect(icon_pos, icon_size, gui.icons.close);
    return clicked;
}

bool gui_begin_window_id(
    GuiId id,
    String title,
    Vector2 pos,
    Vector2 size,
    bool *visible,
    u32 flags)
{
    if (!*visible) return false;
    ASSERT(gui.current_window == GUI_BACKGROUND);

    if (!gui_begin_window_id(id, title, pos, size, flags | GUI_WINDOW_CLOSE)) {
        *visible = false;
    }

    return *visible;
}

bool gui_begin_window_id(
    GuiId id,
    String title,
    Vector2 pos,
    Vector2 size,
    bool visible,
    u32 flags)
{
    if (!visible) return false;
    ASSERT(gui.current_window == GUI_BACKGROUND);

    if (!gui_begin_window_id(id, title, pos, size, flags & ~GUI_WINDOW_CLOSE)) {
        visible = false;
    }

    return visible;
}

bool gui_begin_window_id(GuiId id, String title, GuiWindowState *state, u32 flags)
{
    return gui_begin_window_id(id, title, state->pos, state->size, &state->active, flags | GUI_WINDOW_CLOSE);
}

bool gui_begin_window_id(
    GuiId id,
    String title,
    Vector2 in_pos,
    Vector2 in_size,
    u32 flags)
{
    ASSERT(gui.current_window == GUI_BACKGROUND);

    i32 index = gui_create_window(id, flags, in_pos, in_size);
    GuiWindow *wnd = &gui.windows[index];
    gui.current_window = index;

    Vector2 window_border = gui.style.window.border;

    bool do_titlebar = !(flags & GUI_WINDOW_NO_TITLE);
    Vector2 title_size{};
    if (do_titlebar) title_size = { wnd->size.x - 2.0f*window_border.x, gui.style.window.title_height };

    GuiId title_id = GUI_ID_INTERNAL(id, 1);
    GuiId close_id = GUI_ID_INTERNAL(id, 30);

    // TODO(jesper): alt-drag on any part of the window
    if (flags & GUI_WINDOW_MOVABLE && do_titlebar) {
        gui_hot_rect(title_id, wnd->pos, title_size);
        if (gui_drag(title_id, wnd->pos) && (gui.mouse.dx != 0 || gui.mouse.dy != 0)) {
            Vector2 mouse = { (f32)gui.mouse.x, (f32)gui.mouse.y };
            wnd->pos = gui.drag_start_data + mouse - gui.drag_start_mouse;
        }
    }

    wnd->active = true;
    if (!wnd->last_active) {
        wnd = push_window_to_top(index);
        gui.focused_window = id;
    } else if (gui.focused_window != id &&
               gui.hot_window == id &&
               gui.mouse.left_pressed &&
               !gui.mouse.left_was_pressed)
    {
        wnd = push_window_to_top(index);
        gui.focused_window = id;
    }

    if (gui.hot_window != id &&
        gui.focused_window == id &&
        gui.mouse.left_pressed &&
        !gui.mouse.left_was_pressed)
    {
        gui.focused_window = GUI_ID_INVALID;
    }

    // NOTE(jesper): the window keyboard nav is split between begin/end because children of the window
    // should be given a chance to respond to certain key events first
    // I need to implement something that lets widgets mark events as handled in appropriate hierarchy order
    if (gui.focused_window == id && gui_capture(gui.capture_keyboard)) {
        for (InputEvent e : gui.events) {
            switch (e.type) {
            case IE_KEY_PRESS:
                switch (e.key.keycode) {
                case KC_Q:
                    if (e.key.modifiers == MF_CTRL) {
                        gui.focused_window = GUI_ID_INVALID;

                        if (gui.focused == id) gui.focused = GUI_ID_INVALID;

                        if (gui.current_window == index) gui.current_window = GUI_BACKGROUND;
                        return false;
                    }
                    break;
                default: break;
                }
                break;
            default: break;
            }
        }
    }

    Vector3 title_bg = gui.focused_window != id
        ? gui.style.bg_dark0
        : gui.pressed == title_id ? gui.style.accent_bg_press : gui.hot == title_id ? gui.style.accent_bg_hot : gui.style.accent_bg;

    Vector2 title_pos = wnd->pos + window_border + Vector2{ 2.0f, 2.0f };

    gui_draw_rect(wnd->pos, wnd->size, title_bg);
    gui_draw_rect(wnd->pos + window_border, wnd->size - 2.0f*window_border, gui.style.bg_dark1);
    gui_draw_rect(wnd->pos + window_border, title_size, title_bg);
    gui_draw_text(title, title_pos, { title_pos, title_size }, gui.style.window.title_fg, &gui.style.text.font);

    if (flags & GUI_WINDOW_CLOSE && do_titlebar) {
        if (gui_window_close_button(close_id, wnd->pos, wnd->size)) {
            gui.focused = GUI_ID_INVALID;
            gui.current_window = GUI_BACKGROUND;
            return false;
        }
    }

    Vector2 layout_pos = wnd->pos + window_border + Vector2{ 2.0f, title_size.y + 2.0f } + gui.style.window.margin;
    Vector2 layout_size = wnd->size - 2.0f*window_border - Vector2{ 2.0f, title_size.y + 2.0f } - 2*gui.style.window.margin;

    if (flags & GUI_WINDOW_RESIZABLE) {
        // NOTE(jesper): we're adding some internal margin to the layout to make room
        // for the resize widget
        layout_pos.x += 3.0f;
        layout_pos.y += 3.0f;
        layout_size.x -= 6.0f;
        layout_size.y -= 6.0f;
    }

    GuiLayout layout{
        .type = GUI_LAYOUT_ROW,
        .pos = layout_pos,
        .size = layout_size,
        .row.spacing = 2.0f
    };

    gui_begin_layout(layout);
    wnd->clip_rect = gui_layout_rect();
    return true;
}

void gui_end_window()
{
    ASSERT(gui.current_window > GUI_OVERLAY);

    GuiWindow *wnd = &gui.windows[gui.current_window];
    if (wnd->flags & GUI_WINDOW_RESIZABLE) {
        // NOTE(jesper): we handle the window resizing at the end to ensure it's always on
        // top and has the highest input priority in the window.

        Vector2 window_border = { 1.0f, 1.0f };
        Vector2 title_size = { wnd->size.x - 2.0f*window_border.x, 20.0f };
        Vector2 min_size = { 200.0f, title_size.y + 2.0f*window_border.y + 5.0f };

        GuiId resize_id = GUI_ID_INTERNAL(wnd->id, 2);

        Vector2 mouse = { (f32)gui.mouse.x, (f32)gui.mouse.y };

        Vector2 br = wnd->pos + wnd->size - window_border;
        Vector2 resize_tr{ br.x, br.y - 10.0f };
        Vector2 resize_br{ br.x, br.y };

        Vector2 resize_bl{ br.x - 10.0f, br.y };

        if (gui.hot == resize_id) {
            if (gui.pressed != resize_id && gui.mouse.left_pressed && !gui.mouse.left_was_pressed) {
                gui.pressed = resize_id;
                gui_drag_start(wnd->size);
            } else if (gui.pressed == resize_id && !gui.mouse.left_pressed) {
                gui.pressed = GUI_ID_INVALID;
            } else if (gui.pressed == resize_id && (gui.mouse.dx != 0 || gui.mouse.dy != 0)) {
                Vector2 nsize = gui.drag_start_data + mouse - gui.drag_start_mouse;
                wnd->size.x = MAX(min_size.x, nsize.x);
                wnd->size.y = MAX(min_size.y, nsize.y);

                br = wnd->pos + wnd->size - window_border;
                resize_tr = { br.x, br.y - 10.0f };
                resize_br = { br.x, br.y };
                resize_bl = { br.x - 10.0f, br.y };
            }
        }

        if (gui.pressed == resize_id ||
            point_in_triangle(mouse, resize_tr, resize_br, resize_bl))
        {
            if (gui.next_hot == GUI_ID_INVALID) {
                gui.next_hot = resize_id;
            }
        }

        Vector3 resize_bg = gui.hot == resize_id ? rgb_unpack(0xFFFFFFFF) : rgb_unpack(0xFF5B5B5B);
        gfx_draw_triangle(resize_tr, resize_br, resize_bl, resize_bg, &wnd->command_buffer);
    }
    //gui_hot_rect(wnd->id, wnd->pos, wnd->size);

    // NOTE(jesper): the window keyboard nav is split between begin/end because children of the window
    // should be given a chance to respond to certain key events first
    // I need to implement something that lets widgets mark events as handled in appropriate hierarchy order
    if (gui.focused_window == wnd->id && gui_capture(gui.capture_keyboard)) {
        for (InputEvent e : gui.events) {
            switch (e.type) {
            case IE_KEY_PRESS:
                switch (e.key.keycode) {
                case KC_ESC:
                    gui.focused_window = GUI_ID_INVALID;
                    break;
                default: break;
                }
                break;
            default: break;
            }
        }
    }

    gui_end_layout();
    gui.current_window = GUI_BACKGROUND;
}

GuiGizmoAction gui_2d_gizmo_translate_axis_id(GuiId id, Camera camera, Vector2 *position, Vector2 axis)
{
    GuiGizmoAction action = GUI_GIZMO_NONE;

    // TODO(jesper): these wigets need to support a rotated and translated camera
    GfxCommandBuffer *cmdbuf = &gui.windows[GUI_BACKGROUND].command_buffer;

    Vector2 n{ axis.y, -axis.x };

    // base
    Vector2 br = *position + 0.5f*GIZMO_BAR_SIZE.y*n + axis*GIZMO_BAR_SIZE.x;
    Vector2 tr = *position - 0.5f*GIZMO_BAR_SIZE.y*n + axis*GIZMO_BAR_SIZE.x;
    Vector2 tl = *position - 0.5f*GIZMO_BAR_SIZE.y*n;
    Vector2 bl = *position + 0.5f*GIZMO_BAR_SIZE.y*n;

    // tip
    // NOTE(jesper): -1 axis to cause a 1px overlap in base and tip to not cause
    // any wonkyness with mouse dragging across the arrow
    Vector2 t0 = *position + axis*GIZMO_BAR_SIZE.x - 0.5f*GIZMO_TIP_SIZE.y*n - axis;
    Vector2 t1 = *position + axis*GIZMO_BAR_SIZE.x + axis*GIZMO_TIP_SIZE.x - axis;
    Vector2 t2 = *position + axis*GIZMO_BAR_SIZE.x + 0.5f*GIZMO_TIP_SIZE.y*n - axis;

    Vector2 mouse = ws_from_ss(Vector2{ (f32)gui.mouse.x, (f32)gui.mouse.y } + camera.position.xy);

    if (gui.pressed == id && !gui.mouse.left_pressed) {
        gui.pressed = GUI_ID_INVALID;
        action = GUI_GIZMO_END;
    }

    if (gui.pressed != id &&
        gui.hot == id &&
        gui.mouse.left_pressed &&
        !gui.mouse.left_was_pressed)
    {
        gui.pressed = id;
        action = GUI_GIZMO_BEGIN;
        gui_drag_start(*position);
    }
    if (gui.pressed == id ||
        point_in_rect(mouse, tl, tr, br, tl) ||
        point_in_triangle(mouse, t0, t1, t2))
    {
        gui_hot(id);
    }

    if (gui.pressed == id) {
        Vector2 d = mouse - gui.drag_start_mouse;
        *position = gui.drag_start_data + dot(axis, d) * axis;
    }

    Vector3 color{ .rg = absv(axis), ._b = 0 };
    if (gui.hot == id) color = 0.5f*(color + Vector3{ 1.0f, 1.0f, 1.0f });

    gfx_draw_square(tl, tr, br, bl, color, cmdbuf);
    gfx_draw_triangle(t0, t1, t2, color, cmdbuf);

    Vector2 points[] = { tl, tr, t0, t1, t2, br, bl };
    gfx_draw_line_loop(points, ARRAY_COUNT(points), GIZMO_OUTLINE_COLOR, cmdbuf);
    return action;
}

GuiGizmoAction gui_2d_gizmo_size_axis_id(GuiId id, Camera camera, Vector2 position, Vector2 *size, Vector2 axis)
{
    GuiGizmoAction action = GUI_GIZMO_NONE;

    // TODO(jesper): these wigets need to support a rotated and translated camera
    GfxCommandBuffer *cmdbuf = &gui.windows[GUI_BACKGROUND].command_buffer;

    Vector2 n{ axis.y, -axis.x };

    // TODO(jesper): this is a but ugly to do a yflip on the visual representation of the axis, needed because
    // of the y-flipped mouse vs gl-coordinates and the way I do the size calculations
    Vector2 vaxis{ axis.x, -axis.y };

    // base
    Vector2 br = position + 0.5f*GIZMO_BAR_SIZE.y*n + vaxis*GIZMO_BAR_SIZE.x;
    Vector2 tr = position - 0.5f*GIZMO_BAR_SIZE.y*n + vaxis*GIZMO_BAR_SIZE.x;
    Vector2 tl = position - 0.5f*GIZMO_BAR_SIZE.y*n;
    Vector2 bl = position + 0.5f*GIZMO_BAR_SIZE.y*n;

    // tip
    // NOTE(jesper): -1 axis to cause a 1px overlap in base and tip to not cause
    // any wonkyness with mouse dragging across the arrow
    Vector2 t0 = position + vaxis*GIZMO_BAR_SIZE.x - 0.5f*GIZMO_TIP_SIZE.y*n - axis;
    Vector2 t1 = position + vaxis*GIZMO_BAR_SIZE.x + 0.5f*GIZMO_TIP_SIZE.y*n - axis;
    Vector2 t2 = t1 + vaxis*GIZMO_TIP_SIZE.x;
    Vector2 t3 = t0 + vaxis*GIZMO_TIP_SIZE.x;

    Vector2 mouse = ws_from_ss(Vector2{ (f32)gui.mouse.x, (f32)gui.mouse.y } + camera.position.xy);

    if (gui.pressed == id && !gui.mouse.left_pressed) {
        gui.pressed = GUI_ID_INVALID;
        action = GUI_GIZMO_END;
    }

    if (gui.pressed != id &&
        gui.hot == id &&
        gui.mouse.left_pressed &&
        !gui.mouse.left_was_pressed)
    {
        gui.pressed = id;
        action = GUI_GIZMO_BEGIN;
        gui_drag_start(*size);
    }
    if (gui.pressed == id ||
        point_in_rect(mouse, tl, tr, br, bl) ||
        point_in_rect(mouse, t0, t1, t2, t3))
    {
        gui_hot(id);
    }

    if (gui.pressed == id) {
        Vector2 delta = axis*dot(mouse, axis) - axis*dot(gui.drag_start_mouse, axis);
        delta.y = -delta.y;
        *size = gui.drag_start_data + delta;
    }

    Vector3 color{ .rg = absv(axis), ._b = 0 };
    if (gui.hot == id) color = 0.5f*(color + Vector3{ 1.0f, 1.0f, 1.0f });

    gfx_draw_square(tl, tr, br, bl, color, cmdbuf);
    gfx_draw_square(t0, t1, t2, t3, color, cmdbuf);

    Vector2 points[] = { tl, tr, t0, t3, t2, t1, br, bl };
    gfx_draw_line_loop(points, ARRAY_COUNT(points), GIZMO_OUTLINE_COLOR, cmdbuf);

    return action;
}

GuiGizmoAction gui_2d_gizmo_size_axis_id(GuiId id, Camera camera, Vector2 position, Vector2 *size, Vector2 axis, f32 multiple)
{
    GuiGizmoAction action = GUI_GIZMO_NONE;

    // TODO(jesper): these wigets need to support a rotated camera
    GfxCommandBuffer *cmdbuf = &gui.windows[GUI_BACKGROUND].command_buffer;

    Vector2 n{ axis.y, -axis.x };

    // TODO(jesper): this is a bit ugly to do a yflip on the visual representation of the axis, needed because
    // of the y-flipped mouse vs gl-coordinates and the way I do the size calculations
    Vector2 vaxis{ axis.x, -axis.y };

    // base
    Vector2 br = position + 0.5f*GIZMO_BAR_SIZE.y*n + vaxis*GIZMO_BAR_SIZE.x;
    Vector2 tr = position - 0.5f*GIZMO_BAR_SIZE.y*n + vaxis*GIZMO_BAR_SIZE.x;
    Vector2 tl = position - 0.5f*GIZMO_BAR_SIZE.y*n;
    Vector2 bl = position + 0.5f*GIZMO_BAR_SIZE.y*n;

    // tip
    // NOTE(jesper): -1 axis to cause a 1px overlap in base and tip to not cause
    // any wonkyness with mouse dragging across the arrow
    Vector2 t0 = position + vaxis*GIZMO_BAR_SIZE.x - 0.5f*GIZMO_TIP_SIZE.y*n - vaxis;
    Vector2 t1 = position + vaxis*GIZMO_BAR_SIZE.x + 0.5f*GIZMO_TIP_SIZE.y*n - vaxis;
    Vector2 t2 = t1 + vaxis*GIZMO_TIP_SIZE.x;
    Vector2 t3 = t0 + vaxis*GIZMO_TIP_SIZE.x;

    Vector2 mouse = ws_from_ss(Vector2{ (f32)gui.mouse.x, (f32)gui.mouse.y } + camera.position.xy);

    if (gui.pressed == id && !gui.mouse.left_pressed) {
        gui.pressed = GUI_ID_INVALID;
        action = GUI_GIZMO_END;
    }

    if (gui.pressed != id &&
        gui.hot == id &&
        gui.mouse.left_pressed &&
        !gui.mouse.left_was_pressed)
    {
        gui.pressed = id;
        action = GUI_GIZMO_BEGIN;
        gui_drag_start(*size);
    }
    if (gui.pressed == id ||
        point_in_rect(mouse, tl, tr, br, bl) ||
        point_in_rect(mouse, t0, t1, t2, t3))
    {
        gui_hot(id);
    }

    if (gui.pressed == id) {
        Vector2 delta = axis*dot(mouse, axis) - axis*dot(gui.drag_start_mouse, axis);
        delta.x = round_to(delta.x, multiple);
        delta.y = round_to(delta.y, multiple);
        delta.y = -delta.y;
        *size = gui.drag_start_data + delta;
    }

    Vector3 color{ .rg = absv(axis), ._b = 0 };
    if (gui.hot == id) color = 0.5f*(color + Vector3{ 1.0f, 1.0f, 1.0f });

    gfx_draw_square(tl, tr, br, bl, color, cmdbuf);
    gfx_draw_square(t0, t1, t2, t3, color, cmdbuf);

    Vector2 points[] = { tl, tr, t0, t3, t2, t1, br, bl };
    gfx_draw_line_loop(points, ARRAY_COUNT(points), GIZMO_OUTLINE_COLOR, cmdbuf);

    return action;
}

GuiGizmoAction gui_2d_gizmo_translate_plane_id(GuiId id, Camera camera, Vector2 *position)
{
    GuiGizmoAction action = GUI_GIZMO_NONE;

    // TODO(jesper): these wigets need to support a rotated camera
    GfxCommandBuffer *cmdbuf = &gui.windows[GUI_BACKGROUND].command_buffer;

    Vector2 size{ 20.0f, 20.0f };

    Vector2 offset{ 10.0f, -10.0f };
    Vector2 br = *position + offset + Vector2{ size.x, 0.0f };
    Vector2 tr = *position + offset + Vector2{ size.x, -size.y };
    Vector2 tl = *position + offset + Vector2{ 0.0f, -size.y };
    Vector2 bl = *position + offset;

    Vector2 mouse = ws_from_ss(Vector2{ (f32)gui.mouse.x, (f32)gui.mouse.y } + camera.position.xy);

    if (gui.focused == id && !gui.mouse.left_pressed) {
        gui.focused = GUI_ID_INVALID;
        action = GUI_GIZMO_END;
    }

    if (gui.focused != id &&
        gui.hot == id &&
        gui.mouse.left_pressed &&
        !gui.mouse.left_was_pressed)
    {
        gui.focused = id;
        action = GUI_GIZMO_BEGIN;
        gui_drag_start(*position);
    }
    if (gui.focused == id || point_in_rect(mouse, tl, tr, br, bl)) {
        gui_hot(id);
    }

    if (gui.focused == id) {
        Vector2 d = mouse - gui.drag_start_mouse;
        *position = gui.drag_start_data + d;
    }

    Vector3 color{ 0.5f, 0.5f, 0.0f };
    if (gui.hot == id) color = { 1.0f, 1.0f, 0.0f };

    Vector2 points[] = { tl, tr, br, bl };
    gfx_draw_square(tl, tr, br, bl, color, cmdbuf);
    gfx_draw_line_loop(points, ARRAY_COUNT(points), GIZMO_OUTLINE_COLOR, cmdbuf);
    return action;
}

GuiGizmoAction gui_2d_gizmo_translate_id(GuiId id, Camera camera, Vector2 *position)
{
    GuiGizmoAction a0 = gui_2d_gizmo_translate_axis_id(GUI_ID_INTERNAL(id, 1), camera, position, { 1.0f, 0.0f });
    GuiGizmoAction a1 = gui_2d_gizmo_translate_axis_id(GUI_ID_INTERNAL(id, 2), camera, position, { 0.0f, -1.0f });
    GuiGizmoAction a2 = gui_2d_gizmo_translate_plane_id(GUI_ID_INTERNAL(id, 3), camera, position);
    return (GuiGizmoAction)MAX(a0, MAX(a1, a2));
}

GuiGizmoAction gui_2d_gizmo_translate_id(GuiId id, Camera camera, Vector2 *position, f32 multiple)
{
    GuiGizmoAction a0 = gui_2d_gizmo_translate_axis_id(GUI_ID_INTERNAL(id, 1), camera, position, { 1.0f, 0.0f });
    GuiGizmoAction a1 = gui_2d_gizmo_translate_axis_id(GUI_ID_INTERNAL(id, 2), camera, position, { 0.0f, -1.0f });
    GuiGizmoAction a2 = gui_2d_gizmo_translate_plane_id(GUI_ID_INTERNAL(id, 3), camera, position);

    position->x = round_to(position->x, 0.5f*multiple);
    position->y = round_to(position->y, 0.5f*multiple);

    return (GuiGizmoAction)MAX(a0, MAX(a1, a2));
}


GuiGizmoAction gui_2d_gizmo_size_id(GuiId id, Camera camera, Vector2 position, Vector2 *size)
{
    GuiGizmoAction a0 = gui_2d_gizmo_size_axis_id(GUI_ID_INTERNAL(id, 1), camera, position, size, { 1.0f, 0.0f });
    GuiGizmoAction a1 = gui_2d_gizmo_size_axis_id(GUI_ID_INTERNAL(id, 2), camera, position, size, { 0.0f, 1.0f });
    return (GuiGizmoAction)MAX(a0, a1);
}

GuiGizmoAction gui_2d_gizmo_size_id(GuiId id, Camera camera, Vector2 position, Vector2 *size, f32 multiple)
{
    GuiGizmoAction a0 = gui_2d_gizmo_size_axis_id(GUI_ID_INTERNAL(id, 1), camera, position, size, { 1.0f, 0.0f }, multiple);
    GuiGizmoAction a1 = gui_2d_gizmo_size_axis_id(GUI_ID_INTERNAL(id, 2), camera, position, size, { 0.0f, 1.0f }, multiple);
    return (GuiGizmoAction)MAX(a0, a1);
}

GuiGizmoAction gui_2d_gizmo_size_square_id(GuiId parent, Camera camera, Vector2 *center, Vector2 *size)
{
    GuiGizmoAction action = GUI_GIZMO_NONE;

    GfxCommandBuffer *cmdbuf = &gui.windows[GUI_BACKGROUND].command_buffer;

    Vector2 half_size = 0.5f * *size;
    Vector2 tl = *center - half_size;
    Vector2 br = *center + half_size;
    Vector2 tr{ br.x, tl.y };
    Vector2 bl{ tl.x, br.y };

    Vector2 gizmo_size{ 10.0f, 10.0f };

    Vector2 mouse = ws_from_ss(Vector2{ (f32)gui.mouse.x, (f32)gui.mouse.y } + camera.position.xy);

    Vector2 corners[] = { tl, br, tr, bl };

    GuiId corner_ids[] = {
        GUI_ID_INTERNAL(parent, 0),
        GUI_ID_INTERNAL(parent, 1),
        GUI_ID_INTERNAL(parent, 2),
        GUI_ID_INTERNAL(parent, 3)
    };

    for (i32 i = 0; i < ARRAY_COUNT(corners); i++) {
        GuiId id = corner_ids[i];

        if (gui.focused == id && !gui.mouse.left_pressed) {
            gui.focused = GUI_ID_INVALID;
            action = GUI_GIZMO_END;
        }

        if (gui.focused != id &&
            gui.hot == id &&
            gui.mouse.left_pressed &&
            !gui.mouse.left_was_pressed)
        {
            gui.focused = id;
            action = GUI_GIZMO_BEGIN;
            gui_drag_start(*size, *center);
        }
        if (gui.focused == id || point_in_rect(mouse, corners[i], gizmo_size)) {
            gui_hot(id);
        }

        Vector3 color{ 0.5f, 0.5f, 0.5f };
        if (gui.hot == id) color = { 1.0f, 1.0f, 1.0f };

        gfx_draw_square(corners[i], gizmo_size, Vector4{ .rgb = color, ._a = 1.0f }, cmdbuf);
    }

    if (gui.focused.owner == parent.owner && gui.focused.index == parent.index) {
        Vector2 delta = mouse - gui.drag_start_mouse;

        if (gui.focused == corner_ids[1] || gui.focused == corner_ids[2]) {
            size->x = gui.drag_start_data.x + delta.x;
            center->x = gui.drag_start_data1.x - 0.5f*gui.drag_start_data.x + 0.5f*size->x;
        } else {
            size->x = gui.drag_start_data.x - delta.x;
            center->x = gui.drag_start_data1.x + 0.5f*gui.drag_start_data.x - 0.5f*size->x;
        }

        if (gui.focused == corner_ids[0] || gui.focused == corner_ids[2]) {
            size->y = gui.drag_start_data.y - delta.y;
            center->y = gui.drag_start_data1.y + 0.5f*gui.drag_start_data.y - 0.5f*size->y;
        } else {
            size->y = gui.drag_start_data.y + delta.y;
            center->y = gui.drag_start_data1.y - 0.5f*gui.drag_start_data.y + 0.5f*size->y;
        }
    }

    return action;
}

GuiGizmoAction gui_2d_gizmo_size_square_id(GuiId parent, Camera camera, Vector2 *center, Vector2 *size, f32 multiple)
{
    GuiGizmoAction action = GUI_GIZMO_NONE;

    GfxCommandBuffer *cmdbuf = &gui.windows[GUI_BACKGROUND].command_buffer;

    Vector2 half_size = 0.5f * *size;
    Vector2 tl = *center - half_size;
    Vector2 br = *center + half_size;
    Vector2 tr{ br.x, tl.y };
    Vector2 bl{ tl.x, br.y };

    Vector2 gizmo_size{ 10.0f, 10.0f };

    Vector2 mouse = ws_from_ss(Vector2{ (f32)gui.mouse.x, (f32)gui.mouse.y } + camera.position.xy);

    Vector2 corners[] = { tl, br, tr, bl };

    GuiId corner_ids[] = {
        GUI_ID_INTERNAL(parent, 0),
        GUI_ID_INTERNAL(parent, 1),
        GUI_ID_INTERNAL(parent, 2),
        GUI_ID_INTERNAL(parent, 3)
    };

    bool corner_gizmo_active = false;
    for (i32 i = 0; i < ARRAY_COUNT(corners); i++) {
        GuiId id = corner_ids[i];

        if (gui.focused == id && !gui.mouse.left_pressed) {
            gui.focused = GUI_ID_INVALID;
            action = gui.drag_start_data != *size || gui.drag_start_data1 != *center ? GUI_GIZMO_END : GUI_GIZMO_CANCEL;
        }

        if (gui.focused != id &&
            gui.hot == id &&
            gui.mouse.left_pressed &&
            !gui.mouse.left_was_pressed)
        {
            gui.focused = id;
            action = GUI_GIZMO_BEGIN;

            center->x = round_to(center->x, 0.5f*multiple);
            center->y = round_to(center->y, 0.5f*multiple);

            size->x = round_to(size->x, multiple);
            size->y = round_to(size->y, multiple);
            gui_drag_start(*size, *center);
        }
        if (gui.focused == id || point_in_rect(mouse, corners[i], gizmo_size)) {
            gui_hot(id);
        }

        if (gui.focused == id) corner_gizmo_active = true;

        Vector3 color{ 0.5f, 0.5f, 0.0f };
        if (gui.hot == id) color = { 1.0f, 1.0f, 0.0f };

        gfx_draw_square(corners[i], gizmo_size, Vector4{ .rgb = color, ._a = 1.0f }, cmdbuf);
        gfx_draw_line_square(corners[i], gizmo_size, GIZMO_OUTLINE_COLOR, cmdbuf);
    }

    if (corner_gizmo_active) {
        Vector2 delta = mouse - gui.drag_start_mouse;
        delta.x = round_to(delta.x, multiple);
        delta.y = round_to(delta.y, multiple);

        if (gui.focused == corner_ids[1] || gui.focused == corner_ids[2]) {
            size->x = gui.drag_start_data.x + delta.x;
            center->x = gui.drag_start_data1.x - 0.5f*gui.drag_start_data.x + 0.5f*size->x;
        } else {
            size->x = gui.drag_start_data.x - delta.x;
            center->x = gui.drag_start_data1.x + 0.5f*gui.drag_start_data.x - 0.5f*size->x;
        }

        if (gui.focused == corner_ids[0] || gui.focused == corner_ids[2]) {
            size->y = gui.drag_start_data.y - delta.y;
            center->y = gui.drag_start_data1.y + 0.5f*gui.drag_start_data.y - 0.5f*size->y;
        } else {
            size->y = gui.drag_start_data.y + delta.y;
            center->y = gui.drag_start_data1.y - 0.5f*gui.drag_start_data.y + 0.5f*size->y;
        }
    }

    GuiGizmoAction a0 = gui_2d_gizmo_size_axis_id(GUI_ID_INTERNAL(parent, 4), camera, *center, size, { 1.0f, 0.0f }, multiple);
    GuiGizmoAction a1 = gui_2d_gizmo_size_axis_id(GUI_ID_INTERNAL(parent, 5), camera, *center, size, { 0.0f, 1.0f }, multiple);
    action = (GuiGizmoAction)MAX(action, MAX(a0, a1));

    return action;
}

i32 gui_dropdown_id(GuiId id, Array<String> labels, i32 current_index)
{
    // TODO(jesper): maybe this should be essentially a gui_menu. At the very least, a lot
    // of functionality should be the same in regards to arrow up/down navigation, and press
    // then dragging mouse to an option with release to trigger it
    ASSERT(labels.count > 0);

    i32 index = current_index;

    GuiWindow *wnd = &gui.windows[gui.current_window];
    GfxCommandBuffer *cmdbuf = &wnd->command_buffer;

    FontAtlas *font = &gui.style.text.font;

    f32 margin = 4.0f;
    f32 border = 1.0f;

    Vector2 total_size{ 100.0f + margin + border, font->line_height + margin };
    Vector2 pos = gui_layout_widget(&total_size);

    // TODO(jesper): maybe this should be part of layout_widget procedures with some kind of flags
    Vector2 size{ total_size.x, font->line_height + margin };
    pos.y = floorf(pos.y + 0.5f*(total_size.y-size.y));

    Vector2 inner_size{ size.x - 2*border, size.y - 2*border };
    Vector2 inner_pos{ pos.x + border, pos.y + border };
    Vector2 text_pos{ inner_pos.x + 0.5f*margin, floorf(inner_pos.y + 0.5f*(inner_size.y - font->line_height)) };

    gui_hot_rect(id, pos, size);
    if (gui_pressed(id)) {
        if (gui.focused == id) {
            gui.focused = GUI_ID_INVALID;
        } else {
            gui.focused = id;
        }
    }

    f32 rhs = MIN(size.y, 16.0f);
    f32 rhs_margin = 4.0f;

    f32 rhs_y_o = floorf(0.5f*(inner_size.y-rhs));

    Vector2 rhs_p{ inner_pos.x + inner_size.x - rhs, inner_pos.y };
    Vector2 rhs_p0{ floorf(rhs_p.x + border + rhs_margin), inner_pos.y + rhs_margin + rhs_y_o };
    Vector2 rhs_p1{ rhs_p0.x + (rhs-2.0f*rhs_margin), rhs_p0.y };
    Vector2 rhs_p2{ rhs_p0.x + 0.5f*(rhs_p1.x - rhs_p0.x), rhs_p0.y + (rhs-2.0f*rhs_margin) };

    Vector3 border_col = gui.focused == id ? gui.style.accent_bg : gui.style.bg_light0;
    Vector3 rhs_col = gui.hot == id ? gui.style.accent_bg : gui.style.fg;
    Vector3 bg_col = gui.hot == id ? gui.style.bg_hot : gui.style.bg_dark0;

    gui_draw_rect(pos, size, wnd->clip_rect, border_col);
    gui_draw_rect(inner_pos, inner_size, wnd->clip_rect, bg_col);
    gui_draw_text(labels[index], text_pos, wnd->clip_rect, gui.style.fg, font);
    gui_draw_rect(rhs_p, { border, inner_size.y }, border_col);
    gfx_draw_triangle(rhs_p0, rhs_p1, rhs_p2, rhs_col, cmdbuf);

    // TODO(jesper): I think this needs to use something other than focused to keep track of whether
    // the menu is opened, as I want to use the focused id for tabbing and filtering certain input
    // events
    // I want focused to primarly act as tab-focus. If dropdowns should open when focused and user
    // presses a button, like enter or down-arrow
    if (gui.focused == id) {
        Vector2 p{ pos.x, pos.y + size.y };
        Vector2 inner_p{ p.x + border, p.y };
        Vector2 text_offset{ 0.5f*margin + border, 0.5f*margin };
        Vector2 text_p = p + text_offset;

        Vector2 total_s{ size.x, labels.count * font->line_height + border + 0.5f*margin };
        Vector2 inner_s{ total_s.x - 2*border, total_s.y - border };

        gui_draw_rect(p, total_s, gui.windows[GUI_OVERLAY].clip_rect, border_col, &gui.windows[GUI_OVERLAY].command_buffer);
        gui_draw_rect(inner_p, inner_s, gui.windows[GUI_OVERLAY].clip_rect, gui.style.bg_dark0, &gui.windows[GUI_OVERLAY].command_buffer);

        // TODO(jesper): a lot of this stuff is the same between dropdown, sub-menu, and lister
        for (i32 i = 0; i < labels.count; i++) {
            String label = labels[i];
            GuiId label_id = GUI_ID_INTERNAL(id, i);

            Vector2 rect_p{ inner_p.x, inner_p.y + font->line_height*i };
            Vector2 rect_s{ inner_s.x, font->line_height + border };

            if (gui.hot == label_id) gui_draw_rect(rect_p, rect_s, gui.style.bg_hot, &gui.windows[GUI_OVERLAY].command_buffer);
            gui_draw_text(label, text_p, { rect_p, rect_s }, { 1.0f, 1.0f, 1.0f }, font, &gui.windows[GUI_OVERLAY].command_buffer);
            text_p.y += font->line_height;

            gui_hot_rect(label_id, rect_p, rect_s);
            if ((gui.hot == label_id && gui.pressed == id && !gui.mouse.left_pressed) ||
                gui_clicked(label_id))
            {
                index = i;
                gui.focused = GUI_ID_INVALID;
            }
        }

        if (gui.hot.owner != id.owner &&
            gui.mouse.left_pressed && !gui.mouse.left_was_pressed)
        {
            gui.focused = GUI_ID_INVALID;
        }

    }

    return index;
}

i32 gui_dropdown_id(GuiId id, String* labels, i32 count, i32 current_index)
{
    Array<String> labels_arr{ .data = labels, .count = count };
    return gui_dropdown_id(id, labels_arr, current_index);
}

template<typename T>
i32 gui_dropdown_id(GuiId id, Array<String> labels, Array<T> values, T *value)
{
    i32 current_index = array_find_index(values, *value);
    ASSERT(current_index != -1);

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
    ASSERT(current_index != -1);

    i32 index = gui_dropdown_id(id, labels_arr, values_arr, current_index);
    if (index != current_index && value != nullptr) *value = values_arr[index];
    return index;
}

template<typename T>
T gui_dropdown_id(GuiId id, Array<String> labels, Array<T> values, T value)
{
    i32 current_index = array_find_index(values, value);
    ASSERT(current_index != -1);

    i32 index = gui_dropdown_id(id, labels, current_index);
    return values[index];
}

template<typename T>
T gui_dropdown_id(GuiId id, String *labels, T *values, i32 count, T value)
{
    Array<String> labels_arr{ .data = labels, .count = count };
    Array<T> values_arr{ .data = values, .count = count };

    i32 current_index = array_find_index(values_arr, value);
    ASSERT(current_index != -1);

    i32 index = gui_dropdown_id(id, labels_arr, current_index);
    return values_arr[index];
}

template<typename T>
T gui_dropdown_id(GuiId id, std::initializer_list<String> labels, std::initializer_list<T> values, T value)
{
    Array<String> labels_arr{ .data = (String*)labels.begin(), .count = (i32)labels.size() };
    Array<T> values_arr{ .data = (T*)values.begin(), .count = (i32)values.size() };

    i32 current_index = array_find_index(values_arr, value);
    ASSERT(current_index != -1);

    i32 index = gui_dropdown_id(id, labels_arr, current_index);
    return values_arr[index];
}

void gui_push_id(GuiId id)
{
    array_add(&gui.id_stack, gui.current_id);
    gui.current_id = id;
}

void gui_pop_id()
{
    ASSERT(gui.id_stack.count > 0);
    gui.current_id = gui.id_stack[gui.id_stack.count-1];
    gui.id_stack.count--;
}

bool gui_begin_menu_id(GuiId id)
{
    GuiWindow *wnd = &gui.windows[gui.current_window];
    GuiLayout *cl = gui_current_layout();

    Vector2 size{ cl->size.x, gui.style.text.font.line_height + 10.0f };
    Vector2 pos = gui_layout_widget(size);

    gui_find_or_create_menu(id, size);

    GuiLayout layout{
        .type = GUI_LAYOUT_COLUMN,
        .pos = pos,
        .size = size,
        .column.spacing = 1.0f,
    };
    gui_begin_layout(layout);

    Vector3 bg = rgb_unpack(0xFF212121);
    gui_draw_rect(layout.pos, layout.size, wnd->clip_rect, bg);

    gui_push_id(id);
    return true;
}

bool gui_begin_menu_id(GuiId id, String label)
{
    // TODO(jesper): need to figure out how to do nested menus with id_stack and whatever else
    ASSERT(gui.id_stack.count <= 1);

    GuiWindow *wnd = &gui.windows[gui.current_window];
    GuiLayout *cl = gui_current_layout();

    GuiMenu *menu = gui_find_or_create_menu(id, { 100.0f, 0.0f });
    menu->draw_index = gui.windows[GUI_OVERLAY].command_buffer.commands.count;

    TextQuadsAndBounds td = calc_text_quads_and_bounds(label, &gui.style.text.font);
    Vector2 text_offset{ 5.0f, 5.0f };

    Vector2 size{ td.bounds.size.x + 2.0f*text_offset.x, gui.style.text.font.line_height + 2.0f*text_offset.y };
    Vector2 pos = gui_layout_widget(&size);

    gui_hot_rect(id, pos, size);
    if (gui_clicked(id)) menu->active = !menu->active;

    if (menu->active) {
        // TODO(jesper): this needs to be adjusted and fixed for nested sub-menus. Probably using the id stack
        if (gui.active_menu != id && gui.active_menu != GUI_ID_INVALID) {
            GuiMenu *current = gui_find_menu(gui.active_menu);
            current->active = false;
        }

        gui.active_menu = id;
    }

    Vector3 btn_fg = rgb_unpack(0xFFFFFFFF);
    Vector3 btn_bg = gui.hot == id ? rgb_unpack(0xFF282828) : rgb_unpack(0xFF212121);
    gui_draw_rect(pos, size, wnd->clip_rect, btn_bg);
    gui_draw_text(td.glyphs, pos + text_offset, wnd->clip_rect, btn_fg, &gui.style.text.font);

    if (menu->active) {
        gui_push_id(id);
        gui.current_window = GUI_OVERLAY;

        Vector2 menu_pos = cl->type == GUI_LAYOUT_COLUMN ?
            Vector2{ pos.x, pos.y + size.y } :
            Vector2{ pos.x + cl->size.x, pos.y };

        GuiLayout layout{
            .type = GUI_LAYOUT_ROW,
            .flags = GUI_LAYOUT_EXPAND,
            .pos = menu_pos,
            .size = menu->size,
            .margin.x = 5,
            .margin.y = 5,
            .column.spacing = 2,
        };

        array_add(&gui.overlay_rects, { menu_pos, menu->size });

        gui_begin_layout(layout);
    }

    return menu->active;
}

void gui_end_menu()
{
    GuiLayout *layout = gui_current_layout();
    GuiMenu *menu = gui_find_menu(gui.current_id);
    ASSERT(menu);

    if (menu->active && gui_capture(gui.capture_keyboard)) {
        for (InputEvent e : gui.events) {
            switch (e.type) {
            case IE_KEY_PRESS:
                switch (e.key.keycode) {
                case KC_ESC: menu->active = false; break;
                }
                break;
            default: break;
            }
        }
    }

    gui.current_window = menu->parent_wnd;

    menu->size = layout->size;
    if (layout->type == GUI_LAYOUT_ROW && menu->active) {
        Vector3 bg = rgb_unpack(0xFF212121);
        gui_draw_rect(layout->pos, menu->size, bg, &gui.windows[GUI_OVERLAY].command_buffer, menu->draw_index);
    }

    gui_end_layout();
    gui_pop_id();
}

bool gui_menu_button_id(GuiId id, String label)
{
    ASSERT(gui.current_id != GUI_ID_INVALID);
    id.internal = id.owner;
    id.owner = gui.current_id.owner;

    // TODO(jesper): can we just make this a regular button? only difference is styling
    GuiWindow *wnd = &gui.windows[gui.current_window];

    FontAtlas *font = &gui.style.text.font;
    TextQuadsAndBounds td = calc_text_quads_and_bounds(label, font);
    Vector2 text_offset{ 0.0f, 0.0f };

    // TODO(jesper): I need something here to essentially get the entire row/column rectangle
    // to use for mouse hitbox checking, but still play nice with proper layouting or something? I'm
    // not sure, currently too constipated to think about it.
    Vector2 size{ td.bounds.size.x + 2.0f*text_offset.x, font->line_height + 2.0f*text_offset.y };
    Vector2 pos = gui_layout_widget(&size);

    gui_hot_rect(id, pos, size);
    bool clicked = gui_clicked(id);

    if (clicked) {
        GuiMenu *menu = gui_find_menu(gui.active_menu);
        menu->active = false;
    }

    Vector3 btn_fg = rgb_unpack(0xFFFFFFFF);
    if (gui.hot == id) gui_draw_rect(pos, size, wnd->clip_rect, rgb_unpack(0xFF282828) );
    gui_draw_text(td.glyphs, pos + text_offset, wnd->clip_rect, btn_fg, font);
    return clicked;
}


bool gui_menu_button_id(GuiId id, String label, bool *toggle_value)
{
    bool clicked = gui_menu_button_id(id, label);
    if (clicked) *toggle_value = !*toggle_value;
    return clicked;
}


void gui_vscrollbar_id(GuiId id, f32 line_height, i32 *current, i32 max, i32 num_visible, f32 *offset, GuiAnchor anchor)
{
    GuiWindow *wnd = &gui.windows[gui.current_window];

    GuiId up_id = GUI_ID_INTERNAL(id, 1);
    GuiId dwn_id = GUI_ID_INTERNAL(id, 2);
    GuiId handle_id = GUI_ID_INTERNAL(id, 3);

    Vector2 total_size{ gui.style.scrollbar.thickness, 50.0f };
    Vector2 total_pos = gui_layout_widget(&total_size, anchor);

    Vector2 btn_size{ gui.style.scrollbar.thickness, gui.style.scrollbar.thickness };
    Vector2 btn_up_p{ total_pos.x, total_pos.y };
    Vector2 btn_dwn_p{ total_pos.x, total_pos.y + total_size.y - btn_size.y };

    Vector2 scroll_size{ gui.style.scrollbar.thickness, total_size.y - btn_size.y*2 };
    Vector2 scroll_pos{ total_pos.x, btn_up_p.y + btn_size.y };

    gui_draw_rect(total_pos, total_size, wnd->clip_rect, gui.style.bg_light0);

    i32 step_size = 3;
    if (gui_button_id(up_id, btn_up_p, btn_size, gui.icons.up)) {
        *offset += step_size*line_height;

        if (*offset > line_height) {
            f32 div = *offset / line_height;
            *current = MAX(0, *current-1*div);
            *offset -= line_height*div;
        }

        *current = MIN(*current, max-3);
    }

    if (gui_button_id(dwn_id, btn_dwn_p, btn_size, gui.icons.down)) {
        *offset -= step_size*line_height;

        if (*offset < -line_height) {
            f32 div = *offset / -line_height;
            *current += 1*div;
            *offset += line_height*div;
        }

        *current = MIN(*current, max-3);
    }

    // NOTE(jesper): line_height*total_to_scroll != row_height
    // The row_height is line_height converted to [0, scroll_size.y] coordinate space,
    // but the scroll_to_total and total_to_scroll coordintae space factors convert
    // to/from the space taken on the scroll bar _handle_
    f32 ratio = scroll_size.y/total_size.y;
    f32 row_height = line_height*ratio;

    f32 pixels_per_i = scroll_size.y / max;
    f32 scroll_to_total = row_height/pixels_per_i;
    f32 total_to_scroll = pixels_per_i/row_height;

    if (gui_drag(handle_id, { *offset, (f32)*current }) && gui.mouse.dy != 0) {
        f32 dy = gui.mouse.y - gui.drag_start_mouse.y;

        f32 o = gui.drag_start_data.x*total_to_scroll + dy;
        i32 di = o / (line_height*total_to_scroll);
        o = (o - di*(line_height*total_to_scroll))*scroll_to_total;

        i32 c = di + (i32)gui.drag_start_data.y;
        c = CLAMP(c, 0, max-3);

        if (c == 0) o = MAX(o, 0);
        else if (c == max-3) o = MIN(o, 0);

        if (c > 0) {
            o += line_height;
            c -= 1;
        }

        *offset = o;
        *current = c;
    }

    f32 min_h = 8.0f;

    f32 y0 = *current*pixels_per_i + *offset*total_to_scroll;
    f32 yh = MIN(num_visible, max) * pixels_per_i;
    f32 y1 = y0 + yh;

    y1 = MIN(y1, scroll_size.y);

    // TODO(jesper) find a more appealing version of this that ensures a pixel of distance
    // between scroll handle and bottom/top until it's actually at the bottom/top
    if (yh < min_h) {
        y0 = MAX(0, y0-min_h/2.0f);
        y1 = MIN(scroll_size.y, y0+min_h);
    }

    Vector2 handle_p{ scroll_pos.x, scroll_pos.y + y0 };
    Vector2 handle_s{ gui.style.scrollbar.thickness, y1 - y0 };
    gui_hot_rect(handle_id, handle_p, handle_s);

    gui_draw_button(handle_id, handle_p, handle_s);
}

void gui_vscrollbar_id(GuiId id, f32 *current, f32 total_height, f32 step_size, GuiAnchor anchor)
{
    GuiWindow *wnd = &gui.windows[gui.current_window];
    GuiLayout *cl = gui_current_layout();
    Rect lr = gui_layout_rect(cl);

    GuiId up_id = GUI_ID_INTERNAL(id, 1);
    GuiId dwn_id = GUI_ID_INTERNAL(id, 2);
    GuiId handle_id = GUI_ID_INTERNAL(id, 3);

    Vector2 total_size{ gui.style.scrollbar.thickness, 50.0f };
    Vector2 total_pos = gui_layout_widget(&total_size, anchor);

    Vector2 btn_size{ gui.style.scrollbar.thickness, gui.style.scrollbar.thickness };
    Vector2 btn_up_p{ total_pos.x, total_pos.y };
    Vector2 btn_dwn_p{ total_pos.x, total_pos.y + total_size.y - btn_size.y };

    Vector2 scroll_size{ gui.style.scrollbar.thickness, total_size.y - btn_size.y*2 };
    Vector2 scroll_pos{ total_pos.x, btn_up_p.y + btn_size.y };

    gui_draw_rect(total_pos, total_size, wnd->clip_rect, gui.style.bg_light0);

    f32 scroll_to_total = total_height/scroll_size.y;
    f32 total_to_scroll = scroll_size.y/total_height;
    f32 yh = total_size.y*total_to_scroll;

    f32 max_c = total_height > total_size.y ? total_height - total_size.y : 0;

    // NOTE(jesper): this is a hacky way of checking whether the mouse is over the containing
    // rect of the scrollbar. This definitely needs to change to something more sane
    if (gui_mouse_over_rect(lr.pos, lr.size) &&
        gui_capture(gui.capture_mouse_wheel))
    {
        for (InputEvent e : gui.events) {
            switch (e.type) {
            case IE_MOUSE_WHEEL:
                *current -= step_size*e.mouse_wheel.delta;
                *current = CLAMP(*current, 0, max_c);
                break;
            default: break;
            }
        }
    }

    if (gui_button_id(up_id, btn_up_p, btn_size, gui.icons.up)) {
        *current -= step_size;
        *current = MAX(*current, 0);
    }

    if (gui_button_id(dwn_id, btn_dwn_p, btn_size, gui.icons.down)) {
        *current += step_size;
        *current = MIN(*current, max_c);
    }

    //if (total_height > total_size.y)
    {
        f32 y0 = *current*total_to_scroll;
        f32 y1 = y0 + yh;

        Vector2 scroll_handle_p{ scroll_pos.x, scroll_pos.y + y0 };
        Vector2 scroll_handle_s{ gui.style.scrollbar.thickness, y1 - y0 };

        gui_hot_rect(handle_id, scroll_handle_p, scroll_handle_s);
        if (gui_drag(handle_id, { 0, *current }) && gui.mouse.dy != 0) {
            f32 dy = gui.mouse.y - gui.drag_start_mouse.y;

            f32 c = gui.drag_start_data.y + scroll_to_total*dy;
            c = CLAMP(c, 0, max_c);

            if (c != *current) {
                *current = c;

                y0 = *current*total_to_scroll;
                y1 = y0 + yh;

                scroll_handle_p = { scroll_pos.x, scroll_pos.y + y0 };
                scroll_handle_s = { gui.style.scrollbar.thickness, y1 - y0 };
            }
        }

        gui_draw_button(handle_id, scroll_handle_p, scroll_handle_s);
    }
}

Rect gui_layout_widget_fill()
{
    GuiLayout *cl = gui_current_layout();

    Rect r = gui_layout_rect(cl);

    switch (cl->type) {
    case GUI_LAYOUT_ABSOLUTE:
        break;
    case GUI_LAYOUT_ROW:
    case GUI_LAYOUT_COLUMN:
        cl->available_space = { 0, 0 };
        cl->current = cl->pos + cl->size;
        break;
    }
    return r;
}

void gui_begin_layout(GuiLayout layout)
{
    layout.available_space = layout.size - 2*layout.margin;
    layout.current += layout.margin;
    array_add(&gui.layout_stack, layout);
}

void gui_end_layout()
{
    ASSERT(gui.layout_stack.count > 1);
    gui.layout_stack.count--;
}

GuiListerAction gui_lister_id(GuiId id, Array<String> items, i32 *selected_item)
{
    GuiListerAction result = GUI_LISTER_NONE;

    GuiLister *lister = gui_find_or_create_lister(id);

    GuiWindow *wnd = &gui.windows[gui.current_window];

    FontAtlas *font = &gui.style.text.font;
    f32 item_height = font->line_height;

    Rect r = gui_layout_widget_fill();
    gui_begin_layout({ .type = GUI_LAYOUT_COLUMN, .pos = { r.pos.x + 1, r.pos.y + 1 }, .size = r.size - Vector2{ 2.0f, 2.0f }});
    defer { gui_end_layout(); };

    i32 next_selected_item = MIN(*selected_item, items.count-1);
    if (gui_capture(gui.capture_keyboard)) {
        for (InputEvent e : gui.events) {
            switch (e.type) {
            case IE_KEY_PRESS:
                switch (e.key.keycode) {
                case KC_DOWN:
                    next_selected_item = MIN((*selected_item)+1, items.count-1);
                    break;
                case KC_UP:
                    next_selected_item = MAX((*selected_item)-1, 0);
                    break;
                case KC_ESC:
                    return GUI_LISTER_CANCEL;
                case KC_ENTER:
                    *selected_item = next_selected_item;
                    return GUI_LISTER_FINISH;
                default: break;
                }
                break;
            default: break;
            }
        }
    }

    if (*selected_item != next_selected_item) {
        *selected_item = next_selected_item;

        f32 selected_item_y0 = (*selected_item)*item_height;
        f32 selected_item_y1 = selected_item_y0 + item_height;

        if (selected_item_y0 - lister->offset < 0) {
            lister->offset += selected_item_y0 - lister->offset;
        } else if (selected_item_y1 - lister->offset > r.size.y) {
            lister->offset += selected_item_y1 - lister->offset - r.size.y + 2;
        }

        lister->offset = MAX(lister->offset, 0);
    }


    Vector3 bg = gui.style.bg_dark0;
    Vector3 border_col = gui.focused == id ? gui.style.accent_bg : gui.style.bg_light0;
    Vector3 selected_bg = gui.style.accent_bg;
    Vector3 selected_hot_bg = gui.style.accent_bg_hot;
    Vector3 hot_bg = gui.style.bg_hot;

    gui_draw_rect(r.pos, r.size, wnd->clip_rect, bg);
    gui_draw_rect({ r.pos.x, r.pos.y }, { r.size.x, 1 }, wnd->clip_rect, border_col);
    gui_draw_rect({ r.pos.x, r.pos.y+r.size.y-1}, { r.size.x, 1 }, wnd->clip_rect, border_col);
    gui_draw_rect({ r.pos.x+r.size.x-1, r.pos.y }, { 1, r.size.y }, wnd->clip_rect, border_col);
    gui_draw_rect({ r.pos.x, r.pos.y }, { 1, r.size.y }, wnd->clip_rect, border_col);
    // NOTE(jesper): I'm not using draw_line_rect at this point because there's something I don't understand
    // going on with the pixel coordinate calculations for opengl line drawing
    //gfx_draw_line_rect(r.pos, r.size, gui.style.lister.border, cmdbuf);

    f32 step_size = 5.0f;
    f32 total_height = item_height*items.count;

    gui_vscrollbar_id(GUI_ID_INDEX(id, 1), &lister->offset, total_height, step_size);

    Rect r2 = gui_layout_widget_fill();
    gui_begin_layout({ .type = GUI_LAYOUT_ROW, .pos = r2.pos, .size = r2.size });
    defer { gui_end_layout(); };

    i32 start = lister->offset / item_height;
    i32 end = MIN(items.count, (start + r2.size.y / item_height)+2);
    f32 offset = fmodf(lister->offset, item_height);

    // TODO(jesper): a lot of this stuff is the same between dropdown, sub-menu, and lister
    for (i32 i = start; i < end; i++) {
        Vector2 size{ 0, item_height };
        Vector2 pos = gui_layout_widget(&size);
        pos.y -= offset;

        GuiId item_id = GUI_ID_INTERNAL(id, i+10);

        gui_hot_rect(item_id, pos, size);
        if (gui_clicked(item_id)) {
            *selected_item = i;
            result = GUI_LISTER_SELECT;
        }

        if (i == *selected_item && gui.hot == item_id) gui_draw_rect(pos, size, r2, selected_hot_bg);
        else if (i == *selected_item) gui_draw_rect(pos, size, r2, selected_bg);
        else if (gui.hot == item_id) gui_draw_rect(pos, size, r2, hot_bg);

        gui_textbox(items[i], pos, r2);
    }

    return result;
}

bool gui_input(InputEvent event)
{
    switch (event.type) {
    case IE_TEXT:
        if (gui.capture_text[0]) {
            array_add(&gui.events, event);
            return true;
        }
        break;
    case IE_MOUSE_WHEEL:
        if (gui.capture_mouse_wheel[0]) {
            array_add(&gui.events, event);
            return true;
        }
        break;
    case IE_KEY_RELEASE:
    case IE_KEY_PRESS:
        if (gui.capture_keyboard[0]) {
            array_add(&gui.events, event);
            return true;
        }
        break;
    }

    return false;
}

bool gui_begin_window_id(
    GuiId id,
    String title,
    Vector2 pos,
    Vector2 size,
    Vector2 anchor,
    bool *visible,
    u32 flags)
{
    return gui_begin_window_id(id, title, pos - hadamard(size, anchor), size, visible, flags);
}

bool gui_begin_window_id(
    GuiId id,
    String title,
    Vector2 pos,
    Vector2 size,
    Vector2 anchor,
    bool visible,
    u32 flags)
{
    return gui_begin_window_id(id, title, pos - hadamard(size, anchor), size, visible, flags & ~GUI_WINDOW_CLOSE);
}

void gui_divider(f32 thickness = 1, Vector3 color = gui.style.accent_bg)
{
    GuiWindow *wnd = gui_current_window();

    Vector2 size{ thickness, thickness };
    Vector2 pos = gui_layout_widget(&size);

    gui_draw_rect(pos, size, wnd->clip_rect, color);
}
