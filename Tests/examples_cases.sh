#!/bin/sh
# Examples/ (BOB-22).
#
# Nothing in the suite compiled Examples/ before this: the programs the README
# points a newcomer at were the only Nexa code in the repository that no test
# ever looked at, so a language change could break every one of them and the
# run would still come out green. They are also where the std/gfx showcase
# lives, which makes them worth more than a smoke test.
#
# Three layers, cheapest and most portable first:
#
#   transpile   Every Examples/*.nxa goes through NexaC --source. Catches a
#               parse or semantic error in an example. Needs nothing but NexaC,
#               so it runs anywhere.
#
#   compile     The generated C++ is compiled to an object file. This is the
#               layer that earns its keep: --source only parses and transpiles,
#               so a program can transpile happily and still emit C++ that does
#               not build. gfx examples are compiled against the fake X11 in
#               Tests/gfx_x11_stub, which is what lets them build on a box with
#               no display and no X11 development headers. Linking is not
#               attempted -- building the object proves the emitted code is
#               valid C++, and a real link would need a real backend.
#               Skipped, loudly, when there is no C++ compiler.
#
#   showcase    Examples/paint_demo.nxa is the std/gfx flagship: it is supposed
#               to exercise the shape rasterizers, the global alpha and
#               gfx.save, and the wheel / released / typed input calls, all in
#               one program. That claim is checked here rather than trusted, so
#               that gutting the showcase fails the suite instead of quietly
#               leaving the module's headline example half empty.
#
# Usage: Tests/examples_cases.sh [path-to-NexaC]        (run from the repo root)

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")

SUITE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$SUITE")
STUB="$SUITE/gfx_x11_stub"

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM

fails=0

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

# --- transpile, then compile -------------------------------------------------

echo "-- transpile: every Examples/*.nxa"
if [ -z "$CXX" ]; then
    echo "skip examples compile: no C++ compiler (set NEXA_CXX to force one)"
fi

found=0
for f in "$ROOT"/Examples/*.nxa; do
    [ -e "$f" ] || continue
    found=$((found + 1))
    name=$(basename "$f" .nxa)
    # Example names may contain spaces; the work files must not.
    safe=$(printf '%s' "$name" | tr -c 'A-Za-z0-9._-' '_')
    gen="$WORK/$safe.cpp"

    if ! "$NEXAC" "$f" --source "$gen" > "$WORK/$safe.log" 2>&1; then
        echo "FAIL transpile $name"
        sed 's/^/       /' "$WORK/$safe.log"
        fails=$((fails + 1))
        continue
    fi
    if [ ! -s "$gen" ]; then
        echo "FAIL transpile $name: no source written"
        fails=$((fails + 1))
        continue
    fi
    echo "ok transpile $name"

    [ -n "$CXX" ] || continue
    if "$CXX" -std=c++17 -O0 -I "$STUB" -c "$gen" -o "$WORK/$safe.o" \
            > "$WORK/$safe.cc.log" 2>&1; then
        echo "ok compile $name"
    else
        echo "FAIL compile $name: the generated C++ does not build"
        sed 's/^/       /' "$WORK/$safe.cc.log"
        fails=$((fails + 1))
    fi
done

if [ "$found" -eq 0 ]; then
    echo "FAIL: no examples found under $ROOT/Examples"
    fails=$((fails + 1))
fi

# --- the std/gfx showcase ----------------------------------------------------

echo "-- showcase: Examples/paint_demo.nxa exercises the whole module"

SHOW="$WORK/paint_demo.cpp"

# expect_call <label> <fixed-string>
# Asserts the showcase's generated C++ reaches that runtime function.
expect_call() {
    label=$1
    want=$2
    if grep -F -q "$want" "$SHOW"; then
        echo "ok showcase $label"
    else
        echo "FAIL showcase $label: no call to $want"
        fails=$((fails + 1))
    fi
}

if [ ! -s "$SHOW" ]; then
    echo "FAIL showcase: Examples/paint_demo.nxa did not transpile"
    fails=$((fails + 1))
else
    # shapes (BOB-19): a brush of each kind, and the HUD's frame
    expect_call shapes_fill_circle '__nexa_gfx_fill_circle('
    expect_call shapes_fill_tri    '__nexa_gfx_fill_tri('
    expect_call shapes_fill_poly   '__nexa_gfx_fill_poly('
    expect_call shapes_rect        '__nexa_gfx_rect('
    expect_call shapes_line        '__nexa_gfx_line('

    # alpha and screenshots (BOB-20)
    expect_call alpha_set          '__nexa_gfx_alpha_set('
    expect_call save               '__nexa_gfx_save('

    # input (BOB-21)
    expect_call input_wheel        '__nexa_gfx_wheel()'
    expect_call input_wheel_x      '__nexa_gfx_wheel_x()'
    expect_call input_released     '__nexa_gfx_released('
    expect_call input_typed        '__nexa_gfx_typed()'

    # and the calls that were already there, so the showcase stays a whole
    # program rather than a list of new features
    expect_call open               '__nexa_gfx_open('
    expect_call present            '__nexa_gfx_present('
    expect_call text               '__nexa_gfx_text('
fi

if [ $fails -eq 0 ]; then
    echo "examples ok"
    exit 0
fi
echo "examples: $fails failure(s)"
exit 1
