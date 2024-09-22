#ifndef GFX_OPENGL_PUBLIC_H
#define GFX_OPENGL_PUBLIC_H

namespace PUBLIC {}
using namespace PUBLIC;

extern void gfx_push_command(GfxCommand cmd, GfxCommandBuffer *cmdbuf);
extern GfxProgram *gfx_create_shader(const char *vertex_src, const char *fragment_src);
extern GfxProgram *gfx_create_shader_program(String vertex, String fragment);
extern void init_gfx(Vector2 resolution);
extern bool gfx_change_resolution(Vector2 resolution);
extern void gfx_draw_square(Vector2 center, Vector2 size, Vector4 color, GfxCommandBuffer *cmdbuf);
extern void gfx_draw_rect(Vector2 tl, Vector2 size, Vector4 color, GfxCommandBuffer *cmdbuf);
extern void gfx_draw_square(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector3 color, GfxCommandBuffer *cmdbuf);
extern void gfx_draw_square(Vector2 center, Vector2 size, Vector2 uv_tl, Vector2 uv_br, GLuint texture, GfxCommandBuffer *cmdbuf);
extern void gfx_draw_triangle(Vector2 p0, Vector2 p1, Vector2 p2, Vector3 color, GfxCommandBuffer *cmdbuf);
extern void gfx_draw_line(Vector2 a, Vector2 b, Vector3 color, GfxCommandBuffer *cmdbuf);
extern void gfx_draw_line_loop(Vector2 *points, i32 num_points, Vector3 color, GfxCommandBuffer *cmdbuf);
extern void gfx_draw_line_square(Vector2 center, Vector2 size, Vector3 color, GfxCommandBuffer *cmdbuf);
extern void gfx_draw_line_rect(Vector2 tl, Vector2 size, Vector3 color, GfxCommandBuffer *cmdbuf);
extern void gfx_begin_frame();
extern void gfx_flush_transfers();
extern void gfx_reset_command_buffer(GfxCommandBuffer *cmdbuf);
extern GfxCommandBuffer gfx_command_buffer();
extern void gfx_submit_commands(GfxCommandBuffer cmdbuf, Matrix3 view);
extern GLuint gfx_create_texture(void *pixel_data, i32 width, i32 height);

#endif // GFX_OPENGL_PUBLIC_H
