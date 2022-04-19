struct {
	bool init = false;

    void *data = nullptr;
    i32 data_size = 0;
    Window wnd;
    Display *dsp;
    Window parent_wnd;

    Atom target_atom;
    bool pending = false;
} clipboard;

void init_clipboard(Display *dsp, Window wnd)
{
    // TODO(jesper): I don't like the look of this. If Dsp can change somehow we're probably using an invalid dsp
    // for the clipboard
    clipboard.dsp = dsp;
    clipboard.parent_wnd = wnd;

    clipboard.wnd = XCreateSimpleWindow(dsp, RootWindow(dsp, DefaultScreen(dsp)), -10, -10, 1, 1, 0, 0, 0);
    PANIC_IF(!clipboard.wnd, "failed creading clipboard window");
    LOG_INFO("created clipboard wnd: %p", clipboard.wnd);

    clipboard.target_atom = XInternAtom(dsp, "RAY_CLIPBOARD", False);
	clipboard.init = true;
}

void handle_clipboard_events(XEvent event)
{
	if (!clipboard.init) return;

    switch (event.type) {
    case SelectionClear:
        ASSERT(event.xany.window == clipboard.wnd);
        LOG_INFO("selection ownership lost");
        FREE(mem_dynamic, clipboard.data);
        clipboard.data = nullptr;
        break;
    case SelectionRequest: {
            ASSERT(event.xany.window == clipboard.wnd);
            auto req = event.xselectionrequest;

            LOG_INFO(
                "SelectionRequest: requestor: %p, selection: %s, target: %s, property: %s",
                req.requestor,
                XGetAtomName(clipboard.dsp, req.selection),
                XGetAtomName(clipboard.dsp, req.target),
                XGetAtomName(clipboard.dsp, req.property));

            clipboard.pending = true;

            Atom XA_TARGETS = XInternAtom(clipboard.dsp, "TARGETS", False);
            Atom XA_UTF8_STRING = XInternAtom(clipboard.dsp, "UTF8_STRING", False);

            XEvent ev{};
            ev.xany.type = SelectionNotify;
            ev.xselection.selection = req.selection;
            ev.xselection.target = None;
            ev.xselection.property = None;
            ev.xselection.requestor = req.requestor;
            ev.xselection.time = req.time;

            if (req.target == XA_TARGETS) {
                Atom supported[] = {
                    XA_TARGETS,
                    XA_UTF8_STRING,
                };

                XChangeProperty(
                    clipboard.dsp,
                    req.requestor,
                    req.property,
                    XA_ATOM,
                    32,
                    PropModeReplace,
                    (unsigned char*)supported, ARRAY_COUNT(supported));

                ev.xselection.property = req.property;
                ev.xselection.target = XA_TARGETS;
            } else if (req.target == XA_UTF8_STRING) {
                XChangeProperty(
                    clipboard.dsp,
                    req.requestor,
                    req.property,
                    req.target,
                    8,
                    PropModeReplace,
                    (unsigned char*)clipboard.data, strlen((char*)clipboard.data));

                ev.xselection.target = req.target;
                ev.xselection.property = req.property;
                clipboard.pending = false;
            } else {
                LOG_ERROR("unhandled selection target");
            }

            XSendEvent(clipboard.dsp, req.requestor, False, NoEventMask, &ev);
            XSync(clipboard.dsp, False);
        } break;
#if 0
    case SelectionNotify: {
            // TODO(jesper): if event.xany.window == clipboard.wnd we could probably shirt-circuit this path and copy straight from clipboard.data

            LOG_INFO(
                "SelectionNotify: requestor: %p, selection: %s, target: %s, property: %s",
                event.xselection.requestor,
                XGetAtomName(clipboard.dsp, event.xselection.selection),
                XGetAtomName(clipboard.dsp, event.xselection.target),
                XGetAtomName(clipboard.dsp, event.xselection.property));

            if (gui.text_input) {
                if (event.xselection.property != None) {
                    Atom type, da;
                    int di;
                    unsigned long size, dul;
                    unsigned char *prop_ret = nullptr;

                    XGetWindowProperty(
                        clipboard.dsp,
                        clipboard.parent_wnd,
                        clipboard.target_atom,
                        0, 0,
                        False,
                        AnyPropertyType,
                        &type,
                        &di,
                        &dul,
                        &size,
                        &prop_ret);
                    XFree(prop_ret);

                    Atom incr = XInternAtom(clipboard.dsp, "INCR", False);
                    PANIC_IF(incr == type, "data too large and INCR mechanism not implemented");

                    XGetWindowProperty(
                        clipboard.dsp,
                        clipboard.parent_wnd,
                        clipboard.target_atom,
                        0, size,
                        False,
                        AnyPropertyType,
                        &da,
                        &di,
                        &dul,
                        &dul,
                        &prop_ret);

                    String str{ (char*)prop_ret, (i32)strlen((char*)prop_ret) };

                    i32 start = MIN(gui.edit.cursor, gui.edit.selection);
                    i32 end = MAX(gui.edit.cursor, gui.edit.selection);

                    i32 limit = sizeof gui.edit.buffer - (gui.edit.length - (end - start));
                    str.length = utf8_truncate(str, limit);



                    TextInput ti{ TEXT_INPUT_PASTE };
                    ti.str = duplicate_string(str, mem_frame);
                    array_add(&gui.text_input_queue, ti);

                    LOG_INFO("clipboard content: %.*s", STRFMT(str));
                    XFree(prop_ret);
                }
            }
        } break;
#endif
    }
}

void set_clipboard_data(String str)
{
    char *clip = sz_string(str, mem_dynamic);
    char *e = atomic_exchange((char**)&clipboard.data, clip);

    XSetSelectionOwner(
        clipboard.dsp,
        XInternAtom(clipboard.dsp, "CLIPBOARD", False),
        clipboard.wnd,
        CurrentTime);

    if (e) FREE(mem_dynamic, e);
}

String read_clipboard_str(Allocator /*mem*/)
{
	LOG_ERROR("unimplemented");
	return {};
}

