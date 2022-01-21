#ifndef GFX_OPENGL_H
#define GFX_OPENGL_H

#include "maths.h"
#include "array.h"

#ifdef _WIN32
#include "win32_lite.h"
#include "win32_opengl.h"
#endif

enum GfxCommandType {
    GFX_COMMAND_COLORED_LINE,
    GFX_COMMAND_COLORED_PRIM,
    GFX_COMMAND_TEXTURED_PRIM,
    GFX_COMMAND_GUI_PRIM_COLOR,
    GFX_COMMAND_GUI_PRIM_TEXTURE,
    GFX_COMMAND_GUI_TEXT,
};

struct GfxCommand {
    GfxCommandType type;
    union {
        struct {
            GLuint vbo;
            i32 vbo_offset;
            i32 vertex_count;
        } colored_prim;
        struct { 
            GLuint vbo;
            i32 vbo_offset;
            i32 vertex_count;
            GLuint texture;
        } textured_prim;
        struct {
            GLuint vbo;
            i32 vbo_offset;
            i32 vertex_count;
        } gui_prim;
        struct {
            GLuint vbo;
            GLuint texture;
            i32 vbo_offset;
            i32 vertex_count;
        } gui_prim_texture;
        struct {
            GLuint vbo;
            GLuint font_atlas;
            i32 vbo_offset;
            i32 vertex_count;
            Vector3 color;
        } gui_text;
    };
};

struct GfxCommandBuffer {
    DynamicArray<GfxCommand> commands;
};

struct GfxVertexAttrib {
    GLuint index;
    GLint size;
    GLenum type;
    GLboolean normalized;
    GLsizei stride;
    GLsizei offset;
};

struct GfxContext {
    struct {
        struct {
            GLint cs_from_ws;
        } global;
        struct {
            GLint program;
            GLint ws_from_ls;
            GLint color;
        } basic2d;
        struct {
            GLuint program;
        } pass2d;
        struct {
            GLuint program;
            GLint resolution;
            GLint color;
        } text;
        struct {
            GLuint program;
            GLint resolution;
        } gui_prim;
        struct {
            GLuint program;
            GLint resolution;
        } gui_prim_texture;
    } shaders;

    struct {
        GLuint square;
        GLuint frame;
    } vaos;
    
    struct {
        GLuint square;
        GLuint frame;
    } vbos;

    struct {
    } textures;

    GfxCommandBuffer frame_cmdbuf;
    DynamicArray<f32> frame_vertices;
    Vector2 resolution;
};

extern GfxContext gfx;

struct Camera {
    Matrix4 projection;
    Vector3 position;
    f32 uni_scale = 1.0f;
};

Matrix4 transform(Camera *camera);
Vector2 ws_from_ss(Vector2 ss_pos);

Vector3 rgb_unpack(u32 argb);
Vector4 argb_unpack(u32 argb);
f32 linear_from_sRGB(f32 s);
Vector3 linear_from_sRGB(Vector3 sRGB);
Vector4 linear_from_sRGB(Vector4 sRGB);

void gfx_begin_frame();
void gfx_end_frame();

void gfx_draw_square(Vector2 center, Vector2 size, Vector4 color, GfxCommandBuffer *cmdbuf = &gfx.frame_cmdbuf);
void gfx_draw_square(Vector2 center, Vector2 size, Vector2 uv_tl, Vector2 uv_br, GLuint texture, GfxCommandBuffer *cmdbuf = &gfx.frame_cmdbuf);
void gfx_draw_square(Vector2 p0,  Vector2 p1,  Vector2 p2, Vector2 p3, Vector3 color, GfxCommandBuffer *cmdbuf = &gfx.frame_cmdbuf);

void gfx_draw_triangle(Vector2 p0, Vector2 p1, Vector2 p2, Vector3 color, GfxCommandBuffer *cmdbuf = &gfx.frame_cmdbuf);

void gfx_draw_line(Vector2 a, Vector2 b, Vector3 color, GfxCommandBuffer *cmdbuf = &gfx.frame_cmdbuf);
void gfx_draw_line_square(Vector2 center, Vector2 size, Vector3 color, GfxCommandBuffer *cmdbuf = &gfx.frame_cmdbuf);
void gfx_draw_line_rect(Vector2 tl, Vector2 size, Vector3 color, GfxCommandBuffer *cmdbuf = &gfx.frame_cmdbuf);
void gfx_draw_line_loop(Vector2 *points, i32 num_points, Vector3 color, GfxCommandBuffer *cmdbuf = &gfx.frame_cmdbuf);
GLuint gfx_create_texture(void *pixel_data, i32 width, i32 height);
GLuint gfx_load_texture(u8 *data, i32 size);

void gfx_flush_transfers();

GfxCommandBuffer gfx_command_buffer();
void gfx_reset_command_buffer(GfxCommandBuffer *cmdbuf);
void gfx_submit_commands(GfxCommandBuffer cmdbuf);

#endif // GFX_OPENGL_H