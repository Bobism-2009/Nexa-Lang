#!/bin/sh
# Cover for the gfx input upgrades (BOB-21): gfx.wheel, gfx.wheel_x,
# gfx.released, gfx.typed.
#
# Two halves, both headless — nothing here opens a window or needs a display.
#
#   codegen   gfx.wheel() must reach __nexa_gfx_wheel(), gfx.typed() must be a
#             string everywhere a string can go, and the parser must reject the
#             wrong arity by name rather than letting it through to the C++
#             compiler. These only transpile (--source), so they need no C++
#             compiler at all.
#
#   semantics Tests/gfx_input_semantics.cpp builds the generated runtime against
#             the fake X server in Tests/gfx_x11_stub and feeds it events, so
#             what the new functions actually report is checked rather than
#             assumed. Skipped, loudly, when there is no C++ compiler.
#
# Usage: Tests/gfx_input_cases.sh [path-to-NexaC]      (run from the repo root)

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")

HERE=$(cd "$(dirname "$0")" && pwd)

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM

fails=0

# expect_source <label> <grep-ERE> <source-text>
# Asserts the transpiled C++ contains a line matching the pattern.
expect_source() {
    label=$1
    want=$2
    src=$3

    printf '%s' "$src" > "$WORK/case.nxa"
    if ! "$NEXAC" "$WORK/case.nxa" --source "$WORK/case.cpp" > "$WORK/case.log" 2>&1; then
        echo "FAIL $label: NexaC could not transpile"
        sed 's/^/  /' "$WORK/case.log"
        fails=$((fails + 1))
        return
    fi
    if ! grep -Eq "$want" "$WORK/case.cpp"; then
        echo "FAIL $label: no generated line matches /$want/"
        fails=$((fails + 1))
        return
    fi
    echo "ok $label"
}

# expect_error <label> <expected-substring> <source-text>
# Asserts NexaC refuses the program and says why, without leaking a raw C++
# diagnostic at a user who never wrote C++.
expect_error() {
    label=$1
    want=$2
    src=$3

    printf '%s' "$src" > "$WORK/case.nxa"
    log=$("$NEXAC" "$WORK/case.nxa" --source "$WORK/case.cpp" 2>&1)
    if [ $? -eq 0 ]; then
        echo "FAIL $label: NexaC accepted it"
        fails=$((fails + 1))
        return
    fi
    if ! printf '%s\n' "$log" | grep -qF "$want"; then
        echo "FAIL $label: diagnostic did not mention '$want'"
        printf '%s\n' "$log" | sed 's/^/  /'
        fails=$((fails + 1))
        return
    fi
    if printf '%s\n' "$log" | grep -qE '\.cpp:[0-9]+:[0-9]+: error:'; then
        echo "FAIL $label: a raw C++ diagnostic reached the user"
        printf '%s\n' "$log" | sed 's/^/  /'
        fails=$((fails + 1))
        return
    fi
    echo "ok $label"
}

# --- codegen ----------------------------------------------------------------

expect_source "gfx.wheel calls the runtime" \
    '__nexa_gfx_wheel\(\)' \
    '#include <std/io>
#include <std/gfx>
fn main() {
    io.println(gfx.wheel());
}
'

expect_source "gfx.wheel_x calls the runtime" \
    '__nexa_gfx_wheel_x\(\)' \
    '#include <std/io>
#include <std/gfx>
fn main() {
    io.println(gfx.wheel_x());
}
'

expect_source "gfx.released calls the runtime with the key name" \
    '__nexa_gfx_released\("space"\)' \
    '#include <std/io>
#include <std/gfx>
fn main() {
    io.println(gfx.released("space"));
}
'

# gfx.typed is the one new call that is not an int. If the transpiler forgot
# that, io.println would pick "%d" for a std::string and the user would get a
# C++ diagnostic about a file they never wrote.
expect_source "gfx.typed prints as a string" \
    'printf\("%s\\n", __nexa_gfx_typed\(\)\.c_str\(\)\)' \
    '#include <std/io>
#include <std/gfx>
fn main() {
    io.println(gfx.typed());
}
'

expect_source "gfx.typed infers a string variable" \
    'std::string __nexa_var_[0-9]+ = __nexa_gfx_typed\(\)' \
    '#include <std/io>
#include <std/gfx>
fn main() {
    let t = gfx.typed();
    io.println(t.len());
}
'

expect_source "gfx.typed concatenates as a string" \
    '__nexa_gfx_typed\(\)' \
    '#include <std/io>
#include <std/gfx>
fn main() {
    let t = "you typed: " + gfx.typed();
    io.println(t);
}
'

expect_source "gfx.wheel is an int in arithmetic" \
    'printf\("%d\\n", \(__nexa_gfx_wheel\(\) \* 3\)\)' \
    '#include <std/io>
#include <std/gfx>
fn main() {
    io.println(gfx.wheel() * 3);
}
'

# --- the parser owns the arity ----------------------------------------------

expect_error "gfx.wheel takes no arguments" \
    "gfx.wheel" \
    '#include <std/gfx>
fn main() {
    gfx.wheel(1);
}
'

expect_error "gfx.typed takes no arguments" \
    "gfx.typed" \
    '#include <std/gfx>
fn main() {
    gfx.typed("x");
}
'

expect_error "gfx.released needs a key name" \
    "gfx.released" \
    '#include <std/gfx>
fn main() {
    gfx.released();
}
'

# A near miss should still be named as an unknown gfx method, and the list of
# methods it suggests should now include the new ones.
expect_error "an unknown gfx method lists gfx.released" \
    "released" \
    '#include <std/gfx>
fn main() {
    gfx.relesed("space");
}
'

expect_error "an unknown gfx method lists gfx.wheel" \
    "wheel" \
    '#include <std/gfx>
fn main() {
    gfx.whel();
}
'

expect_error "an unknown gfx method lists gfx.typed" \
    "typed" \
    '#include <std/gfx>
fn main() {
    gfx.typd();
}
'

# --- semantics --------------------------------------------------------------

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
if [ -z "$CXX" ]; then
    echo "skip gfx input semantics: no C++ compiler (set NEXA_CXX to force one)"
else
    printf '%s' '#include <std/gfx>
fn main() {
    gfx.poll();
}
' > "$WORK/gen.nxa"
    if ! "$NEXAC" "$WORK/gen.nxa" --source "$WORK/gen.cpp" > "$WORK/gen.log" 2>&1; then
        echo "FAIL gfx input semantics: NexaC could not transpile the driver program"
        sed 's/^/  /' "$WORK/gen.log"
        fails=$((fails + 1))
    elif ! "$CXX" -std=c++17 -O1 -I "$HERE/gfx_x11_stub" \
            -DNEXA_GEN="\"$WORK/gen.cpp\"" \
            "$HERE/gfx_input_semantics.cpp" "$HERE/gfx_x11_stub/x11_stub.cpp" \
            -o "$WORK/semantics" > "$WORK/build.log" 2>&1; then
        echo "FAIL gfx input semantics: could not build the driver"
        sed 's/^/  /' "$WORK/build.log"
        fails=$((fails + 1))
    else
        out=$("$WORK/semantics" 2>&1)
        status=$?
        printf '%s\n' "$out" | grep '^ok ' | sed 's/^ok /ok semantics: /'
        if [ $status -ne 0 ]; then
            printf '%s\n' "$out" | grep -v '^ok ' | sed 's/^/  /'
            fails=$((fails + 1))
        fi
    fi
fi

if [ $fails -eq 0 ]; then
    echo "gfx_input ok"
    exit 0
fi
echo "gfx_input: $fails failure(s)"
exit 1
