#!/bin/sh
# Two places NexaC once destroyed files it was never pointed at.
#
#   file.remove_all   followed a symbolic link (or a Windows junction) inside the
#                     tree and emptied the directory it pointed to. A link is
#                     removed itself; its target is left alone.
#
#   NexaC init        with no directory wrote main.nxa, nexapkg.json and
#                     .gitignore over whatever the current directory held.
#
# Neither can be a .nxa test: one needs a link made from outside the program, the
# other is the compiler's own command.
#
# Usage: Tests/keep_files_cases.sh [path-to-NexaC]     (run from the repo root)

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM
CR=$(printf '\r')

fails=0
pass() { echo "ok $1"; }
fail() {
    echo "FAIL $1: $2"
    [ $# -ge 3 ] && echo "  got: $3"
    fails=$((fails + 1))
}

EXE=""
WIN=0
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) EXE=".exe"; WIN=1 ;;
esac

# A link to a directory: a junction on Windows (a symlink there needs a privilege
# an ordinary account does not have), a symlink everywhere else.
link_dir() {
    if [ "$WIN" = 1 ]; then
        cmd //c mklink //J "$(cygpath -w "$2")" "$(cygpath -w "$1")" >/dev/null 2>&1
    else
        ln -s "$1" "$2"
    fi
}

# ---- file.remove_all ---------------------------------------------------------

cat > "$WORK/rm.nxa" <<'NXA'
#include <std/io>
#include <std/file>

fn main(args: []string) {
    io.println("removed=", file.remove_all(args[1]), " left=", file.exists(args[1]));
}
NXA

if "$NEXAC" "$WORK/rm.nxa" -o "$WORK/rm$EXE" >"$WORK/rm.log" 2>&1; then
    cd "$WORK" || exit 1

    # A link inside the tree.
    mkdir -p keep tree/sub
    echo precious > keep/precious.txt
    echo x > tree/sub/x.txt
    if link_dir "$WORK/keep" "$WORK/tree/link"; then
        got=$(./rm$EXE tree | tr -d "$CR")
        if [ "$got" = "removed=1 left=0" ]; then pass remove_all_tree; else fail remove_all_tree "tree with a link in it" "$got"; fi
        if [ -f keep/precious.txt ]; then pass remove_all_keeps_target; else fail remove_all_keeps_target "the link's target was emptied"; fi

        # The link itself, named with and without a trailing separator.
        link_dir "$WORK/keep" "$WORK/direct"
        got=$(./rm$EXE direct | tr -d "$CR")
        if [ "$got" = "removed=1 left=0" ] && [ -f keep/precious.txt ]; then pass remove_all_link; else fail remove_all_link "a link named directly" "$got"; fi
        link_dir "$WORK/keep" "$WORK/slashed"
        got=$(./rm$EXE slashed/ | tr -d "$CR")
        if [ "$got" = "removed=1 left=0" ] && [ -f keep/precious.txt ]; then pass remove_all_link_slash; else fail remove_all_link_slash "a link named with a trailing /" "$got"; fi
    else
        echo "skip remove_all link cases: cannot make a link here"
    fi

    # A link whose target is gone is still something to remove.
    if [ "$WIN" = 0 ]; then
        mkdir -p dangle
        ln -s "$WORK/no_such_place" dangle/gone
        got=$(./rm$EXE dangle | tr -d "$CR")
        if [ "$got" = "removed=1 left=0" ] && [ ! -e dangle ] && [ ! -L dangle/gone ]; then pass remove_all_dangling; else fail remove_all_dangling "a dangling link" "$got"; fi
    fi

    # An ordinary tree and a missing path behave as before.
    mkdir -p plain/a/b
    echo y > plain/a/b/y.txt
    got=$(./rm$EXE plain/ | tr -d "$CR")
    if [ "$got" = "removed=1 left=0" ]; then pass remove_all_plain; else fail remove_all_plain "ordinary tree" "$got"; fi
    got=$(./rm$EXE never_was | tr -d "$CR")
    if [ "$got" = "removed=1 left=0" ]; then pass remove_all_missing; else fail remove_all_missing "missing path" "$got"; fi
    cd - >/dev/null || exit 1
else
    fail remove_all_build "did not compile" "$(tail -3 "$WORK/rm.log")"
fi

# ---- NexaC init --------------------------------------------------------------

# An existing project is refused, and nothing in it changes.
mkdir -p "$WORK/proj"
printf 'mine\n' > "$WORK/proj/main.nxa"
printf '{ "name": "p", "dependencies": { "a": "b/c" } }\n' > "$WORK/proj/nexapkg.json"
out=$(cd "$WORK/proj" && "$NEXAC" init 2>&1); rc=$?
if [ "$rc" -ne 0 ] && echo "$out" | grep -q "already a Nexa project"; then pass init_refuses_project; else fail init_refuses_project "exit $rc" "$out"; fi
if [ "$(cat "$WORK/proj/main.nxa")" = "mine" ] && grep -q '"a": "b/c"' "$WORK/proj/nexapkg.json" && [ ! -e "$WORK/proj/.gitignore" ]; then
    pass init_project_untouched
else
    fail init_project_untouched "files changed"
fi

# A directory with sources but no manifest gets the missing files only.
mkdir -p "$WORK/loose"
printf 'mine\n' > "$WORK/loose/main.nxa"
printf 'secret\n' > "$WORK/loose/.gitignore"
out=$(cd "$WORK/loose" && "$NEXAC" init 2>&1); rc=$?
if [ "$rc" -eq 0 ] && [ "$(cat "$WORK/loose/main.nxa")" = "mine" ] && [ "$(cat "$WORK/loose/.gitignore")" = "secret" ] && [ -f "$WORK/loose/nexapkg.json" ]; then
    pass init_keeps_existing
else
    fail init_keeps_existing "exit $rc" "$out"
fi
if echo "$out" | grep -q "main.nxa .*kept"; then pass init_says_kept; else fail init_says_kept "no 'kept' line" "$out"; fi

# An empty directory and a named new one are scaffolded in full.
mkdir -p "$WORK/empty"
out=$(cd "$WORK/empty" && "$NEXAC" init 2>&1); rc=$?
if [ "$rc" -eq 0 ] && [ -f "$WORK/empty/main.nxa" ] && [ -f "$WORK/empty/nexapkg.json" ] && [ -f "$WORK/empty/.gitignore" ]; then pass init_empty; else fail init_empty "exit $rc" "$out"; fi
out=$(cd "$WORK" && "$NEXAC" init fresh 2>&1); rc=$?
if [ "$rc" -eq 0 ] && grep -q '"name": "fresh"' "$WORK/fresh/nexapkg.json" && [ -f "$WORK/fresh/main.nxa" ]; then pass init_named; else fail init_named "exit $rc" "$out"; fi
out=$(cd "$WORK" && "$NEXAC" init proj 2>&1); rc=$?
if [ "$rc" -ne 0 ] && echo "$out" | grep -q "not empty"; then pass init_named_not_empty; else fail init_named_not_empty "exit $rc" "$out"; fi

echo
if [ "$fails" -eq 0 ]; then
    echo "keep_files_cases: all passed"
    exit 0
fi
echo "keep_files_cases: $fails failed"
exit 1
