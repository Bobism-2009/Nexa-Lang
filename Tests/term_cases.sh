#!/bin/sh
# std/term's raw-input side: term.raw, term.getkey, term.key_available.
#
# No test terminal exists, so the program's stdin is fed a byte stream and its
# named keys are checked. getkey reads stdin a byte at a time and decodes the
# escape sequences a terminal sends, so a pipe exercises exactly that decoding.
# POSIX only: the Windows reader is _getch(), which reads the console directly
# and cannot be driven from a pipe, so it is checked by hand, not here.
#
#   sliced   a term program that only prints colour carries none of the reader.
#   keys     a key stream decodes to the right names.
#
# Usage: Tests/term_cases.sh [path-to-NexaC]     (run from the repo root)

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

# sliced: colour only, no input code.
printf '#include <std/io>\n#include <std/term>\nfn main() { io.println(term.color("x", "red")); }\n' > "$WORK/style.nxa"
if ! "$NEXAC" "$WORK/style.nxa" --source "$WORK/style.cpp" > "$WORK/log" 2>&1; then
    echo "FAIL sliced: NexaC could not transpile a term program"; sed 's/^/  /' "$WORK/log"; fails=$((fails + 1))
elif grep -q '__nexa_term_getkey\|__nexa_term_raw\|termios\|_getch' "$WORK/style.cpp"; then
    echo "FAIL sliced: a colour-only program carries the raw-input reader"; fails=$((fails + 1))
else
    echo "ok sliced: colour only, no reader"
fi

CXX="${NEXA_CXX:-}"
if [ -z "$CXX" ]; then
    for c in clang++ g++ c++; do
        if command -v "$c" > /dev/null 2>&1; then CXX=$c; break; fi
    done
fi

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) IS_WINDOWS=1 ;;
    *) IS_WINDOWS=0 ;;
esac
if [ -z "$CXX" ]; then
    echo "skip keys: no C++ compiler (set NEXA_CXX to force one)"
elif [ "$IS_WINDOWS" = 1 ]; then
    echo "skip keys: the Windows reader cannot be driven from a pipe"
else
    cat > "$WORK/keys.nxa" <<'NXA'
#include <std/io>
#include <std/term>
fn main() {
    term.raw(true);
    defer term.raw(false);
    for (i, 32) {
        let k = term.getkey();
        if (k == "") { io.println("[eof]"); break; }
        io.println(k);
        if (k == "q") { break; }
    }
}
NXA
    if ! "$NEXAC" "$WORK/keys.nxa" -o "$WORK/keys" > "$WORK/log" 2>&1; then
        echo "FAIL keys: could not build the key reader"; sed 's/^/  /' "$WORK/log" | tail -8; fails=$((fails + 1))
    else
        # a, arrows, enter, tab, space, backspace, home, end, pageup, pagedown,
        # delete, insert, ctrl-a, ctrl-c, q. A lone ESC is left out: over a pipe
        # it cannot be told from the start of a sequence (a real terminal uses
        # timing), so it is not something this test can pin down.
        printf 'a\033[A\033[B\033[D\033[C\r\t \177\033[H\033[F\033[5~\033[6~\033[3~\033[2~\001\003q' > "$WORK/in.bin"
        "$WORK/keys" < "$WORK/in.bin" > "$WORK/out.txt" 2>&1
        cat > "$WORK/want.txt" <<'WANT'
a
up
down
left
right
enter
tab
space
backspace
home
end
pageup
pagedown
delete
insert
ctrl-a
ctrl-c
q
WANT
        if diff -u --strip-trailing-cr "$WORK/want.txt" "$WORK/out.txt" > "$WORK/diff.txt"; then
            echo "ok keys: the stream decoded to the right names"
        else
            echo "FAIL keys: a key decoded differently"; sed 's/^/  /' "$WORK/diff.txt"; fails=$((fails + 1))
        fi
    fi
fi

if [ $fails -eq 0 ]; then
    echo "term ok"
    exit 0
fi
echo "term: $fails failure(s)"
exit 1
