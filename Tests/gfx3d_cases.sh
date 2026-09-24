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
