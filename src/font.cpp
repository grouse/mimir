#include "font.h"

Font load_font(String path)
{
    Font font{};

    Asset *asset = find_asset(path);
    if (asset) {
        stbtt_fontinfo fi;
        stbtt_InitFont(&fi, asset->data, 0);

        u8 *bitmap = (u8*)ALLOC(mem_dynamic, 1024*1024);
        stbtt_BakeFontBitmap(asset->data, 0, 18.0f, bitmap, 1024, 1024, 0, 256, font.atlas);
        stbtt_GetFontVMetrics(&fi, &font.ascent, &font.descent, &font.line_gap);
        font.scale = stbtt_ScaleForPixelHeight(&fi, 18);
        font.line_height = font.scale * (font.ascent - font.descent + font.line_gap);
        font.baseline = (i32)(font.ascent*font.scale);

        // TODO(jesper): this is not really correct. The text layout code needs to query the codepoint
        // advance and codepoint kerning
        f32 x = 0, y = 0;
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(font.atlas, 1024, 1024, ' ', &x, &y, &q, 1);
        font.space_width = x;

        glGenTextures(1, &font.texture);
        glBindTexture(GL_TEXTURE_2D, font.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1024, 1024, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap); 
    }

    return font;
}

TextQuad get_font_glyph(Font *font, u32 codepoint, Vector2 *pen)
{
    stbtt_aligned_quad q;
    stbtt_GetBakedQuad(font->atlas, 1024, 1024, codepoint, &pen->x, &pen->y, &q, 1);
    return q;
}
    