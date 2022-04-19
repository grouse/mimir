struct LinuxWindow {
    Display *dsp;
    Window handle;

    XIM im;
    XIC ic;
};

LinuxWindow create_opengl_window(const char *window_title, Vector2 resolution)
{
    LinuxWindow wnd{};
    wnd.dsp = XOpenDisplay(0);
    PANIC_IF(!wnd.dsp, "XOpenDisplay fail");

    i32 glx_major, glx_minor;
    PANIC_IF(!glXQueryVersion(wnd.dsp, &glx_major, &glx_minor), "failed querying glx version");
    PANIC_IF(glx_major == 1 && glx_minor < 4, "unsupported version");
    LOG_INFO("GLX version: %d.%d", glx_major, glx_minor);

    i32 fb_attribs[] = {
        GLX_X_RENDERABLE    , 1,
        GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
        GLX_RENDER_TYPE     , GLX_RGBA_BIT,
        GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
        GLX_RED_SIZE        , 8,
        GLX_GREEN_SIZE      , 8,
        GLX_BLUE_SIZE       , 8,
        GLX_ALPHA_SIZE      , 8,
        GLX_DEPTH_SIZE      , 24,
        GLX_STENCIL_SIZE    , 8,
        GLX_DOUBLEBUFFER    , 1,
        0
    };

    int fb_count;
    GLXFBConfig *fbs = glXChooseFBConfig(wnd.dsp, DefaultScreen(wnd.dsp), fb_attribs, &fb_count);
    PANIC_IF(!fbs, "failed finding matching framebuffer configurations");

    i32 chosen_fb = -1, chosen_samples = -1;
    for (i32 i = 0; i < fb_count; i++) {
        i32 sample_buffers, samples;
        glXGetFBConfigAttrib(wnd.dsp, fbs[i], GLX_SAMPLE_BUFFERS, &sample_buffers);
        glXGetFBConfigAttrib(wnd.dsp, fbs[i], GLX_SAMPLES, &samples);

        if (sample_buffers && samples > chosen_samples) {
            chosen_fb = i;
            chosen_samples = samples;
        }
    }

    GLXFBConfig fbc = fbs[chosen_fb];
    XVisualInfo *vi = glXGetVisualFromFBConfig(wnd.dsp, fbc);

    XSetWindowAttributes swa{};
    swa.colormap = XCreateColormap(wnd.dsp, RootWindow(wnd.dsp, vi->screen), vi->visual, AllocNone);

    wnd.handle = XCreateWindow(
        wnd.dsp,
        RootWindow(wnd.dsp, vi->screen),
        0, 0,
        resolution.x, resolution.y,
        0,
        vi->depth,
        InputOutput,
        vi->visual,
        CWOverrideRedirect | CWBackPixmap | CWBorderPixel | CWColormap,
        &swa);
    PANIC_IF(!wnd.handle, "XCreateWindow failed");
    LOG_INFO("created wnd: %p", wnd);
#if 0
    wnd_clip = XCreateSimpleWindow(dsp, RootWindow(dsp, DefaultScreen(dsp)), -10, -10, 1, 1, 0, 0, 0);
    PANIC_IF(!wnd_clip, "failed creading clipboard window");
    LOG_INFO("created clipboad wnd: %p", wnd_clip);
#endif

    XStoreName(wnd.dsp, wnd.handle, window_title);

    XSelectInput(
        wnd.dsp, wnd.handle,
        KeyPressMask | KeyReleaseMask |
        StructureNotifyMask |
        FocusChangeMask |
        PointerMotionMask | ButtonPressMask |
        ButtonReleaseMask | EnterWindowMask);

    XMapWindow(wnd.dsp, wnd.handle);

    //XIM im = XOpenIM(wnd.dsp, NULL, NULL, NULL);
    //XIC ic = XCreateIC(im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, wnd, NULL);

    //Atom target_atom = XInternAtom(wnd.dsp, "RAY_CLIPBOARD", False);

    LOAD_GL_PROC(glXCreateContextAttribsARB);

    i32 context_attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    GLXContext ctx = glXCreateContextAttribsARB(wnd.dsp, fbc, 0, true, context_attribs);
    XSync(wnd.dsp, false);

    glXMakeCurrent(wnd.dsp, wnd.handle, ctx);

    wnd.im = XOpenIM(wnd.dsp, NULL, NULL, NULL);
    wnd.ic = XCreateIC(wnd.im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, wnd.handle, NULL);

    return wnd;
}

void load_opengl_procs()
{
    LOAD_GL_PROC(glXSwapIntervalEXT);
    LOAD_GL_PROC(glXCreateContextAttribsARB);

    LOAD_GL_PROC(glGetString);
    LOAD_GL_PROC(glEnable);
    LOAD_GL_PROC(glDisable);
    LOAD_GL_PROC(glClearColor);
    LOAD_GL_PROC(glClear);
    LOAD_GL_PROC(glDrawArrays);

    LOAD_GL_PROC(glAttachShader);
    LOAD_GL_PROC(glBindAttribLocation);
    LOAD_GL_PROC(glCompileShader);
    LOAD_GL_PROC(glCreateProgram);
    LOAD_GL_PROC(glCreateShader);
    LOAD_GL_PROC(glLinkProgram);
    LOAD_GL_PROC(glShaderSource);
    LOAD_GL_PROC(glGenBuffers);
    LOAD_GL_PROC(glGenVertexArrays);
    LOAD_GL_PROC(glBindVertexArray);
    LOAD_GL_PROC(glBindBuffer);
    LOAD_GL_PROC(glBufferData);
    LOAD_GL_PROC(glBufferSubData);
    LOAD_GL_PROC(glVertexAttribPointer);
    LOAD_GL_PROC(glEnableVertexAttribArray);
    LOAD_GL_PROC(glDisableVertexAttribArray);
    LOAD_GL_PROC(glUseProgram);
    LOAD_GL_PROC(glUniform1f);
    LOAD_GL_PROC(glUniform2f);
    LOAD_GL_PROC(glUniform3f);
    LOAD_GL_PROC(glUniform1fv);
    LOAD_GL_PROC(glUniformMatrix3fv);
    LOAD_GL_PROC(glUniformMatrix4fv);
    LOAD_GL_PROC(glGetUniformLocation);
    LOAD_GL_PROC(glGetShaderiv);
    LOAD_GL_PROC(glGetShaderInfoLog);
    LOAD_GL_PROC(glGetProgramiv);
    LOAD_GL_PROC(glGetProgramInfoLog);
    LOAD_GL_PROC(glTexImage2D);
    LOAD_GL_PROC(glTexStorage2D);
    LOAD_GL_PROC(glGenTextures);
    LOAD_GL_PROC(glBindTexture);
    LOAD_GL_PROC(glTexParameteri);
    LOAD_GL_PROC(glBlendFunc);
    LOAD_GL_PROC(glDebugMessageCallback);
    LOAD_GL_PROC(glPolygonMode);
    LOAD_GL_PROC(glBindBufferRange);
    LOAD_GL_PROC(glScissor);
    LOAD_GL_PROC(glTextureSubImage2D);
}

void opengl_enable_vsync(Display *dsp, Window wnd)
{
    u32 interval;
    glXQueryDrawable(dsp, wnd, GLX_MAX_SWAP_INTERVAL_EXT, &interval);
    glXSwapIntervalEXT(dsp, wnd, interval);
}
