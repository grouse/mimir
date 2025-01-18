#ifndef FONT_H
#define FONT_H

// TODO(jesper): make a FontHandle situation that the user can use
// instead of direct font structures, so that font instances can be
// copied and reused safely without worry about stomping over underlying
// texture atlas handles etc

#include "stb/stb_truetype.h"

#include "core/string.h"
#include "core/map.h"
#include "gfx_opengl.h"

struct Glyph {
    u32 codepoint;
    i32 glyph_index;
    f32 advance;
    i32 x0, y0;
    i32 x1, y1;
    i32 xoff, yoff;
};

struct GlyphRect {
    f32 x0, y0;
    f32 x1, y1;
    f32 s0, t0;
    f32 s1, t1;
};

struct FontAtlas {
    GLuint texture;

    i32 ascent;
    i32 descent;
    i32 line_gap;
    i32 baseline;
    f32 scale;
    f32 line_height;
    f32 space_width;

    bool mono_space;

    stbtt_fontinfo info;
    Vector2 size;
    Vector2 current;
    f32 current_row_height;

    DynamicMap<i32 , Glyph> glyphs;
};

struct GlyphsData {
    FontAtlas *font;
    Vector2 bounds;
    Vector2 offset;
    DynamicArray<GlyphRect> glyphs;
};

FontAtlas create_font(String path, f32 pixel_height, bool mono_space = false);
f32 glyph_advance(FontAtlas *font, u32 codepoint);
GlyphRect get_glyph_rect(FontAtlas *font, u32 codepoint, Vector2 *pen);

#include "generated/font.h"

#endif // FONT_H
