#ifndef MIMIR_INTERNAL_H
#define MIMIR_INTERNAL_H

static u32 hash32(BufferId buffer, u32 seed = MURMUR3_SEED);
static void ts_parse_buffer(Buffer *buffer, TSInputEdit edit);
static void lsp_open(LspConnection *lsp, BufferId buffer_id, String language_id, String content);
static i32 line_from_offset(i64 offset, Array<i64> offsets, i32 guessed_line = 0);
static void app_gather_input(AppWindow *wnd);
static void update_and_render();

#endif // MIMIR_INTERNAL_H
