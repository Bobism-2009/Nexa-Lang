#!/bin/sh
# Codegen cover for wide integer expressions (BOB-15).
#
# Two claims Tests/Lang/wide_int_arith.nxa cannot make about itself:
#
#   1. That the printf conversion matches the argument. io.println(big + 1)
#      emitted printf("%d\n", big + 1) for a 64-bit argument. Reading a long
#      back as an int is undefined behaviour, not a conversion: on x86-64 it
#      happened to print the low 32 bits, but "it printed the wrong number" is
#      the symptom, not the bug. Only the generated C++ shows the bug itself.
#
#   2. That the user never sees the C++ compiler notice it. clang says
#      "format specifies type 'int' but the argument has type 'long'
#      [-Wformat]", and that warning reached the user, naming a line of a
#      machine-written file they never typed.
#
# Only diagnostics that name a .cpp line count. Toolchain chatter that does not
# (a cross-compiler shim reporting an unused flag, say) is not about the code.
#
# Usage: Tests/wide_int_codegen_cases.sh [path-to-NexaC]      (run from the repo root)

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

# expect_run <label> <expected-stdout> <source-text>
# Asserts the program builds without a single diagnostic about the generated C++,
# and that it prints exactly the expected bytes.
expect_run() {
    label=$1
    want=$2
    src=$3

    printf '%s' "$src" > "$WORK/case.nxa"
    log=$("$NEXAC" "$WORK/case.nxa" -o "$WORK/case" 2>&1)
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

# expect_source <label> <grep-ERE> <source-text>
# Asserts the transpiled C++ contains a line matching the pattern. --source never
# invokes the C++ compiler, so this is the cheap half of the cover.
expect_source() {
    label=$1
    want=$2
    src=$3

    printf '%s' "$src" > "$WORK/case.nxa"
    if ! "$NEXAC" "$WORK/case.nxa" --source "$WORK/case.cpp" > "$WORK/case.log" 2>&1; then
        echo "FAIL $label: NexaC could not transpile"
        sed 's/^/  /' "$WORK/case.log"
        fails=$((fails + 1))
        return
    fi
    if ! grep -Eq "$want" "$WORK/case.cpp"; then
        echo "FAIL $label: no generated line matches /$want/"
        grep -n 'printf' "$WORK/case.cpp" | sed 's/^/  /'
        fails=$((fails + 1))
        return
    fi
    echo "ok $label"
}

# --- the repro from BOB-15 --------------------------------------------------
expect_run "wide expression prints its own width" \
    '3000000000
3000000001
5000000001
5000000001' \
    '#include <std/io>
fn main() {
    let big: long = 3000000000;
    io.println(big);
    io.println(big + 1);
    let sz: size_t = 5000000000;
    io.println(sz + 1);
    let ul: unsigned long = 5000000000;
    io.println(ul + 1);
}
'

# --- io.print (no newline) shares the conversion-picking path ---------------
expect_run "wide expression through io.print" \
    '3000000001 5000000001' \
    '#include <std/io>
fn main() {
    let big: long = 3000000000;
    let sz: size_t = 5000000000;
    io.print(big + 1);
    io.print(" ");
    io.print(sz + 1);
    io.println("");
}
'

# --- an inferred `let` is declared at the width of its initialiser ----------
expect_run "inferred let keeps the width" \
    '3000000001
10000000000' \
    '#include <std/io>
fn main() {
    let big: long = 3000000000;
    let y = big + 1;
    io.println(y);
    let sz: size_t = 5000000000;
    let z = sz * 2;
    io.println(z);
}
'

# --- the conversion is fixed-width, so it does not depend on the target -----
# --- ABI the way "%ld" and "%zu" do (long is 32-bit on LLP64 Windows) -------
expect_source "long prints through a fixed-width conversion" \
    'printf\("%lld\\n", static_cast<long long>' \
    '#include <std/io>
fn main() {
    let big: long = 3000000000;
    io.println(big + 1);
}
'

expect_source "size_t prints through a fixed-width conversion" \
    'printf\("%llu\\n", static_cast<unsigned long long>' \
    '#include <std/io>
fn main() {
    let sz: size_t = 5000000000;
    io.println(sz + 1);
}
'

# --- int-width types keep "%d"/"%u" and stay uncast, so that a future -------
# --- inference bug there is still a -Wformat warning and not silent ---------
expect_source "int expressions still print through %d" \
    'printf\("%d\\n", \(__nexa_var_[0-9]+ \+ 1\)\);' \
    '#include <std/io>
fn main() {
    let n: int = 2;
    io.println(n + 1);
}
'

expect_source "unsigned int expressions print through %u" \
    'printf\("%u\\n", \(__nexa_var_[0-9]+ \+ 1\)\);' \
    '#include <std/io>
fn main() {
    let un: unsigned int = 7;
    io.println(un + 1);
}
'

if [ $fails -eq 0 ]; then
    echo "wide_int_codegen ok"
    exit 0
fi
echo "wide_int_codegen: $fails failure(s)"
exit 1
