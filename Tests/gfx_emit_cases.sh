#!/bin/sh
# What a gfx program has to carry: the image decoder (BOB-24) and the gfx
# runtime itself (BOB-25).
#
# The std/gfx runtime is ~2,800 lines and stb_image another ~8,000, and a
# program that opens a window and draws reaches a small corner of either. Both
# are therefore sliced: the transpiler records which gfx builtins a program
# calls, and only the feature groups those can reach are emitted. This suite is
# the cover for the slicing, not for what the features do -- the other
# Tests/gfx_*_cases.sh suites test the behaviour of the code that survives.
#
# Three layers, cheapest first, matching the other Tests/gfx_*_cases.sh suites:
#
#   decoder  Which programs carry stb_image. Transpile only (--source), for
#            both targets that append it (native Linux and --wasm). Never
#            invokes the C++ compiler, so it runs anywhere NexaC does.
#
#   groups   Which feature groups of the gfx runtime a program carries: each
#            group is present when a call reaches it and absent from a draw
#            loop that cannot. Transpile only, both targets.
#
#   link     The slicing leaves a program that still compiles and links. Every
#            gfx builtin gets a one-call program of its own, built against the
#            fake X11 in Tests/gfx_x11_stub and run -- which is what proves the
#            dependency closure: a lone gfx.fill_circle has to drag in the
#            ellipse maths and the span writer, a lone gfx.save the little-
#            endian writers and gfx.get. No display needed; none of these
#            programs opens a window. Skipped when this machine has no C++
#            compiler.
#
# Usage: Tests/gfx_emit_cases.sh [path-to-NexaC]        (run from the repo root)

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")

SUITE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$SUITE/.." && pwd)

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM

fails=0
skips=0

# A draw loop that never touches an image: open, poll, clear, plot, present.
# This is the program the founder reported as slow to compile.
DRAW_ONLY='    gfx.open("t", 8, 8, 1);
    gfx.poll();
    gfx.clear(0, 0, 0);
    gfx.plot(1, 1, 255, 255, 255);
    gfx.present();
    gfx.close();'

# transpile <out-name> <body> [extra NexaC flags...]
# Writes a one-function gfx program and transpiles it. Returns non-zero and
# reports the failure itself if NexaC refuses.
transpile() {
    name=$1
    body=$2
    shift 2
    printf '#include <std/gfx>\nfn main() {\n%s\n}\n' "$body" > "$WORK/$name.nxa"
    if ! "$NEXAC" "$WORK/$name.nxa" "$@" --source "$WORK/$name.cpp" \
            > "$WORK/$name.log" 2>&1; then
        echo "FAIL $name: NexaC could not transpile"
        sed 's/^/  /' "$WORK/$name.log"
        fails=$((fails + 1))
        return 1
    fi
    return 0
}

# --- decoder: which gfx programs carry stb_image ----------------------------

# stb_image's internals are all named stbi__*, so counting those lines
# distinguishes "the blob is here" from "it is not" without depending on any
# particular line of it.
has_stb() {
    grep -q 'stbi__' "$1"
}

# expect_no_decoder <label> <body> [flags...]
expect_no_decoder() {
    label=$1
    shift
    transpile "$label" "$@" || return
    if has_stb "$WORK/$label.cpp"; then
        echo "FAIL $label: emitted stb_image for a program that cannot decode"
        fails=$((fails + 1))
        return
    fi
    # And nothing is left referring to it. __nexa_gfx_decode_rgba is the only
    # caller of the two stb entry points and it is sliced out by the same flag,
    # so there is no reference left to satisfy and no stub is emitted for one.
    if grep -q '__nexa_gfx_stbi' "$WORK/$label.cpp"; then
        echo "FAIL $label: dropped stb_image but left a reference to it behind"
        grep -n '__nexa_gfx_stbi' "$WORK/$label.cpp" | head -n 3 | sed 's/^/  /'
        fails=$((fails + 1))
        return
    fi
    echo "ok $label"
}

# expect_decoder <label> <body> [flags...]
expect_decoder() {
    label=$1
    shift
    transpile "$label" "$@" || return
    if ! has_stb "$WORK/$label.cpp"; then
        echo "FAIL $label: no stb_image, so this program cannot decode at runtime"
        fails=$((fails + 1))
        return
    fi
    if ! grep -q '__nexa_gfx_decode_rgba' "$WORK/$label.cpp"; then
        echo "FAIL $label: the decoder is there but nothing dispatches to it"
        fails=$((fails + 1))
        return
    fi
    echo "ok $label"
}

echo "-- decoder: which gfx programs carry the image decoder"

expect_no_decoder "draw_only_has_no_decoder" "$DRAW_ONLY"

# gfx.save writes a BMP by hand rather than going through stb, and the text
# calls read a built-in font table, so neither drags the decoder in.
expect_no_decoder "save_and_text_have_no_decoder" \
    '    gfx.text(0, 0, "a", 1, 2, 3);
    let w: int = gfx.text_width("a");
    gfx.save("out.bmp");'

# Reading the size of an image needs the table images are stored in, but not
# the decoder that fills it: there is no way to have put one there.
expect_no_decoder "image_size_alone_has_no_decoder" \
    '    let w: int = gfx.image_w(1);
    let h: int = gfx.image_h(1);'

# The three calls that can reach __nexa_gfx_decode_rgba.
expect_decoder "image_has_the_decoder" \
    '    let i: int = gfx.image("a.png");'
expect_decoder "decode_has_the_decoder" \
    '    let i: int = gfx.decode("....");'
# gfx.blit's path overload calls __nexa_gfx_image itself, and which overload a
# gfx.blit takes is only known once the third argument's type is inferred --
# long after the usage scan runs. So every gfx.blit has to count, and this case
# is the one that would silently return a null image if it ever stopped.
expect_decoder "path_blit_has_the_decoder" \
    '    gfx.blit(0, 0, "a.png");'
expect_decoder "handle_blit_has_the_decoder" \
    '    let i: int = gfx.image("a.png");
    gfx.blit(0, 0, i);'
# gfx.blit_rot and gfx.icon take a path exactly the way gfx.blit does, so they
# have to count as a load for exactly the same reason.
expect_decoder "path_blit_rot_has_the_decoder" \
    '    gfx.blit_rot(0, 0, "a.png", 45);'
expect_decoder "path_icon_has_the_decoder" \
    '    let ok: int = gfx.icon("a.png");'

# The window polish that touches no image at all still must not drag the
# decoder in behind it.
expect_no_decoder "cursor_alone_has_no_decoder" \
    '    let c: int = gfx.cursor(0);'
expect_no_decoder "arcs_and_round_rects_have_no_decoder" \
    '    gfx.arc(1, 1, 3, 0, 90, 1, 2, 3);
    gfx.pie(1, 1, 3, 0, 90, 1, 2, 3);
    gfx.round_rect(0, 0, 4, 4, 1, 1, 2, 3);
    gfx.fill_round_rect(0, 0, 4, 4, 1, 1, 2, 3);'

# Both targets that append the blob have to slice it the same way.
expect_no_decoder "wasm_draw_only_has_no_decoder" "$DRAW_ONLY" --wasm
expect_decoder "wasm_image_has_the_decoder" \
    '    let i: int = gfx.image("a.png");' --wasm

# --- groups: which of the gfx runtime a program carries ---------------------

echo "-- groups: which feature groups of the gfx runtime a program carries"

# One sentinel per group: a definition only that group emits. Matching the
# definition rather than a call means the program's own main cannot satisfy it.
#
# group <name> <sentinel-ERE> <body> [flags...]
# Asserts the sentinel is in the emission for `body` and is not in the draw
# loop's, which reaches nothing but the core.
group() {
    name=$1
    want=$2
    body=$3
    shift 3
    label="group_$name"
    transpile "$label" "$body" "$@" || return
    if ! grep -Eq "$want" "$WORK/$label.cpp"; then
        echo "FAIL $label: a program that uses it did not carry /$want/"
        fails=$((fails + 1))
        return
    fi
    if grep -Eq "$want" "$WORK/$DRAW_LABEL.cpp"; then
        echo "FAIL $label: a draw loop carried /$want/, which it cannot reach"
        fails=$((fails + 1))
        return
    fi
    echo "ok $label"
}

# run_groups <suffix> [flags...] -- the whole table against one target.
run_groups() {
    suffix=$1
    shift
    DRAW_LABEL="draw_only$suffix"
    transpile "$DRAW_LABEL" "$DRAW_ONLY" "$@" || return

    group "alpha$suffix" '^static int __nexa_gfx_alpha_get' \
        '    let a: int = gfx.alpha();' "$@"
    group "get$suffix" '^static int __nexa_gfx_get\(' \
        '    let p: int = gfx.get(1, 2);' "$@"
    group "shapes_fill$suffix" '^static void __nexa_gfx_fill_poly_pts' \
        '    gfx.fill_tri(0, 0, 1, 0, 0, 1, 1, 2, 3);' "$@"
    group "shapes_outline$suffix" '^static void __nexa_gfx_poly_pts' \
        '    gfx.tri(0, 0, 1, 0, 0, 1, 1, 2, 3);' "$@"
    group "arc$suffix" '^static void __nexa_gfx_arc' \
        '    gfx.arc(1, 1, 3, 0, 90, 1, 2, 3);' "$@"
    group "pie$suffix" '^static void __nexa_gfx_pie' \
        '    gfx.pie(1, 1, 3, 0, 90, 1, 2, 3);' "$@"
    group "round_rect$suffix" '^static void __nexa_gfx_round_rect' \
        '    gfx.round_rect(0, 0, 4, 4, 1, 1, 2, 3);' "$@"
    group "fill_round_rect$suffix" '^static void __nexa_gfx_fill_round_rect' \
        '    gfx.fill_round_rect(0, 0, 4, 4, 1, 1, 2, 3);' "$@"
    group "line$suffix" '^static void __nexa_gfx_line\(' \
        '    gfx.line(0, 0, 1, 1, 1, 2, 3);' "$@"
    group "line_thick$suffix" '^static void __nexa_gfx_line_thick' \
        '    gfx.line(0, 0, 1, 1, 1, 2, 3, 4);' "$@"
    group "text$suffix" '^static const unsigned char __nexa_gfx_font5x7' \
        '    gfx.text(0, 0, "a", 1, 2, 3);' "$@"
    group "mouse$suffix" '^static int __nexa_gfx_mouse_x' \
        '    let n: int = gfx.mouse_x();' "$@"
    group "keys$suffix" '^static int __nexa_gfx_vk' \
        '    let k: int = gfx.pressed("space");' "$@"
    group "typed$suffix" '^static std::string __nexa_gfx_typed' \
        '    let s: string = gfx.typed();' "$@"
    group "wheel$suffix" '^static int __nexa_gfx_wheel\(\)' \
        '    let n: int = gfx.wheel();' "$@"
    group "image_store$suffix" '^struct __nexa_GfxImg' \
        '    let n: int = gfx.image_w(1);' "$@"
    group "blit$suffix" '^static int __nexa_gfx_blit\(' \
        '    gfx.blit(0, 0, 1);' "$@"
    group "blit_rot$suffix" '^static int __nexa_gfx_blit_rot\(' \
        '    gfx.blit_rot(0, 0, 1, 45);' "$@"
    group "icon$suffix" '^static int __nexa_gfx_icon\(' \
        '    let ok: int = gfx.icon(1);' "$@"
    group "cursor$suffix" '^static int __nexa_gfx_cursor\(' \
        '    let c: int = gfx.cursor();' "$@"
    group "save$suffix" '^static int __nexa_gfx_save' \
        '    let ok: int = gfx.save("o.bmp");' "$@"
    group "dialogs$suffix" '^static std::string __nexa_gfx_opendialog' \
        '    let p: string = gfx.opendialog();' "$@"
    group "audio$suffix" '^static int __nexa_gfx_sample' \
        '    let n: int = gfx.sample(1);' "$@"
    group "sound$suffix" '^static int __nexa_gfx_wav_decode' \
        '    let n: int = gfx.sound("b.wav");' "$@"
    group "window$suffix" '^static int __nexa_gfx_resize' \
        '    gfx.resize(4, 4);' "$@"
}

run_groups ""
# The wasm target slices the same runtime, so the whole table runs twice.
run_groups "_wasm" --wasm

# gfx.typed() is the only reader of the typed-text queue, but the backends feed
# it from inside the window procedure and the X11 event loop, which are core.
# Slicing the queue therefore has to leave those callees behind as no-ops.
if ! grep -q 'static void __nexa_gfx_type_push_latin1(const char\*, int) {}' \
        "$WORK/draw_only.cpp"; then
    echo "FAIL typed_stub: a draw loop lost the no-op typed-text helpers"
    fails=$((fails + 1))
else
    echo "ok typed_stub"
fi

# gfx.close() calls the audio shutdown itself, so slicing audio out has to
# leave that behind too.
if ! grep -q 'static void __nexa_gfx_audio_close() {}' "$WORK/draw_only.cpp"; then
    echo "FAIL audio_close_stub: a draw loop lost the no-op audio shutdown"
    fails=$((fails + 1))
else
    echo "ok audio_close_stub"
fi

# The same again for the mixer (BOB-39): gfx.poll() tops it up and gfx.close()
# stops its voices, and both of those are core.
if ! grep -q 'static void __nexa_gfx_mix_pump() {}' "$WORK/draw_only.cpp" ||
   ! grep -q 'static void __nexa_gfx_sound_reset() {}' "$WORK/draw_only.cpp"; then
    echo "FAIL sound_stub: a draw loop lost the no-op mixer hooks"
    fails=$((fails + 1))
else
    echo "ok sound_stub"
fi

# The mixer's output goes out through gfx.sample's enqueue path, so gfx.play
# has to bring the platform audio block with it even with no gfx.audio in
# sight -- and the traffic is one way: gfx.sample alone must not carry a mixer.
transpile "sound_only" '    let v: int = gfx.play(1);' || true
if [ -f "$WORK/sound_only.cpp" ]; then
    if ! grep -q '^static int __nexa_gfx_sample' "$WORK/sound_only.cpp"; then
        echo "FAIL sound_pulls_audio: gfx.play did not carry the audio block it mixes into"
        fails=$((fails + 1))
    else
        echo "ok sound_pulls_audio"
    fi
fi
if grep -q '^static int __nexa_gfx_wav_decode' "$WORK/group_audio.cpp"; then
    echo "FAIL audio_carries_no_mixer: gfx.sample dragged the mixer in with it"
    fails=$((fails + 1))
else
    echo "ok audio_carries_no_mixer"
fi

# The file slurp is shared by the image decoder and the WAV loader, so it has
# to follow either one and neither more. A draw loop reads no files at all.
if ! grep -q '^static std::string __nexa_gfx_read_file' "$WORK/group_sound.cpp" ||
   ! grep -q '^static std::string __nexa_gfx_read_file' "$WORK/image_has_the_decoder.cpp"; then
    echo "FAIL read_file_shared: a program that loads from a path lost the file read"
    fails=$((fails + 1))
elif grep -q '__nexa_gfx_read_file' "$WORK/draw_only.cpp"; then
    echo "FAIL read_file_shared: a draw loop carried a file read it cannot reach"
    fails=$((fails + 1))
else
    echo "ok read_file_shared"
fi

# Tearing a window down is core and hands the cursor and the icon back, so
# slicing those out has to leave the same kind of no-op behind.
for stub in __nexa_gfx_cursor_reset __nexa_gfx_icon_reset; do
    if ! grep -q "static void $stub() {}" "$WORK/draw_only.cpp"; then
        echo "FAIL ${stub}_stub: a draw loop lost the no-op window-polish teardown"
        fails=$((fails + 1))
    else
        echo "ok ${stub}_stub"
    fi
done

# --- size: the point of all of it -------------------------------------------

echo "-- size: the file the C++ compiler is handed"

# size_under <label> <file> <ceiling>
size_under() {
    lines=$(wc -l < "$2")
    if [ "$lines" -gt "$3" ]; then
        echo "FAIL $1: $lines lines of C++ (ceiling $3)"
        fails=$((fails + 1))
    else
        echo "ok $1 ($lines lines)"
    fi
}

# The draw loop measured 9,591 lines with the stb blob, 1,602 with the whole
# gfx runtime and 490 sliced. The ceilings are loose enough not to be a
# tripwire for an honest new line of core runtime, and tight enough that a
# re-introduced blob (~8,000 lines) or an unsliced runtime (~1,600) cannot
# sneak under them.
size_under "draw_only_stays_small" "$WORK/draw_only.cpp" 900
size_under "wasm_draw_only_stays_small" "$WORK/draw_only_wasm.cpp" 900

# Examples/paint_demo.nxa is the program the founder measured: 217 lines of
# Nexa that transpiled to 1,793 lines of C++ before the slicing and 1,341
# after. It uses 24 gfx builtins, so it is the case where slicing has the
# least to remove.
if "$NEXAC" "$ROOT/Examples/paint_demo.nxa" --source "$WORK/paint_demo.cpp" \
        > "$WORK/paint_demo.log" 2>&1; then
    size_under "paint_demo_stays_small" "$WORK/paint_demo.cpp" 1500
else
    echo "FAIL paint_demo_stays_small: NexaC could not transpile it"
    sed 's/^/  /' "$WORK/paint_demo.log"
    fails=$((fails + 1))
fi

# --- link: the sliced runtime still builds and runs -------------------------

echo "-- link: one program per gfx builtin, against the X11 stub"

pick_cxx() {
    if [ -n "${NEXA_CXX:-}" ]; then
        echo "$NEXA_CXX"
        return
    fi
    for c in clang++ g++ c++; do
        if command -v "$c" > /dev/null 2>&1; then
            echo "$c"
            return
        fi
    done
    echo ""
}

CXX=$(pick_cxx)

# Same route as the headless layer of Tests/gfx_cases.sh: a real gfx link needs
# the X11 development libraries, so fall back to the fake X11 in
# Tests/gfx_x11_stub. Sound here because none of these programs opens a window.
link_probe_done=0
link_ok=1

# link_case <label> <body>
# Transpiles one call on its own, builds it and runs it. A missing dependency
# in a feature group's closure surfaces here as a compile or link error.
link_case() {
    label="link_$1"
    body=$2
    [ "$link_ok" -eq 1 ] || return
    transpile "$label" "$body" || return
    if ! "$CXX" -std=c++17 -O0 -I "$SUITE/gfx_x11_stub" \
            "$WORK/$label.cpp" "$SUITE/gfx_x11_stub/x11_stub.cpp" \
            -o "$WORK/$label.bin" > "$WORK/$label.build" 2>&1 \
            || [ ! -x "$WORK/$label.bin" ]; then
        if [ "$link_probe_done" -eq 0 ]; then
            # The first failure might mean this machine cannot build a gfx
            # program at all rather than that the slicing is wrong. Tell the
            # two apart once, and skip the rest of the layer if so.
            if ! grep -qE '__nexa_gfx|__nexa_var|undefined (symbol|reference)' \
                    "$WORK/$label.build"; then
                echo "SKIP link: this machine cannot build a gfx program"
                grep -E 'error|Error' "$WORK/$label.build" | head -n 3 | sed 's/^/       /'
                skips=$((skips + 1))
                link_ok=0
                return
            fi
        fi
        link_probe_done=1
        echo "FAIL $label: the sliced runtime does not build"
        grep -E 'error:|undefined' "$WORK/$label.build" | head -n 5 | sed 's/^/  /'
        fails=$((fails + 1))
        return
    fi
    link_probe_done=1
    diags=$(grep -E '\.cpp:[0-9]+:[0-9]+: (warning|error):' "$WORK/$label.build")
    if [ -n "$diags" ]; then
        echo "FAIL $label: the C++ compiler had something to say about generated code"
        printf '%s\n' "$diags" | head -n 5 | sed 's/^/  /'
        fails=$((fails + 1))
        return
    fi
    if ! (cd "$WORK" && "./$label.bin" > "$label.run" 2>&1); then
        echo "FAIL $label: program exited non-zero"
        sed 's/^/  /' "$WORK/$label.run"
        fails=$((fails + 1))
        return
    fi
    echo "ok $label"
}

if [ -z "$CXX" ]; then
    echo "SKIP link: no C++ compiler on this machine"
    skips=$((skips + 1))
else
    link_case "close" '    gfx.close();'
    link_case "closed" '    let c: int = gfx.closed();'
    link_case "poll" '    gfx.poll();'
    link_case "present" '    gfx.present();'
    link_case "clear" '    gfx.clear(1, 2, 3);'
    link_case "plot" '    gfx.plot(1, 2, 3, 4, 5);'
    link_case "get" '    let p: int = gfx.get(1, 2);'
    link_case "alpha_get" '    let a: int = gfx.alpha();'
    link_case "alpha_set" '    gfx.alpha(128);'
    link_case "fill" '    gfx.fill(1, 2, 3, 4, 5, 6, 7);'
    link_case "rect" '    gfx.rect(1, 2, 3, 4, 5, 6, 7);'
    link_case "line" '    gfx.line(1, 2, 3, 4, 5, 6, 7);'
    link_case "line_thick" '    gfx.line(1, 2, 3, 4, 5, 6, 7, 2);'
    link_case "circle" '    gfx.circle(1, 2, 3, 4, 5, 6);'
    link_case "fill_circle" '    gfx.fill_circle(1, 2, 3, 4, 5, 6);'
    link_case "ellipse" '    gfx.ellipse(1, 2, 3, 4, 5, 6, 7);'
    link_case "fill_ellipse" '    gfx.fill_ellipse(1, 2, 3, 4, 5, 6, 7);'
    link_case "arc" '    gfx.arc(1, 2, 3, 0, 90, 5, 6, 7);'
    link_case "pie" '    gfx.pie(1, 2, 3, 0, 90, 5, 6, 7);'
    link_case "round_rect" '    gfx.round_rect(1, 2, 8, 6, 2, 5, 6, 7);'
    link_case "fill_round_rect" '    gfx.fill_round_rect(1, 2, 8, 6, 2, 5, 6, 7);'
    link_case "tri" '    gfx.tri(0, 0, 4, 0, 0, 4, 1, 2, 3);'
    link_case "fill_tri" '    gfx.fill_tri(0, 0, 4, 0, 0, 4, 1, 2, 3);'
    link_case "poly" '    let xs: []int = [0, 4, 0];
    let ys: []int = [0, 0, 4];
    let n: int = gfx.poly(xs, ys, 1, 2, 3);'
    link_case "fill_poly" '    let xs: []int = [0, 4, 0];
    let ys: []int = [0, 0, 4];
    let n: int = gfx.fill_poly(xs, ys, 1, 2, 3);'
    link_case "text" '    let w: int = gfx.text(0, 0, "a", 1, 2, 3);'
    link_case "text_size_get" '    let n: int = gfx.text_size();'
    link_case "text_size_set" '    gfx.text_size(2);'
    link_case "text_width" '    let w: int = gfx.text_width("a");'
    link_case "text_height" '    let h: int = gfx.text_height("a");'
    link_case "key" '    let k: int = gfx.key("space");'
    link_case "pressed" '    let k: int = gfx.pressed("space");'
    link_case "released" '    let k: int = gfx.released("space");'
    link_case "typed" '    let s: string = gfx.typed();'
    link_case "wheel" '    let n: int = gfx.wheel();'
    link_case "wheel_x" '    let n: int = gfx.wheel_x();'
    link_case "mouse_x" '    let n: int = gfx.mouse_x();'
    link_case "mouse_y" '    let n: int = gfx.mouse_y();'
    link_case "mouse" '    let n: int = gfx.mouse("left");'
    link_case "width" '    let n: int = gfx.width();'
    link_case "height" '    let n: int = gfx.height();'
    link_case "scale" '    let n: int = gfx.scale();'
    link_case "resize" '    let ok: int = gfx.resize(4, 4);'
    link_case "title_get" '    let s: string = gfx.title();'
    link_case "title_set" '    gfx.title("hi");'
    link_case "fullscreen" '    let f: int = gfx.fullscreen();'
    link_case "drop" '    let s: string = gfx.drop();'
    link_case "opendialog" '    let p: string = gfx.opendialog();'
    link_case "save" '    let ok: int = gfx.save("/dev/null");'
    link_case "image" '    let i: int = gfx.image("nope.png");'
    link_case "decode" '    let i: int = gfx.decode("xx");'
    link_case "image_w" '    let n: int = gfx.image_w(1);'
    link_case "image_h" '    let n: int = gfx.image_h(1);'
    link_case "blit_handle" '    let n: int = gfx.blit(0, 0, 1);'
    link_case "blit_path" '    let n: int = gfx.blit(0, 0, "nope.png");'
    link_case "blit_rot_handle" '    let n: int = gfx.blit_rot(0, 0, 1, 45);'
    link_case "blit_rot_path" '    let n: int = gfx.blit_rot(0, 0, "nope.png", 45, 8, 8);'
    link_case "icon_handle" '    let ok: int = gfx.icon(1);'
    link_case "icon_path" '    let ok: int = gfx.icon("nope.png");'
    link_case "cursor_get" '    let c: int = gfx.cursor();'
    link_case "cursor_set" '    let c: int = gfx.cursor(0);'
    link_case "audio" '    let ok: int = gfx.audio();'
    link_case "sample" '    let n: int = gfx.sample(1);'
    link_case "audio_queued" '    let n: int = gfx.audio_queued();'
    link_case "audio_flush" '    let n: int = gfx.audio_flush();'
    link_case "sound" '    let n: int = gfx.sound("nope.wav");'
    link_case "play" '    let v: int = gfx.play(1);'
    link_case "loop" '    let v: int = gfx.loop(1, 128);'
    link_case "stop_all" '    let n: int = gfx.stop();'
    link_case "stop_voice" '    let n: int = gfx.stop(1);'
    link_case "volume_get" '    let n: int = gfx.volume();'
    link_case "volume_set" '    let n: int = gfx.volume(128);'
fi

# --- headers: what slicing must not take away -------------------------------
# Two passes act on the generated file in order: duplicate `#include <...>`
# lines are dropped, then the inactive platform branches are deleted. Together
# they can lose a header outright -- the dedup keeps the copy inside a platform
# branch, slicing then deletes that branch, and the target that needed the
# header is left with none. That is not a gfx bug in itself; it bites whenever
# two modules happen to want the same header and one of them is guarded, which
# is why the gfx audio backends (`<thread>` for the macOS AudioQueue wait,
# `<dlfcn.h>` for the ALSA dlopen) collide with std/thread and std/dll.
#
# So this layer is written as the general rule rather than the two known pairs:
# if the emitted C++ names the symbol, the header has to be in the same file,
# on every target. A third module pairing that trips the same wire fails here
# without anyone remembering to add a case for it.

# needs <file> <symbol-pattern> <header>
# Reports a failure when the file uses the symbol and does not include the
# header. Silent when the symbol is not there: that target sliced it away.
needs() {
    if ! grep -qE "$2" "$1"; then
        return 0
    fi
    if grep -qF "#include <$3>" "$1"; then
        return 0
    fi
    echo "  uses $2 with no #include <$3>"
    return 1
}

# header_case <label> <nxa-source> [NexaC flags...]
header_case() {
    label="hdr_$1"
    src=$2
    shift 2
    printf '%s' "$src" > "$WORK/$label.nxa"
    if ! "$NEXAC" "$WORK/$label.nxa" "$@" --source "$WORK/$label.cpp" \
            > "$WORK/$label.log" 2>&1; then
        echo "FAIL $label: NexaC could not transpile"
        sed 's/^/  /' "$WORK/$label.log"
        fails=$((fails + 1))
        return
    fi
    bad=0
    needs "$WORK/$label.cpp" 'std::thread|std::this_thread' 'thread' || bad=1
    needs "$WORK/$label.cpp" 'dlopen|dlsym|dlclose' 'dlfcn.h' || bad=1
    needs "$WORK/$label.cpp" 'std::atomic' 'atomic' || bad=1
    needs "$WORK/$label.cpp" 'std::chrono' 'chrono' || bad=1
    if [ $bad -ne 0 ]; then
        echo "FAIL $label: slicing left the generated C++ without a header it uses"
        fails=$((fails + 1))
        return
    fi
    echo "ok $label"
}

echo "-- headers: a header slicing must not have taken away"

# gfx.play pulls the whole audio backend ladder in, whose macOS branch includes
# <thread>; std/thread's unguarded copy came later and was dropped as a
# duplicate, so every non-macOS build lost it.
AUDIO_THREAD='#include <std/gfx>
#include <std/thread>

fn worker() {
    let x: int = 1;
}

fn main() {
    let s: int = gfx.sound("beep.wav");
    gfx.play(s, 200);
    let t: int = thread.spawn(worker);
    thread.join(t);
}
'

# The same collision one module over: the ALSA branch dlopens libasound, and
# std/dll dlopens what it was asked to.
AUDIO_DLL='#include <std/gfx>
#include <std/dll>

fn main() {
    gfx.audio(44100);
    gfx.sample(0);
    let h: int = dll.load("./lib.so");
    dll.call(h, "func_name");
}
'

# And both at once, which is where a fix that only moves one of the two
# includes around shows up.
AUDIO_BOTH='#include <std/gfx>
#include <std/thread>
#include <std/dll>

fn worker() {
    let x: int = 1;
}

fn main() {
    let s: int = gfx.sound("beep.wav");
    gfx.play(s, 200);
    gfx.audio_flush();
    let t: int = thread.spawn(worker);
    thread.join(t);
    let h: int = dll.load("./lib.so");
    dll.call(h, "func_name");
}
'

# Every target this machine can emit for. macOS is the one that cannot be
# cross-emitted from here, and it is the branch that keeps its own copy of both
# headers, so it is the case least able to regress.
header_case "audio_thread_native" "$AUDIO_THREAD"
header_case "audio_thread_win" "$AUDIO_THREAD" --win
header_case "audio_thread_wasm" "$AUDIO_THREAD" --wasm
header_case "audio_dll_native" "$AUDIO_DLL"
header_case "audio_dll_win" "$AUDIO_DLL" --win
header_case "audio_dll_wasm" "$AUDIO_DLL" --wasm
header_case "audio_both_native" "$AUDIO_BOTH"
header_case "audio_both_win" "$AUDIO_BOTH" --win
header_case "audio_both_wasm" "$AUDIO_BOTH" --wasm

# A source-level rule is only worth what the compiler says about it, so the
# native combination is built for real as well -- same fake X11 as the link
# layer above, since none of these programs opens a window.
if [ -z "$CXX" ] || [ "$link_ok" -ne 1 ]; then
    echo "SKIP hdr_build: no usable C++ compiler on this machine"
    skips=$((skips + 1))
else
    printf '%s' "$AUDIO_BOTH" > "$WORK/hdr_build.nxa"
    if ! "$NEXAC" "$WORK/hdr_build.nxa" --source "$WORK/hdr_build.cpp" \
            > "$WORK/hdr_build.log" 2>&1; then
        echo "FAIL hdr_build: NexaC could not transpile"
        sed 's/^/  /' "$WORK/hdr_build.log"
        fails=$((fails + 1))
    elif ! "$CXX" -std=c++17 -O0 -I "$SUITE/gfx_x11_stub" -c \
            "$WORK/hdr_build.cpp" -o "$WORK/hdr_build.o" \
            > "$WORK/hdr_build.cc" 2>&1; then
        echo "FAIL hdr_build: gfx.play + thread.spawn + dll.load does not compile"
        grep -E 'error:' "$WORK/hdr_build.cc" | head -n 5 | sed 's/^/  /'
        fails=$((fails + 1))
    else
        echo "ok hdr_build"
    fi
fi

# --- report -----------------------------------------------------------------

if [ $fails -eq 0 ]; then
    if [ $skips -gt 0 ]; then
        echo "gfx emit ok ($skips layer(s) skipped: see above)"
    else
        echo "gfx emit ok"
    fi
    exit 0
fi
echo "gfx emit: $fails failure(s)"
exit 1
