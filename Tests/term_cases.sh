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

# flags_sliced: termios flags cost nothing until named. A colour program has
# none of them; one that only names constants has those constants and no flag
# functions.
if grep -q '__nexa_tf_\|__nexa_term_get_flag' "$WORK/style.cpp"; then
    echo "FAIL flags_sliced: a colour-only program carries the termios flags"; fails=$((fails + 1))
else
    printf '#include <std/io>\n#include <std/term>\nfn main() { io.println(term.ICANON | term.ECHO); }\n' > "$WORK/k.nxa"
    if ! "$NEXAC" "$WORK/k.nxa" --source "$WORK/k.cpp" > "$WORK/log" 2>&1; then
        echo "FAIL flags_sliced: could not transpile term.ICANON"; sed 's/^/  /' "$WORK/log"; fails=$((fails + 1))
    elif grep -q '__nexa_term_get_flag' "$WORK/k.cpp" || grep -q '__nexa_tf_ISIG' "$WORK/k.cpp" ||
         ! grep -q '__nexa_tf_ICANON = (int)(ICANON)' "$WORK/k.cpp"; then
        echo "FAIL flags_sliced: constants-only program carries more (or less) than its two constants"; fails=$((fails + 1))
    else
        echo "ok flags_sliced: only the named constants, no flag functions"
    fi
fi

# flags: on a real terminal (a pty from script(1)), clearing ICANON and ECHO
# takes, VMIN reads back, and restore() puts the terminal back. Off one, every
# read is -1 and every write 0. POSIX only; the Windows console mode is checked
# by hand.
if [ -z "$CXX" ]; then
    echo "skip flags: no C++ compiler (set NEXA_CXX to force one)"
elif [ "$IS_WINDOWS" = 1 ]; then
    echo "skip flags: no pty to drive the Windows console from here"
else
    cat > "$WORK/flags.nxa" <<'NXA'
#include <std/io>
#include <std/term>
fn main() {
    let l = term.c_lflag();
    if (l < 0) {
        io.println("off ", term.c_iflag(), " ", term.set_c_lflag(0), " ", term.c_cc(term.VMIN), " ", term.restore());
        return;
    }
    io.println("set ", term.set_c_lflag(l & ~(term.ICANON | term.ECHO)));
    io.println("raw ", (term.c_lflag() & (term.ICANON | term.ECHO)) == 0);
    io.println("vmin ", term.set_c_cc(term.VMIN, 1), " ", term.c_cc(term.VMIN));
    io.println("iflag ", term.set_c_iflag(term.c_iflag() & ~(term.ISTRIP | term.ICRNL)),
               " ", (term.c_iflag() & term.ISTRIP) == 0);
    io.println("cs8 ", (term.c_cflag() & term.CSIZE) == term.CS8);
    io.println("restore ", term.restore(), " ", term.c_lflag() == l);
}
NXA
    if ! "$NEXAC" "$WORK/flags.nxa" -o "$WORK/flags" > "$WORK/log" 2>&1; then
        echo "FAIL flags: could not build"; sed 's/^/  /' "$WORK/log" | tail -8; fails=$((fails + 1))
    else
        off=$("$WORK/flags" < /dev/null 2>&1 | tr -d '\r')
        if [ "$off" = "off -1 0 -1 1" ]; then
            echo "ok flags_off_tty: reads -1, writes 0"
        else
            echo "FAIL flags_off_tty: got [$off]"; fails=$((fails + 1))
        fi
        if ! command -v script > /dev/null 2>&1; then
            echo "skip flags_pty: no script(1) for a pty"
        else
            script -qec "$WORK/flags" /dev/null < /dev/null > "$WORK/pty.txt" 2>&1
            want='set 1
raw true
vmin 1 1
iflag 1 true
cs8 true
restore 1 true'
            got=$(tr -d '\r' < "$WORK/pty.txt")
            if [ "$got" = "$want" ]; then
                echo "ok flags_pty: set, read back and restored on a pty"
            else
                echo "FAIL flags_pty: got [$got]"; fails=$((fails + 1))
            fi
        fi
    fi
fi

if [ $fails -eq 0 ]; then
    echo "term ok"
    exit 0
fi
echo "term: $fails failure(s)"
exit 1
