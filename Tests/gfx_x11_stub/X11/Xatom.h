/* Xatom stand-in for the headless gfx tests. See X11/Xlib.h.
 *
 * The predefined atoms: the handful of atom ids an X server has before any
 * client interns anything, fixed by the protocol rather than handed out. Only
 * XA_ATOM is used by the runtime -- a property whose value is a list of atoms,
 * such as _NET_WM_STATE, says so by taking that as its type -- but the ones
 * around it are here so the numbering is the protocol's own and reads as such.
 */
#ifndef NEXA_TEST_X11_XATOM_H
#define NEXA_TEST_X11_XATOM_H

#include <X11/Xlib.h>

#define XA_PRIMARY ((Atom)1)
#define XA_SECONDARY ((Atom)2)
#define XA_ARC ((Atom)3)
#define XA_ATOM ((Atom)4)
#define XA_BITMAP ((Atom)5)
#define XA_CARDINAL ((Atom)6)
#define XA_COLORMAP ((Atom)7)
#define XA_CURSOR ((Atom)8)
#define XA_INTEGER ((Atom)19)
#define XA_STRING ((Atom)31)
#define XA_WINDOW ((Atom)33)

#endif
