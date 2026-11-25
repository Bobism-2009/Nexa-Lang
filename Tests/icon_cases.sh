#!/bin/sh
# nexapkg.json "icon": a .ico that a Windows build puts in the program.
#
#   windows    A Windows build (on Windows, or --win where a cross toolchain is) is
#              larger by about the .ico, still runs, and leaves no .rc or object behind.
#   elsewhere  A Linux or macOS program has no icon in the file: the same bytes as
#              the same program with no icon set.
#   missing    An icon that is not there, or not a .ico, is an error on every
#              platform, so a typo shows on the author's machine.
#
# Usage: Tests/icon_cases.sh [path-to-NexaC]     (run from the repo root)

set -u

NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*|[A-Za-z]:*) ;;
    *) NEXAC="$(pwd)/$NEXAC" ;;
esac
ICO="$(pwd)/Logo/nexa.ico"
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexaicon)
trap 'rm -rf "$WORK"' EXIT
pass=0
fail=0
ok() { echo "ok   $1"; pass=$((pass + 1)); }
bad() { echo "FAIL $1: $2"; fail=$((fail + 1)); }

mkdir -p "$WORK/app/src" "$WORK/plain"
cp "$ICO" "$WORK/app/app.ico"
cat > "$WORK/app/src/main.nxa" <<'NXA'
#include <std/io>
fn main() {
    io.println("icon ok");
}
NXA
cp "$WORK/app/src/main.nxa" "$WORK/plain/main.nxa"
printf '{\n  "name": "app",\n  "icon": "app.ico"\n}\n' > "$WORK/app/nexapkg.json"

case "$(uname -s 2>/dev/null)" in
    MINGW* | MSYS* | CYGWIN*) host=windows ;;
    *) host=other ;;
esac

if [ "$host" = windows ]; then
    winflag=""
else
    winflag="--win"
fi

# Windows: on Windows itself, or cross-built where a MinGW toolchain is installed.
if [ "$host" = windows ] || "$NEXAC" "$WORK/plain/main.nxa" --win -o "$WORK/plain/probe.exe" >/dev/null 2>&1; then
    if "$NEXAC" "$WORK/app/src/main.nxa" $winflag -o "$WORK/app/with.exe" >"$WORK/b1.log" 2>&1 &&
       "$NEXAC" "$WORK/plain/main.nxa" $winflag -o "$WORK/plain/without.exe" >"$WORK/b2.log" 2>&1; then
        with=$(wc -c < "$WORK/app/with.exe" | tr -d ' ')
        without=$(wc -c < "$WORK/plain/without.exe" | tr -d ' ')
        ico=$(wc -c < "$ICO" | tr -d ' ')
        if grep -q "no windres" "$WORK/b1.log"; then
            echo "skip windows: no windres here ($(grep -m1 windres "$WORK/b1.log"))"
        elif [ $((with - without)) -ge "$ico" ]; then
            ok "windows icon ($without -> $with bytes, the .ico is $ico)"
        else
            bad windows "with the icon $with bytes, without $without: the $ico-byte icon is not in it"
        fi
        if [ "$host" = windows ]; then
            out=$("$WORK/app/with.exe" | tr -d '\r')
            if [ "$out" = "icon ok" ]; then ok "windows runs"; else bad "windows runs" "printed [$out]"; fi
        fi
        left=$(ls "$WORK/app/src" "$WORK/app" | grep -c -E '\.rc$|\.icon\.o$')
        if [ "$left" = 0 ]; then ok "windows leaves nothing behind"; else bad "windows leaves nothing behind" "$(ls "$WORK/app/src")"; fi
    else
        bad windows "build failed: $(tail -3 "$WORK/b1.log" "$WORK/b2.log")"
    fi
else
    echo "skip windows: no Windows toolchain for --win here"
fi

if [ "$host" = other ]; then
    if "$NEXAC" "$WORK/app/src/main.nxa" -o "$WORK/app/native" >"$WORK/b3.log" 2>&1 &&
       "$NEXAC" "$WORK/plain/main.nxa" -o "$WORK/plain/native" >"$WORK/b4.log" 2>&1; then
        if cmp -s "$WORK/app/native" "$WORK/plain/native"; then
            ok "elsewhere the program is unchanged"
        else
            bad elsewhere "an icon setting changed a $(uname -s) program"
        fi
    else
        bad elsewhere "build failed: $(tail -3 "$WORK/b3.log")"
    fi
fi

printf '{\n  "name": "app",\n  "icon": "nope.ico"\n}\n' > "$WORK/app/nexapkg.json"
if "$NEXAC" "$WORK/app/src/main.nxa" --source "$WORK/x.cpp" >"$WORK/e1.log" 2>&1; then
    bad missing "an icon that is not there built"
elif grep -q 'nexapkg.json "icon": no file' "$WORK/e1.log"; then
    ok "missing icon is an error"
else
    bad missing "said: $(tail -2 "$WORK/e1.log")"
fi
printf '{\n  "name": "app",\n  "icon": "app.png"\n}\n' > "$WORK/app/nexapkg.json"
if "$NEXAC" "$WORK/app/src/main.nxa" --source "$WORK/x.cpp" >"$WORK/e2.log" 2>&1; then
    bad "not ico" "a .png icon built"
elif grep -q 'it takes a .ico file' "$WORK/e2.log"; then
    ok "a .png is an error"
else
    bad "not ico" "said: $(tail -2 "$WORK/e2.log")"
fi

echo
echo "$pass passed, $fail failed"
[ "$fail" = 0 ]
