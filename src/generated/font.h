#ifndef FONT_PUBLIC_H
#define FONT_PUBLIC_H

namespace PUBLIC {}
using namespace PUBLIC;

extern Glyph find_or_create_glyph(FontAtlas *font, u32 codepoint);
extern GlyphsData calc_glyphs_data(String text, FontAtlas *font, Allocator mem);
extern DynamicArray<GlyphRect> calc_glyph_rects(String text, FontAtlas *font, Allocator mem);

#endif // FONT_PUBLIC_H
