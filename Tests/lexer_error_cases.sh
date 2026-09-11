#!/bin/sh
# Error-path cover for the lexer/literal layer (BOB-7).
#
# These cases cannot live in a .nxa test, because the point of each one is that
# NexaC refuses to compile it. Each case asserts three things:
#   1. NexaC exits non-zero,
#   2. it prints a "[Nexa] Error:" naming the source file and the right line,
#   3. no C++ compiler diagnostic leaks through to the user.
#
# Usage: Tests/lexer_error_cases.sh [path-to-NexaC]      (run from the repo root)

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

# expect_error <label> <expected-substring> <source-text>
expect_error() {
    label=$1
    want=$2
    src=$3

    printf '%s' "$src" > "$WORK/case.nxa"
    out=$("$NEXAC" "$WORK/case.nxa" --source "$WORK/case.cpp" 2>&1)
    status=$?

    if [ $status -eq 0 ]; then
        echo "FAIL $label: NexaC exited 0, expected a compile error"
        fails=$((fails + 1))
        return
    fi
    case $out in
        *"$want"*) ;;
        *)
            echo "FAIL $label: expected \"$want\""
            echo "  got: $out"
            fails=$((fails + 1))
            return
            ;;
    esac
    # The whole point of diagnosing at the Nexa level is that clang never speaks.
    case $out in
        *"error:"*)
            echo "FAIL $label: a raw C++ compiler error leaked through"
            echo "  got: $out"
            fails=$((fails + 1))
            return
            ;;
    esac
    case $out in
        *case.nxa*) ;;
        *)
            echo "FAIL $label: diagnostic does not name the source file"
            echo "  got: $out"
            fails=$((fails + 1))
            return
            ;;
    esac
    echo "ok $label"
}

# --- unknown escape sequences: used to silently drop the backslash -----------
expect_error "unknown escape" \
    "Unknown escape sequence '\\q' in string literal at line 3" \
    '#include <std/io>
fn main() {
    let s = "\q\z";
    io.println(s);
}
'

expect_error "unknown escape in char" \
    "Unknown escape sequence '\\q' in character literal at line 3" \
    '#include <std/io>
fn main() {
    let c = '"'"'\q'"'"';
    io.println(c);
}
'

expect_error "short hex escape" \
    "needs exactly two hex digits" \
    '#include <std/io>
fn main() {
    let s = "\x4";
    io.println(s);
}
'

# --- unterminated literals: used to swallow the rest of the file ------------
expect_error "unterminated string" \
    "Unterminated string literal at line 3" \
    '#include <std/io>
fn main() {
    let s = "abc;
    io.println(s);
}
'

expect_error "unterminated string at eof" \
    "Unterminated string literal at line 3" \
    '#include <std/io>
fn main() {
    let s = "abc'

expect_error "unterminated char" \
    "Unterminated character literal at line 3" \
    '#include <std/io>
fn main() {
    let c = '"'"'a;
    io.println(c);
}
'

expect_error "unterminated raw string" \
    "Unterminated raw string literal" \
    '#include <std/io>
fn main() {
    let s = R"(abc;
    io.println(s);
}
'

# --- unclosed block comment: used to delete code silently -------------------
expect_error "unterminated block comment" \
    "Unterminated block comment (opened with '/*') at line 2" \
    '#include <std/io>
/* never closed
fn main() {
    io.println("hi");
}
'

# --- integer literals out of 64-bit range: used to reach clang verbatim -----
expect_error "decimal too large" \
    "Integer literal '99999999999999999999999' is out of range for a 64-bit integer (max 18446744073709551615) at line 3" \
    '#include <std/io>
fn main() {
    let x = 99999999999999999999999;
    io.println(x);
}
'

expect_error "u64 max plus one" \
    "is out of range for a 64-bit integer (max 18446744073709551615)" \
    '#include <std/io>
fn main() {
    let x = 18446744073709551616;
    io.println(x);
}
'

expect_error "i64 min minus one" \
    "is out of range for a 64-bit integer (min -9223372036854775808)" \
    '#include <std/io>
fn main() {
    let x = -9223372036854775809;
    io.println(x);
}
'

expect_error "hex too large" \
    "Integer literal '0x10000000000000000' is out of range" \
    '#include <std/io>
fn main() {
    let x = 0x10000000000000000;
    io.println(x);
}
'

expect_error "octal bad digit" \
    "Invalid digit '9' in octal literal" \
    '#include <std/io>
fn main() {
    let x = 09;
    io.println(x);
}
'

# --- the line number must point at the literal, not at the following code ---
expect_error "line number of a later literal" \
    "Unterminated string literal at line 6" \
    '#include <std/io>
fn main() {
    let a = "one";
    let b = "two";
    io.println(a + b);
    let c = "three;
}
'

if [ $fails -ne 0 ]; then
    echo "lexer_error_cases: $fails failure(s)"
    exit 1
fi
echo "lexer_error_cases ok"
