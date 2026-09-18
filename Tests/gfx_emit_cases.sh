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
    # The frame limiter is a clock and a wait, and present() is core, so a
    # draw loop that never asks for a cap must not carry either.
    group "maxfps$suffix" '^static void __nexa_gfx_pace' \
        '    gfx.maxfps(60);' "$@"
    # Nothing but the call itself takes a window's frame off -- unlike
    # fullscreen, which maximising a window also reaches -- so a program that
    # never says gfx.borderless cannot get there and must not carry it.
    group "borderless$suffix" '^static int __nexa_gfx_borderless' \
        '    gfx.borderless(1);' "$@"
    # And the same for the other window-manager toggle: nothing but the call
    # puts a window above other programs.
    group "ontop$suffix" '^static int __nexa_gfx_ontop' \
        '    gfx.ontop(1);' "$@"
    # The third of the trio slices further than either: the function, and with
    # it the X11 window that could be see-through and the layered-window blit
    # on Windows. What stays behind is the flag, which gfx.clear reads.
    group "transparent$suffix" '^static int __nexa_gfx_transparent' \
        '    gfx.transparent(1);' "$@"
}

run_groups ""
# The wasm target slices the same runtime, so the whole table runs twice.
run_groups "_wasm" --wasm

# gfx.typed() is the only reader of the typed-text queue, and since BOB-57 the
# backend code that fills it -- WM_CHAR, the X11 KeyPress branch, the Cocoa
# key-down branch, the browser keydown callback -- is sliced by the same flag.
# So the queue goes away whole, rather than staying behind as the no-op
# push helpers a live caller used to need.
for target in "" _wasm; do
    if grep -q '__nexa_gfx_type_push' "$WORK/draw_only$target.cpp"; then
        echo "FAIL typed_gone$target: a draw loop carried typed-text helpers nothing feeds"
        grep -n '__nexa_gfx_type_push' "$WORK/draw_only$target.cpp" | head -n 3 | sed 's/^/  /'
        fails=$((fails + 1))
    else
        echo "ok typed_gone$target"
    fi
done

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

# gfx.fullscreen is core, and it reads two flags that belong to calls which are
# not: `borderless` says whether a WS_POPUP window is fullscreen's own or one
# the program unframed, and `ontop` picks the window the fullscreen exit
# inserts itself after -- HWND_NOTOPMOST there would otherwise cancel a
# gfx.ontop(1) the moment the program left fullscreen. So the *functions* slice
# away and the *fields* must not, on every target, or a draw loop stops
# compiling. Checked against the emission of a program that says neither.
for target in "" _wasm; do
    f="$WORK/draw_only$target.cpp"
    [ -f "$f" ] || continue
    if grep -q '__nexa_gfx_ontop' "$f"; then
        echo "FAIL ontop_field$target: a draw loop carried gfx.ontop, which it cannot reach"
        fails=$((fails + 1))
    elif ! grep -q '^    int ontop;' "$f" || ! grep -q '^    int borderless;' "$f"; then
        echo "FAIL ontop_field$target: a draw loop lost a flag its fullscreen reads"
        fails=$((fails + 1))
    else
        echo "ok ontop_field$target"
    fi
done

# Windows is where the field is actually read, and the Win32 branch is sliced
# out of every other target -- so this is the case that would notice the
# fullscreen exit going back to a bare HWND_NOTOPMOST.
if transpile "win_draw_only" "$DRAW_ONLY" --win; then
    if grep -q '__nexa_gfx_ontop' "$WORK/win_draw_only.cpp"; then
        echo "FAIL ontop_field_win: a Windows draw loop carried gfx.ontop"
        fails=$((fails + 1))
    elif ! grep -q '__nexa_g.ontop ? HWND_TOPMOST' "$WORK/win_draw_only.cpp"; then
        echo "FAIL ontop_field_win: the Win32 fullscreen path does not read __nexa_g.ontop"
        grep -n 'HWND_' "$WORK/win_draw_only.cpp" | head -n 5 | sed 's/^/  /'
        fails=$((fails + 1))
    else
        echo "ok ontop_field_win"
    fi
fi

# gfx.transparent slices the same way and one layer deeper, because what it can
# reach is not only a function: on X11 the *window* is created differently when
# the call is in the program at all, since a visual is fixed at creation and no
# call changes it afterwards. So a draw loop must carry neither -- and must
# still carry the flag, which its own gfx.clear reads.
for target in "" _wasm; do
    f="$WORK/draw_only$target.cpp"
    [ -f "$f" ] || continue
    if grep -q '__nexa_gfx_transparent' "$f"; then
        echo "FAIL transparent_field$target: a draw loop carried gfx.transparent"
        fails=$((fails + 1))
    elif ! grep -q '^    int transparent;' "$f"; then
        echo "FAIL transparent_field$target: a draw loop lost the flag its gfx.clear reads"
        fails=$((fails + 1))
    elif ! grep -q '__nexa_g.transparent ? (unsigned char)0 : (unsigned char)255' "$f"; then
        echo "FAIL transparent_field$target: gfx.clear does not read the flag"
        fails=$((fails + 1))
    else
        echo "ok transparent_field$target"
    fi
done

# The X11 window is the part that cannot be decided at call time, so it is the
# part slicing has to get right: a draw loop gets XCreateSimpleWindow on the
# screen's own visual, and a program that says gfx.transparent anywhere gets
# XCreateWindow on a depth-32 one -- even if all it ever does is ask.
if ! grep -q 'XCreateSimpleWindow' "$WORK/draw_only.cpp" ||
   grep -qE 'XMatchVisualInfo|XCreateWindow\(' "$WORK/draw_only.cpp"; then
    echo "FAIL transparent_visual: a draw loop did not get the ordinary X11 window"
    fails=$((fails + 1))
else
    echo "ok transparent_visual_draw_only"
fi
if transpile "transparent_query" '    let t: int = gfx.transparent();'; then
    bad=0
    # XMatchVisualInfo depth 32 TrueColor, and a window created with a
    # colormap AND a border pixel: leaving border_pixel out of the valuemask
    # is a BadMatch on a non-default visual rather than a default.
    grep -q 'XMatchVisualInfo(__nexa_g.dpy, scr, 32, TrueColor' \
        "$WORK/transparent_query.cpp" || bad=1
    grep -q 'XCreateWindow(' "$WORK/transparent_query.cpp" || bad=1
    grep -q 'swa.border_pixel = 0;' "$WORK/transparent_query.cpp" || bad=1
    grep -q 'CWBackPixel | CWBorderPixel' "$WORK/transparent_query.cpp" || bad=1
    grep -q 'swa_mask |= CWColormap' "$WORK/transparent_query.cpp" || bad=1
    # A GC has to share its drawable's depth, so the 32-bit window needs one.
    grep -q 'XCreateGC(__nexa_g.dpy, __nexa_g.win' "$WORK/transparent_query.cpp" || bad=1
    if [ $bad -ne 0 ]; then
        echo "FAIL transparent_visual: a program that only asks did not get the ARGB window"
        grep -nE 'XCreate|XMatchVisual|swa' "$WORK/transparent_query.cpp" | head -n 8 | sed 's/^/  /'
        fails=$((fails + 1))
    else
        echo "ok transparent_visual_query"
    fi
fi

# Windows is the one platform whose present path has to change, and the Win32
# branch is sliced out of every other target -- so this is where the layered
# window would quietly go missing.
if transpile "win_transparent" '    gfx.transparent(1);' --win; then
    bad=0
    grep -q 'ex |= WS_EX_LAYERED;' "$WORK/win_transparent.cpp" || bad=1
    grep -q 'UpdateLayeredWindow(__nexa_g.hwnd' "$WORK/win_transparent.cpp" || bad=1
    grep -q 'ULW_ALPHA);' "$WORK/win_transparent.cpp" || bad=1
    grep -q 'bf.AlphaFormat = AC_SRC_ALPHA;' "$WORK/win_transparent.cpp" || bad=1
    if [ $bad -ne 0 ]; then
        echo "FAIL transparent_win: the Win32 layered-window present is not there"
        fails=$((fails + 1))
    else
        echo "ok transparent_win"
    fi
fi
if transpile "win_draw_only2" "$DRAW_ONLY" --win; then
    # The call forms rather than the bare names: the core WM_PAINT handler
    # names UpdateLayeredWindow in a comment explaining why it stands aside for
    # one, and a comment is not machinery to carry.
    if grep -qE 'UpdateLayeredWindow\(|\| WS_EX_LAYERED' "$WORK/win_draw_only2.cpp"; then
        echo "FAIL transparent_win: a Windows draw loop carried the layered window"
        grep -nE 'UpdateLayeredWindow\(|WS_EX_LAYERED' "$WORK/win_draw_only2.cpp" | head -n 3 | sed 's/^/  /'
        fails=$((fails + 1))
    else
        echo "ok transparent_win_draw_only"
    fi
fi

# --- input: the collection side of the four input families ------------------

echo "-- input: what a program that reads no input collects"

# BOB-57. Every other group above is a function the program calls and nothing
# else can reach. The four input families are the only ones with a second side:
# a reader reports something that happened *to* the program, so behind it sits
# the code that collects it -- state the backends write as events arrive, the
# branches of the event pump that write it, and on X11 the events the window is
# subscribed to at all. The reader side was already sliced; this layer is the
# cover for the collection side.
#
# The founder's report is the case: a program that opens a window and presents
# it, and says nothing about keys, the mouse, the wheel or typed text.
WINDOW_ONLY='    gfx.open("t", 8, 8, 1);
    gfx.present();'

transpile "window_only" "$WINDOW_ONLY"
transpile "window_only_wasm" "$WINDOW_ONLY" --wasm
transpile "window_only_win" "$WINDOW_ONLY" --win

# emit_has <label> <file> <ERE> / emit_lacks <label> <file> <ERE>
emit_has() {
    if [ ! -f "$2" ]; then
        echo "FAIL $1: no emission to check"
        fails=$((fails + 1))
        return
    fi
    if ! grep -Eq "$3" "$2"; then
        echo "FAIL $1: the emission does not contain /$3/"
        fails=$((fails + 1))
        return
    fi
    echo "ok $1"
}

emit_lacks() {
    if [ ! -f "$2" ]; then
        echo "FAIL $1: no emission to check"
        fails=$((fails + 1))
        return
    fi
    if grep -Eq "$3" "$2"; then
        echo "FAIL $1: the emission still contains /$3/"
        grep -nE "$3" "$2" | head -n 3 | sed 's/^/  /'
        fails=$((fails + 1))
        return
    fi
    echo "ok $1"
}

# Nothing named for any of the four, on any target. Deliberately a blunt
# pattern over the whole file rather than one sentinel per group: the point of
# the report was that a window-only program carried input machinery, and the
# way to check that is to look for any of it.
#
# __nexa_gfx_ekey is the one name spelled like input that is not input, and it
# is checked below rather than here: the browser has no way to say "Escape was
# pressed" other than a keydown callback, so gfx.fullscreen owns one.
for target in "" _wasm _win; do
    emit_lacks "window_only_no_input$target" "$WORK/window_only$target.cpp" \
        '__nexa_gfx_(vk|key|pressed|released|key_snapshot|mouse|mouse_x|mouse_y|mouse_apply|mouse_refresh|map_mouse|wheel|wheel_add|wheel_x|typed|type_push|type_cap|has_focus|input_publish|dom_key_is_text|emouse|ewheel|mac_held)'
    emit_lacks "window_only_no_input_state$target" "$WORK/window_only$target.cpp" \
        '__nexa_g\.(mx|my|min|mlb|mmb|mrb|k_now|k_prev|keys|wheel_acc_x|wheel_acc_y|wheel_x|wheel_y|type_acc|type_buf|type_lead)'
done

# X11 is where the slicing shows on the wire rather than only in the file: the
# event mask is a request to the server, and Tests/gfx_x11_stub records it.
# Here the emission is enough to pin which mask is asked for.
emit_has "window_only_bare_mask" "$WORK/window_only.cpp" \
    '^        ExposureMask \| StructureNotifyMask\);$'
emit_lacks "window_only_no_keysym" "$WORK/window_only.cpp" 'X11/keysym\.h'

# The two collection points that are *not* input and must survive: Escape
# leaves fullscreen, on Win32 through WM_KEYDOWN and in the browser through the
# keydown callback. gfx.fullscreen is core, so a program that never reads a key
# still has to be able to leave fullscreen with one.
emit_has "window_only_win_keeps_fullscreen_escape" "$WORK/window_only_win.cpp" \
    'msg == WM_KEYDOWN && wParam == VK_ESCAPE'
emit_lacks "window_only_win_no_collection" "$WORK/window_only_win.cpp" \
    'WM_CHAR|WM_MOUSEWHEEL|WM_MOUSEHWHEEL|GetAsyncKeyState'
emit_has "window_only_wasm_keeps_fullscreen_escape" "$WORK/window_only_wasm.cpp" \
    'emscripten_set_keydown_callback'
emit_has "window_only_wasm_escape_still_leaves" "$WORK/window_only_wasm.cpp" \
    'code == 27 && down && __nexa_g\.fullscreen'
emit_lacks "window_only_wasm_no_collection" "$WORK/window_only_wasm.cpp" \
    'emscripten_set_keyup_callback|emscripten_set_mouse|emscripten_set_wheel_callback'
# The keydown callback stays, but what it did on the way past does not: the
# key-state table is gfx.key's and the text push is gfx.typed's.
emit_lacks "window_only_wasm_ekey_collects_nothing" "$WORK/window_only_wasm.cpp" \
    '__nexa_g\.keys\[code\]|__nexa_gfx_type_push_utf8\(e->key'

# Each family on its own. The interesting cases are the neighbours: the wheel
# rides on X11 ButtonPress, so wheel-only has to subscribe to button presses
# without pulling the mouse in; and gfx.typed() reads KeyPress without wanting
# a single XK_ name, which is the whole of <X11/keysym.h>.
transpile "keys_only" '    let k: int = gfx.key("w");'
transpile "mouse_only" '    let n: int = gfx.mouse_x();'
transpile "wheel_only" '    let n: int = gfx.wheel();'
transpile "typed_only" '    let s: string = gfx.typed();'
transpile "all_input" '    let k: int = gfx.key("w");
    let n: int = gfx.mouse_x();
    let w: int = gfx.wheel();
    let s: string = gfx.typed();'

emit_has "keys_only_mask" "$WORK/keys_only.cpp" \
    'KeyPressMask \| KeyReleaseMask\);$'
emit_has "keys_only_keysym" "$WORK/keys_only.cpp" 'X11/keysym\.h'
emit_lacks "keys_only_no_neighbours" "$WORK/keys_only.cpp" \
    'PointerMotionMask|ButtonPressMask|__nexa_gfx_wheel_add|__nexa_gfx_type_push|__nexa_gfx_mouse_refresh'

emit_has "mouse_only_mask" "$WORK/mouse_only.cpp" \
    'PointerMotionMask \| ButtonPressMask \| ButtonReleaseMask\);$'
emit_has "mouse_only_refresh" "$WORK/mouse_only.cpp" '^static void __nexa_gfx_mouse_refresh\(\) \{'
emit_lacks "mouse_only_no_neighbours" "$WORK/mouse_only.cpp" \
    'KeyPressMask|KeyReleaseMask|X11/keysym\.h|__nexa_gfx_wheel_add|__nexa_gfx_type_push'

# Wheel-only takes ButtonPressMask -- that is the event the notches arrive on
# -- and neither of the other two button-and-motion bits, which belong to the
# mouse.
emit_has "wheel_only_mask" "$WORK/wheel_only.cpp" \
    '^        ExposureMask \| StructureNotifyMask \| ButtonPressMask\);$'
emit_has "wheel_only_add" "$WORK/wheel_only.cpp" '^static void __nexa_gfx_wheel_add'
emit_lacks "wheel_only_no_neighbours" "$WORK/wheel_only.cpp" \
    'PointerMotionMask|ButtonReleaseMask|KeyPressMask|X11/keysym\.h|__nexa_gfx_mouse_refresh|__nexa_gfx_type_push'

emit_has "typed_only_mask" "$WORK/typed_only.cpp" \
    '^        ExposureMask \| StructureNotifyMask \| KeyPressMask\);$'
emit_has "typed_only_push" "$WORK/typed_only.cpp" '^static void __nexa_gfx_type_push_cp'
emit_lacks "typed_only_no_neighbours" "$WORK/typed_only.cpp" \
    'KeyReleaseMask|X11/keysym\.h|PointerMotionMask|ButtonPressMask|__nexa_gfx_wheel_add|__nexa_gfx_mouse_refresh'

# And a program that reads all four asks for exactly the mask every gfx program
# asked for before any of this: the slicing takes nothing away from a program
# that uses the feature.
emit_has "all_input_full_mask" "$WORK/all_input.cpp" \
    'ExposureMask \| StructureNotifyMask \|
        KeyPressMask \| KeyReleaseMask \|
        PointerMotionMask \| ButtonPressMask \| ButtonReleaseMask\);'

# --- drop: the fifth input family -------------------------------------------

echo "-- drop: a dropped file is collected only where it is read"

transpile "drop_only" '    let p: string = gfx.drop();'
transpile "drop_only_win" '    let p: string = gfx.drop();' --win
transpile "drop_only_wasm" '    let p: string = gfx.drop();' --wasm
transpile "dialog_only_win" '    let p: string = gfx.opendialog("*.txt");' --win
transpile "dialog_only_wasm" '    let p: string = gfx.opendialog("*.txt");' --wasm

# Nothing of the family in a program that never reads a dropped path: not the
# reader, not the field it reads, and not the subscription that fills it. On
# Windows that subscription is DragAcceptFiles -- WM_DROPFILES is sent only to
# a window that has asked for it -- which is the exact counterpart of
# XSelectInput on X11.
for target in "" _wasm _win; do
    emit_lacks "window_only_no_drop$target" "$WORK/window_only$target.cpp" \
        '__nexa_gfx_drop|__nexa_g\.drop_path|WM_DROPFILES|DragAcceptFiles|DragQueryFile|DragFinish|nexaDropPath|registerForDraggedTypes'
done

emit_has "drop_only_win_handler" "$WORK/drop_only_win.cpp" 'msg == WM_DROPFILES'
emit_has "drop_only_win_subscribes" "$WORK/drop_only_win.cpp" 'DragAcceptFiles'
emit_has "drop_only_win_shellapi" "$WORK/drop_only_win.cpp" '#include <shellapi\.h>'
emit_has "drop_only_wasm_collects" "$WORK/drop_only_wasm.cpp" 'nexaDropPath'
emit_has "drop_only_reader" "$WORK/drop_only.cpp" '^static std::string __nexa_gfx_drop\(\)'

# gfx.drop() is the reader, not the picker: a program that only reads a dropped
# path gets no file dialog and no <commdlg.h> with it.
emit_lacks "drop_only_win_no_dialog" "$WORK/drop_only_win.cpp" \
    '__nexa_gfx_opendialog|__nexa_gfx_filter_safe|commdlg\.h'

# The coupling the other way round is real and must hold: in the browser
# gfx.open_dialog clicks a hidden <input> and the file it picks is delivered
# through gfx.drop(), so a dialog needs the drop family under it. The runtime
# is emitted once for all four backends, so it holds on every slice.
emit_has "dialog_only_wasm_needs_drop" "$WORK/dialog_only_wasm.cpp" \
    '__nexa_gfx_drop|nexaDropPath'
emit_has "dialog_only_win_needs_drop" "$WORK/dialog_only_win.cpp" '__nexa_g\.drop_path'
emit_has "dialog_only_win_commdlg" "$WORK/dialog_only_win.cpp" '#include <commdlg\.h>'

# --- keys: the live read and the edge read ----------------------------------

echo "-- keys: gfx.key does not carry gfx.pressed's snapshots"

transpile "key_live_only" '    let k: int = gfx.key("w");'
transpile "key_live_only_win" '    let k: int = gfx.key("w");' --win
transpile "key_edge" '    let k: int = gfx.pressed("w");'
transpile "key_edge_win" '    let k: int = gfx.pressed("w");' --win
transpile "key_released" '    let k: int = gfx.released("w");'

# gfx.key() asks the backend whether the key is down at the moment it is asked
# -- GetAsyncKeyState, XQueryKeymap, CGEventSourceKeyState -- and keeps no
# answer, so none of the machinery that remembers last frame comes with it.
for target in "" _win; do
    emit_has "key_live_only_reader$target" "$WORK/key_live_only$target.cpp" \
        '^static int __nexa_gfx_key\(const std::string&'
    emit_has "key_live_only_backend$target" "$WORK/key_live_only$target.cpp" \
        '^static int __nexa_gfx_vk\(const std::string&'
    emit_lacks "key_live_only_no_snapshots$target" "$WORK/key_live_only$target.cpp" \
        '__nexa_gfx_key_names|__nexa_gfx_key_slot|__nexa_gfx_key_snapshot|__nexa_gfx_pressed|__nexa_gfx_released|__nexa_g\.k_now|__nexa_g\.k_prev'
done

# And both edge readers bring all of it, the live read included: a snapshot is
# taken by asking the live reader once per name in the table.
for label in key_edge key_edge_win key_released; do
    emit_has "${label}_names" "$WORK/$label.cpp" '^static const char\* const __nexa_gfx_key_names'
    emit_has "${label}_snapshot" "$WORK/$label.cpp" '^static void __nexa_gfx_key_snapshot\(\) \{'
    emit_has "${label}_polls" "$WORK/$label.cpp" '^    __nexa_gfx_key_snapshot\(\);$'
    emit_has "${label}_state" "$WORK/$label.cpp" '__nexa_g\.k_now'
    emit_has "${label}_live" "$WORK/$label.cpp" '^static int __nexa_gfx_vk\(const std::string&'
done

# --- headers: earned by a symbol that survives ------------------------------

echo "-- headers: a header only where something still names it"

transpile "image_win" '    let i: int = gfx.image("a.png");' --win
transpile "audio_win" '    let r: int = gfx.audio(44100);' --win
transpile "image_mac_free" '    let i: int = gfx.image("a.png");'

# The founder's report: a window-only Windows program carried the file dialog,
# COM, the imaging codecs and the multimedia API, all four sliced down to
# nothing. A header is not free -- <wincodec.h> alone is thousands of lines of
# COM interface -- so each now comes with the code that names it.
emit_lacks "window_only_win_no_unearned_headers" "$WORK/window_only_win.cpp" \
    'commdlg\.h|objbase\.h|wincodec\.h|mmsystem\.h|shellapi\.h'
emit_has "image_win_wic" "$WORK/image_win.cpp" '#include <wincodec\.h>'
emit_has "image_win_com" "$WORK/image_win.cpp" '#include <objbase\.h>'
emit_lacks "image_win_no_audio_header" "$WORK/image_win.cpp" 'mmsystem\.h'
emit_has "audio_win_mmsystem" "$WORK/audio_win.cpp" '#include <mmsystem\.h>'
emit_lacks "audio_win_no_codec_headers" "$WORK/audio_win.cpp" 'wincodec\.h|objbase\.h'

# <windows.h> is core and always there -- and exactly once. std/gfx and
# std/time both want it, and before the include dedup was moved to run after
# the platform ladders are stripped, neither copy counted as unconditional and
# both were emitted.
printf '#include <std/gfx>\n#include <std/time>\nfn main() {\n    gfx.open("t", 8, 8, 1);\n    gfx.present();\n    time.sleep(1);\n}\n' \
    > "$WORK/gfx_and_time.nxa"
if "$NEXAC" "$WORK/gfx_and_time.nxa" --win --source "$WORK/gfx_and_time.cpp" \
        > "$WORK/gfx_and_time.log" 2>&1; then
    n=$(grep -c '^#include <windows\.h>$' "$WORK/gfx_and_time.cpp")
    if [ "$n" = "1" ]; then
        echo "ok gfx_and_time_one_windows_h"
    else
        echo "FAIL gfx_and_time_one_windows_h: $n copies of <windows.h>, expected 1"
        fails=$((fails + 1))
    fi
else
    echo "FAIL gfx_and_time_one_windows_h: NexaC could not transpile it"
    sed 's/^/  /' "$WORK/gfx_and_time.log"
    fails=$((fails + 1))
fi

# The std headers are chosen by reading back what the slicing left rather than
# from a table, so the check is the same shape: a draw loop names no file and
# no container, and carries neither header.
emit_lacks "window_only_no_unused_std_headers" "$WORK/window_only.cpp" \
    '#include <(cstdio|algorithm|cmath)>'
emit_has "image_mac_free_reads_a_file" "$WORK/image_mac_free.cpp" '#include <cstdio>'

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
# gfx runtime and 490 sliced -- 666 since gfx.transparent (BOB-51) put real
# alpha in the framebuffer, which is core, and back to 491 since BOB-57 sliced
# the input-collection side as well: a draw loop calls gfx.poll() but reads no
# key, mouse, wheel or typed text, so it now carries none of the code that
# collects them either. The ceilings are loose enough not to be a tripwire for
# an honest new line of core runtime, and tight enough that a re-introduced
# blob (~8,000 lines) or an unsliced runtime (~1,800) cannot sneak under them.
size_under "draw_only_stays_small" "$WORK/draw_only.cpp" 900
size_under "wasm_draw_only_stays_small" "$WORK/draw_only_wasm.cpp" 900

# And the founder's own case, a rung below it: a program that opens a window
# and presents it reads no input at all, so nothing of the four families is
# emitted for it. It measured 600 lines before BOB-57 and 425 after.
size_under "window_only_stays_small" "$WORK/window_only.cpp" 600

# Examples/paint_demo.nxa is the program the founder measured: 217 lines of
# Nexa that transpiled to 1,793 lines of C++ before the slicing and 1,341
# after. It uses 24 gfx builtins, so it is the case where slicing has the
# least to remove.
#
# It measured 1,525 after gfx.transparent (BOB-51), which is what the fourth
# byte of a pixel meaning something costs a program that never asks for it:
# the destination-alpha arm of the blend, the alpha the clear and the resize
# write, and the premultiply on the way out of the X11 present. All core, all
# reachable from gfx.clear, none of it sliceable -- so the ceiling moved rather
# than the code. It still catches what it was built to catch: paint_demo with
# the runtime unsliced is past 2,000 lines, and the stb blob is 8,000 on its
# own.
if "$NEXAC" "$ROOT/Examples/paint_demo.nxa" --source "$WORK/paint_demo.cpp" \
        > "$WORK/paint_demo.log" 2>&1; then
    size_under "paint_demo_stays_small" "$WORK/paint_demo.cpp" 1650
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
    link_case "borderless_get" '    let b: int = gfx.borderless();'
    link_case "borderless_set" '    let b: int = gfx.borderless(1);'
    link_case "ontop_get" '    let t: int = gfx.ontop();'
    link_case "ontop_set" '    let t: int = gfx.ontop(1);'
    link_case "transparent_get" '    let t: int = gfx.transparent();'
    link_case "transparent_set" '    let t: int = gfx.transparent(1);'
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
    link_case "maxfps" '    gfx.maxfps(60);'
    link_case "maxfps_off" '    gfx.maxfps(0);'
fi

# --- value: which gfx calls may be used as a value --------------------------
# The link layer above binds each builtin the way it is meant to be used. This
# layer is the other half: that NexaC *refuses* the wrong way, and refuses it
# in Nexa rather than letting clang complain about a generated variable name.
#
# The split is the runtime signatures in include/GfxRuntime.hpp -- the drawing
# calls plus poll, present, close and maxfps are `static void` -- and it is
# written down in three places that have to agree: the hint tables in
# Parser.hpp, the int/string lists in inferExprNexaType, and this layer. A new
# builtin that is added to none of them lands in the void set by default, so
# the accept half is what catches it.
#
# Transpile only: none of this reaches the C++ compiler.

echo "-- value: void builtins refused as values, the rest usable as values"

# value_reject <label> <call>
# A void builtin bound to a name. NexaC must fail, naming the call.
value_reject() {
    label="value_$1"
    printf '#include <std/gfx>\nfn main() {\n    let V = %s;\n}\n' "$2" > "$WORK/$label.nxa"
    if out=$("$NEXAC" "$WORK/$label.nxa" --source "$WORK/$label.cpp" 2>&1); then
        echo "FAIL $label: NexaC accepted a void gfx call as a value"
        fails=$((fails + 1))
        return
    fi
    case $out in
        *"; you aren't allowed to turn it into a variable"*) echo "ok $label" ;;
        *)
            echo "FAIL $label: refused, but not with the void-call diagnostic"
            printf '%s\n' "$out" | tail -n 2 | sed 's/^/  /'
            fails=$((fails + 1))
            ;;
    esac
}

# value_accept <label> <call> [string]
# A value-returning builtin handed to a function. Deliberately not `let n: int
# = ...`: an annotated let never asks inferExprNexaType what the call returns,
# so it would not notice one misfiled as void.
value_accept() {
    label="value_$1"
    sink=take
    [ "${3:-int}" = "string" ] && sink=takes
    printf '#include <std/gfx>\nfn take(n: int): int { return n; }\nfn takes(s: string): int { return 1; }\nfn main() {\n    let V = %s(%s);\n}\n' \
        "$sink" "$2" > "$WORK/$label.nxa"
    if ! out=$("$NEXAC" "$WORK/$label.nxa" --source "$WORK/$label.cpp" 2>&1); then
        echo "FAIL $label: NexaC refused a value-returning gfx call used as a value"
        printf '%s\n' "$out" | tail -n 2 | sed 's/^/  /'
        fails=$((fails + 1))
        return
    fi
    echo "ok $label"
}

value_reject "close" 'gfx.close()'
value_reject "poll" 'gfx.poll()'
value_reject "present" 'gfx.present()'
value_reject "maxfps" 'gfx.maxfps(60)'
value_reject "clear" 'gfx.clear(1, 2, 3)'
value_reject "plot" 'gfx.plot(1, 2, 3, 4, 5)'
value_reject "fill" 'gfx.fill(1, 2, 3, 4, 5, 6, 7)'
value_reject "rect" 'gfx.rect(1, 2, 3, 4, 5, 6, 7)'
value_reject "line" 'gfx.line(1, 2, 3, 4, 5, 6, 7)'
value_reject "line_thick" 'gfx.line(1, 2, 3, 4, 5, 6, 7, 2)'
value_reject "circle" 'gfx.circle(1, 2, 3, 4, 5, 6)'
value_reject "fill_circle" 'gfx.fill_circle(1, 2, 3, 4, 5, 6)'
value_reject "ellipse" 'gfx.ellipse(1, 2, 3, 4, 5, 6, 7)'
value_reject "fill_ellipse" 'gfx.fill_ellipse(1, 2, 3, 4, 5, 6, 7)'
value_reject "arc" 'gfx.arc(1, 2, 3, 0, 90, 5, 6, 7)'
value_reject "pie" 'gfx.pie(1, 2, 3, 0, 90, 5, 6, 7)'
value_reject "round_rect" 'gfx.round_rect(1, 2, 8, 6, 2, 5, 6, 7)'
value_reject "fill_round_rect" 'gfx.fill_round_rect(1, 2, 8, 6, 2, 5, 6, 7)'
value_reject "tri" 'gfx.tri(0, 0, 4, 0, 0, 4, 1, 2, 3)'
value_reject "fill_tri" 'gfx.fill_tri(0, 0, 4, 0, 0, 4, 1, 2, 3)'

value_accept "open" 'gfx.open("t", 8, 8, 1)'
value_accept "resize" 'gfx.resize(4, 4)'
value_accept "closed" 'gfx.closed()'
value_accept "width" 'gfx.width()'
value_accept "height" 'gfx.height()'
value_accept "scale" 'gfx.scale()'
value_accept "get" 'gfx.get(1, 2)'
value_accept "key" 'gfx.key("space")'
value_accept "pressed" 'gfx.pressed("space")'
value_accept "released" 'gfx.released("space")'
value_accept "wheel" 'gfx.wheel()'
value_accept "wheel_x" 'gfx.wheel_x()'
value_accept "mouse" 'gfx.mouse("left")'
value_accept "mouse_x" 'gfx.mouse_x()'
value_accept "mouse_y" 'gfx.mouse_y()'
value_accept "typed" 'gfx.typed()' string
value_accept "text" 'gfx.text(0, 0, "a", 1, 2, 3)'
value_accept "text_size_get" 'gfx.text_size()'
value_accept "text_size_set" 'gfx.text_size(2)'
value_accept "text_width" 'gfx.text_width("a")'
value_accept "text_height" 'gfx.text_height("a")'
value_accept "title_get" 'gfx.title()' string
value_accept "title_set" 'gfx.title("hi")'
value_accept "drop" 'gfx.drop()' string
value_accept "opendialog" 'gfx.opendialog()' string
value_accept "fullscreen" 'gfx.fullscreen()'
value_accept "borderless" 'gfx.borderless(1)'
value_accept "ontop" 'gfx.ontop(1)'
value_accept "transparent" 'gfx.transparent(1)'
value_accept "alpha_get" 'gfx.alpha()'
value_accept "alpha_set" 'gfx.alpha(128)'
value_accept "cursor_get" 'gfx.cursor()'
value_accept "cursor_set" 'gfx.cursor(0)'
value_accept "save" 'gfx.save("/dev/null")'
value_accept "image" 'gfx.image("nope.png")'
value_accept "decode" 'gfx.decode("xx")'
value_accept "image_w" 'gfx.image_w(1)'
value_accept "image_h" 'gfx.image_h(1)'
value_accept "blit" 'gfx.blit(0, 0, 1)'
value_accept "blit_rot" 'gfx.blit_rot(0, 0, 1, 45)'
value_accept "icon" 'gfx.icon(1)'
value_accept "audio" 'gfx.audio()'
value_accept "sample" 'gfx.sample(1)'
value_accept "audio_queued" 'gfx.audio_queued()'
value_accept "audio_flush" 'gfx.audio_flush()'
value_accept "sound" 'gfx.sound("nope.wav")'
value_accept "play" 'gfx.play(1)'
value_accept "loop" 'gfx.loop(1, 128)'
value_accept "stop" 'gfx.stop()'
value_accept "volume_get" 'gfx.volume()'
value_accept "volume_set" 'gfx.volume(128)'
# gfx.poly and gfx.fill_poly want slice variables rather than an inline call,
# so the link layer above is what covers them as values.

# --- headers: what slicing must not take away -------------------------------
# Two passes act on the generated file in order: duplicate `#include <...>`
# lines are dropped, then the inactive platform branches are deleted. Together
# they can lose a header outright -- the dedup keeps the copy inside a platform
# branch, slicing then deletes that branch, and the target that needed the
# header is left with none. That is not a gfx bug in itself; it bites whenever
# two modules happen to want the same header and one of them is guarded, which
# is why the gfx audio backends (`<thread>` for the macOS AudioQueue wait) and
# std/network's non-Emscripten branch (`<dlfcn.h>` for the libssl dlopen)
# collide with std/thread and std/dll.
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

# The same wire one module over: std/dll dlopens what it was asked to, and the
# audio backend is the guarded neighbour that must not have taken its header.
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
