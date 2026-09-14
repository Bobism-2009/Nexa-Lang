#!/bin/sh
# What a gfx program has to carry: the image decoder (BOB-24).
#
# stb_image is about 8,000 of the lines NexaC emits for a gfx program, and on a
# fresh -O2 build it is roughly three quarters of the compile time. A program
# that never decodes an image should not pay for it, so the transpiler emits the
# decoder only when one of the three calls that can reach it is used, and a
# two-line stub otherwise -- the gfx runtime always emits
# __nexa_gfx_decode_rgba, which calls into stb, so something has to be there for
# it to link against.
#
# Two layers, cheapest first, matching the other Tests/gfx_*_cases.sh suites:
#
#   slicing   Which programs carry the decoder. Transpile only (--source), for
#            both targets that append it (native Linux and --wasm). Never
#            invokes the C++ compiler, so it runs anywhere NexaC does.
#
#   link     The stub actually satisfies the reference. Build a draw-only gfx
#            program against the fake X11 in Tests/gfx_x11_stub and run it. No
#            display needed -- the program never opens a window. Skipped when
#            this machine has no C++ compiler.
#
# The decoder's own behaviour is not retested here; Tests/gfx_cases.sh already
# decodes Tests/gfx_2x2.png through the headless layer, and it is one of the
# programs that must still carry stb.
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

# stb_image's internals are all named stbi__*, so counting those lines
# distinguishes "the blob is here" from "the two-line stub is here" without
# depending on any particular line of it.
has_stb() {
    grep -q 'stbi__' "$1"
}

has_stub() {
    grep -q '^unsigned char\* __nexa_gfx_stbi_load_rgba(const unsigned char\*, int, int\*, int\*)' "$1"
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
    # Without the stub the program would still compile and then fail to link,
    # because the gfx runtime declares and calls both of these.
    if ! has_stub "$WORK/$label.cpp"; then
        echo "FAIL $label: dropped stb_image without leaving the stub behind"
        fails=$((fails + 1))
        return
    fi
    if ! grep -q '^void __nexa_gfx_stbi_free(void\*) {}' "$WORK/$label.cpp"; then
        echo "FAIL $label: stub is missing __nexa_gfx_stbi_free"
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
    if has_stub "$WORK/$label.cpp"; then
        echo "FAIL $label: emitted both the decoder and the stub"
        fails=$((fails + 1))
        return
    fi
    echo "ok $label"
}

echo "-- slicing: which gfx programs carry the image decoder"

expect_no_decoder "draw_only_has_no_decoder" "$DRAW_ONLY"

# gfx.save writes a BMP by hand rather than going through stb, and the text
# calls read a built-in font table, so neither drags the decoder in.
expect_no_decoder "save_and_text_have_no_decoder" \
    '    gfx.text(0, 0, "a", 1, 2, 3);
    let w: int = gfx.text_width("a");
    gfx.save("out.bmp");'

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

# Both targets that append the blob have to slice it the same way.
expect_no_decoder "wasm_draw_only_has_no_decoder" "$DRAW_ONLY" --wasm
expect_decoder "wasm_image_has_the_decoder" \
    '    let i: int = gfx.image("a.png");' --wasm

# The point of all this is the size of the file the C++ compiler is handed. The
# draw loop measured 9,591 lines with the blob and 1,602 without; 3,000 is a
# loose ceiling that a re-introduced blob (~8,000 lines) cannot sneak under.
lines=$(wc -l < "$WORK/draw_only_has_no_decoder.cpp")
if [ "$lines" -gt 3000 ]; then
    echo "FAIL draw_only_stays_small: $lines lines of C++ for a draw loop"
    fails=$((fails + 1))
else
    echo "ok draw_only_stays_small ($lines lines)"
fi

# --- link: the stub satisfies the reference ---------------------------------

echo "-- link: a program with the stub still builds and runs"

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
# Tests/gfx_x11_stub. Sound here because this program never opens a window.
if [ -z "$CXX" ]; then
    echo "SKIP stub_links: no C++ compiler on this machine"
    skips=$((skips + 1))
elif [ ! -f "$WORK/save_and_text_have_no_decoder.cpp" ]; then
    echo "SKIP stub_links: the program did not transpile (reported above)"
    skips=$((skips + 1))
else
    # Reuse the no-window program from the slicing layer: with no gfx.open it
    # runs to completion under the stub instead of spinning in a draw loop.
    if ! "$CXX" -std=c++17 -O0 -I "$SUITE/gfx_x11_stub" \
            "$WORK/save_and_text_have_no_decoder.cpp" \
            "$SUITE/gfx_x11_stub/x11_stub.cpp" -o "$WORK/prog" \
            > "$WORK/link.log" 2>&1 || [ ! -x "$WORK/prog" ]; then
        # An unresolved __nexa_gfx_stbi_load_rgba lands here, and so does a box
        # that cannot build gfx at all. Tell the two apart.
        if grep -q '__nexa_gfx_stbi' "$WORK/link.log"; then
            echo "FAIL stub_links: the stub does not satisfy the decoder reference"
            grep '__nexa_gfx_stbi' "$WORK/link.log" | head -n 3 | sed 's/^/  /'
            fails=$((fails + 1))
        else
            echo "SKIP stub_links: this machine cannot build a gfx program"
            grep -E 'error|Error' "$WORK/link.log" | head -n 3 | sed 's/^/       /'
            skips=$((skips + 1))
        fi
    else
        diags=$(grep -E '\.cpp:[0-9]+:[0-9]+: (warning|error):' "$WORK/link.log")
        if [ -n "$diags" ]; then
            echo "FAIL stub_links: the C++ compiler had something to say about generated code"
            printf '%s\n' "$diags" | sed 's/^/  /'
            fails=$((fails + 1))
        else
            out=$(cd "$WORK" && ./prog 2>&1)
            rc=$?
            if [ $rc -ne 0 ]; then
                echo "FAIL stub_links: program exited $rc"
                printf '%s\n' "$out" | sed 's/^/  /'
                fails=$((fails + 1))
            else
                echo "ok stub_links (via the X11 stub)"
            fi
        fi
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
