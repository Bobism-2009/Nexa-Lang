#!/bin/sh
# NexaC and nexapkg are one binary under two names.
#
# Which program runs is decided by argv[0]: a symlink called `nexapkg` on Unix,
# a second copy called `nexapkg.exe` on Windows. That is the whole mechanism,
# and it is invisible until it is wrong -- a binary that fails the test just
# carries on being the compiler, and answers every package command by printing
# the compiler's help. No error, no exit code, nothing to notice except that
# `nexapkg install` did not install anything.
#
# Which is exactly what happened: the Windows arm required twelve characters
# before it would look at the .exe form, and "nexapkg.exe" is eleven, so the
# alias never once dispatched there. Unix was fine the whole time, which is
# why it survived -- the symlink has no extension and matched a different test.
#
# So this checks the dispatch by the only thing that decides it: the name the
# binary is invoked by. Copies are used rather than symlinks so the same test
# runs on Windows, where a symlink needs a privilege an ordinary build does not
# have.
#
#   sh Tests/nexapkg_alias_cases.sh          (from the repo root)
#
# Usage: Tests/nexapkg_alias_cases.sh [path-to-NexaC]

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

# Windows needs the extension to be found at all; Unix does not use one.
case "$NEXAC" in
    *.exe) EXT=".exe" ;;
    *)     EXT="" ;;
esac

# expect_dispatch <label> <name-to-install-as> <grep-ERE the output must match>
expect_dispatch() {
    label=$1
    name=$2
    want=$3

    cp "$NEXAC" "$WORK/$name" || { echo "FAIL $label: could not copy"; fails=$((fails + 1)); return; }
    chmod +x "$WORK/$name" 2>/dev/null
    out=$("$WORK/$name" 2>&1 | head -5)
    if ! printf '%s' "$out" | grep -qE "$want"; then
        echo "FAIL $label: run as '$name', output does not match /$want/"
        printf '%s\n' "$out" | sed 's/^/  /' | head -3
        fails=$((fails + 1))
    fi
}

# The package manager, by its own name.
expect_dispatch "nexapkg"        "nexapkg$EXT"   'nexapkg - Nexa package manager'

# And still the compiler by its. With no arguments the compiler prints its
# usage rather than its help, which is the line to look for.
expect_dispatch "NexaC"          "NexaC$EXT"     'Usage: NexaC init'
expect_dispatch "nexac"          "nexac$EXT"     'Usage: NexaC init'

# A name that merely starts with nexapkg is not nexapkg: the alias is an exact
# name, not a prefix, so a build artefact called nexapkg-old.exe stays the
# compiler rather than quietly becoming the package manager.
expect_dispatch "nexapkg-old"    "nexapkg-old$EXT" 'Usage: NexaC init'

# The subcommand route works whatever the binary is called, and is what the
# documentation offers when the alias is not installed.
out=$("$NEXAC" nexapkg 2>&1 | head -3)
if ! printf '%s' "$out" | grep -q 'nexapkg - Nexa package manager'; then
    echo "FAIL subcommand: NexaC nexapkg did not reach the package manager"
    printf '%s\n' "$out" | sed 's/^/  /' | head -3
    fails=$((fails + 1))
fi

# The package manager has to actually be running, not just printing a banner:
# a command with no manifest says so, where the compiler would have complained
# about a missing .nxa instead.
cp "$NEXAC" "$WORK/nexapkg$EXT" 2>/dev/null
out=$(cd "$WORK" && "./nexapkg$EXT" list 2>&1 | head -3)
if ! printf '%s' "$out" | grep -q 'nexapkg'; then
    echo "FAIL nexapkg list: not the package manager's answer"
    printf '%s\n' "$out" | sed 's/^/  /' | head -3
    fails=$((fails + 1))
fi

if [ $fails -eq 0 ]; then
    echo "nexapkg alias ok"
    exit 0
fi
echo "nexapkg alias: $fails failure(s)"
exit 1
