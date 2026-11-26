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
#   uncaught  a kept C++ exception nothing catches: "Uncaught error", status 1,
#             nothing printed lost (it escaped to JavaScript, unreported).
#   browser   in headless Chrome, when there is one: std/http from a page (it
#             failed every request: a synchronous fetch is a worker's only),
#             and std/gfx's mouse, wheel, keys and typed text, closure-compiled.
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

# --- uncaught ---------------------------------------------------------------
# Kept C++ exceptions: one nothing catches says so and the status is 1, and what
# was printed is all there -- a line with no newline yet comes out as the program
# ends, after the message, as with NexaC's own errors on wasm.
cp "$HERE/Lang/throw_header_text.hpp" "$WORK/"
cat > "$WORK/un.nxa" <<'EOF'
#include "throw_header_text.hpp"
#include <std/io>
fn main() {
    io.print("partial ");
    throw header_text();
}
EOF
if "$NEXAC" "$WORK/un.nxa" --wasm -o "$WORK/un.js" > "$WORK/un.log" 2>&1; then
    run un "$WORK/un.js" > "$WORK/un.out"; st=$?
    if [ "$(tr -d '\r' < "$WORK/un.out" | tr '\n' '|')" = "Uncaught error: from std::string|partial |" ] && [ $st -eq 1 ]; then
        passes=$((passes + 1))
    else
        echo "FAIL uncaught: a C++ exception nothing catches (status $st)"
        run un "$WORK/un.js" 2>&1 | head -4 | sed 's/^/  /'
        fails=$((fails + 1))
    fi
else
    echo "FAIL uncaught: the build failed"; fails=$((fails + 1))
fi

# --- browser ----------------------------------------------------------------
# What node cannot run: std/http (the browser's fetch) and std/gfx input, in
# headless Chrome through Tests/wasm_browser.mjs. NEXA_CHROME names a browser;
# otherwise the usual places. Skipped without one.
chrome=${NEXA_CHROME:-}
if [ -z "$chrome" ]; then
    for c in "/c/Program Files/Google/Chrome/Application/chrome.exe" \
             "/c/Program Files (x86)/Microsoft/Edge/Application/msedge.exe" \
             "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"; do
        [ -x "$c" ] && { chrome=$c; break; }
    done
fi
if [ -z "$chrome" ]; then
    for c in google-chrome google-chrome-stable chromium chromium-browser; do
        command -v "$c" >/dev/null 2>&1 && { chrome=$(command -v "$c"); break; }
    done
fi
if [ -z "$chrome" ]; then
    echo "skip browser: no Chrome or Chromium (set NEXA_CHROME)"
else
    drive() { node "$HERE/wasm_browser.mjs" "$chrome" "$@" 2>&1 | tr -d '\r'; }

    port=$((18000 + $$ % 1000))
    cat > "$WORK/net.nxa" <<EOF
#include <std/network>
#include <std/io>
fn main() {
    io.println(http.get("http://127.0.0.1:$port/x").value());
    io.println(http.post("http://127.0.0.1:$port/p", "payload").value());
    io.println(http.get("http://127.0.0.1:$port/h", ["X-T: tok"]).value());
    io.println("missing ok=", http.get("http://127.0.0.1:$port/missing").ok());
}
EOF
    echo '[["wait",3000],["eval","document.getElementById(\"nexa-out\").textContent"]]' > "$WORK/net.json"
    want='eval: "GET /x [] \nPOST /p [payload] \nGET /h [] tok\nmissing ok=false\n"'
    if "$NEXAC" "$WORK/net.nxa" --wasm -o "$WORK/net.js" > "$WORK/net.log" 2>&1; then
        got=$(drive "$WORK/net.html" "$WORK/net.json" --serve $port)
        if [ "$got" = "$want" ]; then
            passes=$((passes + 1))
        else
            echo "FAIL browser http: GET, POST, a header and a 404 from the page"
            printf '%s\n' "$got" | head -5 | sed 's/^/  /'
            fails=$((fails + 1))
        fi
    else
        echo "FAIL browser http: the build failed"; fails=$((fails + 1))
    fi

    cat > "$WORK/inp.nxa" <<'EOF'
#include <std/gfx>
#include <std/io>
#include <std/time>
fn main() {
    gfx.open("input", 200, 120, 2);
    let wheel = 0;
    let typed = "";
    let last = "";
    let n = 0;
    while (gfx.closed() != 1 && n < 100000) {
        gfx.poll();
        n++;
        wheel = wheel + gfx.wheel();
        typed = typed + gfx.typed();
        let s = f"m={gfx.mouse_x()},{gfx.mouse_y()} left={gfx.mouse("left")} a={gfx.key("a")} wheel={wheel} typed={typed}";
        if (s != last) { io.println(s); last = s; }
        gfx.clear(20, 20, 60);
        gfx.present();
        time.sleep(16);
    }
}
EOF
    cat > "$WORK/inp.json" <<'EOF'
[["wait",1500],["canvas"],
 ["move",100,100],["wait",150],["down",100,100],["wait",150],["up",100,100],["wait",150],
 ["wheel",100,100,-120],["wait",150],
 ["keydown","a","KeyA",65,"a"],["wait",150],["keyup","a","KeyA",65],["wait",150]]
EOF
    if "$NEXAC" "$WORK/inp.nxa" --wasm -o "$WORK/inp.js" > "$WORK/inp.log" 2>&1; then
        got=$(drive "$WORK/inp.html" "$WORK/inp.json" | sed 's/^console\.log: //' | tr '\n' '|')
        want="m=0,0 left=0 a=0 wheel=0 typed=|m=50,50 left=0 a=0 wheel=0 typed=|m=50,50 left=1 a=0 wheel=0 typed=|m=50,50 left=0 a=0 wheel=0 typed=|m=50,50 left=0 a=0 wheel=1 typed=|m=50,50 left=0 a=1 wheel=1 typed=a|m=50,50 left=0 a=0 wheel=1 typed=a|"
        if [ "$got" = "$want" ]; then
            passes=$((passes + 1))
        else
            echo "FAIL browser gfx: mouse, button, wheel, key and typed text on the canvas"
            printf '%s\n' "$got" | tr '|' '\n' | head -8 | sed 's/^/  /'
            fails=$((fails + 1))
        fi
    else
        echo "FAIL browser gfx: the build failed"; fails=$((fails + 1))
    fi
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
