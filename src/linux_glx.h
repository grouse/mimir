#ifndef LINUX_GLX_H
#define LINUX_GLX_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "linux_opengl.h"

/// GLX constants
#define GLX_X_RENDERABLE		0x8012
#define GLX_DRAWABLE_TYPE		0x8010
#define GLX_RENDER_TYPE			0x8011
#define GLX_X_VISUAL_TYPE		0x22
#define GLX_SAMPLE_BUFFERS              0x186a0 /*100000*/
#define GLX_SAMPLES                     0x186a1 /*100001*/

#define GLX_RED_SIZE		8
#define GLX_GREEN_SIZE		9
#define GLX_BLUE_SIZE		10
#define GLX_ALPHA_SIZE		11
#define GLX_DEPTH_SIZE		12
#define GLX_STENCIL_SIZE	13
#define GLX_DOUBLEBUFFER	5
#define GLX_WINDOW_BIT			0x00000001
#define GLX_RGBA_BIT			0x00000001
#define GLX_TRUE_COLOR			0x8002

#define GLX_CONTEXT_MAJOR_VERSION_ARB           0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB           0x2092
#define GLX_CONTEXT_FLAGS_ARB                   0x2094
#define GLX_CONTEXT_PROFILE_MASK_ARB            0x9126
#define GLX_CONTEXT_DEBUG_BIT_ARB               0x0001
#define GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB  0x0002
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB        0x00000001
#define GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002

#define GLX_SWAP_INTERVAL_EXT               0x20F1
#define GLX_MAX_SWAP_INTERVAL_EXT           0x20F2

extern "C" {
    struct __GLXFBConfigRec {
        int visualType;
        int transparentType;
        /*    colors are floats scaled to ints */
        int transparentRed, transparentGreen, transparentBlue, transparentAlpha;
        int transparentIndex;

        int visualCaveat;

        int associatedVisualId;
        int screen;

        int drawableType;
        int renderType;

        int maxPbufferWidth, maxPbufferHeight, maxPbufferPixels;
        int optimalPbufferWidth, optimalPbufferHeight;  /* for SGIX_pbuffer */

        int visualSelectGroup;	/* visuals grouped by select priority */

        unsigned int id;

        GLboolean rgbMode;
        GLboolean colorIndexMode;
        GLboolean doubleBufferMode;
        GLboolean stereoMode;
        GLboolean haveAccumBuffer;
        GLboolean haveDepthBuffer;
        GLboolean haveStencilBuffer;

        /* The number of bits present in various buffers */
        GLint accumRedBits, accumGreenBits, accumBlueBits, accumAlphaBits;
        GLint depthBits;
        GLint stencilBits;
        GLint indexBits;
        GLint redBits, greenBits, blueBits, alphaBits;
        GLuint redMask, greenMask, blueMask, alphaMask;

        GLuint multiSampleSize;     /* Number of samples per pixel (0 if no ms) */

        GLuint nMultiSampleBuffers; /* Number of availble ms buffers */
        GLint maxAuxBuffers;

        /* frame buffer level */
        GLint level;

        /* color ranges (for SGI_color_range) */
        GLboolean extendedRange;
        GLdouble minRed, maxRed;
        GLdouble minGreen, maxGreen;
        GLdouble minBlue, maxBlue;
        GLdouble minAlpha, maxAlpha;
    };

    typedef struct __GLXFBConfigRec* GLXFBConfig;
    typedef struct __GLXcontextRec* GLXContext;

    typedef void (*__GLXextFuncPtr)(void);
    typedef XID GLXDrawable;
}

//#define LOAD_GL_ARB_PROC(proc, handle) proc = (proc##ARB_t*)gl_get_proc(#proc "ARB", handle)
#define LOAD_GL_PROC(proc) proc = (proc##_t*)glXGetProcAddressARB(#proc)


DECL_GL_PROC_S(GLfunction, glXGetProcAddressARB, const char*);
DECL_GL_PROC_S(Bool, glXQueryVersion, Display * dpy, int * major, int * minor);
DECL_GL_PROC_S(GLXFBConfig*, glXChooseFBConfig, Display * dpy, int screen, const int * attrib_list, int * nelements);
DECL_GL_PROC_S(int, glXGetFBConfigAttrib, Display *dpy, GLXFBConfig config, int attribute, int *value);
DECL_GL_PROC_S(XVisualInfo*, glXGetVisualFromFBConfig, Display *dpy, GLXFBConfig config);
DECL_GL_PROC_S(Bool, glXMakeCurrent, Display * dpy, GLXDrawable drawable, GLXContext ctx);
DECL_GL_PROC_S(void, glXSwapBuffers, Display * dpy, GLXDrawable drawable);
DECL_GL_PROC_S(GLXDrawable, glXGetCurrentDrawable, void );
DECL_GL_PROC_S(void, glXQueryDrawable, Display *dpy, GLXDrawable draw, int attribute, unsigned int *value );
DECL_GL_PROC(void, glXSwapIntervalEXT, Display *dpy, GLXDrawable drawable, int interval);
DECL_GL_PROC(GLXContext, glXCreateContextAttribsARB,Display *dpy, GLXFBConfig config, GLXContext share_context, Bool direct, const int *attrib_list);

#endif // LINUX_GLX_H
