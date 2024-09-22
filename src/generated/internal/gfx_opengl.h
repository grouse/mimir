#ifndef GFX_OPENGL_INTERNAL_H
#define GFX_OPENGL_INTERNAL_H

static bool gfx_link_program(GfxProgram *program);
static void gl_debug_proc(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *);

#endif // GFX_OPENGL_INTERNAL_H
