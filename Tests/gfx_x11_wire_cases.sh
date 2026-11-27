#!/bin/sh
# NexaC's own X11 client (include/X11WireRuntime.hpp), which a Linux std/gfx
# or std/ui executable speaks to the X server with in place of a baked-in
# static libX11.
#
#   emit     the generated program carries the client behind NEXA_X11_WIRE
#   link     the executable NexaC builds needs no libX11.so, and a window
#            program stays small: it was 1.1MB with the static Xlib
#   window   on a headless Xvfb: a window opens, what was drawn is what the
#            server holds (read back with xwd), and gfx_open_close_test runs
#            through title, resize, borderless, ontop, fullscreen and the
#            32-bit transparent window -- over the Unix socket, over TCP, with
#            an MIT-MAGIC-COOKIE-1, and refused with the wrong one
#   keys     key translation is libX11's: Tests/gfx_x11_wire_keys.cpp dumps
#            every keycode under sixteen modifier states, built against
#            libX11 and against the client, and the two must agree
#
# The window layer needs Xvfb (and xwd, xauth for their parts); the keys layer
# needs Xvfb and libX11's headers. Each is skipped, and says so, without them.
# Linux only: the client is Linux's.
#
# Usage: Tests/gfx_x11_wire_cases.sh [path-to-NexaC]   (from the repo root)

set -u

case "$(uname -s 2>/dev/null)" in
    Linux) ;;
    *) echo "SKIP gfx_x11_wire: the X11 client is Linux's"; echo "gfx_x11_wire ok"; exit 0 ;;
esac

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
WORK=$(mktemp -d)
XPIDS=""
cleanup() {
    for p in $XPIDS; do kill "$p" 2>/dev/null; done
    rm -rf "$WORK"
}
trap cleanup EXIT INT TERM

fails=0
skips=0
ok() { echo "ok $1"; }
bad() { echo "FAIL $1"; fails=$((fails + 1)); }
skip() { echo "SKIP $1"; skips=$((skips + 1)); }

cat > "$WORK/pix.nxa" <<'NXA'
#include <std/gfx>
#include <std/io>
#include <std/time>
fn main() {
    if (gfx.open("wire-test", 4, 2, 8) != 1) {
        io.println("no window");
        return;
    }
    let frames = 0;
    while (gfx.closed() != 1 && frames < 100) {
        gfx.poll();
        gfx.clear(0, 0, 0);
        gfx.fill(0, 0, 2, 1, 255, 0, 0);
        gfx.fill(2, 0, 2, 1, 0, 255, 0);
        gfx.fill(0, 1, 2, 1, 0, 0, 255);
        gfx.fill(2, 1, 2, 1, 255, 255, 255);
        gfx.present();
        time.sleep(20);
        frames = frames + 1;
    }
    io.println("frames=" + frames);
}
NXA

# --- emit -------------------------------------------------------------------

if ! "$NEXAC" "$WORK/pix.nxa" --source "$WORK/pix.cpp" > "$WORK/emit.log" 2>&1; then
    bad "emit: NexaC could not transpile a gfx program"
    sed 's/^/  /' "$WORK/emit.log"
    exit 1
fi
if grep -q '^#if defined(NEXA_X11_WIRE)$' "$WORK/pix.cpp" && grep -q 'static Display\* XOpenDisplay' "$WORK/pix.cpp" &&
   grep -q '^#include <X11/Xlib.h>$' "$WORK/pix.cpp"; then
    ok "emit: the client behind NEXA_X11_WIRE, Xlib's headers otherwise"
else
    bad "emit: the gfx program does not carry the client and the Xlib fallback"
fi

# --- link -------------------------------------------------------------------

if ! "$NEXAC" "$WORK/pix.nxa" -o "$WORK/pix" > "$WORK/build.log" 2>&1; then
    bad "link: NexaC could not build a gfx program"
    sed 's/^/  /' "$WORK/build.log" | tail -5
    exit 1
fi
if grep -qi 'warning' "$WORK/build.log"; then
    bad "link: the build warned"
    grep -i 'warning' "$WORK/build.log" | head -3 | sed 's/^/  /'
else
    ok "link: built with no warnings"
fi
needed=$( (readelf -d "$WORK/pix" 2>/dev/null || objdump -p "$WORK/pix" 2>/dev/null) | grep NEEDED)
if printf '%s\n' "$needed" | grep -qiE 'libX11|libxcb|libXau|libXdmcp'; then
    bad "link: the program still needs an X library: $(printf '%s' "$needed" | tr '\n' ' ')"
else
    ok "link: no X library among the program's dependencies"
fi
size=$(wc -c < "$WORK/pix" | tr -d ' ')
if [ "$size" -lt 65536 ]; then ok "link: a window program is $size bytes"
else bad "link: a window program is $size bytes, 64KB or more"; fi

# --- window -----------------------------------------------------------------

# A display number nothing is using, from $1 up, in XN.
free_display() {
    XN=$1
    while [ -e "/tmp/.X11-unix/X$XN" ] || [ -e "/tmp/.X$XN-lock" ]; do XN=$((XN + 1)); done
}

# An Xvfb on the first free display from $1 up; the rest are its options.
start_xvfb() {
    free_display "$1"
    n=$XN
    shift
    Xvfb ":$n" -screen 0 320x240x24 "$@" > /dev/null 2>&1 &
    XPIDS="$XPIDS $!"
    i=0
    while [ ! -e "/tmp/.X11-unix/X$n" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
    XN=$n
}

if ! command -v Xvfb > /dev/null 2>&1; then
    skip "window: no Xvfb (apt install xvfb)"
else
    "$NEXAC" "$ROOT/Tests/gfx_open_close_test.nxa" -o "$WORK/oc" > "$WORK/oc.log" 2>&1 ||
        bad "window: gfx_open_close_test did not build"
    start_xvfb 90 -nolisten tcp
    UNIX_N=$XN
    if command -v xwd > /dev/null 2>&1; then
        DISPLAY=":$UNIX_N" "$WORK/pix" > "$WORK/pix.out" 2>&1 &
        P=$!
        sleep 1
        DISPLAY=":$UNIX_N" xwd -root -silent > "$WORK/shot.xwd" 2>/dev/null
        wait $P
        # No window manager, so the window is where it was put: 80,80 and a
        # one-pixel border; each of the four colours is 16x8 pixels.
        got=$(python3 - "$WORK/shot.xwd" <<'PY' 2>/dev/null
import struct, sys
b = open(sys.argv[1], 'rb').read()
h = struct.unpack('>25I', b[:100])
data = b[h[0] + 12 * h[19]:]
out = []
for x, y in ((85, 85), (110, 85), (85, 92), (110, 92)):
    o = y * h[12] + x * (h[11] // 8)
    v = int.from_bytes(data[o:o + 4], 'little' if h[7] == 0 else 'big')
    out.append('%d,%d,%d' % ((v >> 16) & 255, (v >> 8) & 255, v & 255))
print(' '.join(out))
PY
)
        if [ "$got" = "255,0,0 0,255,0 0,0,255 255,255,255" ]; then
            ok "window: the server holds what was drawn ($(cat "$WORK/pix.out"))"
        else
            bad "window: read back [$got], drew red, green, blue, white ($(cat "$WORK/pix.out"))"
        fi
    else
        skip "window: no xwd to read the window back with (apt install x11-apps)"
    fi
    (cd "$ROOT" && DISPLAY=":$UNIX_N" "$WORK/oc" > "$WORK/oc.out" 2>&1)
    if [ $? -eq 0 ]; then ok "window: gfx_open_close_test over the Unix socket"
    else bad "window: gfx_open_close_test over the Unix socket: $(cat "$WORK/oc.out")"; fi

    start_xvfb $((UNIX_N + 1)) -listen tcp -ac
    (cd "$ROOT" && DISPLAY="127.0.0.1:$XN" "$WORK/oc" > "$WORK/oc.out" 2>&1)
    if [ $? -eq 0 ]; then ok "window: gfx_open_close_test over TCP"
    else bad "window: gfx_open_close_test over TCP: $(cat "$WORK/oc.out")"; fi

    if command -v xauth > /dev/null 2>&1 && command -v mcookie > /dev/null 2>&1; then
        # The server reads its -auth file as it starts, so the cookie goes in
        # first, for the display it is about to be.
        free_display $((XN + 1))
        xauth -q -f "$WORK/xa" add ":$XN" . "$(mcookie)" 2> /dev/null
        xauth -q -f "$WORK/xbad" add ":$XN" . "$(mcookie)" 2> /dev/null
        start_xvfb "$XN" -nolisten tcp -auth "$WORK/xa"
        (cd "$ROOT" && XAUTHORITY="$WORK/xa" DISPLAY=":$XN" "$WORK/oc" > "$WORK/oc.out" 2>&1)
        if [ $? -eq 0 ]; then ok "window: the right cookie is let in"
        else bad "window: the right cookie: $(cat "$WORK/oc.out")"; fi
        XAUTHORITY="$WORK/xbad" DISPLAY=":$XN" "$WORK/pix" > "$WORK/bad.out" 2>&1
        if grep -q '^no window$' "$WORK/bad.out"; then ok "window: a wrong cookie is no window, not a crash"
        else bad "window: a wrong cookie: $(cat "$WORK/bad.out")"; fi
    else
        skip "window: no xauth/mcookie for the cookie cases (apt install xauth)"
    fi
fi

# --- keys -------------------------------------------------------------------

CXX="${NEXA_CXX:-}"
if [ -z "$CXX" ]; then
    for c in g++ clang++ c++; do
        if command -v "$c" > /dev/null 2>&1; then CXX=$c; break; fi
    done
fi
if ! command -v Xvfb > /dev/null 2>&1; then
    skip "keys: no Xvfb"
elif [ -z "$CXX" ]; then
    skip "keys: no C++ compiler"
elif ! "$CXX" -std=c++17 -O1 -DREAL "$HERE/gfx_x11_wire_keys.cpp" -o "$WORK/keys_real" -lX11 > "$WORK/kr.log" 2>&1; then
    skip "keys: libX11's headers are not installed (apt install libx11-dev)"
elif ! printf '#include <cstdio>\n#include "X11WireRuntime.hpp"\nint main() { std::fputs(nexa::x11WireRuntimeCpp().c_str(), stdout); }\n' > "$WORK/gen.cpp" ||
     ! "$CXX" -std=c++17 -I "$ROOT/include" "$WORK/gen.cpp" -o "$WORK/gen" > "$WORK/kw.log" 2>&1 ||
     ! "$WORK/gen" > "$WORK/wire.inc" ||
     ! "$CXX" -std=c++17 -O1 -Wall -Wextra -I "$WORK" "$HERE/gfx_x11_wire_keys.cpp" -o "$WORK/keys_wire" > "$WORK/kw.log" 2>&1; then
    bad "keys: the client does not compile on its own"
    sed 's/^/  /' "$WORK/kw.log" | head -5
elif grep -q 'warning' "$WORK/kw.log"; then
    bad "keys: the client compiles with warnings"
    sed 's/^/  /' "$WORK/kw.log" | head -5
else
    start_xvfb 95 -nolisten tcp
    DISPLAY=":$XN" "$WORK/keys_real" > "$WORK/real.txt" 2>&1
    DISPLAY=":$XN" "$WORK/keys_wire" > "$WORK/wire.txt" 2>&1
    if [ ! -s "$WORK/real.txt" ] || grep -q 'no display' "$WORK/real.txt"; then
        skip "keys: libX11 could not open the Xvfb"
    elif cmp -s "$WORK/real.txt" "$WORK/wire.txt"; then
        ok "keys: libX11 and the client agree on all $(wc -l < "$WORK/real.txt" | tr -d ' ') lines"
    else
        bad "keys: libX11 and the client disagree on $(diff "$WORK/real.txt" "$WORK/wire.txt" | grep -c '^<') lines"
        diff "$WORK/real.txt" "$WORK/wire.txt" | head -8 | sed 's/^/  /'
    fi
fi

# --- report -----------------------------------------------------------------

if [ "$fails" -ne 0 ]; then
    echo "gfx_x11_wire: $fails failure(s)"
    exit 1
fi
if [ "$skips" -ne 0 ]; then echo "gfx_x11_wire ok ($skips layer(s) skipped: see above)"
else echo "gfx_x11_wire ok"; fi
