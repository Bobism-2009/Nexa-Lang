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
#   sound     gfx3d's sound calls go to the mixer std/gfx uses, and there is
#             one of it for the program however many modules ask. So this layer
#             checks the shared name is what emits, and then counts: a second
#             mixer would link and then open the device twice. It also pins the
#             two things that are easy to get subtly wrong -- that the platform
#             headers sit OUTSIDE the [nexa:audio-*] markers, since
#             Tests/gfx_sound_cases.sh cuts that range out and a header inside
#             it vanishes with the device, and that gfx3d.poll pumps the mixer
#             BEFORE its window check, so a program playing a sound without a
#             window is not silent. Transpile only.
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
expect_emit "light"       '__nexa_gfx3d_light\(0, 1, 0\)'               '    gfx3d.light(0, 1, 0);'
expect_emit "light colour" '__nexa_gfx3d_light\(0, 1, 0, 255, 200, 150\)' '    gfx3d.light(0, 1, 0, 255, 200, 150);'
expect_emit "ambient set" '__nexa_gfx3d_ambient\(60\)'                  '    gfx3d.ambient(60);'
# Reading and setting are two runtime calls, not one with a sentinel level:
# every value 0..255 is a real one, so none is free to mean "tell me".
expect_emit "ambient read" '__nexa_gfx3d_ambient_get\(\)'               '    let a = gfx3d.ambient();'
expect_emit "tri"         '__nexa_gfx3d_tri\(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12\)' \
                          '    gfx3d.tri(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);'
expect_emit "renderer"    '__nexa_gfx3d_renderer\("vulkan"\)'           '    gfx3d.renderer("vulkan");'
expect_emit "antialias"   '__nexa_gfx3d_antialias\(1\)'                 '    gfx3d.antialias(1);'
expect_emit "fog"         '__nexa_gfx3d_fog\(1, 2, 3, 4, 5\)'            '    gfx3d.fog(1, 2, 3, 4, 5);'
expect_emit "fog off"     '__nexa_gfx3d_fog_off\(\)'                    '    let f = gfx3d.fog();'
expect_emit "mirror"      '__nexa_gfx3d_mirror\(0, 1, 2, 0, 1, 0, 3, 4\)' '    gfx3d.mirror(0, 1, 2, 0, 1, 0, 3, 4);'
expect_emit "mirror end"  '__nexa_gfx3d_mirror_end\(\)'                 '    let m = gfx3d.mirror();'
expect_emit "shadow"      '__nexa_gfx3d_shadow\(\)'                     '    let s = gfx3d.shadow();'
expect_emit "lamp"        '__nexa_gfx3d_lamp\(1, 2, 3, 4\)'              '    gfx3d.lamp(1, 2, 3, 4);'
expect_emit "lamp colour" '__nexa_gfx3d_lamp\(1, 2, 3, 4, 5, 6, 7\)'     '    gfx3d.lamp(1, 2, 3, 4, 5, 6, 7);'
expect_emit "lamp off"    '__nexa_gfx3d_lamp_off\(\)'                    '    let l = gfx3d.lamp();'
expect_emit "glint"       '__nexa_gfx3d_glint\(\)'                       '    let g = gfx3d.glint();'
expect_emit "glint level" '__nexa_gfx3d_glint_set\(40\)'                 '    gfx3d.glint(40);'
expect_emit "bloom"       '__nexa_gfx3d_bloom\(\)'                       '    let b = gfx3d.bloom();'
expect_emit "bloom level" '__nexa_gfx3d_bloom_set\(40\)'                 '    gfx3d.bloom(40);'
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

# --- model layer ------------------------------------------------------------
#
# gfx3d.model reads a file, which makes it the one part of the module whose
# behaviour can be checked here without a window or a GPU: the .obj goes in and
# a triangle count comes out. So this layer actually runs the parser over files
# written for it, rather than only pinning what the calls emit.
#
# The format is forgiving by design -- a model that names a material this
# renderer cannot honour still has a shape worth drawing -- so the cases below
# are mostly about what is skipped, not what is read.

expect_emit "model"      '__nexa_gfx3d_model\("a.obj"\)'   '    let m = gfx3d.model("a.obj");'
expect_emit "model_tris" '__nexa_gfx3d_model_tris\(1\)'    '    let n = gfx3d.model_tris(1);'
expect_emit "draw"       '__nexa_gfx3d_draw\(1, 2, 3, 4, 5, 6, 7, 8\)'                          '    gfx3d.draw(1, 2, 3, 4, 5, 6, 7, 8);'

# The .obj reader is a block of its own, so a program that never mentions a
# model must not carry it -- nor the <vector> and file parsing it brings in.
printf '#include <std/gfx3d>
fn main() {
    gfx3d.cube(0.0, 0.0, 0.0, 1.0, 1, 2, 3);
}
' > "$WORK/nomodel.nxa"
if "$NEXAC" "$WORK/nomodel.nxa" --source "$WORK/nomodel.cpp" > /dev/null 2>&1; then
    if grep -qE '__nexa_g3_obj_load|__nexa_G3Model' "$WORK/nomodel.cpp"; then
        echo "FAIL model slicing: the .obj reader is in a program that loads none"
        fails=$((fails + 1))
    fi
else
    echo "FAIL model slicing: NexaC could not transpile"
    fails=$((fails + 1))
fi

# And a program that does mention one must carry it, since the codegen lines
# above grep main and never compile: a usage flag that failed to switch the
# block on would satisfy every one of them and then fail in the linker.
printf '#include <std/gfx3d>
fn main() {
    let m = gfx3d.model("a.obj");
    gfx3d.draw(m, 0.0, 0.0, 0.0, 1.0, 1, 2, 3);
}
' > "$WORK/yesmodel.nxa"
if "$NEXAC" "$WORK/yesmodel.nxa" --source "$WORK/yesmodel.cpp" > /dev/null 2>&1; then
    for sym in '__nexa_g3_obj_load' '__nexa_gfx3d_draw' '__nexa_gfx3d_model'; do
        if ! grep -q "$sym" "$WORK/yesmodel.cpp"; then
            echo "FAIL model slicing: no $sym in a program that loads a model"
            fails=$((fails + 1))
        fi
    done
else
    echo "FAIL model slicing: NexaC could not transpile"
    fails=$((fails + 1))
fi

# Multisample is chosen when the window opens, so a program that never asks
# carries none of the chooser. One that does carries the chooser and the enable.
printf '#include <std/gfx3d>
fn main() {
    gfx3d.cube(0.0, 0.0, 0.0, 1.0, 1, 2, 3);
}
' > "$WORK/noaa.nxa"
if "$NEXAC" "$WORK/noaa.nxa" --source "$WORK/noaa.cpp" > /dev/null 2>&1; then
    if grep -qE '__nexa_g3_aa|__nexa_gfx3d_antialias|0x809D' "$WORK/noaa.cpp"; then
        echo "FAIL antialias slicing: the multisample path is in a program that never asks"
        fails=$((fails + 1))
    fi
else
    echo "FAIL antialias slicing: NexaC could not transpile"
    fails=$((fails + 1))
fi
printf '#include <std/gfx3d>
fn main() {
    gfx3d.antialias(1);
    gfx3d.open("t", 320, 240);
}
' > "$WORK/yesaa.nxa"
if "$NEXAC" "$WORK/yesaa.nxa" --source "$WORK/yesaa.cpp" > /dev/null 2>&1; then
    # The other platforms' choosers are in the same string, then the host
    # strips the #if bodies it will not compile, so a Windows run only has
    # the wgl chooser left to see.
    for sym in '__nexa_gfx3d_antialias' 'static int __nexa_g3_aa' '__nexa_g3_aa_pf' \
               '0x809D' 'wglChoosePixelFormatARB'; do
        if ! grep -q "$sym" "$WORK/yesaa.cpp"; then
            echo "FAIL antialias slicing: no $sym in a program that asks"
            fails=$((fails + 1))
        fi
    done
else
    echo "FAIL antialias slicing: NexaC could not transpile"
    fails=$((fails + 1))
fi

# Fog is a draw-time blend, so a program that never asks carries none of the
# loader or the shader lines. One that does carries both.
printf '#include <std/gfx3d>
fn main() {
    gfx3d.cube(0.0, 0.0, 0.0, 1.0, 1, 2, 3);
}
' > "$WORK/nofog.nxa"
if "$NEXAC" "$WORK/nofog.nxa" --source "$WORK/nofog.cpp" > /dev/null 2>&1; then
    if grep -qE 'glFogf|__nexa_gfx3d_fog|vEye' "$WORK/nofog.cpp"; then
        echo "FAIL fog slicing: fog is in a program that never asks"
        fails=$((fails + 1))
    fi
else
    echo "FAIL fog slicing: NexaC could not transpile"
    fails=$((fails + 1))
fi
printf '#include <std/gfx3d>
fn main() {
    gfx3d.fog(180, 190, 200, 8, 40);
    gfx3d.open("t", 320, 240);
}
' > "$WORK/yesfog.nxa"
if "$NEXAC" "$WORK/yesfog.nxa" --source "$WORK/yesfog.cpp" > /dev/null 2>&1; then
    for sym in '__nexa_gfx3d_fog' 'glFogf' 'vEye' 'uFogOn'; do
        if ! grep -q "$sym" "$WORK/yesfog.cpp"; then
            echo "FAIL fog slicing: no $sym in a program that asks"
            fails=$((fails + 1))
        fi
    done
else
    echo "FAIL fog slicing: NexaC could not transpile"
    fails=$((fails + 1))
fi

# A mirror is a second draw of the world. A program that never asks carries
# none of the copy or the glass.
printf '#include <std/gfx3d>
fn main() {
    gfx3d.cube(0.0, 0.0, 0.0, 1.0, 1, 2, 3);
}
' > "$WORK/nomirror.nxa"
if "$NEXAC" "$WORK/nomirror.nxa" --source "$WORK/nomirror.cpp" > /dev/null 2>&1; then
    if grep -qE 'glCopyTexSubImage2D|__nexa_gfx3d_mirror|uMirrorOn' "$WORK/nomirror.cpp"; then
        echo "FAIL mirror slicing: a mirror is in a program that never asks"
        fails=$((fails + 1))
    fi
else
    echo "FAIL mirror slicing: NexaC could not transpile"
    fails=$((fails + 1))
fi
printf '#include <std/gfx3d>
fn main() {
    gfx3d.mirror(0, 1, -3, 0, 0, 1, 3, 2);
    gfx3d.open("t", 320, 240);
}
' > "$WORK/yesmirror.nxa"
if "$NEXAC" "$WORK/yesmirror.nxa" --source "$WORK/yesmirror.cpp" > /dev/null 2>&1; then
    for sym in '__nexa_gfx3d_mirror' 'glCopyTexSubImage2D' 'uMirrorOn' '__nexa_g3_mirror_pane'; do
        if ! grep -q "$sym" "$WORK/yesmirror.cpp"; then
            echo "FAIL mirror slicing: no $sym in a program that asks"
            fails=$((fails + 1))
        fi
    done
else
    echo "FAIL mirror slicing: NexaC could not transpile"
    fails=$((fails + 1))
fi

# A shadow is the light's picture. A program that never asks carries none of
# the copy, the packed depth, or the Vulkan pass.
printf '#include <std/gfx3d>
fn main() {
    gfx3d.cube(0.0, 0.0, 0.0, 1.0, 1, 2, 3);
}
' > "$WORK/noshadow.nxa"
if "$NEXAC" "$WORK/noshadow.nxa" --source "$WORK/noshadow.cpp" > /dev/null 2>&1; then
    if grep -qE '__nexa_gfx3d_shadow|uShadowOn|shadow_hold_end' "$WORK/noshadow.cpp"; then
        echo "FAIL shadow slicing: a shadow is in a program that never asks"
        fails=$((fails + 1))
    fi
else
    echo "FAIL shadow slicing: NexaC could not transpile"
    fails=$((fails + 1))
fi
printf '#include <std/gfx3d>
fn main() {
    gfx3d.shadow();
    gfx3d.open("t", 320, 240);
}
' > "$WORK/yesshadow.nxa"
if "$NEXAC" "$WORK/yesshadow.nxa" --source "$WORK/yesshadow.cpp" > /dev/null 2>&1; then
    for sym in '__nexa_gfx3d_shadow' 'uShadowOn' 'shadow_hold_end'; do
        if ! grep -q "$sym" "$WORK/yesshadow.cpp"; then
            echo "FAIL shadow slicing: no $sym in a program that asks"
            fails=$((fails + 1))
        fi
    done
else
    echo "FAIL shadow slicing: NexaC could not transpile"
    fails=$((fails + 1))
fi

# A lamp is a light with a place. A program that never asks carries none of it,
# and a shadow on its own stays the shadow it was.
printf '#include <std/gfx3d>
fn main() {
    gfx3d.cube(0.0, 0.0, 0.0, 1.0, 1, 2, 3);
}
' > "$WORK/nolamp.nxa"
if "$NEXAC" "$WORK/nolamp.nxa" --source "$WORK/nolamp.cpp" > /dev/null 2>&1; then
    if grep -qE '__nexa_gfx3d_lamp|uLampCol|vLampPos' "$WORK/nolamp.cpp"; then
        echo "FAIL lamp slicing: a lamp is in a program that never asks"
        fails=$((fails + 1))
    fi
else
    echo "FAIL lamp slicing: NexaC could not transpile"
    fails=$((fails + 1))
fi
printf '#include <std/gfx3d>
fn main() {
    gfx3d.shadow();
    gfx3d.open("t", 320, 240);
}
' > "$WORK/shadownolamp.nxa"
if "$NEXAC" "$WORK/shadownolamp.nxa" --source "$WORK/shadownolamp.cpp" > /dev/null 2>&1; then
    if grep -qE '__nexa_gfx3d_lamp|uLampCol|vLampPos' "$WORK/shadownolamp.cpp"; then
        echo "FAIL lamp slicing: a lamp is in a shadow that never asks"
        fails=$((fails + 1))
    fi
else
    echo "FAIL lamp slicing: NexaC could not transpile the shadow"
    fails=$((fails + 1))
fi
printf '#include <std/gfx3d>
fn main() {
    gfx3d.lamp(0, 1, 0, 4);
    gfx3d.open("t", 320, 240);
}
' > "$WORK/yeslamp.nxa"
if "$NEXAC" "$WORK/yeslamp.nxa" --source "$WORK/yeslamp.cpp" > /dev/null 2>&1; then
    for sym in '__nexa_gfx3d_lamp' 'uLampCol' 'vLampPos' 'const int N = 8'; do
        if ! grep -q "$sym" "$WORK/yeslamp.cpp"; then
            echo "FAIL lamp slicing: no $sym in a program that asks"
            fails=$((fails + 1))
        fi
    done
else
    echo "FAIL lamp slicing: NexaC could not transpile"
    fails=$((fails + 1))
fi
printf '#include <std/gfx3d>
fn main() {
    gfx3d.shadow();
    gfx3d.lamp(0, 1, 0, 4, 255, 200, 140);
    gfx3d.open("t", 320, 240);
}
' > "$WORK/lampshadow.nxa"
if "$NEXAC" "$WORK/lampshadow.nxa" --source "$WORK/lampshadow.cpp" > /dev/null 2>&1; then
    if ! grep -q 'uLamp - vLampPos' "$WORK/lampshadow.cpp"; then
        echo "FAIL lamp slicing: the shadow shader does not add the lamp"
        fails=$((fails + 1))
    fi
else
    echo "FAIL lamp slicing: NexaC could not transpile lamp and shadow"
    fails=$((fails + 1))
fi

# A glint is a tight highlight. A program that never asks carries none of it,
# and a lamp or a shadow on its own stays as it was.
printf '#include <std/gfx3d>
fn main() {
    gfx3d.cube(0.0, 0.0, 0.0, 1.0, 1, 2, 3);
}
' > "$WORK/noglint.nxa"
if "$NEXAC" "$WORK/noglint.nxa" --source "$WORK/noglint.cpp" > /dev/null 2>&1; then
    if grep -qE '__nexa_gfx3d_glint|uGlint|__nexa_g3_glint_on' "$WORK/noglint.cpp"; then
        echo "FAIL glint slicing: a glint is in a program that never asks"
        fails=$((fails + 1))
    fi
else
    echo "FAIL glint slicing: NexaC could not transpile"
    fails=$((fails + 1))
fi
if "$NEXAC" "$WORK/yeslamp.nxa" --source "$WORK/lampnoglint.cpp" > /dev/null 2>&1; then
    if grep -qE '__nexa_gfx3d_glint|uGlint' "$WORK/lampnoglint.cpp"; then
        echo "FAIL glint slicing: a glint is in a lamp that never asks"
        fails=$((fails + 1))
    fi
else
    echo "FAIL glint slicing: NexaC could not transpile the lamp"
    fails=$((fails + 1))
fi
if "$NEXAC" "$WORK/shadownolamp.nxa" --source "$WORK/shadownoglint.cpp" > /dev/null 2>&1; then
    if grep -qE '__nexa_gfx3d_glint|uGlint' "$WORK/shadownoglint.cpp"; then
        echo "FAIL glint slicing: a glint is in a shadow that never asks"
        fails=$((fails + 1))
    fi
else
    echo "FAIL glint slicing: NexaC could not transpile the shadow"
    fails=$((fails + 1))
fi
printf '#include <std/gfx3d>
fn main() {
    gfx3d.glint();
    gfx3d.open("t", 320, 240);
}
' > "$WORK/yesglint.nxa"
if "$NEXAC" "$WORK/yesglint.nxa" --source "$WORK/yesglint.cpp" > /dev/null 2>&1; then
    for sym in '__nexa_gfx3d_glint' 'uGlint' 'pow(snh, 256.0)'; do
        if ! grep -q "$sym" "$WORK/yesglint.cpp"; then
            echo "FAIL glint slicing: no $sym in a program that asks"
            fails=$((fails + 1))
        fi
    done
else
    echo "FAIL glint slicing: NexaC could not transpile"
    fails=$((fails + 1))
fi
printf '#include <std/gfx3d>
fn main() {
    gfx3d.shadow();
    gfx3d.glint(200);
    gfx3d.open("t", 320, 240);
}
' > "$WORK/glintshadow.nxa"
if "$NEXAC" "$WORK/glintshadow.nxa" --source "$WORK/glintshadow.cpp" > /dev/null 2>&1; then
    if ! awk '
        /vec3 c = vCol/ { c = NR; sun = 0 }
        /uSunCol \* \(uGlint/ { if (c) sun = NR }
        /if \(vLight\.w > 0\.0\)/ { if (!(c && sun && c < sun && sun < NR)) bad = 1 }
        END { exit bad ? 1 : 0 }
    ' "$WORK/glintshadow.cpp"; then
        echo "FAIL glint slicing: the sun highlight is not darkened by the shadow"
        fails=$((fails + 1))
    fi
else
    echo "FAIL glint slicing: NexaC could not transpile glint and shadow"
    fails=$((fails + 1))
fi
printf '#include <std/gfx3d>
fn main() {
    gfx3d.shadow();
    gfx3d.lamp(0, 1, 0, 4, 255, 200, 140);
    gfx3d.glint();
    gfx3d.open("t", 320, 240);
}
' > "$WORK/glintlamp.nxa"
if "$NEXAC" "$WORK/glintlamp.nxa" --source "$WORK/glintlamp.cpp" > /dev/null 2>&1; then
    if ! awk '
        /c \*= mix\(1\.0, mix\(1\.0, 0\.42/ { dark = NR }
        /uLampCol \* \(uGlint/ { if (dark && NR > dark) after = 1 }
        END { exit after ? 0 : 1 }
    ' "$WORK/glintlamp.cpp"; then
        echo "FAIL glint slicing: the lamp highlight is not after the shadow"
        fails=$((fails + 1))
    fi
else
    echo "FAIL glint slicing: NexaC could not transpile glint, lamp and shadow"
    fails=$((fails + 1))
fi

# A bloom is a glow around bright pixels. A program that never asks carries
# none of it, and a glint on its own stays a glint.
if "$NEXAC" "$WORK/noglint.nxa" --source "$WORK/nobloom.cpp" > /dev/null 2>&1; then
    if grep -qE '__nexa_gfx3d_bloom|__nexa_g3_bloom_present|uStrength' "$WORK/nobloom.cpp"; then
        echo "FAIL bloom slicing: a bloom is in a program that never asks"
        fails=$((fails + 1))
    fi
else
    echo "FAIL bloom slicing: NexaC could not transpile"
    fails=$((fails + 1))
fi
if "$NEXAC" "$WORK/yesglint.nxa" --source "$WORK/glintnobloom.cpp" > /dev/null 2>&1; then
    if grep -qE '__nexa_gfx3d_bloom|__nexa_g3_bloom_present' "$WORK/glintnobloom.cpp"; then
        echo "FAIL bloom slicing: a bloom is in a glint that never asks"
        fails=$((fails + 1))
    fi
else
    echo "FAIL bloom slicing: NexaC could not transpile the glint"
    fails=$((fails + 1))
fi
printf '#include <std/gfx3d>
fn main() {
    gfx3d.bloom();
    gfx3d.open("t", 320, 240);
}
' > "$WORK/yesbloom.nxa"
if "$NEXAC" "$WORK/yesbloom.nxa" --source "$WORK/yesbloom.cpp" > /dev/null 2>&1; then
    for sym in '__nexa_gfx3d_bloom' '__nexa_g3_bloom_present' 'smoothstep(0.72, 1.0, b)'; do
        if ! grep -q "$sym" "$WORK/yesbloom.cpp"; then
            echo "FAIL bloom slicing: no $sym in a program that asks"
            fails=$((fails + 1))
        fi
    done
else
    echo "FAIL bloom slicing: NexaC could not transpile"
    fails=$((fails + 1))
fi

# The parser, over files written for it. Needs a C++ compiler but no display:
# loading a model opens no window, deliberately, so that a program can have its
# models ready before it has somewhere to put them.
mkdir -p "$WORK/obj"
cat > "$WORK/obj/cube.obj" <<'OBJ'
# quads, no normals: six faces fan to twelve triangles
v -1.0 -1.0 -1.0
v  1.0 -1.0 -1.0
v  1.0  1.0 -1.0
v -1.0  1.0 -1.0
v -1.0 -1.0  1.0
v  1.0 -1.0  1.0
v  1.0  1.0  1.0
v -1.0  1.0  1.0
f 1 2 3 4
f 5 6 7 8
f 1 2 6 5
f 2 3 7 6
f 3 4 8 7
f 4 1 5 8
OBJ

# Everything the format can throw at it that this renderer does not read, plus
# the two index forms that are easy to get wrong: a full v/vt/vn reference and
# a negative one counting back from the end.
cat > "$WORK/obj/awkward.obj" <<'OBJ'
mtllib nothing.mtl
o thing
g thing
s off
usemtl none
v 0.0 0.0 0.0
v 4.0 0.0 0.0
v 0.0 4.0 0.0
vt 0.0 0.0
vn 0.0 0.0 1.0
f 1/1/1 2/1/1 -1//-1
OBJ

cat > "$WORK/obj/notobj.obj" <<'OBJ'
this file is not a model
and neither is this line
OBJ

cat > "$WORK/obj/empty.obj" <<'OBJ'
OBJ

cat > "$WORK/model.nxa" <<'NXA'
#include <std/gfx3d>
#include <std/io>

fn main() {
    let cube: int = gfx3d.model("cube.obj");
    io.println("cube=" + cube + " tris=" + gfx3d.model_tris(cube));
    io.println("cached=" + gfx3d.model("cube.obj"));
    let awk: int = gfx3d.model("awkward.obj");
    io.println("awkward=" + awk + " tris=" + gfx3d.model_tris(awk));
    io.println("missing=" + gfx3d.model("no_such.obj"));
    io.println("notobj=" + gfx3d.model("notobj.obj"));
    io.println("empty=" + gfx3d.model("empty.obj"));
    io.println("tris0=" + gfx3d.model_tris(0));
    io.println("tris99=" + gfx3d.model_tris(99));
}
NXA

cat > "$WORK/obj/model.expected" <<'EXP'
cube=1 tris=12
cached=1
awkward=2 tris=1
missing=0
notobj=0
empty=0
tris0=0
tris99=0
EXP

if "$NEXAC" "$WORK/model.nxa" -o "$WORK/obj/model" > "$WORK/model.log" 2>&1; then
    mbin="$WORK/obj/model"
    [ -x "$mbin" ] || mbin="$WORK/obj/model.exe"
    # Run from the directory holding the .obj files, since the paths are
    # relative and the point is the parser, not path resolution.
    if (cd "$WORK/obj" && "$mbin" > "$WORK/obj/model.raw" 2>&1); then
        if ! diff -u --strip-trailing-cr "$WORK/obj/model.expected" "$WORK/obj/model.raw" > "$WORK/obj/model.diff" 2>&1; then
            echo "FAIL model parse: output moved"
            sed 's/^/  /' "$WORK/obj/model.diff"
            fails=$((fails + 1))
        fi
    else
        echo "FAIL model parse: the program did not run"
        sed 's/^/  /' "$WORK/obj/model.raw"
        fails=$((fails + 1))
    fi
else
    echo "skip model parse: could not build (no C++ toolchain)"
    skips=$((skips + 1))
fi

# --- texture layer ----------------------------------------------------------
#
# gfx3d.texture decodes with std/gfx's decoder, so it is sliced like the model
# reader: a program that loads no texture carries no decoder. A program that
# decodes in both modules must carry one decoder, not two -- which only a
# compiler can see, since both chunks are written out and one is guarded away.
# What a texture looks like on a shape is, like the winding above, beyond this
# suite; Examples/texture3d_demo.nxa is the look.

expect_emit "texture" '__nexa_gfx3d_texture\("a.png"\)' '    let t = gfx3d.texture("a.png");'
expect_emit "use"     '__nexa_gfx3d_use\(1\)'            '    gfx3d.use(1);'

printf '#include <std/gfx3d>\nfn main() {\n    gfx3d.cube(0.0, 0.0, 0.0, 1.0, 255, 0, 0);\n}\n' > "$WORK/notex.nxa"
if "$NEXAC" "$WORK/notex.nxa" --source "$WORK/notex.cpp" > /dev/null 2>&1; then
    if grep -qE '__nexa_gfx_decode_rgba|__nexa_gfx3d_texture|wincodec|stbi' "$WORK/notex.cpp"; then
        echo "FAIL texture slicing: a program that loads no texture carries the decoder"
        fails=$((fails + 1))
    fi
else
    echo "FAIL texture slicing: NexaC could not transpile"
    fails=$((fails + 1))
fi

mkdir -p "$WORK/tex"
cp "$SUITE/gfx_2x2.png" "$WORK/tex/a.png"
printf 'not an image\n' > "$WORK/tex/bad.png"
cat > "$WORK/tex.nxa" <<'NXA'
#include <std/gfx3d>
#include <std/io>

fn main() {
    let t: int = gfx3d.texture("a.png");
    io.println("tex=" + t);
    io.println("cached=" + gfx3d.texture("a.png"));
    io.println("missing=" + gfx3d.texture("no_such.png"));
    io.println("bad=" + gfx3d.texture("bad.png"));
    gfx3d.use(t);
    gfx3d.use(99);
    gfx3d.use(0);
    // Drawing with no window is a no-op, textured or not.
    gfx3d.use(t);
    gfx3d.sphere(0.0, 0.0, 0.0, 1.0, 255, 255, 255);
    io.println("done");
}
NXA
cat > "$WORK/tex/tex.expected" <<'EXP'
tex=1
cached=1
missing=0
bad=0
done
EXP
if "$NEXAC" "$WORK/tex.nxa" -o "$WORK/tex/tex" > "$WORK/tex.log" 2>&1; then
    tbin="$WORK/tex/tex"
    [ -x "$tbin" ] || tbin="$WORK/tex/tex.exe"
    if (cd "$WORK/tex" && "$tbin" > "$WORK/tex/tex.raw" 2>&1); then
        if ! diff -u --strip-trailing-cr "$WORK/tex/tex.expected" "$WORK/tex/tex.raw" > "$WORK/tex/tex.diff" 2>&1; then
            echo "FAIL texture load: output moved"
            sed 's/^/  /' "$WORK/tex/tex.diff"
            fails=$((fails + 1))
        fi
    else
        echo "FAIL texture load: the program did not run"
        sed 's/^/  /' "$WORK/tex/tex.raw"
        fails=$((fails + 1))
    fi
else
    echo "skip texture load: could not build (no C++ toolchain)"
    skips=$((skips + 1))
fi

# Both modules decoding: one decoder, or the build fails on a redefinition.
cat > "$WORK/bothdecode.nxa" <<'NXA'
#include <std/gfx>
#include <std/gfx3d>

fn main() {
    let a: int = gfx.image("a.png");
    let b: int = gfx3d.texture("a.png");
    if (a + b > 100) { gfx3d.use(b); }
}
NXA
if "$NEXAC" "$WORK/bothdecode.nxa" -o "$WORK/tex/both" > "$WORK/both.log" 2>&1; then
    :
elif grep -qiE 'redefinition|already defined|duplicate' "$WORK/both.log"; then
    echo "FAIL texture decoder: gfx.image and gfx3d.texture carry two decoders"
    grep -iE 'error' "$WORK/both.log" | head -n 3 | sed 's/^/  /'
    fails=$((fails + 1))
else
    echo "skip texture decoder: could not build (no C++ toolchain)"
    skips=$((skips + 1))
fi

# --- sound layer ------------------------------------------------------------
#
# gfx3d's sound calls are the only ones in the module that do not emit a
# __nexa_gfx3d_ name: they go to the mixer std/gfx uses, because there is one
# mixer for both modules. So what is checked here is not just "the call
# emitted" but "it emitted the shared one, and there is exactly one of it".
#
# That last part is the whole reason this is not simply nine more codegen
# lines. Two mixers in one program would compile and link perfectly and then
# open the same device twice -- two streams fighting on waveOut, an outright
# failure on the Linux kernel PCM path -- and only in a program that used both
# std/gfx and std/gfx3d sound, which is the case nothing else here builds.

expect_emit "sound"        '__nexa_gfx_sound\("a.wav"\)'              '    let s = gfx3d.sound("a.wav");'
# The defaults are gfx's, because it is gfx's call: no volume is full volume,
# and the third argument is the loop bit that separates play from loop.
expect_emit "play default" '__nexa_gfx_voice_start\(__nexa_var_[0-9]+, 255, 0\)' \
                           '    let s = gfx3d.sound("a.wav");
    let v = gfx3d.play(s);'
expect_emit "play volume"  '__nexa_gfx_voice_start\(__nexa_var_[0-9]+, 128, 0\)' \
                           '    let s = gfx3d.sound("a.wav");
    let v = gfx3d.play(s, 128);'
expect_emit "loop bit"     '__nexa_gfx_voice_start\(__nexa_var_[0-9]+, 255, 1\)' \
                           '    let s = gfx3d.sound("a.wav");
    let v = gfx3d.loop(s);'
# stop() with nothing is every voice, which the runtime spells as voice 0.
expect_emit "stop all"     '__nexa_gfx_stop\(0\)'                     '    gfx3d.stop();'
expect_emit "stop one"     '__nexa_gfx_stop\(7\)'                     '    gfx3d.stop(7);'
# Read and set are two runtime calls for the reason gfx3d.ambient's are: every
# level 0..255 is a real one, so none is free to mean "tell me".
expect_emit "volume read"  '__nexa_gfx_volume_get\(\)'                '    let v = gfx3d.volume();'
expect_emit "volume set"   '__nexa_gfx_volume_set\(90\)'              '    gfx3d.volume(90);'
expect_emit "audio default" '__nexa_gfx_audio\(44100\)'               '    let a = gfx3d.audio();'
expect_emit "audio rate"   '__nexa_gfx_audio\(22050\)'                '    let a = gfx3d.audio(22050);'
expect_emit "sample"       '__nexa_gfx_sample\(1234\)'                '    gfx3d.sample(1234);'
expect_emit "audio_queued" '__nexa_gfx_audio_queued\(\)'              '    let q = gfx3d.audio_queued();'
# Flushing plays what play() started, so the mixer gets its turn first.
expect_emit "audio_flush pumps first" '__nexa_gfx_mix_pump\(\), __nexa_gfx_audio_flush\(\)' \
                                      '    gfx3d.audio_flush();'

# One mixer, not two. Every one of these counts a *definition*, so a second
# copy of the stack shows up as 2 and the emitted program would not have
# compiled anyway -- which is the point: this fails at the count rather than
# leaving someone to read a redefinition error out of clang.
printf '#include <std/gfx>\n#include <std/gfx3d>\nfn main() {\n    let a = gfx.sound("a.wav");\n    let b = gfx3d.sound("b.wav");\n    gfx.play(a);\n    gfx3d.play(b);\n}\n' > "$WORK/both.nxa"
if "$NEXAC" "$WORK/both.nxa" --source "$WORK/both.cpp" > "$WORK/both.log" 2>&1; then
    for sym in '^static int __nexa_gfx_voice_start' \
               '^static int __nexa_gfx_wav_decode' \
               '^static int __nexa_gfx_sound' \
               '^static void __nexa_gfx_mix_pump' \
               '^static std::string __nexa_gfx_read_file' \
               '^static int __nexa_gfx_audio\('; do
        n=$(grep -cE "$sym" "$WORK/both.cpp")
        if [ "$n" != "1" ]; then
            echo "FAIL one mixer: $n definitions matching /$sym/, want 1"
            fails=$((fails + 1))
        fi
    done
    # The device backend is bracketed by a marker pair, so exactly one block
    # means exactly two marker lines.
    n=$(grep -c 'nexa:audio-backend' "$WORK/both.cpp")
    if [ "$n" != "2" ]; then
        echo "FAIL one mixer: $n audio-backend marker lines, want 2 (one block)"
        fails=$((fails + 1))
    fi
else
    echo "FAIL one mixer: NexaC could not transpile gfx + gfx3d together"
    sed 's/^/  /' "$WORK/both.log"
    fails=$((fails + 1))
fi

# And a gfx3d-only program that plays a sound must actually carry the mixer.
# The codegen layer above greps main and never invokes a C++ compiler, so a
# usage flag that failed to switch the shared stack on would satisfy every one
# of those lines -- the call emits either way -- and then fail in the linker,
# in somebody's build rather than here. This is the only thing that looks.
printf '#include <std/gfx3d>\nfn main() {\n    let s = gfx3d.sound("a.wav");\n    gfx3d.play(s);\n}\n' > "$WORK/only3d.nxa"
if "$NEXAC" "$WORK/only3d.nxa" --source "$WORK/only3d.cpp" > "$WORK/only3d.log" 2>&1; then
    for sym in '^static int __nexa_gfx_voice_start' \
               '^static int __nexa_gfx_sound' \
               '^static void __nexa_gfx_mix_pump\(\) \{$'; do
        if ! grep -qE "$sym" "$WORK/only3d.cpp"; then
            echo "FAIL gfx3d-only sound: no definition matching /$sym/ (std/gfx is not included)"
            fails=$((fails + 1))
        fi
    done
else
    echo "FAIL gfx3d-only sound: NexaC could not transpile"
    sed 's/^/  /' "$WORK/only3d.log"
    fails=$((fails + 1))
fi

# The markers bracket the device, not the headers. <windows.h> was moved inside
# them once, which built here and broke Tests/gfx_sound_cases.sh -- that suite
# cuts the marked range out to drop a fake speaker in, so a header between the
# markers is a header that vanishes from the sliced file, and the Win32 window
# is written against that one. Nothing else in this suite would notice.
printf '#include <std/gfx3d>\nfn main() {\n    let s = gfx3d.sound("a.wav");\n    gfx3d.play(s);\n}\n' > "$WORK/hdr.nxa"
if "$NEXAC" "$WORK/hdr.nxa" --source "$WORK/hdr.cpp" > /dev/null 2>&1; then
    if awk '/nexa:audio-backend-begin/ { inb = 1 } inb { print } /nexa:audio-backend-end/ { exit }' \
            "$WORK/hdr.cpp" | grep -qE '#include <(windows|emscripten)\.h>'; then
        echo "FAIL audio markers: a platform header is inside them; slicing removes it"
        fails=$((fails + 1))
    fi
else
    echo "FAIL audio markers: NexaC could not transpile"
    fails=$((fails + 1))
fi

# The browser gets WebAudio and nothing else. This is transpile-only -- no em++
# needed, because choosing the platform happens before anything is compiled --
# and it is worth its own line because gfx3d's own ladder is written against
# NEXA_WASM while the audio backend's is written against __EMSCRIPTEN__. Those
# are two different spellings of the same target, and a slice that got one
# right and the other wrong would take waveOut to the browser.
if "$NEXAC" "$WORK/hdr.nxa" --wasm --source "$WORK/hdrw.cpp" > "$WORK/hdrw.log" 2>&1; then
    if grep -q 'waveOut' "$WORK/hdrw.cpp"; then
        echo "FAIL wasm sound: waveOut survived into the browser slice"
        fails=$((fails + 1))
    fi
    if grep -q 'mmsystem' "$WORK/hdrw.cpp"; then
        echo "FAIL wasm sound: <mmsystem.h> survived into the browser slice"
        fails=$((fails + 1))
    fi
    if ! grep -q 'nexaAC' "$WORK/hdrw.cpp"; then
        echo "FAIL wasm sound: no WebAudio context in the browser slice"
        fails=$((fails + 1))
    fi
else
    echo "FAIL wasm sound: NexaC could not transpile for --wasm"
    sed 's/^/  /' "$WORK/hdrw.log"
    fails=$((fails + 1))
fi

# A gfx3d program that plays nothing must not carry the mixer -- and must still
# have something for gfx3d.poll and gfx3d.close to call, because both name the
# hooks unconditionally. Empty stubs, in exactly the program that needs them.
printf '#include <std/gfx3d>\nfn main() {\n    gfx3d.poll();\n    gfx3d.close();\n}\n' > "$WORK/quiet.nxa"
if "$NEXAC" "$WORK/quiet.nxa" --source "$WORK/quiet.cpp" > /dev/null 2>&1; then
    if grep -qE '__nexa_gfx_voice_start|__nexa_wav_u32|waveOut' "$WORK/quiet.cpp"; then
        echo "FAIL quiet gfx3d: the mixer is in a program that plays nothing"
        fails=$((fails + 1))
    fi
    for stub in '__nexa_gfx_mix_pump\(\) \{\}' \
                '__nexa_gfx_sound_reset\(\) \{\}' \
                '__nexa_gfx_audio_close\(\) \{\}'; do
        if ! grep -qE "$stub" "$WORK/quiet.cpp"; then
            echo "FAIL quiet gfx3d: no stub matching /$stub/"
            fails=$((fails + 1))
        fi
    done
else
    echo "FAIL quiet gfx3d: NexaC could not transpile"
    fails=$((fails + 1))
fi

# gfx3d.poll() tops the mixer up, and does it before the window check, so a
# program that plays a sound without opening a window still gets one. The
# ordering is the assertion: mix_pump has to come before the `ready` return,
# and a patch that tidied it below would leave a silent program that looks
# right. gfx3d.close() takes the stream down for the same reason.
printf '#include <std/gfx3d>\nfn main() {\n    let s = gfx3d.sound("a.wav");\n    gfx3d.play(s);\n    gfx3d.poll();\n}\n' > "$WORK/pump.nxa"
if "$NEXAC" "$WORK/pump.nxa" --source "$WORK/pump.cpp" > /dev/null 2>&1; then
    if ! awk '/^static void __nexa_gfx3d_poll/ { inp = 1 }
              inp && /__nexa_gfx_mix_pump\(\)/ { found = 1 }
              inp && /__nexa_g3\.ready/ { exit }
              END { exit !found }' "$WORK/pump.cpp"; then
        echo "FAIL poll pumps: __nexa_gfx_mix_pump is not ahead of the ready check in gfx3d.poll"
        fails=$((fails + 1))
    fi
    if ! awk '/^static void __nexa_gfx3d_close/ { inc = 1 }
              inc && /__nexa_gfx_sound_reset\(\)/ { found = 1 }
              inc && /__nexa_g3\.ready/ { exit }
              END { exit !found }' "$WORK/pump.cpp"; then
        echo "FAIL close quiets: __nexa_gfx_sound_reset is not ahead of the ready check in gfx3d.close"
        fails=$((fails + 1))
    fi
else
    echo "FAIL poll pumps: NexaC could not transpile"
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

# --- overlay layer ----------------------------------------------------------
#
# std/gfx drawn over a gfx3d window. The bridge that does it is emitted only for
# a program that draws with gfx, so first which programs get it; then the CPU
# half, run. The GL half -- laying the framebuffer over the frame -- needs a
# window and cannot be checked here.

overlay_emit() {
    label=$1
    want=$2   # "bridge" or "stubs"
    body=$3
    printf '%s\n' "$body" > "$WORK/ov.nxa"
    if ! "$NEXAC" "$WORK/ov.nxa" --source "$WORK/ov.cpp" > "$WORK/ov.log" 2>&1; then
        echo "FAIL overlay $label: NexaC could not transpile"
        fails=$((fails + 1))
        return
    fi
    if grep -q 'static void __nexa_g3_overlay_present(void) {}' "$WORK/ov.cpp"; then got=stubs
    elif grep -q 'static void __nexa_g3_overlay_present(void) {$' "$WORK/ov.cpp"; then got=bridge
    else got=none; fi
    if [ "$got" != "$want" ]; then
        echo "FAIL overlay $label: got $got, want $want"
        fails=$((fails + 1))
    fi
}

overlay_emit "gfx draws on a 3D window" bridge '#include <std/gfx>
#include <std/gfx3d>
fn main() {
    gfx3d.open("t", 8, 8);
    gfx.fill(0, 0, 4, 4, 1, 2, 3);
    gfx3d.present();
}'
# gfx reached for nothing but sound: a full-window upload every frame to show
# nothing at all is exactly what the bridge is left out to avoid.
overlay_emit "gfx only for sound" stubs '#include <std/gfx>
#include <std/gfx3d>
fn main() {
    let s = gfx.sound("a.wav");
    gfx3d.open("t", 8, 8);
    gfx.play(s);
}'
overlay_emit "gfx3d alone" stubs '#include <std/gfx3d>
fn main() {
    gfx3d.open("t", 8, 8);
}'

printf '#include <std/gfx>\n#include <std/gfx3d>\nfn main() {\n    gfx3d.open("t", 8, 8);\n    gfx.fill(0, 0, 4, 4, 1, 2, 3);\n    gfx.alpha(128);\n    gfx3d.present();\n}\n' > "$WORK/ovs.nxa"
if "$NEXAC" "$WORK/ovs.nxa" --source "$WORK/ovs.cpp" > "$WORK/ovs.log" 2>&1; then
    CXX=${NEXA_CXX:-}
    [ -n "$CXX" ] || { command -v clang++ >/dev/null 2>&1 && CXX=clang++; }
    [ -n "$CXX" ] || { command -v g++ >/dev/null 2>&1 && CXX=g++; }
    # Both runtimes' window code is in the generated file, opened or not.
    OVLANG=""
    case "$(uname -s 2>/dev/null)" in
        MINGW*|MSYS*|CYGWIN*) OVLINK="-luser32 -lgdi32" ;;
        Darwin)               OVLINK="-framework Cocoa -framework ApplicationServices"; OVLANG="-x objective-c++" ;;
        *)                    OVLINK="-lX11 -ldl" ;;
    esac
    if [ -z "$CXX" ]; then
        echo "skip overlay: no C++ compiler"
        skips=$((skips + 1))
    elif "$CXX" -std=c++17 -O1 -DNEXA_GEN="\"$WORK/ovs.cpp\"" $OVLANG "$SUITE/gfx3d_overlay_semantics.cpp" $OVLINK -o "$WORK/ov_sem" > "$WORK/ov_build.log" 2>&1; then
        ovbin="$WORK/ov_sem"
        [ -x "$ovbin" ] || ovbin="$WORK/ov_sem.exe"
        if ! "$ovbin" > "$WORK/ov.out" 2>&1; then
            echo "FAIL overlay semantics:"
            sed 's/^/  /' "$WORK/ov.out" | head -12
            fails=$((fails + 1))
        fi
    else
        echo "skip overlay: the driver would not build on this machine"
        sed 's/^/  /' "$WORK/ov_build.log" | tail -3
        skips=$((skips + 1))
    fi
else
    echo "FAIL overlay semantics: NexaC could not transpile"
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

echo "-- shader: gfx3d.shader, and what a program without one carries"
# The emitted translator, which is what lets one shader file serve both the
# browser dialect the reference describes and the 3.3 core context that
# actually compiles it. Checked in the emission: a driver lenient enough to
# accept "attribute" in a core context (NVIDIA is) would hide a missing
# translation on the machine that happens to run the suite.
printf '#include <std/gfx3d>
fn main(): int {
    gfx3d.renderer("opengl 3.3");
    gfx3d.open("t", 8, 8);
    let s: int = gfx3d.shader("a.vert", "a.frag");
    return 0;
}
' > "$WORK/xl.nxa"
if ! "$NEXAC" "$WORK/xl.nxa" --source "$WORK/xl.cpp" > "$WORK/xl.log" 2>&1; then
    echo "FAIL shader: NexaC could not transpile the translator case"
    fails=$((fails + 1))
else
    miss=""
    for frag in '#version 330 core' 'attribute' 'varying' 'texture2D' 'gl_FragColor' 'nexa_FragColor' 'precision '; do
        grep -q "$frag" "$WORK/xl.cpp" || miss="$miss [$frag]"
    done
    if [ -n "$miss" ]; then
        echo "FAIL shader: the GLSL translator is missing:$miss"
        fails=$((fails + 1))
    else
        echo "ok shader: the emission carries the browser-to-330-core translation"
    fi
fi

# Vulkan takes SPIR-V, so the emission has to carry the other half too: the
# magic-number sniff that tells a .spv from GLSL, and the calls that build the
# pipelines. Guarded for Windows, where the Vulkan renderer lives.
if ! "$NEXAC" "$WORK/xl.nxa" --source "$WORK/vk.cpp" > "$WORK/vk.log" 2>&1; then
    echo "FAIL shader: NexaC could not transpile the vulkan case"
    fails=$((fails + 1))
else
    miss=""
    for frag in '0x07230203' 'custom_up' 'custom_use' '__nexa_g3_is_spirv'; do
        grep -q "$frag" "$WORK/vk.cpp" || miss="$miss [$frag]"
    done
    if [ -n "$miss" ]; then
        echo "FAIL shader: the Vulkan half is missing:$miss"
        fails=$((fails + 1))
    else
        echo "ok shader: the emission carries the SPIR-V path for Vulkan"
    fi
fi
printf '#include <std/io>
#include <std/gfx3d>
fn main(): int {
    gfx3d.renderer("opengl 3.3");
    gfx3d.open("t", 8, 8);
    let s: int = gfx3d.shader("a.vert", "a.frag");
    gfx3d.use_shader(s);
    gfx3d.shader_set(s, "uTime", 1.0);
    io.println(gfx3d.shader_error());
    return 0;
}
' > "$WORK/sh.nxa"
printf '#include <std/gfx3d>
fn main(): int {
    gfx3d.open("t", 8, 8);
    gfx3d.poll();
    return 0;
}
' > "$WORK/nosh.nxa"

if ! "$NEXAC" "$WORK/sh.nxa" --source "$WORK/sh.cpp" > "$WORK/sh.log" 2>&1; then
    echo "FAIL shader: NexaC could not transpile a custom-shader program"
    sed 's/^/  /' "$WORK/sh.log" | tail -4
    fails=$((fails + 1))
elif ! grep -q '__nexa_gfx3d_shader' "$WORK/sh.cpp"; then
    echo "FAIL shader: no shader code in a program that asks for one"
    fails=$((fails + 1))
elif ! grep -q 'GetShaderInfoLog' "$WORK/sh.cpp"; then
    echo "FAIL shader: no info log, so a shader that will not build cannot say why"
    fails=$((fails + 1))
else
    echo "ok shader: the table, the reader and the driver's info logs are there"
fi

# Sliced: a 3D program that never asks for a custom shader must carry none of
# it -- not the table, not the file reader, and not the two extra GL symbols.
if ! "$NEXAC" "$WORK/nosh.nxa" --source "$WORK/nosh.cpp" > "$WORK/nosh.log" 2>&1; then
    echo "FAIL shader: NexaC could not transpile a plain gfx3d program"
    fails=$((fails + 1))
else
    left=""
    for sym in __nexa_gfx3d_shader NEXA_G3_SHADERS NEXA_SH_CreateShader GetShaderInfoLog GetProgramInfoLog; do
        if grep -q "$sym" "$WORK/nosh.cpp"; then left="$left $sym"; fi
    done
    if [ -n "$left" ]; then
        echo "FAIL shader: a program with no custom shader still carries:$left"
        fails=$((fails + 1))
    else
        echo "ok shader: a program without one carries none of it"
    fi
fi

# The build-time warning. The default window is legacy OpenGL, which has no
# shader to replace, so a program that never asks for a renderer is told so
# at build time. That warning is not an offer to install the Vulkan SDK.
if "$NEXAC" "$WORK/nosh2.nxa" --source "$WORK/x.cpp" > /dev/null 2>&1; then : ; fi
printf '#include <std/gfx3d>
fn main(): int {
    gfx3d.open("t", 8, 8);
    let s: int = gfx3d.shader("a.vert", "a.frag");
    gfx3d.use_shader(s);
    return 0;
}
' > "$WORK/warn.nxa"
warned=$("$NEXAC" "$WORK/warn.nxa" --source "$WORK/warn.cpp" < /dev/null 2>&1 | grep -c 'opengl 3.3')
if [ "$warned" -gt 0 ]; then
    echo "ok shader: a custom shader with no renderer asked for is warned about"
else
    echo "FAIL shader: no warning for a custom shader without the 3.3 renderer"
    fails=$((fails + 1))
fi
quiet=$("$NEXAC" "$WORK/sh.nxa" --source "$WORK/sh2.cpp" < /dev/null 2>&1 | grep -ci 'warning')
if [ "$quiet" = "0" ]; then
    echo "ok shader: and silent when the program does ask for it"
else
    echo "FAIL shader: warned even though the program asks for the 3.3 renderer"
    fails=$((fails + 1))
fi

echo "-- shader: NexaC compiling GLSL to SPIR-V as part of the build"
# Vulkan has no runtime GLSL compiler, so a program that names shader files gets
# them compiled at build time. Done with a stand-in for glslangValidator: what
# is being tested is NexaC's end -- that it looks for a compiler, runs it once
# per stage, and writes the .spv where the runtime will look -- not whether
# Khronos can compile GLSL.
# Minimal shaders that a real glslangValidator accepts -- it rejects anything
# without a #version, and on a machine with the SDK installed these are what it
# is handed rather than the stand-in.
sp_glsl() {
    if [ "$2" = "vert" ]; then
        printf '%s' '#version 450
void main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }
' > "$1"
    else
        printf '%s' '#version 450
layout(location = 0) out vec4 oCol;
void main() { oCol = vec4(1.0); }
' > "$1"
    fi
}
mkdir -p "$WORK/sp/shaders" "$WORK/sp/bin"
sp_glsl "$WORK/sp/shaders/a.vert" vert
sp_glsl "$WORK/sp/shaders/a.frag" frag
# SPIR-V's magic word, little-endian, which is what the runtime sniffs for.
printf '\003\002\043\007\000\000\001\000' > "$WORK/sp/magic.bin"
case "$(uname -s 2>/dev/null)" in
    MINGW*|MSYS*|CYGWIN*)
        MAGIC_WIN=$(cd "$WORK/sp" && pwd -W 2>/dev/null || echo "$WORK/sp")
        {
            printf '@echo off\r\n'
            printf 'set "OUT="\r\n'
            printf ':loop\r\n'
            printf 'if "%%~1"=="" goto done\r\n'
            printf 'if /i "%%~1"=="-o" set "OUT=%%~2"\r\n'
            printf 'shift\r\n'
            printf 'goto loop\r\n'
            printf ':done\r\n'
            printf 'if "%%OUT%%"=="" exit /b 1\r\n'
            printf 'copy /y "%s\magic.bin" "%%OUT%%" >nul\r\n' "$(echo "$MAGIC_WIN" | tr '/' '\134')"
        } > "$WORK/sp/bin/glslangValidator.cmd"
        ;;
    *)
        {
            echo '#!/bin/sh'
            echo 'OUT=""'
            echo 'while [ $# -gt 0 ]; do'
            echo '    if [ "$1" = "-o" ]; then OUT="$2"; fi'
            echo '    shift'
            echo 'done'
            echo '[ -n "$OUT" ] || exit 1'
            echo "cp \"$WORK/sp/magic.bin\" \"\$OUT\""
        } > "$WORK/sp/bin/glslangValidator"
        chmod +x "$WORK/sp/bin/glslangValidator"
        ;;
esac
printf '%s' '#include <std/gfx3d>
fn main() {
    gfx3d.renderer("vulkan 1.4");
    gfx3d.open("t", 320, 240);
    let s: int = gfx3d.shader("shaders/a.vert", "shaders/a.frag");
    gfx3d.use_shader(s);
    return 0;
}
' > "$WORK/sp/p.nxa"
spout=$(PATH="$WORK/sp/bin:$PATH" "$NEXAC" "$WORK/sp/p.nxa" --source "$WORK/sp/p.cpp" < /dev/null 2>&1)
if [ -f "$WORK/sp/shaders/a.vert.spv" ] && [ -f "$WORK/sp/shaders/a.frag.spv" ]; then
    echo "ok shader: the build wrote a .spv for each stage, beside its source"
else
    echo "FAIL shader: the build found a SPIR-V compiler but wrote no .spv"
    echo "$spout" | sed 's/^/    /'
    fails=$((fails + 1))
fi
if printf '%s' "$spout" | grep -q 'Compiled 1 shader pair'; then
    echo "ok shader: and said so, once for the pair it translated"
else
    echo "FAIL shader: the build compiled shaders without reporting it"
    fails=$((fails + 1))
fi
# A path in a program is written relative to the program, the way an #include
# is, so compiling has to work from a directory that is not the program's.
rm -f "$WORK/sp/shaders/a.vert.spv" "$WORK/sp/shaders/a.frag.spv"
(cd "$WORK" && PATH="$WORK/sp/bin:$PATH" "$NEXAC" "sp/p.nxa" --source "$WORK/sp/p2.cpp" < /dev/null >/dev/null 2>&1)
if [ -f "$WORK/sp/shaders/a.vert.spv" ]; then
    echo "ok shader: a shader path is resolved against the program, not the cwd"
else
    echo "FAIL shader: shader paths only resolve when the cwd is the program's"
    fails=$((fails + 1))
fi
# Without a compiler the build is not over: the OpenGL renderer compiles GLSL
# itself, so this is a warning and a note about how to do it by hand. This case
# also stands guard over something that is not an assertion: with input
# redirected there must be no dialog to click, or the suite hangs here rather
# than failing.
#
# "Without a compiler" cannot be staged on a machine that has one. NexaC looks
# where the SDK installs -- C:\VulkanSDK and the like -- and the environment
# cannot be made to forget that: SystemDrive is not a variable Windows lets a
# child override. So ask first, and say so rather than failing for the wrong
# reason. The case still carries its weight wherever no SDK is installed.
nosp=$(PATH="$WORK/sp/empty:/usr/bin:/bin" VULKAN_SDK="" "$NEXAC" "$WORK/sp/p.nxa" --source "$WORK/sp/p3.cpp" < /dev/null 2>&1)
if ! printf '%s' "$nosp" | grep -qi 'No SPIR-V compiler found'; then
    echo "skip shader: a SPIR-V compiler is installed, so 'none found' cannot be staged"
    skips=$((skips + 1))
elif printf '%s' "$nosp" | grep -q 'glslangValidator'; then
    echo "ok shader: with no compiler to be found, the build says what to install"
else
    echo "FAIL shader: a missing SPIR-V compiler is passed over in silence"
    fails=$((fails + 1))
fi
if [ -f "$WORK/sp/p3.cpp" ]; then
    echo "ok shader: and builds anyway, since one renderer needs no .spv"
else
    echo "FAIL shader: a missing SPIR-V compiler stopped the build"
    fails=$((fails + 1))
fi
# Nothing to compile, nothing to ask about -- and that covers two things: a
# program with no shader at all, and one naming files that are not on disk. The
# second is what the rest of this suite does, over and over, so getting it wrong
# does not fail a case: it stops the run on a dialog nobody can see.
printf '%s' '#include <std/gfx3d>
fn main() { gfx3d.open("t", 320, 240); gfx3d.close(); return 0; }
' > "$WORK/sp/plain.nxa"
quietsp=$(PATH="$WORK/sp/empty:/usr/bin:/bin" "$NEXAC" "$WORK/sp/plain.nxa" --source "$WORK/sp/plain.cpp" < /dev/null 2>&1 | grep -ciE 'SPIR-V|Compiled [0-9]+ shader')
if [ "$quietsp" = "0" ]; then
    echo "ok shader: a 3D program with no shader is never asked about SPIR-V"
else
    echo "FAIL shader: a program with no shader was asked about SPIR-V anyway"
    fails=$((fails + 1))
fi
# Named but absent: a path that is wrong, or a file written later by something
# else. Not a reason to go asking for a compiler.
printf '%s' '#include <std/gfx3d>
fn main() {
    gfx3d.renderer("vulkan 1.4");
    gfx3d.open("t", 320, 240);
    let s: int = gfx3d.shader("nowhere/a.vert", "nowhere/a.frag");
    gfx3d.use_shader(s);
    return 0;
}
' > "$WORK/sp/absent.nxa"
absent=$(PATH="$WORK/sp/bin:$PATH" "$NEXAC" "$WORK/sp/absent.nxa" --source "$WORK/sp/absent.cpp" < /dev/null 2>&1 | grep -ciE 'SPIR-V|Compiled [0-9]+ shader')
if [ "$absent" = "0" ]; then
    echo "ok shader: shader files that are not there are not asked about"
else
    echo "FAIL shader: a missing shader file set off the SPIR-V compiler hunt"
    fails=$((fails + 1))
fi

# The other half: handed GLSL, the Vulkan runtime looks for the .spv the build
# left beside it, so one path in the program serves both renderers.
if grep -q 'vp + ".spv"' "$WORK/sp/p.cpp"; then
    echo "ok shader: the Vulkan path falls back to the .spv beside the GLSL"
else
    echo "FAIL shader: nothing looks for the .spv the build writes"
    fails=$((fails + 1))
fi

echo "-- shader: getting a value into a Vulkan shader"
# gfx3d.shader_set has no uniform to look up on Vulkan, so the renderer pushes
# sixteen floats after the transform and a name takes the next free one. The
# push constant range has to grow to hold them, and name both stages -- a push
# must name every stage of every range it touches -- and all of that belongs to
# programs that load a shader and to no others.
printf '%s' '#include <std/gfx3d>
fn main() {
    gfx3d.renderer("vulkan 1.4");
    gfx3d.open("t", 320, 240);
    let s: int = gfx3d.shader("a.vert", "a.frag");
    gfx3d.use_shader(s);
    gfx3d.shader_set(s, "uTime", 1.0);
    return 0;
}
' > "$WORK/pcu.nxa"
if ! "$NEXAC" "$WORK/pcu.nxa" --source "$WORK/pcu.cpp" < /dev/null > /dev/null 2>&1; then
    echo "FAIL shader: NexaC could not transpile a program that writes a value"
    fails=$((fails + 1))
else
    miss=""
    grep -q 'float userPC\[16\]' "$WORK/pcu.cpp" || miss="$miss userPC"
    grep -q 'pc{ 1 | 16, 0, 128 }' "$WORK/pcu.cpp" || miss="$miss push-range"
    grep -q '64, 64, d.userPC' "$WORK/pcu.cpp" || miss="$miss the-push"
    grep -q 'nexa_vk14::custom_set' "$WORK/pcu.cpp" || miss="$miss shader_set"
    grep -q '__nexa_g3_vk_slots' "$WORK/pcu.cpp" || miss="$miss slot-table"
    if [ -n "$miss" ]; then
        echo "FAIL shader: a value cannot reach a Vulkan shader, missing:$miss"
        fails=$((fails + 1))
    else
        echo "ok shader: a Vulkan shader is pushed the sixteen floats shader_set writes"
    fi
fi
# And a 3D program that loads no shader keeps the layout it had: 64 bytes, the
# vertex stage, and no block to push.
if ! grep -q 'pc{ 1, 0, 64 }' "$WORK/sp/plain.cpp"; then
    echo "FAIL shader: a plain 3D program's push constant range was widened for it"
    fails=$((fails + 1))
else
    left=""
    for sym in userPC __nexa_g3_vk_slots custom_set; do
        grep -q "$sym" "$WORK/sp/plain.cpp" && left="$left $sym"
    done
    if [ -n "$left" ]; then
        echo "FAIL shader: a program with no shader still carries:$left"
        fails=$((fails + 1))
    else
        echo "ok shader: and a program without one keeps the 64-byte vertex-only range"
    fi
fi

echo "-- shader: which renderer the program actually asked for"
# Reported from another machine: a program that asks for OpenGL 3.3 was being
# offered a Vulkan shader compiler. That driver compiles GLSL itself, at run
# time, so there is nothing to compile and nothing to install -- and the files
# being right there on disk is what made it ask.
mkdir -p "$WORK/sp/shaders"
sp_glsl "$WORK/sp/shaders/r.vert" vert
sp_glsl "$WORK/sp/shaders/r.frag" frag
sp_prog() {
    printf '%s' '#include <std/gfx3d>
fn main() {
' > "$2"
    printf '%s
' "$1" >> "$2"
    printf '%s' '    gfx3d.open("t", 320, 240);
    let s: int = gfx3d.shader("shaders/r.vert", "shaders/r.frag");
    gfx3d.use_shader(s);
    return 0;
}
' >> "$2"
}
# The question is "was this treated as possibly-Vulkan", which is not the same
# as "did it mention SPIR-V". Where a compiler is installed such a program is
# simply compiled, quietly; where none is, it is asked about. Either answer
# counts, and a program that can only be OpenGL gives neither.
sp_asks() {
    sp_prog "$1" "$WORK/sp/q.nxa"
    PATH="$WORK/sp/empty:/usr/bin:/bin" VULKAN_SDK=""         "$NEXAC" "$WORK/sp/q.nxa" --source "$WORK/sp/q.cpp" < /dev/null 2>&1 | grep -ciE 'SPIR-V|Compiled [0-9]+ shader'
}
if [ "$(sp_asks '    gfx3d.renderer("opengl 3.3");')" = "0" ]; then
    echo "ok shader: an OpenGL 3.3 program is not offered a SPIR-V compiler"
else
    echo "FAIL shader: an OpenGL program was asked to install a Vulkan compiler"
    fails=$((fails + 1))
fi
if [ "$(sp_asks '    gfx3d.renderer("OpenGL 3.3");')" = "0" ]; then
    echo "ok shader: and the name is read without regard to case"
else
    echo "FAIL shader: OpenGL spelled with capitals was taken for Vulkan"
    fails=$((fails + 1))
fi
if [ "$(sp_asks '    gfx3d.renderer("opengl");')" = "0" ]; then
    echo "ok shader: the legacy profile is left alone too"
else
    echo "FAIL shader: a legacy-OpenGL program was asked about SPIR-V"
    fails=$((fails + 1))
fi
if [ "$(sp_asks '    gfx3d.renderer("vulkan 1.4");')" != "0" ]; then
    echo "ok shader: a Vulkan program is still asked"
else
    echo "FAIL shader: a Vulkan program was not offered a compiler it needs"
    fails=$((fails + 1))
fi
# Named both, so it may well end up on Vulkan: asked. Taken one call at a time
# rather than whichever came last.
both='    gfx3d.renderer("vulkan 1.4");
    gfx3d.renderer("opengl 3.3");'
if [ "$(sp_asks "$both")" != "0" ]; then
    echo "ok shader: a program naming both renderers is asked"
else
    echo "FAIL shader: naming OpenGL last hid an earlier Vulkan"
    fails=$((fails + 1))
fi
# A name built at run time is not a reason to install the SDK. The default
# window is OpenGL, and the program can still name Vulkan as a literal.
if [ "$(sp_asks '    let r: string = "vul" + "kan 1.4";
    gfx3d.renderer(r);')" = "0" ]; then
    echo "ok shader: a renderer named at run time is not offered the SDK"
else
    echo "FAIL shader: a runtime renderer name was treated as Vulkan"
    fails=$((fails + 1))
fi

echo "-- shader: finding a SPIR-V compiler that is not on PATH"
# Also reported: the Vulkan SDK installs glslangValidator and does not put it on
# PATH for a shell that is already open, so every build offered the install
# again. NexaC looks where the SDK puts things rather than only asking PATH.
case "$(uname -s 2>/dev/null)" in
    MINGW*|MSYS*|CYGWIN*)
        if [ -z "$CXX" ]; then
            echo "skip shader: no C++ compiler to build a stand-in glslangValidator"
            skips=$((skips + 1))
        else
            mkdir -p "$WORK/sp/sdk/Bin"
            printf '%s' '#include <cstdio>
#include <cstring>
int main(int argc, char** argv) {
    const char* out = 0;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
    }
    if (!out) return 1;
    FILE* o = std::fopen(out, "wb");
    if (!o) return 1;
    const unsigned char spv[8] = { 0x03, 0x02, 0x23, 0x07, 0x00, 0x00, 0x01, 0x00 };
    std::fwrite(spv, 1, sizeof spv, o);
    std::fclose(o);
    return 0;
}
' > "$WORK/sp/stub.cpp"
            if ! $CXX -std=c++17 -O0 -D_CRT_SECURE_NO_WARNINGS                     -o "$WORK/sp/sdk/Bin/glslangValidator.exe" "$WORK/sp/stub.cpp" > /dev/null 2>&1; then
                echo "skip shader: the stand-in glslangValidator would not build"
                skips=$((skips + 1))
            else
                rm -f "$WORK/sp/shaders/r.vert.spv" "$WORK/sp/shaders/r.frag.spv"
                sp_prog '    gfx3d.renderer("vulkan 1.4");' "$WORK/sp/sdk.nxa"
                SDKW=$(cd "$WORK/sp/sdk" && pwd -W 2>/dev/null || echo "$WORK/sp/sdk")
                sdkout=$(PATH="$WORK/sp/empty:/usr/bin:/bin" VULKAN_SDK="$SDKW"                     "$NEXAC" "$WORK/sp/sdk.nxa" --source "$WORK/sp/sdk.cpp" < /dev/null 2>&1)
                if [ -f "$WORK/sp/shaders/r.vert.spv" ]; then
                    echo "ok shader: a compiler in the SDK is used though PATH has none"
                else
                    echo "FAIL shader: a compiler off PATH was not found, so nothing compiled"
                    echo "$sdkout" | sed 's/^/    /'
                    fails=$((fails + 1))
                fi
                if ! printf '%s' "$sdkout" | grep -qi 'No SPIR-V compiler found'; then
                    echo "ok shader: and the install is not offered when one is already there"
                else
                    echo "FAIL shader: offered to install a compiler that is already installed"
                    fails=$((fails + 1))
                fi
            fi
        fi
        ;;
    *)
        echo "skip shader: the SDK search is Windows-only"
        skips=$((skips + 1))
        ;;
esac
# And the error a program sees should name what went wrong, not send somebody who
# just chose Vulkan back to the renderer they chose it over.
if grep -q 'no compiled shader for Vulkan' "$WORK/sp/q.cpp" &&
   ! grep -q 'GLSL as written is compiled by the driver' "$WORK/sp/q.cpp"; then
    echo "ok shader: the Vulkan shader error names the missing .spv"
else
    echo "FAIL shader: the Vulkan error still answers with the other renderer"
    fails=$((fails + 1))
fi

# A build that nobody is watching must never stop on a dialog. The gate is the
# console, not the device type: on Windows _isatty says yes to NUL, so a build
# run with < /dev/null -- which is how every case here says there is nobody
# present -- used to put up a modal and wait forever. There is no assertion that
# can catch a hang, so this case is a stopwatch: a transpile that cannot find a
# compiler should take about as long as one that is not looking for it.
case "$(uname -s 2>/dev/null)" in
    MINGW*|MSYS*|CYGWIN*)
        sp_prog '    gfx3d.renderer("vulkan 1.4");' "$WORK/sp/hang.nxa"
        hstart=$(date +%s)
        PATH="$WORK/sp/empty:/usr/bin:/bin" VULKAN_SDK=""             "$NEXAC" "$WORK/sp/hang.nxa" --source "$WORK/sp/hang.cpp" < /dev/null > /dev/null 2>&1
        hend=$(date +%s)
        if [ -f "$WORK/sp/hang.cpp" ] && [ $((hend - hstart)) -lt 30 ]; then
            echo "ok shader: a build with no console carries on instead of asking"
        else
            echo "FAIL shader: the build stalled or wrote nothing -- a modal with no one to click it"
            fails=$((fails + 1))
        fi
        ;;
    *)
        echo "skip shader: the console gate is Windows-only"
        skips=$((skips + 1))
        ;;
esac

echo "-- shader: no SPIR-V where there is no Vulkan"
# nexa_vk14 is behind _WIN32 from top to bottom, so a browser build and a
# cross-build cannot reach Vulkan. Asking those for a SPIR-V compiler is the
# same mistake as asking an OpenGL program for one.
sp_prog '' "$WORK/sp/nor.nxa"
for flag in "--wasm" "--target arm64-linux"; do
    asked=$(PATH="$WORK/sp/empty:/usr/bin:/bin" VULKAN_SDK=""         "$NEXAC" "$WORK/sp/nor.nxa" $flag --source "$WORK/sp/nor.cpp" < /dev/null 2>&1 | grep -ciE 'SPIR-V|Compiled [0-9]+ shader')
    if [ "$asked" = "0" ]; then
        echo "ok shader: a $flag build is not asked about SPIR-V"
    else
        echo "FAIL shader: a $flag build was asked for a compiler it cannot use"
        fails=$((fails + 1))
    fi
done
# The same program on Windows names no renderer, so the default is OpenGL and
# the SDK is not offered. A program that names Vulkan is covered above.
nasked=$(PATH="$WORK/sp/empty:/usr/bin:/bin" VULKAN_SDK=""     "$NEXAC" "$WORK/sp/nor.nxa" --source "$WORK/sp/nor2.cpp" < /dev/null 2>&1 | grep -ciE 'SPIR-V|Compiled [0-9]+ shader')
if [ "$nasked" = "0" ]; then
    echo "ok shader: a program that names no renderer is not offered a SPIR-V compiler"
else
    echo "FAIL shader: no renderer was treated as Vulkan"
    fails=$((fails + 1))
fi

echo "-- shader: a stage at a time, and either compiler front end"
case "$(uname -s 2>/dev/null)" in
    MINGW*|MSYS*|CYGWIN*)
        if [ -z "$CXX" ] || [ ! -x "$WORK/sp/sdk/Bin/glslangValidator.exe" ]; then
            echo "skip shader: no stand-in compiler was built"
            skips=$((skips + 1))
        else
            # glslc takes -fshader-stage= where glslangValidator takes -S; the
            # stand-in accepts either name, so what is checked is that NexaC
            # picks the flags by the tool it found.
            mkdir -p "$WORK/sp/sdkc/Bin"
            cp "$WORK/sp/sdk/Bin/glslangValidator.exe" "$WORK/sp/sdkc/Bin/glslc.exe"
            rm -f "$WORK/sp/shaders/r.vert.spv" "$WORK/sp/shaders/r.frag.spv"
            SDKC=$(cd "$WORK/sp/sdkc" && pwd -W 2>/dev/null || echo "$WORK/sp/sdkc")
            sp_prog '    gfx3d.renderer("vulkan 1.4");' "$WORK/sp/gc.nxa"
            PATH="$WORK/sp/empty:/usr/bin:/bin" VULKAN_SDK="$SDKC"                 "$NEXAC" "$WORK/sp/gc.nxa" --source "$WORK/sp/gc.cpp" < /dev/null > /dev/null 2>&1
            if [ -f "$WORK/sp/shaders/r.vert.spv" ] && [ -f "$WORK/sp/shaders/r.frag.spv" ]; then
                echo "ok shader: glslc is driven with the flags glslc takes"
            else
                echo "FAIL shader: glslc was called the way glslangValidator is called"
                fails=$((fails + 1))
            fi
            # One half already compiled, the other not: the other still needs
            # doing, and a pair is not an all-or-nothing unit.
            cp "$WORK/sp/shaders/r.vert.spv" "$WORK/sp/shaders/m.vert.spv"
            sp_glsl "$WORK/sp/shaders/m.frag" frag
            rm -f "$WORK/sp/shaders/m.frag.spv"
            printf '%s' '#include <std/gfx3d>
fn main() {
    gfx3d.renderer("vulkan 1.4");
    gfx3d.open("t", 320, 240);
    let s: int = gfx3d.shader("shaders/m.vert.spv", "shaders/m.frag");
    gfx3d.use_shader(s);
    return 0;
}
' > "$WORK/sp/mix.nxa"
            SDKW2=$(cd "$WORK/sp/sdk" && pwd -W 2>/dev/null || echo "$WORK/sp/sdk")
            PATH="$WORK/sp/empty:/usr/bin:/bin" VULKAN_SDK="$SDKW2"                 "$NEXAC" "$WORK/sp/mix.nxa" --source "$WORK/sp/mix.cpp" < /dev/null > /dev/null 2>&1
            if [ -f "$WORK/sp/shaders/m.frag.spv" ]; then
                echo "ok shader: a compiled vertex beside a written-out fragment is finished"
            else
                echo "FAIL shader: a pair with one .spv in it had neither half compiled"
                fails=$((fails + 1))
            fi
        fi
        ;;
    *)
        echo "skip shader: the SDK search is Windows-only"
        skips=$((skips + 1))
        ;;
esac

echo "-- shader: a Vulkan shader reaches textured geometry too"
# gfx3d draws textured geometry through a pipeline of its own -- draw_tex and
# g.pipeTex, not the d.pipeTri the untextured shapes use. A custom shader built
# into only one of them is skipped for anything carrying a texture while shading
# everything else, which is a rule nobody could guess and which the OpenGL
# renderer does not have. Found by drawing one textured cube beside one plain
# one: the plain one took the shader's colour and the textured one did not.
printf '%s' '#include <std/gfx3d>
fn main() {
    gfx3d.renderer("vulkan 1.4");
    gfx3d.open("t", 320, 240);
    let s: int = gfx3d.shader("shaders/a.vert", "shaders/a.frag");
    let t: int = gfx3d.texture("t.png");
    gfx3d.use_shader(s);
    gfx3d.use(t);
    gfx3d.cube(0.0, 0.0, 0.0, 1.0, 200, 80, 80);
    return 0;
}
' > "$WORK/sp/tex.nxa"
if ! "$NEXAC" "$WORK/sp/tex.nxa" --source "$WORK/sp/tex.cpp" < /dev/null > /dev/null 2>&1; then
    echo "FAIL shader: NexaC could not transpile a textured custom-shader program"
    fails=$((fails + 1))
else
    miss=""
    grep -q 'pipeCustomTex' "$WORK/sp/tex.cpp" || miss="$miss pipelines"
    grep -q 'custom_tex_pipes' "$WORK/sp/tex.cpp" || miss="$miss builder"
    # The textured layout has to admit the shader's push block, which is 128
    # bytes across both stages rather than the built-in 64 and 4.
    grep -q 'layout_of(d, oneB, 1, 1 | 16, 0, 128' "$WORK/sp/tex.cpp" || miss="$miss wide-push-range"
    grep -q 'useCustom ? g.pipeCustomTex' "$WORK/sp/tex.cpp" || miss="$miss the-bind"
    if [ -n "$miss" ]; then
        echo "FAIL shader: textured geometry would skip a custom shader, missing:$miss"
        fails=$((fails + 1))
    else
        echo "ok shader: the textured pipelines are built from the custom shader too"
    fi
fi
# Sliced: a 3D program with textures and no custom shader keeps the narrow
# layout it had, and carries none of this.
if ! grep -q 'layout_of(d, oneB, 1, 1, 0, 64, 1, 0x10, 64, 4' "$WORK/sp/plain.cpp"; then
    echo "FAIL shader: a program with no shader lost the narrow textured layout"
    fails=$((fails + 1))
else
    left=""
    for sym in pipeCustomTex custom_tex_pipes useCustom; do
        grep -q "$sym" "$WORK/sp/plain.cpp" && left="$left $sym"
    done
    if [ -n "$left" ]; then
        echo "FAIL shader: a program with no shader still carries:$left"
        fails=$((fails + 1))
    else
        echo "ok shader: and a program without one keeps the textured path it had"
    fi
fi

echo "-- shader: one dialect, translated for Vulkan at build time"
# A shader written the way the reference describes is what the OpenGL driver
# compiles. Vulkan wants the same program spelled differently, so NexaC rewrites
# the declarations and compiles the result -- which is what lets one pair of
# files serve both renderers instead of two pairs that must be kept in step.
#
# Two modules come out per stage, not one: the untextured pipelines have no
# descriptor set and no uv to offer, so a module declaring either is invalid on
# them. The .tex.spv twin is the one the textured pipelines get.
case "$(uname -s 2>/dev/null)" in
    MINGW*|MSYS*|CYGWIN*)
        if [ -z "$CXX" ] || [ ! -x "$WORK/sp/sdk/Bin/glslangValidator.exe" ]; then
            echo "skip shader: no stand-in compiler was built"
            skips=$((skips + 1))
        else
            mkdir -p "$WORK/sp/xl"
            printf '%s' 'attribute vec3 aPos;
attribute vec3 aCol;
attribute vec2 aUV;
uniform mat4 uMVP;
uniform float uTime;
varying vec3 vCol;
varying vec2 vUV;
void main() {
    vCol = aCol;
    vUV = aUV;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
' > "$WORK/sp/xl/d.vert"
            printf '%s' 'precision mediump float;
uniform sampler2D uTex;
uniform float uTexOn;
uniform float uTime;
varying vec3 vCol;
varying vec2 vUV;
void main() {
    vec3 c = vCol;
    if (uTexOn > 0.5) { c *= texture2D(uTex, vUV).rgb; }
    gl_FragColor = vec4(c * uTime, 1.0);
}
' > "$WORK/sp/xl/d.frag"
            printf '%s' '#include <std/gfx3d>
fn main() {
    gfx3d.renderer("vulkan 1.4");
    gfx3d.open("t", 320, 240);
    let s: int = gfx3d.shader("xl/d.vert", "xl/d.frag");
    gfx3d.use_shader(s);
    gfx3d.shader_set(s, "uTime", 1.0);
    return 0;
}
' > "$WORK/sp/xl.nxa"
            SDKX=$(cd "$WORK/sp/sdk" && pwd -W 2>/dev/null || echo "$WORK/sp/sdk")
            xlout=$(PATH="$WORK/sp/empty:/usr/bin:/bin" VULKAN_SDK="$SDKX" \
                "$NEXAC" "$WORK/sp/xl.nxa" --source "$WORK/sp/xl.cpp" < /dev/null 2>&1)
            got=""
            for f in d.vert.spv d.frag.spv d.vert.tex.spv d.frag.tex.spv; do
                [ -f "$WORK/sp/xl/$f" ] || got="$got $f"
            done
            if [ -z "$got" ]; then
                echo "ok shader: a browser-dialect pair becomes four Vulkan modules"
            else
                echo "FAIL shader: the translation produced no:$got"
                printf '%s' "$xlout" | sed 's/^/    /' | head -8
                fails=$((fails + 1))
            fi
            # The stand-in writes the same bytes whatever it is handed, so what
            # is checked here is NexaC's rewriting, by reading the GLSL it would
            # have compiled. A real compiler proves the rest.
            if printf '%s' "$xlout" | grep -qi 'would not compile'; then
                echo "FAIL shader: the translated GLSL was rejected"
                printf '%s' "$xlout" | sed 's/^/    /' | head -10
                fails=$((fails + 1))
            else
                echo "ok shader: and the compiler took every one of them"
            fi
        fi
        ;;
    *)
        echo "skip shader: the build-time translation needs the Windows SDK search"
        skips=$((skips + 1))
        ;;
esac
# A shader asking for something gfx3d does not send is told so by name, rather
# than failing later inside the compiler.
printf '%s' 'attribute vec3 aWhat;
void main() { gl_Position = vec4(aWhat, 1.0); }
' > "$WORK/sp/shaders/bad.vert"
printf '%s' 'precision mediump float;
varying vec3 vCol;
void main() { gl_FragColor = vec4(vCol, 1.0); }
' > "$WORK/sp/shaders/bad.frag"
printf '%s' '#include <std/gfx3d>
fn main() {
    gfx3d.renderer("vulkan 1.4");
    gfx3d.open("t", 320, 240);
    let s: int = gfx3d.shader("shaders/bad.vert", "shaders/bad.frag");
    gfx3d.use_shader(s);
    return 0;
}
' > "$WORK/sp/bad.nxa"
badout=$(PATH="$WORK/sp/bin:$PATH" "$NEXAC" "$WORK/sp/bad.nxa" --source "$WORK/sp/bad.cpp" < /dev/null 2>&1)
if printf '%s' "$badout" | grep -q 'aWhat'; then
    echo "ok shader: an attribute gfx3d does not send is named in the complaint"
else
    echo "FAIL shader: an unknown attribute was passed to the compiler unexplained"
    fails=$((fails + 1))
fi
# The runtime half: the textured twin is looked for beside the plain one.
if grep -q '__nexa_g3_tex_twin' "$WORK/pcu.cpp"; then
    echo "ok shader: the runtime looks for the .tex.spv twin"
else
    echo "FAIL shader: nothing loads the textured module, so sampling cannot work"
    fails=$((fails + 1))
fi

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
