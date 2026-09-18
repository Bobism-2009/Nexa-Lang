// Semantics cover for the input-collection slicing (BOB-57): the X11 event
// mask, executed.
//
// Which events a window is subscribed to is a request to the X server and
// nothing else: no X call reports the mask back, and a program cannot tell
// from the inside which events it is not being sent. So this is the same shape
// as the gfx.borderless and gfx.transparent drivers -- what is checked is what
// went out on the wire, and Tests/gfx_x11_stub is what records it.
//
// The claim is that the mask follows the program. A program that reads no
// input asks for redraws and size changes and no more; each family adds the
// events it reads and nothing else; and a program that reads all four asks for
// exactly the mask every gfx program asked for before any of this existed.
// Which of those is being checked is NEXA_WANT_MASK, because the driver has to
// be built once per program: the generated runtime it includes *is* the thing
// under test, so one binary can only hold one.
//
// Built and run by Tests/gfx_input_cases.sh. NEXA_GEN is the generated .cpp.

#include <cstdio>
#include <string>

#ifndef NEXA_GEN
#error "define NEXA_GEN to the generated C++ file"
#endif
#ifndef NEXA_WANT_MASK
#error "define NEXA_WANT_MASK to the event mask the program should ask for"
#endif
#ifndef NEXA_CASE
#define NEXA_CASE "mask"
#endif

// The generated program has its own main(); this driver supplies the real one.
#define main __nexa_program_main
#include NEXA_GEN
#undef main

int main() {
    nexa_x11_stub_reset(1);
    if (__nexa_gfx_open("t", 8, 8, 1) != 1) {
        std::printf("FAIL %s: the fake server refused the window\n", NEXA_CASE);
        return 1;
    }
    long got = nexa_x11_stub_event_mask();
    long want = (long)(NEXA_WANT_MASK);
    if (got != want) {
        std::printf("FAIL %s: asked the server for mask 0x%lx, wanted 0x%lx\n",
                    NEXA_CASE, (unsigned long)got, (unsigned long)want);
        return 1;
    }
    std::printf("ok %s\n", NEXA_CASE);
    return 0;
}
