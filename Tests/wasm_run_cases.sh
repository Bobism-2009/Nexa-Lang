#!/bin/sh
# --wasm programs, run: what they print, and what they weigh.
#
# wasm_cases.sh checks what a --wasm build leaves on disk; this one runs it.
# Each case is a Tests/Lang program built with --wasm and run under node, whose
# output must be its .expected, byte for byte (line endings aside) -- the same
# bar a desktop build is held to. The ones here are the ones that broke on wasm
# while the desktop was fine, and one of each kind of runtime besides:
#
#   throw_header_text   keeps C++'s exceptions (a C++ header); Emscripten's
#                       default only throws them, so the first one aborted
#   flat_expr_chains    long expressions, which outgrew Emscripten's 64KB stack
#   thread_jobs         time.sleep on a thread, which needed ASYNCIFY to start
#   readln_eof          stdin
#   the rest            strings, lists, maps, files, own errors, defers, switch
#
#   sleep     time.sleep on the page's thread prints both sides of it, in order.
#   budget    hello world and an empty program stay small: the loader with the
#             module inside it, the file a page downloads.
#
# Needs em++ (Emscripten) and node; skipped without either. About a minute:
# each case is an Emscripten link.
#
# Usage: Tests/wasm_run_cases.sh [path-to-NexaC]

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")
HERE=$(cd "$(dirname "$0")" && pwd)

have_emcc=0
if command -v em++ >/dev/null 2>&1; then
    have_emcc=1
elif [ -x "$HOME/emsdk/upstream/emscripten/em++" ] || [ -x "$HOME/emsdk/upstream/emscripten/em++.exe" ]; then
    have_emcc=1
fi
if [ $have_emcc -eq 0 ] || ! command -v node >/dev/null 2>&1; then
    echo "skip: needs em++ (install Emscripten, or set EMSDK) and node"
    echo "wasm run skipped"
    exit 0
fi

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM
fails=0
passes=0

# run <name> <js> [stdin]: node's stdout and stderr together, as run_tests.sh takes them
run() {
    in=${3:-/dev/null}
    if command -v timeout >/dev/null 2>&1; then
        timeout 60 node "$2" < "$in" 2>&1
    else
        node "$2" < "$in" 2>&1
    fi
}

for t in method_defaults string_tokens collections_more file_extras try_noexc_defer switch \
         throw_header_text flat_expr_chains thread_jobs readln_eof; do
    src="$HERE/Lang/$t.nxa"
    [ -f "$src" ] || { echo "FAIL $t: no $src"; fails=$((fails + 1)); continue; }
    if ! "$NEXAC" "$src" --wasm -o "$WORK/$t.js" > "$WORK/$t.log" 2>&1; then
        echo "FAIL $t: the --wasm build failed"
        tail -5 "$WORK/$t.log" | sed 's/^/  /'
        fails=$((fails + 1))
        continue
    fi
    stdin=/dev/null
    [ -f "$HERE/Lang/$t.stdin" ] && stdin="$HERE/Lang/$t.stdin"
    run "$t" "$WORK/$t.js" "$stdin" | tr -d '\r' > "$WORK/$t.out"
    tr -d '\r' < "$HERE/Lang/$t.expected" > "$WORK/$t.want"
    if cmp -s "$WORK/$t.out" "$WORK/$t.want"; then
        passes=$((passes + 1))
    else
        echo "FAIL $t: the output is not $t.expected"
        diff "$WORK/$t.out" "$WORK/$t.want" | head -6 | sed 's/^/  /'
        fails=$((fails + 1))
    fi
done

# --- sleep ------------------------------------------------------------------
cat > "$WORK/sl.nxa" <<'EOF'
#include <std/io>
#include <std/time>
fn main() {
    io.println("before");
    let t = time.now_ms();
    time.sleep(30);
    io.println("after ", time.now_ms() - t >= 25);
}
EOF
if "$NEXAC" "$WORK/sl.nxa" --wasm -o "$WORK/sl.js" > "$WORK/sl.log" 2>&1 &&
   [ "$(run sl "$WORK/sl.js" | tr -d '\r' | tr '\n' '|')" = "before|after true|" ]; then
    passes=$((passes + 1))
else
    echo "FAIL sleep: time.sleep on the page's thread"
    run sl "$WORK/sl.js" 2>&1 | head -3 | sed 's/^/  /'
    fails=$((fails + 1))
fi

# --- budget -----------------------------------------------------------------
# Measured with Emscripten 6.0.8: 4.0KB and 6.6KB (7.5KB and 10.9KB before the
# loader was closure-compiled and emmalloc chosen). The ceilings leave room for
# another Emscripten's loader, not for a regression.
budget() {
    if ! "$NEXAC" "$WORK/$1.nxa" --wasm -o "$WORK/$1.js" > "$WORK/$1.log" 2>&1; then
        echo "FAIL budget $1: the build failed"; fails=$((fails + 1)); return
    fi
    sz=$(wc -c < "$WORK/$1.js" | tr -d ' ')
    if [ "$sz" -le "$2" ]; then
        passes=$((passes + 1))
    else
        echo "FAIL budget $1: $sz bytes, over $2"
        fails=$((fails + 1))
    fi
}
printf 'fn main() { }\n' > "$WORK/empty.nxa"
printf '#include <std/io>\nfn main() { io.println("hi"); }\n' > "$WORK/hello.nxa"
budget empty 5500
budget hello 8000

echo "$passes passed, $fails failed"
[ $fails -eq 0 ] || exit 1
echo "wasm run ok"
