/* Xutil stand-in for the headless gfx tests (BOB-21). See X11/Xlib.h. */
#ifndef NEXA_TEST_X11_XUTIL_H
#define NEXA_TEST_X11_XUTIL_H

#include <X11/Xlib.h>

#define NoValue 0x0000
#define USPosition (1L << 0)
#define USSize (1L << 1)
#define PMinSize (1L << 4)
#define PMaxSize (1L << 5)

typedef struct {
    long flags;
    int x, y;
    int width, height;
    int min_width, min_height;
    int max_width, max_height;
} XSizeHints;

/* How a program asks the server for a visual other than the screen's own --
   gfx.transparent wants depth 32 TrueColor, which is the one with an alpha
   channel. Declared here rather than in Xlib.h because that is where the real
   X11 puts it. */
typedef struct {
    Visual* visual;
    VisualID visualid;
    int screen;
    int depth;
    int c_class;
    unsigned long red_mask, green_mask, blue_mask;
    int colormap_size;
    int bits_per_rgb;
} XVisualInfo;

extern Status XMatchVisualInfo(Display* d, int screen, int depth, int c_class,
                               XVisualInfo* out);

extern int XSetWMNormalHints(Display* d, Window w, XSizeHints* h);
extern XSizeHints* XAllocSizeHints(void);

#endif
