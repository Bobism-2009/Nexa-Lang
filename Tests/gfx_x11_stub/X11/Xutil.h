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

extern int XSetWMNormalHints(Display* d, Window w, XSizeHints* h);
extern XSizeHints* XAllocSizeHints(void);

#endif
