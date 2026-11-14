#!/bin/sh
# An included file keeps its own functions and structs. Two files can both
# declare load and Config; the file you compile still has its own bare names.
#
# Usage: Tests/include_names_cases.sh [path-to-NexaC]   (from the repo root)

set -u
NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*) ;;
    *) NEXAC="$(pwd)/$NEXAC" ;;
esac
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexainc)
trap 'rm -rf "$WORK"' EXIT
FAIL=0

bad() { echo "FAIL $1: $2"; FAIL=$((FAIL + 1)); }
ok() { echo "ok   $1"; }

cat > "$WORK/a.nxa" <<'EOF'
struct Config { n: int; }

fn load(): int { return 1; }

fn make(): Config {
    return Config { n: load() + 1 };
}
EOF

cat > "$WORK/b.nxa" <<'EOF'
struct Config { n: int; }

fn load(): int { return 3; }
EOF

cat > "$WORK/app.nxa" <<'EOF'
#include <std/io>
#include "a.nxa"
#include "b.nxa"

fn load(): int { return 4; }

fn main(): int {
    io.println(a.load());
    io.println(b.load());
    io.println(load());
    let c: a.Config = a.make();
    io.println(c.n);
    let d: b.Config = b.Config { n: b.load() };
    io.println(d.n);
    return 0;
}
EOF

out=$("$NEXAC" "$WORK/app.nxa" -o "$WORK/app" 2>&1)
if [ $? -ne 0 ]; then
    bad "build" "$out"
else
    bin="$WORK/app"
    [ -f "$bin" ] || bin="$WORK/app.exe"
    got=$(printf '%s' "$("$bin" 2>&1)" | tr -d '\r')
    want=$(printf '1\n3\n4\n2\n3')
    if [ "$got" = "$want" ]; then
        ok "two files share no names"
    else
        bad "run" "$got"
    fi
fi

cat > "$WORK/c1.nxa" <<'EOF'
fn id(): int { return 1; }
EOF
mkdir -p "$WORK/sub"
cat > "$WORK/sub/c1.nxa" <<'EOF'
fn id(): int { return 2; }
EOF
cat > "$WORK/clash.nxa" <<'EOF'
#include "c1.nxa"
#include "sub/c1.nxa"
fn main(): int { return 0; }
EOF
out=$("$NEXAC" "$WORK/clash.nxa" --source "$WORK/clash.cpp" 2>&1)
if [ $? -eq 0 ]; then
    bad "clash" "two c1.nxa files were both accepted"
else
    case $out in
        *already\ names*) ok "same file name is refused" ;;
        *) bad "clash" "$out" ;;
    esac
fi

cat > "$WORK/renamed.nxa" <<'EOF'
#include <std/io>
#include "c1.nxa"
#include "sub/c1.nxa" as other
fn main(): int {
    io.println(c1.id());
    io.println(other.id());
    return 0;
}
EOF
out=$("$NEXAC" "$WORK/renamed.nxa" -o "$WORK/renamed" 2>&1)
if [ $? -ne 0 ]; then
    bad "as" "$out"
else
    bin="$WORK/renamed"
    [ -f "$bin" ] || bin="$WORK/renamed.exe"
    got=$(printf '%s' "$("$bin" 2>&1)" | tr -d '\r')
    want=$(printf '1\n2')
    if [ "$got" = "$want" ]; then
        ok "as gives the second file its own name"
    else
        bad "as run" "$got"
    fi
fi

cat > "$WORK/inner.nxa" <<'EOF'
fn use_entry(): int { return entry_only(); }
EOF
cat > "$WORK/see.nxa" <<'EOF'
#include "inner.nxa"
fn entry_only(): int { return 1; }
fn main(): int { return inner.use_entry(); }
EOF
out=$("$NEXAC" "$WORK/see.nxa" --source "$WORK/see.cpp" 2>&1)
if [ $? -eq 0 ]; then
    bad "sees entry" "an included file called the entry file's function"
else
    case $out in
        *another\ file*) ok "an included file cannot see the entry file" ;;
        *) bad "sees entry" "$out" ;;
    esac
fi

cat > "$WORK/kinds.nxa" <<'EOF'
enum Color { Red; Green; }
let count = 1;
fn bump(): int { count = count + 1; return count; }
fn paint(c: Color): int { if (c == Color.Red) { return 1; } return 2; }
EOF
cat > "$WORK/kinds_app.nxa" <<'EOF'
#include <std/io>
#include "kinds.nxa"
fn main(): int {
    io.println(kinds.paint(kinds.Color.Red));
    io.println(kinds.bump());
    kinds.count = kinds.count + 3;
    io.println(kinds.count);
    return 0;
}
EOF
out=$("$NEXAC" "$WORK/kinds_app.nxa" -o "$WORK/kinds_app" 2>&1)
if [ $? -ne 0 ]; then
    bad "kinds" "$out"
else
    bin="$WORK/kinds_app"
    [ -f "$bin" ] || bin="$WORK/kinds_app.exe"
    got=$(printf '%s' "$("$bin" 2>&1)" | tr -d '\r')
    want=$(printf '1\n2\n5')
    if [ "$got" = "$want" ]; then
        ok "enum, global and assignment use the file name"
    else
        bad "kinds run" "$got"
    fi
fi

if [ $FAIL -ne 0 ]; then
    echo "include_names_cases: $FAIL failure(s)"
    exit 1
fi
echo "include_names_cases ok"
