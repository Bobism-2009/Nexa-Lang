#!/bin/sh
# include/nexa_vk14_embed.inc against the headers it is a copy of.
#
# The Vulkan 1.4 renderer is written twice. Once as ordinary headers --
# nexa_vk14.hpp, which includes nexa_vk14_scene.hpp, which includes
# nexa_vk14_spv.inc -- and once inside the raw string in nexa_vk14_embed.inc,
# which is what NexaC actually writes into a program that draws in 3D.
#
# Nothing in the NexaC build includes the headers, so nothing makes the two
# agree: an edit to one is simply absent from the other, and the compiler
# cannot say so, because a raw string is not code to it. That has already
# happened once -- include/vk14/mesh.vert carried a debug probe the embedded
# SPIR-V never had.
#
# The embed is exactly the header with its two #includes expanded in place, so
# this rebuilds it and compares. The headers are the source; the embed is the
# copy, and this is what keeps it one.
#
#   embed   nexa_vk14_embed.inc is the headers, expanded.
#
#   sh Tests/vk14_embed_cases.sh                (from the repo root)
#   sh Tests/vk14_embed_cases.sh --write        rewrite the embed from the headers
#
# Takes a NexaC path, like the other suites, and ignores it: this compares
# files in the tree and builds nothing.

set -u

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
INC="$ROOT/include"
WRITE=0
for a in "$@"; do
    [ "$a" = "--write" ] && WRITE=1
done
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM

# One level of #include "nexa_vk14*", repeated until none is left. The chain is
# two deep; the loop does not care how deep it is.
expand() {
    cp "$1" "$WORK/e"
    i=0
    while grep -q '^#include "nexa_vk14' "$WORK/e"; do
        i=$((i + 1))
        if [ "$i" -gt 8 ]; then
            echo "FAIL embed: #include \"nexa_vk14...\" still nesting after 8 passes"
            exit 1
        fi
        awk -v inc="$INC" '
            /^#include "nexa_vk14/ {
                f = $0
                sub(/^#include "/, "", f)
                sub(/".*$/, "", f)
                path = inc "/" f
                while ((getline line < path) > 0) print line
                close(path)
                # The blank line the expansion leaves where the directive was,
                # which is what the embed holds.
                print ""
                next
            }
            { print }
        ' "$WORK/e" > "$WORK/e2"
        mv "$WORK/e2" "$WORK/e"
    done
}

expand "$INC/nexa_vk14.hpp"

# The body as the embed holds it: the header text, with the raw-string wrapper
# this file puts around it. The closing )NEXA_VK14" follows the last line, so
# the body carries no newline of its own at the end.
{
    echo 'inline std::string nexa_vk14_embed() {'
    echo 'return R"NEXA_VK14('
    cat "$WORK/e"
    echo ')NEXA_VK14";'
    echo '}'
} > "$WORK/want"

# Compared with line endings left out of it: the tree is CRLF and a checkout
# elsewhere may not be.
if diff -u --strip-trailing-cr "$WORK/want" "$INC/nexa_vk14_embed.inc" > "$WORK/d" 2>&1; then
    echo "ok embed: nexa_vk14_embed.inc matches the headers"
    echo "vk14_embed ok"
    exit 0
fi

if [ "$WRITE" = "1" ]; then
    # Written back with the endings the tree already uses, so the file does not
    # flip end to end -- see the note in Tests/README if this ever surprises you.
    if head -2 "$INC/nexa_vk14_embed.inc" | od -c | grep -q '\r'; then
        sed 's/$/\r/' "$WORK/want" > "$INC/nexa_vk14_embed.inc"
    else
        cp "$WORK/want" "$INC/nexa_vk14_embed.inc"
    fi
    echo "ok embed: nexa_vk14_embed.inc rewritten from the headers"
    echo "vk14_embed ok"
    exit 0
fi

echo "FAIL embed: nexa_vk14_embed.inc and the headers have drifted apart"
echo "  the headers are the source; rerun with --write to rebuild the embed"
sed 's/^/  /' "$WORK/d" | head -40
echo "vk14_embed: 1 failure(s)"
exit 1
