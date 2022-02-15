#ifndef FONT_H
#define FONT_H

#include "external/stb/stb_truetype.h"

typedef stbtt_aligned_quad TextQuad;

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


Font load_font(String path);

TextQuad get_font_glyph(Font *font, u32 codepoint, Vector2 *pen);


#endif // FONT_H
