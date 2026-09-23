#!/bin/sh
# std/gfx3d cover.
#
# gfx3d has the same problem gfx has, and then one more. A window cannot be
# opened on a machine with no display, a GL context cannot be made on one with
# no driver, and unlike gfx there is no framebuffer to read pixels back out of
# -- gfx3d draws through the GPU, so there is no gfx3d.get and nothing a test
# could assert about a rendered frame from inside Nexa. What is left that
# every machine can check is the two ends:
#
#   codegen   Transpile only (--source). Pins which __nexa_gfx3d_* call each
#             form emits and that the arguments arrive in the order they were
#             written. Never invokes the C++ compiler, so it runs anywhere
#             NexaC runs.
#
#   emit      The runtime is emitted only for a program that calls gfx3d, and
#             the GL entry points are looked up by name rather than linked, so
#             the generated file must contain the loader and must not contain a
#             GL header include. This is the "written from scratch" promise in
#             SYNTAX/Modules.txt, and it is the one that would rot quietly:
#             adding #include <GL/gl.h> would work on the machine that added it
#             and break every machine without the package.
#
#   headless  Build a program that never opens a window and run it, checking
#             the closed-window answers. Needs a C++ compiler but no display.
#             Skipped when the build fails, which on Linux means no X11
#             development headers.
#
#   wasm      The browser backend is not OpenGL 1.1 -- WebGL has no glBegin,
#             no matrix stack and no fixed-function anything -- so --wasm has
#             to compile the shader path INSTEAD of the desktop one. Both
#             halves of that are checked, because a preprocessor ladder
#             compiles either way and is only right one of them. Needs em++.
#
# Arity, unknown methods, void-in-value-position and argument types live in
# Tests/Lang/errors/gfx3d_*.nxa, where run_tests.sh already checks that NexaC
# does the complaining and clang never gets to speak.
#
# NOT covered here, and worth saying so plainly: which way a triangle faces.
# The curved shapes were first written wound inside-out, and every one of
# these layers passed on them -- the call emitted, the runtime was present,
# the program ran. What it looked like was a sphere with a gash through its
# equator, because back-face culling dropped the near surface and left the
# inside of the far one showing. Catching that needs a rendered frame and a
# look at it, which this suite has no way to do: gfx3d draws through the GPU
# and has no gfx3d.get to read a pixel back with. Change the winding in
# __nexa_g3_band or the basis swap in __nexa_g3_cap and run
# Examples/shapes3d_demo.nxa; the suite will not tell you.
#
# Skips are reported but do not fail the run. run_tests.sh only prints a
# suite's output when it fails, so run this directly to see what a machine
# actually ran:
#
#   sh Tests/gfx3d_cases.sh          (from the repo root)
#
# Usage: Tests/gfx3d_cases.sh [path-to-NexaC]

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")

SUITE=$(cd "$(dirname "$0")" && pwd)

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM

fails=0
skips=0

# The runtime is pasted in ahead of main and calls plenty of its own helpers,
# so only main is the transpiler's output for the program under test.
main_body() {
    awk '/^int main\(/ { inmain = 1 } inmain { print } inmain && /^\}/ { exit }' "$1"
}

# expect_emit <label> <grep-ERE> <body>
expect_emit() {
    label=$1
    want=$2
    body=$3

    printf '#include <std/gfx3d>\nfn main() {\n%s\n}\n' "$body" > "$WORK/case.nxa"
    if ! "$NEXAC" "$WORK/case.nxa" --source "$WORK/case.cpp" > "$WORK/case.log" 2>&1; then
        echo "FAIL $label: NexaC could not transpile"
        sed 's/^/  /' "$WORK/case.log"
        fails=$((fails + 1))
        return
    fi
    if ! main_body "$WORK/case.cpp" | grep -qE "$want"; then
        echo "FAIL $label: main does not match /$want/"
        main_body "$WORK/case.cpp" | sed 's/^/  /'
        fails=$((fails + 1))
    fi
}

# --- codegen layer ----------------------------------------------------------

expect_emit "open"        '__nexa_gfx3d_open\("t", 640, 480\)'          '    gfx3d.open("t", 640, 480);'
expect_emit "close"       '__nexa_gfx3d_close\(\)'                      '    gfx3d.close();'
expect_emit "poll"        '__nexa_gfx3d_poll\(\)'                       '    gfx3d.poll();'
expect_emit "present"     '__nexa_gfx3d_present\(\)'                    '    gfx3d.present();'
expect_emit "maxfps"      '__nexa_gfx3d_maxfps\(60\)'                   '    gfx3d.maxfps(60);'
expect_emit "clear"       '__nexa_gfx3d_clear\(1, 2, 3\)'               '    gfx3d.clear(1, 2, 3);'
expect_emit "camera"      '__nexa_gfx3d_camera\(1, 2, 3, 4, 5, 6\)'     '    gfx3d.camera(1, 2, 3, 4, 5, 6);'
expect_emit "perspective" '__nexa_gfx3d_perspective\(60, 1, 99\)'       '    gfx3d.perspective(60, 1, 99);'
expect_emit "cube"        '__nexa_gfx3d_cube\(1, 2, 3, 4, 5, 6, 7\)'    '    gfx3d.cube(1, 2, 3, 4, 5, 6, 7);'
expect_emit "box"         '__nexa_gfx3d_box\(1, 2, 3, 4, 5, 6, 7, 8, 9\)' '    gfx3d.box(1, 2, 3, 4, 5, 6, 7, 8, 9);'
expect_emit "sphere"      '__nexa_gfx3d_sphere\(1, 2, 3, 4, 5, 6, 7\)'  '    gfx3d.sphere(1, 2, 3, 4, 5, 6, 7);'
expect_emit "capsule"     '__nexa_gfx3d_capsule\(1, 2, 3, 4, 5, 6, 7, 8, 9, 10\)' '    gfx3d.capsule(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);'
expect_emit "cylinder"    '__nexa_gfx3d_cylinder\(1, 2, 3, 4, 5, 6, 7, 8, 9, 10\)' '    gfx3d.cylinder(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);'
expect_emit "cone"        '__nexa_gfx3d_cone\(1, 2, 3, 4, 5, 6, 7, 8, 9, 10\)' '    gfx3d.cone(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);'
expect_emit "line3"       '__nexa_gfx3d_line3\(1, 2, 3, 4, 5, 6, 7, 8, 9\)' '    gfx3d.line3(1, 2, 3, 4, 5, 6, 7, 8, 9);'
expect_emit "grid"        '__nexa_gfx3d_grid\(20, 1, 5, 6, 7\)'         '    gfx3d.grid(20, 1, 5, 6, 7);'
expect_emit "translate"   '__nexa_gfx3d_translate\(1, 2, 3\)'           '    gfx3d.translate(1, 2, 3);'
expect_emit "rotate"      '__nexa_gfx3d_rotate\(0, 90, 0\)'             '    gfx3d.rotate(0, 90, 0);'
expect_emit "scale"       '__nexa_gfx3d_scale\(2\)'                     '    gfx3d.scale(2);'
expect_emit "reset"       '__nexa_gfx3d_reset\(\)'                      '    gfx3d.reset();'
expect_emit "tri"         '__nexa_gfx3d_tri\(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12\)' \
                          '    gfx3d.tri(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);'
expect_emit "renderer"    '__nexa_gfx3d_renderer\("vulkan"\)'           '    gfx3d.renderer("vulkan");'
expect_emit "key"         '__nexa_gfx3d_key\("w"\)'                     '    let k = gfx3d.key("w");'
expect_emit "pressed"     '__nexa_gfx3d_pressed\("space"\)'             '    let k = gfx3d.pressed("space");'
expect_emit "released"    '__nexa_gfx3d_released\("space"\)'            '    let k = gfx3d.released("space");'
expect_emit "typed"       '__nexa_gfx3d_typed\(\)'                      '    let t = gfx3d.typed();'
expect_emit "wheel"       '__nexa_gfx3d_wheel\(\)'                      '    let w = gfx3d.wheel();'
expect_emit "wheel_x"     '__nexa_gfx3d_wheel_x\(\)'                    '    let w = gfx3d.wheel_x();'
expect_emit "mouse"       '__nexa_gfx3d_mouse\("left"\)'                '    let m = gfx3d.mouse("left");'
expect_emit "mouse_x"     '__nexa_gfx3d_mouse_x\(\)'                    '    let m = gfx3d.mouse_x();'
expect_emit "mouse_y"     '__nexa_gfx3d_mouse_y\(\)'                    '    let m = gfx3d.mouse_y();'

# An input call is nearly always written as a condition, and a condition is
# emitted by a different path from an expression -- one with an allow-list of
# node types and `return "false"` for everything else. gfx3d.key was not on
# that list, so `if (gfx3d.key("w"))` compiled to `if (false)`: the whole
# input family read as never-happening, in a program that built and ran
# without a word. Worth a test of its own, because nothing else would catch
# it -- the call emits perfectly well everywhere except where it is used.
expect_emit "key in a condition"      'if \(__nexa_gfx3d_key\("w"\)\)'       '    if (gfx3d.key("w")) { gfx3d.close(); }'
expect_emit "mouse in a condition"    'if \(__nexa_gfx3d_mouse\("left"\)\)'  '    if (gfx3d.mouse("left")) { gfx3d.close(); }'
expect_emit "typed in a condition"    'if \(!\(__nexa_gfx3d_typed\(\)\).empty\(\)\)' '    if (gfx3d.typed()) { gfx3d.close(); }'
expect_emit "backend"     '__nexa_gfx3d_backend\(\)'                    '    let b = gfx3d.backend();'
expect_emit "width"       '__nexa_gfx3d_width\(\)'                      '    let w = gfx3d.width();'
expect_emit "closed"      '__nexa_gfx3d_closed\(\)'                     '    let c = gfx3d.closed();'

# backend() is the one call in the module that answers with text, so it has to
# concatenate rather than be counted. Before exprIsString knew about it, this
# reached clang as std::to_string(std::string).
printf '#include <std/gfx3d>\n#include <std/io>\nfn main() {\n    io.println("b: " + gfx3d.backend());\n}\n' > "$WORK/cat.nxa"
if "$NEXAC" "$WORK/cat.nxa" --source "$WORK/cat.cpp" > "$WORK/cat.log" 2>&1; then
    if main_body "$WORK/cat.cpp" | grep -q 'to_string(__nexa_gfx3d_backend'; then
        echo "FAIL backend concat: wrapped in to_string; backend() returns a string"
        fails=$((fails + 1))
    fi
else
    echo "FAIL backend concat: NexaC could not transpile"
    fails=$((fails + 1))
fi

# --- emit layer -------------------------------------------------------------

# A program that never mentions gfx3d must not carry its runtime.
printf '#include <std/io>\nfn main() {\n    io.println("hi");\n}\n' > "$WORK/none.nxa"
if "$NEXAC" "$WORK/none.nxa" --source "$WORK/none.cpp" > /dev/null 2>&1; then
    if grep -q '__nexa_gfx3d_' "$WORK/none.cpp"; then
        echo "FAIL emit: gfx3d runtime emitted for a program that never calls it"
        fails=$((fails + 1))
    fi
else
    echo "FAIL emit: could not transpile the no-gfx3d program"
    fails=$((fails + 1))
fi

printf '#include <std/gfx3d>\nfn main() {\n    gfx3d.open("t", 8, 8);\n}\n' > "$WORK/one.nxa"
if "$NEXAC" "$WORK/one.nxa" --source "$WORK/one.cpp" > /dev/null 2>&1; then
    # Written from scratch: the entry points are fetched by name, so no GL
    # development header may appear in the generated file.
    if grep -qE '#include[ ]*<GL/|#include[ ]*<OpenGL/|#include[ ]*<GLES' "$WORK/one.cpp"; then
        echo "FAIL emit: generated C++ includes a GL header; the runtime must declare its own"
        grep -nE '#include[ ]*<GL/|#include[ ]*<OpenGL/|#include[ ]*<GLES' "$WORK/one.cpp" | sed 's/^/  /'
        fails=$((fails + 1))
    fi
    for sym in __nexa_g3_load_gl __nexa_g3_perspective __nexa_g3_look_at __nexa_g3_platform_open \n               __nexa_g3_shade __nexa_g3_basis __nexa_g3_band __nexa_g3_cap __nexa_g3_disc; do
        if ! grep -q "$sym" "$WORK/one.cpp"; then
            echo "FAIL emit: $sym missing from the generated runtime"
            fails=$((fails + 1))
        fi
    done
    # libGLU is the other thing "from scratch" rules out: gluPerspective and
    # gluLookAt are what __nexa_g3_perspective / __nexa_g3_look_at replace.
    if grep -qE 'gluPerspective|gluLookAt|#include[ ]*<GL/glu' "$WORK/one.cpp"; then
        echo "FAIL emit: generated C++ reaches for libGLU"
        fails=$((fails + 1))
    fi
else
    echo "FAIL emit: could not transpile the gfx3d program"
    fails=$((fails + 1))
fi

# --- headless layer ---------------------------------------------------------

# Never opens a window, so it needs no display: these are the answers the
# module gives before the first gfx3d.open, which SYNTAX/Modules.txt spells out.
cat > "$WORK/headless.nxa" <<'NXA'
#include <std/gfx3d>
#include <std/io>

fn main() {
    io.println("closed=" + gfx3d.closed());
    io.println("width=" + gfx3d.width());
    io.println("height=" + gfx3d.height());
    io.println("backend=[" + gfx3d.backend() + "]");
    io.println("renderer_opengl=" + gfx3d.renderer("opengl"));
    io.println("renderer_vulkan=" + gfx3d.renderer("vulkan"));
    io.println("renderer_bogus=" + gfx3d.renderer("metal"));
    gfx3d.clear(1, 2, 3);
    gfx3d.cube(0.0, 0.0, 0.0, 1.0, 1, 2, 3);
    gfx3d.present();
    io.println("survived");
}
NXA

cat > "$WORK/headless.expected" <<'EXP'
closed=0
width=0
height=0
backend=[]
renderer_opengl=1
renderer_vulkan=1
renderer_bogus=0
survived
EXP

if "$NEXAC" "$WORK/headless.nxa" -o "$WORK/headless" > "$WORK/hl.log" 2>&1; then
    hlbin="$WORK/headless"
    [ -x "$hlbin" ] || hlbin="$WORK/headless.exe"
    if "$hlbin" > "$WORK/hl.raw" 2>&1; then
        # Windows stdio writes CRLF and the expected text here is LF. The
        # line endings are the console's business, not the module's, so
        # they are compared past rather than pinned.
        if ! diff -u --strip-trailing-cr "$WORK/headless.expected" "$WORK/hl.raw" > "$WORK/hl.diff" 2>&1; then
            echo "FAIL headless: output moved"
            sed 's/^/  /' "$WORK/hl.diff"
            fails=$((fails + 1))
        fi
    else
        echo "FAIL headless: the program did not run"
        sed 's/^/  /' "$WORK/hl.raw"
        fails=$((fails + 1))
    fi
else
    echo "skip headless: could not build (no C++ toolchain, or no X11 headers on Linux)"
    skips=$((skips + 1))
fi


# --- transform layer --------------------------------------------------------

# Where a point lands is arithmetic, so unlike every other shape in this module
# it can be checked without a window, a driver or a display. It is also the
# part most worth checking: a rotation with its handedness backwards still
# spins, and a composition in the wrong order still moves things -- both look
# busy and both are wrong.

printf '#include <std/gfx3d>
fn main() {
    gfx3d.open("t", 8, 8);
    gfx3d.translate(1, 2, 3);
    gfx3d.rotate(0, 90, 0);
    gfx3d.scale(2);
    gfx3d.reset();
    gfx3d.clear(0, 0, 0);
}
' > "$WORK/xf.nxa"
if "$NEXAC" "$WORK/xf.nxa" --source "$WORK/xf.cpp" > "$WORK/xf.log" 2>&1; then
    CXX=${NEXA_CXX:-}
    [ -n "$CXX" ] || { command -v clang++ >/dev/null 2>&1 && CXX=clang++; }
    [ -n "$CXX" ] || { command -v g++ >/dev/null 2>&1 && CXX=g++; }
    # The generated file carries this platform's window code whether or not a
    # window is ever opened, so the driver has to link what that code calls.
    XFLANG=""
    case "$(uname -s 2>/dev/null)" in
        MINGW*|MSYS*|CYGWIN*) XFLINK="-luser32 -lgdi32" ;;
        Darwin)               XFLINK="-framework Cocoa"; XFLANG="-x objective-c++" ;;
        *)                    XFLINK="-lX11 -ldl" ;;
    esac
    if [ -z "$CXX" ]; then
        echo "skip transform: no C++ compiler"
        skips=$((skips + 1))
    elif "$CXX" -std=c++17 -O1 -DNEXA_GEN="\"$WORK/xf.cpp\"" $XFLANG "$SUITE/gfx3d_transform_semantics.cpp" $XFLINK -o "$WORK/xf_sem" > "$WORK/xf_build.log" 2>&1; then
        xfbin="$WORK/xf_sem"
        [ -x "$xfbin" ] || xfbin="$WORK/xf_sem.exe"
        if ! "$xfbin" > "$WORK/xf.out" 2>&1; then
            echo "FAIL transform semantics:"
            sed 's/^/  /' "$WORK/xf.out" | head -8
            fails=$((fails + 1))
        fi
    else
        echo "skip transform: the driver would not build on this machine"
        sed 's/^/  /' "$WORK/xf_build.log" | tail -3
        skips=$((skips + 1))
    fi
else
    echo "FAIL transform: NexaC could not transpile the transform program"
    sed 's/^/  /' "$WORK/xf.log" | tail -3
    fails=$((fails + 1))
fi

# --- wasm layer -------------------------------------------------------------

# The browser is the one backend that is not OpenGL 1.1. WebGL has no glBegin,
# no matrix stack and no fixed-function anything, so --wasm has to compile the
# shader path INSTEAD of the desktop one -- not as well as it. That is a
# preprocessor ladder, which is exactly the kind of thing that compiles either
# way and is wrong one of them, so both halves are checked: the shader path has
# to be there and the fixed-function path has to be gone.

have_emcc=0
if command -v em++ >/dev/null 2>&1; then
    have_emcc=1
elif [ -x "$HOME/emsdk/upstream/emscripten/em++" ] || [ -x "$HOME/emsdk/upstream/emscripten/em++.exe" ]; then
    have_emcc=1
fi

if [ $have_emcc -eq 0 ]; then
    echo "skip wasm: no em++ (install Emscripten, or set EMSDK)"
    skips=$((skips + 1))
else
    cat > "$WORK/w.nxa" <<'NXA'
#include <std/gfx3d>

fn main() {
    gfx3d.open("w", 320, 240);
    gfx3d.camera(3.0, 2.0, 4.0, 0.0, 0.0, 0.0);
    gfx3d.clear(10, 10, 20);
    gfx3d.cube(0.0, 0.0, 0.0, 1.0, 200, 80, 60);
    gfx3d.tri(0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1, 2, 3);
    gfx3d.present();
    gfx3d.close();
}
NXA
    if "$NEXAC" "$WORK/w.nxa" --wasm -o "$WORK/w" > "$WORK/w.log" 2>&1; then
        [ -f "$WORK/w.js" ]   || { echo "FAIL wasm: no loader"; fails=$((fails + 1)); }
        [ -f "$WORK/w.html" ] || { echo "FAIL wasm: no page"; fails=$((fails + 1)); }
        # A 3D program draws into a canvas, so it must get the canvas page and
        # not the text console one.
        if [ -f "$WORK/w.html" ] && ! grep -q '<canvas' "$WORK/w.html"; then
            echo "FAIL wasm: a gfx3d page has no canvas on it"
            fails=$((fails + 1))
        fi
        # The shader path is what WebGL can actually run.
        for sym in glCreateShader glLinkProgram glDrawArrays glUniformMatrix4fv emscripten_webgl_create_context; do
            if ! grep -a -q "$sym" "$WORK/w.js" 2>/dev/null; then
                echo "FAIL wasm: $sym missing; the WebGL backend was not compiled in"
                fails=$((fails + 1))
            fi
        done
        # And the fixed-function path must not have come along: none of these
        # exist in WebGL, so their presence would mean the wrong branch won.
        for sym in glLoadMatrixf glMatrixMode glVertex3f; do
            if grep -a -q "$sym" "$WORK/w.js" 2>/dev/null; then
                echo "FAIL wasm: $sym reached a WebGL build; the desktop branch was taken"
                fails=$((fails + 1))
            fi
        done
    else
        echo "FAIL wasm: --wasm build of a gfx3d program failed"
        sed 's/^/  /' "$WORK/w.log" | tail -6
        fails=$((fails + 1))
    fi
fi
# --- report -----------------------------------------------------------------

if [ $fails -eq 0 ]; then
    if [ $skips -gt 0 ]; then
        echo "gfx3d ok ($skips layer(s) skipped: see above)"
    else
        echo "gfx3d ok"
    fi
    exit 0
fi
echo "gfx3d: $fails failure(s)"
exit 1
