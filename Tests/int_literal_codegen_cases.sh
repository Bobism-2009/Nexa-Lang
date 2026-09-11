#!/bin/sh
# Codegen cover for integer literals (BOB-10).
#
# Two claims a .nxa test cannot make about itself:
#
#   1. What a *bare* literal prints. io.println(18446744073709551615) never makes
#      a variable, so the printf conversion is picked from the literal's own
#      width; picking "%d" for a wider literal reads the wrong number of bytes
#      off the varargs list and prints a truncated number instead of failing.
#
#   2. That the C++ NexaC hands the compiler draws no diagnostic. A decimal past
#      i64 max has no signed type in C++, so one emitted verbatim earned
#      -Wimplicitly-unsigned-literal -- a warning pointing at machine-written
#      code the user never typed, on a literal Nexa explicitly accepts.
#
# Only diagnostics that name a .cpp line count. Toolchain chatter that does not
# (a cross-compiler shim reporting an unused flag, say) is not about the code.
#
# Usage: Tests/int_literal_codegen_cases.sh [path-to-NexaC]      (run from the repo root)

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

# Diagnostics attributable to the generated C++, i.e. "file.cpp:LINE:COL: warning:".
cpp_diagnostics() {
    grep -E '\.cpp:[0-9]+:[0-9]+: (warning|error):' || true
}

# build <label> <source-text> -> writes the binary to $WORK/case, echoes the build log
build() {
    printf '%s' "$2" > "$WORK/case.nxa"
    "$NEXAC" "$WORK/case.nxa" -o "$WORK/case" 2>&1
}

# expect_run <label> <expected-stdout> <source-text>
# Asserts the program builds without a single diagnostic about the generated C++,
# and that it prints exactly the expected bytes.
expect_run() {
    label=$1
    want=$2
    src=$3

    log=$(build "$label" "$src")
    status=$?
    if [ $status -ne 0 ]; then
        echo "FAIL $label: NexaC exited $status"
        echo "$log" | sed 's/^/  /'
        fails=$((fails + 1))
        return
    fi

    diags=$(printf '%s\n' "$log" | cpp_diagnostics)
    if [ -n "$diags" ]; then
        echo "FAIL $label: the C++ compiler had something to say about generated code"
        echo "$diags" | sed 's/^/  /'
        fails=$((fails + 1))
        return
    fi

    got=$("$WORK/case")
    if [ "$got" != "$want" ]; then
        echo "FAIL $label: wrong output"
        echo "  want: $(printf '%s' "$want" | tr '\n' '|')"
        echo "  got:  $(printf '%s' "$got" | tr '\n' '|')"
        fails=$((fails + 1))
        return
    fi
    echo "ok $label"
}

# --- the repro from BOB-10: u64 max through a declared unsigned long ---------
expect_run "u64 max round trip" \
    '18446744073709551615' \
    '#include <std/io>
fn main() {
    let uMax: unsigned long = 18446744073709551615;
    io.println(uMax);
}
'

# --- bare literals: the printf conversion follows the literal, not `int` -----
expect_run "bare literal widths" \
    '0
42
2147483647
-2147483648
2147483648
9999999999
-9999999999
9223372036854775807
-9223372036854775808
18446744073709551615' \
    '#include <std/io>
fn main() {
    io.println(0);
    io.println(42);
    io.println(2147483647);
    io.println(-2147483648);
    io.println(2147483648);
    io.println(9999999999);
    io.println(-9999999999);
    io.println(9223372036854775807);
    io.println(-9223372036854775808);
    io.println(18446744073709551615);
}
'

# --- hex and octal reach the same width, by their own C++ typing rules -------
expect_run "hex and octal widths" \
    '255
18446744073709551615
34
18446744073709551615' \
    '#include <std/io>
fn main() {
    io.println(0xFF);
    io.println(0xFFFFFFFFFFFFFFFF);
    io.println(042);
    io.println(01777777777777777777777);
}
'

# --- i64 min is a negation of a magnitude past i64 max, so emitting it -------
# --- verbatim made the expression unsigned and `< 0` came out false ----------
expect_run "i64 min keeps its sign" \
    'negative
below zero
sum -9223372036854775807' \
    '#include <std/io>
fn main() {
    if (-9223372036854775808 < 0) { io.println("negative"); } else { io.println("NOT NEGATIVE"); }
    let iMin: long = -9223372036854775808;
    if (iMin < 0) { io.println("below zero"); } else { io.println("NOT BELOW ZERO"); }
    io.println("sum " + (iMin + 1));
}
'

# --- u64 max is above every signed value, not below zero --------------------
expect_run "u64 max stays unsigned" \
    'above i64 max
not negative' \
    '#include <std/io>
fn main() {
    let uMax: unsigned long = 18446744073709551615;
    if (uMax > 9223372036854775807) { io.println("above i64 max"); } else { io.println("NOT ABOVE"); }
    if (uMax < 0) { io.println("NEGATIVE"); } else { io.println("not negative"); }
}
'

# --- and the boundary test itself has to build without a peep ---------------
HARDENING=Tests/lexer_hardening_test.nxa
if [ -f "$HARDENING" ]; then
    log=$("$NEXAC" "$HARDENING" -o "$WORK/hardening" 2>&1)
    status=$?
    diags=$(printf '%s\n' "$log" | cpp_diagnostics)
    if [ $status -ne 0 ]; then
        echo "FAIL lexer_hardening builds: NexaC exited $status"
        echo "$log" | sed 's/^/  /'
        fails=$((fails + 1))
    elif [ -n "$diags" ]; then
        echo "FAIL lexer_hardening builds warning-free:"
        echo "$diags" | sed 's/^/  /'
        fails=$((fails + 1))
    else
        echo "ok lexer_hardening builds warning-free"
    fi
else
    echo "SKIP lexer_hardening builds warning-free: $HARDENING not found (run from the repo root)"
fi

if [ $fails -eq 0 ]; then
    echo "int_literal_codegen ok"
    exit 0
fi
echo "int_literal_codegen: $fails failure(s)"
exit 1
