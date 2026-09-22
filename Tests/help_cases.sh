#!/bin/sh
# NexaC --help cover.
#
# The help NexaC prints is a SUMMARY of the language, not a second copy of it.
# SYNTAX/*.txt says what a call means and is free to take four hundred lines
# doing it; a help topic says what exists and has to fit on a screen. That
# split is the only reason --help is maintainable at this size -- and it is
# also exactly how the old numbered pages rotted, because nothing checked that
# the summary still named everything the language had.
#
# So this suite checks the half a summary is responsible for: coverage of
# NAMES.
#
#   modules   Every module with a section in SYNTAX/Modules.txt has a topic,
#             and every call documented in that section is named on its topic's
#             page. A module that grows a call fails here until --help learns
#             the name. Nothing is checked about what the page SAYS the call
#             does; that is SYNTAX/'s job, and duplicating it is the failure
#             mode this whole rework exists to undo.
#
#   index     Bare `NexaC --help` names every topic, so no page can become
#             unreachable by being left out of the index.
#
#   lookup    The ways of asking that are not a topic name: a qualified call
#             (gfx.blit), a retired include (std/http), and one of the nine
#             numbered pages --help used to have. Each has to answer by naming
#             the topic that replaced it rather than failing.
#
#   unknown   A name that is nothing at all exits non-zero and suggests the
#             near miss, instead of printing the index as though it had
#             understood.
#
# Never invokes the C++ compiler, so it runs anywhere NexaC itself runs.
#
#   sh Tests/help_cases.sh           (from the repo root)
#
# Usage: Tests/help_cases.sh [path-to-NexaC]

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")

SUITE=$(cd "$(dirname "$0")" && pwd)
SPEC="$SUITE/../SYNTAX/Modules.txt"
if [ ! -f "$SPEC" ]; then
    echo "FAIL: $SPEC not found; run this from the repo root"
    exit 1
fi

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM

fails=0

# --- modules layer ----------------------------------------------------------

# The call prefixes a module owns. Every module but std/network owns the one
# that matches its name; std/network is the whole wire, so it owns three.
# A section quotes other modules freely -- the std/gfx pages show os.load
# feeding gfx.decode -- so a call only counts against the section that owns
# its prefix.
prefixes_for() {
    case $1 in
        network) echo "http tcp udp" ;;
        inline)  echo "" ;;
        *)       echo "$1" ;;
    esac
}

# Calls documented in one module's section of SYNTAX/Modules.txt.
#
# Two spellings count. The plain one is `prefix.name(`. The other is the
# alias run the spec uses to put two names on one line --
# `gfx.width() / height() / scale()`, `os.environ() / os.env()` -- where the
# trailing names carry no prefix and would otherwise go unchecked.
calls_in_section() {
    mod=$1
    want=$(prefixes_for "$mod")
    [ -n "$want" ] || return 0

    awk -v mod="$mod" -v want="$want" '
        BEGIN { n = split(want, w, " ") }
        /^#include <std\// {
            sec = $0
            sub(/^#include <std\//, "", sec)
            sub(/>.*$/, "", sec)
            inmod = (sec == mod)
            next
        }
        /^[^ \t]/ { inmod = 0 }
        !inmod { next }
        {
            line = $0
            content = line
            sub(/^[ 	]+/, "", content)

            # A definition line opens with the call it defines. Prose that
            # merely mentions one -- "distinct from time.sleep(ms)" -- is
            # indented under a definition and does not start with a prefix, so
            # it contributes nothing. Without this the summary would be held
            # to names the language does not have.
            def_line = 0
            for (i = 1; i <= n; i++) {
                if (index(content, w[i] ".") == 1) def_line = 1
            }
            if (!def_line) next

            for (i = 1; i <= n; i++) {
                p = w[i] "[.]"
                rest = line
                while (match(rest, p "[a-z_0-9]+")) {
                    print substr(rest, RSTART + length(w[i]) + 1, RLENGTH - length(w[i]) - 1)
                    rest = substr(rest, RSTART + RLENGTH)
                }
            }
            # alias runs: "gfx.width() / height() / scale()" puts two more
            # names on the line with no prefix of their own.
            rest = line
            while (match(rest, "/ *[a-z_0-9]+[(]")) {
                a = substr(rest, RSTART, RLENGTH)
                sub(/^\/ */, "", a)
                sub(/\($/, "", a)
                print a
                rest = substr(rest, RSTART + RLENGTH)
            }
        }
    ' "$SPEC" | sort -u
}

# Module sections present in the spec, in the order they appear.
modules=$(grep '^#include <std/' "$SPEC" | sed 's|^#include <std/||; s|>.*$||')

for mod in $modules; do
    if ! "$NEXAC" --help "std/$mod" > "$WORK/page.txt" 2>&1; then
        echo "FAIL help std/$mod: no such topic (SYNTAX/Modules.txt documents it)"
        fails=$((fails + 1))
        continue
    fi
    if [ ! -s "$WORK/page.txt" ]; then
        echo "FAIL help std/$mod: topic rendered empty"
        fails=$((fails + 1))
        continue
    fi

    missing=""
    for call in $(calls_in_section "$mod"); do
        if ! grep -q "\.$call\b" "$WORK/page.txt" && ! grep -q "\b$call\b" "$WORK/page.txt"; then
            missing="$missing $call"
        fi
    done
    if [ -n "$missing" ]; then
        echo "FAIL help std/$mod: page does not name:$missing"
        echo "     (documented in SYNTAX/Modules.txt; add them to include/Help.hpp)"
        fails=$((fails + 1))
    fi
done

# --- index layer ------------------------------------------------------------

"$NEXAC" --help > "$WORK/index.txt" 2>&1
for mod in $modules; do
    if ! grep -q "std/$mod" "$WORK/index.txt"; then
        echo "FAIL help index: std/$mod is not listed"
        fails=$((fails + 1))
    fi
done
for topic in core options nexapkg; do
    if ! grep -q "^  $topic" "$WORK/index.txt"; then
        echo "FAIL help index: $topic is not listed"
        fails=$((fails + 1))
    fi
done

# --- lookup layer -----------------------------------------------------------

# expect_help <label> <arg> <grep-ERE>
expect_help() {
    label=$1
    arg=$2
    want=$3

    "$NEXAC" --help "$arg" > "$WORK/out.txt" 2>&1
    if ! grep -qE "$want" "$WORK/out.txt"; then
        echo "FAIL $label: --help $arg does not match /$want/"
        sed 's/^/  /' "$WORK/out.txt" | head -5
        fails=$((fails + 1))
    fi
}

expect_help "qualified call" gfx.blit "gfx\.blit is in std/gfx"
expect_help "qualified call" http.get "http\.get is in std/network"
expect_help "bare call"      typed    "std/gfx"
expect_help "retired include" std/http "std/http was retired"
expect_help "retired include" std/tcp  "std/network"
expect_help "numbered page"  --page8  "named, not numbered"
expect_help "numbered page"  2        "NexaC --help core"
expect_help "alias"          gfx      "^std/gfx"
expect_help "alias"          net      "^std/network"
expect_help "bracket form"   "<std/io>" "^std/io"

# --- unknown layer ----------------------------------------------------------

if "$NEXAC" --help std/gxf > "$WORK/out.txt" 2>&1; then
    echo "FAIL help unknown: --help std/gxf exited 0"
    fails=$((fails + 1))
elif ! grep -q "std/gfx" "$WORK/out.txt"; then
    echo "FAIL help unknown: --help std/gxf does not suggest std/gfx"
    sed 's/^/  /' "$WORK/out.txt"
    fails=$((fails + 1))
fi

if "$NEXAC" --help zzzznotathing > "$WORK/out.txt" 2>&1; then
    echo "FAIL help unknown: a nonsense topic exited 0"
    fails=$((fails + 1))
fi

# --- report -----------------------------------------------------------------

if [ $fails -eq 0 ]; then
    echo "help ok"
    exit 0
fi
echo "help: $fails failure(s)"
exit 1
