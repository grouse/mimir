#include "gui.h"
#include "allocator.h"
#include "string.h"

#include "win32_lite.h"

extern Allocator mem_frame;

void win32_gui_textinput(
    DynamicArray<TextInput> *text_input_queue, 
    UINT message, 
    WPARAM wparam)
{
    u16 c = (u16)wparam;

    switch (message) {
    case WM_CHAR:
        if (c < 0x20) {
            switch (c) {
            case 0x8: /* backspace */ array_add(&gui.text_input_queue, { TEXT_INPUT_BACKSPACE }); break;
            case 0x1: /* ctrl-a */ array_add(text_input_queue, { TEXT_INPUT_SELECT_ALL }); break;
            case 0x3: /* ctrl-c */ array_add(text_input_queue, { TEXT_INPUT_COPY }); break;
            case 0x16: { // ctrl-v
                    if (!OpenClipboard(NULL)) break;
                    defer { CloseClipboard(); };

                    HANDLE clip = GetClipboardData(CF_UNICODETEXT);
                    if (!clip) break;

                    wchar_t *ptr = (wchar_t*)GlobalLock(clip);
                    if (!ptr) break;
                    defer { GlobalUnlock(ptr); };

                    String content{};
                    content.length = utf8_length((u16*)ptr, wcslen(ptr));
                    content.data = (char*)ALLOC(mem_frame, content.length);
                    utf8_from_utf16((u8*)content.data, content.length, (u16*)ptr, wcslen(ptr));

                    TextInput ti{ TEXT_INPUT_PASTE };
                    ti.str = content;
                    array_add(text_input_queue, ti);
                } break;
            case 0xd: /* enter */ array_add(text_input_queue, { TEXT_INPUT_ENTER }); break;
            default: 
                LOG_INFO("unhandled special key: '%c' (0x%x)", c, c);
                break;
            }
        } else {
            static u16 utf16_high;
            if (c >= 0xD800 && c < 0xDC00) {
                utf16_high = c;
                break;
            }

            u32 utf32 = c;
            if (c >= 0xDC00 && c <= 0xDFFF) {
                utf32 = (((utf16_high - 0xD800) << 10) | c) + 0x1000000;
            }

            TextInput ti{ TEXT_INPUT_CHAR };
            ti.length = utf8_from_utf32(ti.c, utf32);
            array_add(text_input_queue, ti);
        }
        break;
    case WM_KEYDOWN:
        switch (wparam) {
        case VK_LEFT: array_add(text_input_queue, { TEXT_INPUT_CURSOR_LEFT }); break;
        case VK_RIGHT: array_add(text_input_queue, { TEXT_INPUT_CURSOR_RIGHT }); break;
        case VK_DELETE: array_add(text_input_queue, { TEXT_INPUT_DEL }); break;
        }

    }
}