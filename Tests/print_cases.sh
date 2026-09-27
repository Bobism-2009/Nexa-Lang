#!/bin/sh
# What io.print / io.println write, byte for byte, where a .expected file cannot
# say it: a string with a NUL in it is written whole ("a\0b" is three characters;
# printing stopped at the \0 once), in every way a string reaches a print.
#
# Usage: Tests/print_cases.sh [path-to-NexaC]     (run from the repo root)

set -u

NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*) ;;
    *) NEXAC="$(pwd)/$NEXAC" ;;
esac
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexaprint)
trap 'rm -rf "$WORK"' EXIT

cat > "$WORK/zero.nxa" <<'NXA'
#include <std/io>

fn main() {
    let s = "a\0b";
    io.println(s);
    io.print(s);
    io.println("|", s, "|");
    io.println(s + "c");
}
NXA
if ! "$NEXAC" "$WORK/zero.nxa" -o "$WORK/zero" >"$WORK/build.log" 2>&1; then
    echo "FAIL print: build failed"; tail -3 "$WORK/build.log"; exit 1
fi
EXE="$WORK/zero"
[ -f "$EXE.exe" ] && EXE="$EXE.exe"
got=$("$EXE" | tr -d '\r' | od -An -c | tr -s ' \n' ' ')
want=' a \0 b \n a \0 b | a \0 b | \n a \0 b c \n '
if [ "$got" = "$want" ]; then
    echo "print ok"
else
    echo "FAIL print: a string with a NUL was not written whole"
    echo "  want:$want"
    echo "  got: $got"
    exit 1
fi
