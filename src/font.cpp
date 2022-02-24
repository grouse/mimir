#include "font.h"

Font create_font(String path, f32 pixel_height)
{
    Font font{};

    Asset *asset = find_asset(path);
    if (asset) {
        stbtt_InitFont(&font.info, asset->data, 0);

        stbtt_GetFontVMetrics(&font.info, &font.ascent, &font.descent, &font.line_gap);
        font.scale = stbtt_ScaleForPixelHeight(&font.info, pixel_height);
        font.line_height = font.scale * (font.ascent - font.descent + font.line_gap);
        font.baseline = (i32)(font.ascent*font.scale);
        font.size = { 1024, 1024 };
        font.current = { 1, 1 };

        int advance, lsb;
        stbtt_GetCodepointHMetrics(&font.info, ' ', &advance, &lsb);
        font.space_width = font.scale*advance;

        glGenTextures(1, &font.texture);
        glBindTexture(GL_TEXTURE_2D, font.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        // TODO(jesper): figure out if we need to do this or what the preferred way to clear a 
        // texture to black is
        u8 *black = (u8*)ALLOC(mem_dynamic, 1024*1024);
        memset(black, 0, 1024*1024);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, 1024, 1024);
    }

    return font;
}

Glyph find_or_create_glyph(Font *font, u32 codepoint)
{
    Glyph *existing = find(&font->glyphs, codepoint);
    if (existing) return *existing;

    int x0, y0, x1, y1;
    stbtt_GetCodepointBitmapBox(&font->info, codepoint, font->scale, font->scale, &x0, &y0, &x1, &y1);

    i32 w = x1-x0;
    i32 h = y1-y0;

    // NOTE(jesper): need to align to 4 for the glTexSubImage2D. The alignment
    // probably needs to be queries from the API somehow, I don't know
    i32 wa = ALIGN(w, 4);
    i32 ha = ALIGN(h, 4);

    u8 *pixels = (u8*)ALLOC(mem_tmp, wa*ha);
    memset(pixels, 0, wa*ha);
    stbtt_MakeCodepointBitmap(&font->info, pixels, w, h, wa, font->scale, font->scale, codepoint);

    int advance, lsb;
    stbtt_GetCodepointHMetrics(&font->info, codepoint, &advance, &lsb);

    if (font->current.x + w + 1 >= font->size.x) {
        font->current.x = 1;
        font->current.y += font->current_row_height + 1;
        font->current_row_height = 0;
    }
    ASSERT(font->current.y < font->size.y);

    i32 x = font->current.x;
    i32 y = font->current.y;

    glTextureSubImage2D(font->texture, 0, x, y, wa, ha, GL_RED, GL_UNSIGNED_BYTE, pixels);

    font->current.x += w;
    font->current_row_height = MAX(font->current_row_height, h);

#if 0
    LOG_RAW("------- '%c (0x%x)' -------\n", (char)codepoint, codepoint);
    LOG_RAW("x0: %d, y0: %d, x1: %d, y1: %d\n", x0, y0, x1, y1);
    LOG_RAW("x: %d, y: %d, w: %d, h: %d, wa: %d, ha: %d\n", x, y, w, h, wa, ha);
    for (i32 j = 0; j < h; j++) {
        for (i32 i = 0; i < w; i++) {
            LOG_RAW("%c", " .:ioVM@"[pixels[j*wa+i]>>5]);
        }
        LOG_RAW("\n");
    }

    LOG_RAW("\n");
#endif

    Glyph glyph{
        .codepoint = codepoint,
        .advance = font->scale*advance,
        .x0 = x,    .y0 = y,
        .x1 = x+w,  .y1 = y+h,
        .xoff = x0, .yoff = y0,
    };
    set(&font->glyphs, codepoint, glyph);
    return glyph;
}

f32 glyph_advance(Font *font, u32 codepoint)
{
    Glyph glyph = find_or_create_glyph(font, codepoint);
    return glyph.advance;
}

GlyphRect get_glyph_rect(Font *font, u32 codepoint, Vector2 *pen)
{
    Glyph glyph = find_or_create_glyph(font, codepoint);
    
    f32 iuv_x = 1.0f / font->size.x;
    f32 iuv_y = 1.0f / font->size.y;

    GlyphRect g{
        .x0 = floorf((pen->x+(f32)glyph.xoff) + 0.5f),
        .y0 = floorf((pen->y+(f32)glyph.yoff) + 0.5f),
        .x1 = g.x0 + glyph.x1 - glyph.x0,
        .y1 = g.y0 + glyph.y1 - glyph.y0,
    
        .s0 = glyph.x0*iuv_x,
        .t0 = glyph.y0*iuv_y,
        .s1 = glyph.x1*iuv_x,
        .t1 = glyph.y1*iuv_y,
    };
    
    pen->x += glyph.advance;
    return g;
}
    