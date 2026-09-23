#!/bin/sh
# --wasm output shape.
#
# What a --wasm build leaves on disk is the whole point of the flag, and it is
# the part that is easy to get wrong without noticing: a .js loader on its own
# is not something a person can open, and a separately emitted .wasm cannot be
# fetched over file:// at all -- a browser refuses the request, so a page built
# that way is silently broken until it is served. So the default bakes the
# .wasm into the .js and writes a page, and --wasm-split is the deliberate
# opt-out for when you are serving it anyway.
#
#   flags     The argument rules, which need no toolchain at all: --wasm-split
#             is meaningless without --wasm and is refused by name. Runs
#             everywhere.
#
#   shape     Build for real and check what appears. Default: a .html and a .js
#             and NO sibling .wasm, with nothing in the loader still reaching
#             for one. Split: all three, with the loader fetching the .wasm.
#             Both: the page points at its own loader by relative name, so the
#             pair stays movable.
#
#   shell     The page is a canvas for a std/gfx program and a text console
#             otherwise, and in both cases its inline script must parse -- a
#             broken shell fails in a browser console nobody is watching, not
#             at build time. Checked with node when node is around.
#
# The build layers need em++ and are skipped without it. They are the slowest
# thing in the tree when it is present -- four Emscripten links -- so on a
# machine with a toolchain this is most of what `make test` spends its time
# on. That is the price of checking the output shape for real rather than
# trusting the flag was threaded through.
#
#   sh Tests/wasm_cases.sh           (from the repo root)
#
# Usage: Tests/wasm_cases.sh [path-to-NexaC]

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM

fails=0
skips=0

# --- flags layer ------------------------------------------------------------

printf '#include <std/io>\nfn main() {\n    io.println("x");\n}\n' > "$WORK/h.nxa"

out=$("$NEXAC" "$WORK/h.nxa" --wasm-split -o "$WORK/h" 2>&1)
if [ $? -eq 0 ]; then
    echo "FAIL flags: --wasm-split without --wasm exited 0"
    fails=$((fails + 1))
elif ! printf '%s' "$out" | grep -q -- '--wasm-split only means anything with --wasm'; then
    echo "FAIL flags: --wasm-split without --wasm was not diagnosed by name"
    printf '%s\n' "$out" | sed 's/^/  /' | head -3
    fails=$((fails + 1))
fi

# --- build layers -----------------------------------------------------------

have_emcc=0
if command -v em++ >/dev/null 2>&1; then
    have_emcc=1
elif [ -x "$HOME/emsdk/upstream/emscripten/em++" ] || [ -x "$HOME/emsdk/upstream/emscripten/em++.exe" ]; then
    have_emcc=1
fi

if [ $have_emcc -eq 0 ]; then
    echo "skip shape/shell: no em++ (install Emscripten, or set EMSDK)"
    skips=$((skips + 1))
else
    # default: one loader with the module inside it, and a page
    if "$NEXAC" "$WORK/h.nxa" --wasm -o "$WORK/one" > "$WORK/one.log" 2>&1; then
        [ -f "$WORK/one.html" ] || { echo "FAIL shape: --wasm wrote no .html"; fails=$((fails + 1)); }
        [ -f "$WORK/one.js" ]   || { echo "FAIL shape: --wasm wrote no .js"; fails=$((fails + 1)); }
        if [ -f "$WORK/one.wasm" ]; then
            echo "FAIL shape: --wasm left a separate .wasm; it should be baked into the .js"
            fails=$((fails + 1))
        fi
        # The loader must not still be reaching for a file that is not there.
        if grep -a -q 'one\.wasm' "$WORK/one.js" 2>/dev/null; then
            echo "FAIL shape: the baked loader still refers to one.wasm"
            fails=$((fails + 1))
        fi
        if ! grep -q 'src="one\.js"' "$WORK/one.html"; then
            echo "FAIL shape: the page does not point at its own loader by relative name"
            fails=$((fails + 1))
        fi
    else
        echo "FAIL shape: --wasm build failed"
        sed 's/^/  /' "$WORK/one.log" | tail -5
        fails=$((fails + 1))
    fi

    # split: three files, and the loader does fetch the module
    if "$NEXAC" "$WORK/h.nxa" --wasm --wasm-split -o "$WORK/three" > "$WORK/three.log" 2>&1; then
        for f in html js wasm; do
            [ -f "$WORK/three.$f" ] || { echo "FAIL shape: --wasm-split wrote no .$f"; fails=$((fails + 1)); }
        done
        if ! grep -a -q 'three\.wasm' "$WORK/three.js" 2>/dev/null; then
            echo "FAIL shape: the split loader does not reference three.wasm"
            fails=$((fails + 1))
        fi
        if ! grep -q 'src="three\.js"' "$WORK/three.html"; then
            echo "FAIL shape: the split page does not point at its own loader"
            fails=$((fails + 1))
        fi
        # --split is the documented alias; it has to mean the same thing.
        if "$NEXAC" "$WORK/h.nxa" --wasm --split -o "$WORK/alias" > /dev/null 2>&1; then
            [ -f "$WORK/alias.wasm" ] || { echo "FAIL shape: --split did not split"; fails=$((fails + 1)); }
        else
            echo "FAIL shape: --split (alias) build failed"
            fails=$((fails + 1))
        fi
    else
        echo "FAIL shape: --wasm-split build failed"
        sed 's/^/  /' "$WORK/three.log" | tail -5
        fails=$((fails + 1))
    fi

    # the two shells, and that the loader will actually adopt what they set
    printf '#include <std/gfx>\nfn main() {\n    gfx.open("w", 8, 8, 1);\n    gfx.present();\n}\n' > "$WORK/g.nxa"
    if "$NEXAC" "$WORK/g.nxa" --wasm -o "$WORK/g" > "$WORK/g.log" 2>&1; then
        grep -q '<canvas' "$WORK/g.html" || {
            echo "FAIL shell: a std/gfx page has no canvas"; fails=$((fails + 1)); }
    else
        echo "skip shell(gfx): std/gfx wasm build failed on this machine"
        skips=$((skips + 1))
    fi
    grep -q '<canvas' "$WORK/one.html" 2>/dev/null && {
        echo "FAIL shell: a console page has a canvas on it"; fails=$((fails + 1)); }
    grep -q 'nexa-out' "$WORK/one.html" 2>/dev/null || {
        echo "FAIL shell: a console page has nowhere for io.println to land"; fails=$((fails + 1)); }

    # The page sets Module before the loader runs; the loader only honours that
    # if it adopts a pre-existing Module and routes output through print.
    if ! grep -a -q 'typeof Module' "$WORK/one.js" 2>/dev/null; then
        echo "FAIL shell: the loader does not adopt a pre-declared Module, so the page's wiring is dead"
        fails=$((fails + 1))
    fi

    if command -v node >/dev/null 2>&1; then
        for page in one g; do
            [ -f "$WORK/$page.html" ] || continue
            sed -n '/^<script>$/,/^<\/script>$/p' "$WORK/$page.html" | sed '1d;$d' > "$WORK/$page.shell.js"
            if [ -s "$WORK/$page.shell.js" ] && ! node --check "$WORK/$page.shell.js" 2>"$WORK/$page.syn"; then
                echo "FAIL shell: $page.html inline script does not parse"
                sed 's/^/  /' "$WORK/$page.syn" | head -3
                fails=$((fails + 1))
            fi
        done
        # and the whole thing actually runs
        if [ -f "$WORK/one.js" ] && ! node "$WORK/one.js" > "$WORK/one.out" 2>&1; then
            echo "FAIL shape: node could not run the baked loader"
            sed 's/^/  /' "$WORK/one.out" | head -3
            fails=$((fails + 1))
        fi
    else
        echo "skip shell(parse): no node"
        skips=$((skips + 1))
    fi
fi

# --- report -----------------------------------------------------------------

if [ $fails -eq 0 ]; then
    if [ $skips -gt 0 ]; then
        echo "wasm ok ($skips layer(s) skipped: see above)"
    else
        echo "wasm ok"
    fi
    exit 0
fi
echo "wasm: $fails failure(s)"
exit 1
