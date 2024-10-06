#ifndef MIMIR_INTERNAL_H
#define MIMIR_INTERNAL_H

static void ts_parse_buffer(Buffer *buffer, TSInputEdit edit);
static void lsp_did_open(subprocess_s process, String path, String language_id, String content);
static void app_gather_input(AppWindow *wnd);
static void update_and_render();

#endif // MIMIR_INTERNAL_H
