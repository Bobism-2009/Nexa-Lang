#!/bin/sh
# xs.find(f), xs.any(f), xs.all(f), xs.filter(f), xs.map(f): slice methods that
# take a function of one element, and xs.sort_by(f), which takes a function of two.
#
# Usage: Tests/slice_fn_cases.sh [path-to-NexaC]   (from the repo root)

set -u
NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*|[A-Za-z]:*) ;;
    *) NEXAC="$(pwd)/$NEXAC" ;;
esac
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexaslicefn)
trap 'rm -rf "$WORK"' EXIT
FAIL=0

bad() { echo "FAIL $1: $2"; FAIL=$((FAIL + 1)); }
ok() { echo "ok   $1"; }

run_eq() {
    label=$1
    src=$2
    want=$3
    printf '%s\n' "$src" > "$WORK/p.nxa"
    out=$("$NEXAC" "$WORK/p.nxa" -o "$WORK/p" 2>&1)
    if [ $? -ne 0 ]; then
        bad "$label" "$out"
        return
    fi
    bin="$WORK/p"
    [ -f "$bin" ] || bin="$WORK/p.exe"
    got=$(printf '%s' "$("$bin" 2>&1)" | tr -d '\r')
    if [ "$got" = "$want" ]; then
        ok "$label"
    else
        bad "$label" "expected [$want] got [$got]"
    fi
}

expect_has() {
    label=$1
    src=$2
    want=$3
    printf '%s\n' "$src" > "$WORK/bad.nxa"
    out=$("$NEXAC" "$WORK/bad.nxa" --source "$WORK/bad.cpp" 2>&1)
    if [ $? -eq 0 ]; then
        bad "$label" "NexaC accepted it"
        return
    fi
    case $out in
        *"$want"*) ok "$label" ;;
        *) bad "$label" "$out" ;;
    esac
}

# The C++ ahead of main() -- headers, runtime helpers, types -- is the same for
# both programs: what the first one uses brings no code of its own.
prelude_eq() {
    label=$1
    printf '%s\n' "$2" > "$WORK/a.nxa"
    printf '%s\n' "$3" > "$WORK/b.nxa"
    if ! "$NEXAC" "$WORK/a.nxa" --source "$WORK/a.cpp" >/dev/null 2>&1 ||
       ! "$NEXAC" "$WORK/b.nxa" --source "$WORK/b.cpp" >/dev/null 2>&1; then
        bad "$label" "NexaC refused a program"
        return
    fi
    sed '/^int main/,$d' "$WORK/a.cpp" > "$WORK/a.pre"
    sed '/^int main/,$d' "$WORK/b.cpp" > "$WORK/b.pre"
    if [ -s "$WORK/a.pre" ] && cmp -s "$WORK/a.pre" "$WORK/b.pre"; then
        ok "$label"
    else
        bad "$label" "the C++ before main() differs"
    fi
}

run_eq "find, any, all, filter, map on ints" \
'#include <std/io>
fn even(x: int): bool { return x % 2 == 0; }
fn main() {
    let xs = [3, 8, 1, 12, 7];
    io.println(xs.find(fn (x: int) { return x > 5; }).or(-1), " ", xs.find(fn (x: int) { return x > 50; }).or(-1));
    io.println(xs.any(even), " ", xs.all(even), " ", xs.all(fn (x: int) { return x > 0; }));
    let ev = xs.filter(even);
    io.println(len(ev), " ", ev[0], " ", ev[1]);
    io.println(xs.map(fn (x: int) { return x * x; }).sum());
}' "8 -1
true false true
2 8 12
267"

run_eq "strings, structs, and a map to another type" \
'#include <std/io>
struct Item { name: string; qty: int; }
fn main() {
    let names = ["ann", "bo", "cyd"];
    io.println(names.map(fn (s: string): string { return s.upper(); }).join(","));
    io.println(names.map(fn (s: string): int { return len(s); }).sum());
    io.println(names.filter(fn (s: string) { return len(s) == 3; }).join("+"));
    let items = [Item { name: "pen", qty: 3 }, Item { name: "cup", qty: 0 }];
    match (items.find(fn (it: Item) { return it.qty == 0; })) {
        Some { it } => { io.println("out: " + it.name); }
        None => { io.println("all stocked"); }
    }
    io.println(items.map(fn (it: Item): string { return it.name; }).join(" "));
}' "ANN,BO,CYD
8
ann+cyd
out: cup
pen cup"

run_eq "chained, captured, with ?, at file scope, on empty" \
'#include <std/io>
fn even(x: int): bool { return x % 2 == 0; }
let tens = [1, 2, 3].map(fn (x: int) { return x * 10; });
fn first_big(xs: []int, lim: int): Option[int] {
    let v = xs.find(fn (x: int) { return x > lim; })?;
    return Some(v * 2);
}
fn main() {
    let xs = [3, 8, 1, 12, 7];
    let lim = 6;
    io.println(xs.filter(fn (x: int) { return x > lim; }).map(fn (x: int) { return x + 1; }).sum());
    io.println(first_big(xs, 9).or(0), " ", first_big(xs, 99).or(0), " ", tens.sum());
    let none: []int;
    io.println(none.any(even), " ", none.all(even), " ", len(none.filter(even)), " ", none.find(even).is_none());
    let seen = 0;
    let same = xs.map(fn (x: int) { seen = seen + 1; return x; });
    io.println(seen, " ", len(same));
    let flags = [false, true];
    io.println(flags.find(fn (b: bool) { return b; }).is_some(), " ", flags.all(fn (b: bool) { return b; }));
}' "30
24 0 60
false true 0 true
5 5
true false"

run_eq "a user struct keeps its own find" \
'#include <std/io>
struct Bag { xs: []int; fn find(n: int): int { return n * 2; } fn map(n: int): int { return n + 1; } }
fn main() {
    let b = Bag { xs: [1] };
    io.println(b.find(4), " ", b.map(4));
}' "8 5"

expect_has "the function takes an element" '#include <std/io>
fn main() { let n = ["a"].find(fn (s: int) { return s == 1; }); }' \
"so it takes (x: string) or just (x); this one takes (int)"

run_eq "a parameter with no type is an element; the return type is what it returns" \
'#include <std/io>
struct Item { name: string; qty: int; }
struct Shop { items: []Item; }
fn names_of(xs: []Item): []string { return xs.map(fn (it) { return it.name; }); }
fn main() {
    let names = ["ann", "bo", "cyd", "dee"];
    io.println(names.filter(fn (s) { return len(s) > 2; }).join(","));
    io.println(names.map(fn (s) { return s.upper(); }).join(","), " ", names.map(fn (s) { return len(s); }).sum());
    let shop = Shop { items: [Item { name: "pen", qty: 3 }, Item { name: "cup", qty: 0 }] };
    io.println(shop.items.find(fn (it) { return it.qty == 0; }).value().name, " ", names_of(shop.items).join("+"));
    io.println(shop.items.any(fn (it) { return it.name == "pen"; }), " ", shop.items.all(fn (it) { return it.qty > 0; }));
    let fs = [1.5, 2.5];
    io.println(fs.map(fn (x) { return x * 2.0; }).sum(), " ", fs.map(fn (x) { return x > 2.0; })[1]);
    let grid = [[1, 2], [3]];
    io.println(grid.map(fn (row) { return len(row); }).sum(), " ", grid.filter(fn (row) { return len(row) > 1; })[0][1]);
    let up = names.map(fn (s) { let t = s.upper(); return t + "!"; });
    let mix = [1, 2].map(fn (x) { if (x > 1) { return 2.5; } return x; });
    io.println(up[0], " ", mix.sum());
}' "ann,cyd,dee
ANN,BO,CYD,DEE 11
cup pen+cup
true false
8 true
3 2
ANN! 3.5"

run_eq "a global let calls a method on another global" \
'#include <std/io>
let words = ["ann", "bo"];
let nums = [3, 1, 2];
let a = words.join(",");
let b = words.has("bo");
let c = nums.min() + nums.max() + nums.sum();
let d = words.first().or("");
let e = words.index_of("bo");
let f = words.map(fn (s) { return s.upper(); }).join("");
fn main() { io.println(a, " ", b, " ", c, " ", d, " ", e, " ", f); }' "ann,bo true 10 ann 1 ANNBO"

expect_has "a function, not a value" '#include <std/io>
fn main() { let n = [1].any(5); }' "any(...) takes a function, as in xs.any(fn (x: int)"

expect_has "a test returns a bool" '#include <std/io>
fn main() { let n = [1].filter(fn (x: int): string { return "a"; }); }' \
"so it returns a bool; this one returns a string"

expect_has "map makes a value" '#include <std/io>
fn main() { let n = [1].map(fn (x: int) { io.println(x); }); }' "so it returns a value; this one returns nothing"

expect_has "one argument" '#include <std/io>
fn main() { let n = [1].all(); }' "all takes one function"

run_eq "sort_by: a literal, a named fn, structs, empty" \
'#include <std/io>
struct P { name: string; age: int; }
fn by_len(a: string, b: string): bool { return len(a) < len(b); }
fn main() {
    let xs = [5, 3, 9, 1, 7];
    xs.sort_by(fn (a, b) { return a > b; });
    let ws = ["ccc", "a", "bb", "dd", "e"];
    ws.sort_by(by_len);
    let ps: []P = [P { name: "ann", age: 36 }, P { name: "bo", age: 25 }, P { name: "cy", age: 36 }, P { name: "di", age: 25 }];
    ps.sort_by(fn (a: P, b: P): bool { return a.age < b.age; });
    let es: []int;
    es.sort_by(fn (a, b) { return a < b; });
    io.println(xs[0], xs[4], " ", ws.join(","), " ", ps.map(fn (p) { return p.name; }).join(","), " ", len(es));
}' "91 a,e,bb,dd,ccc bo,di,ann,cy 0"

run_eq "sort_by is stable past one run, and sorts" \
'#include <std/io>
struct R { key: int; at: int; }
fn main() {
    let rs: []R;
    let i = 0;
    while (i < 5000) { rs.push(R { key: (i * 7919) % 13, at: i }); i += 1; }
    rs.sort_by(fn (a, b) { return a.key < b.key; });
    let bad = 0;
    let j = 1;
    while (j < len(rs)) {
        if (rs[j - 1].key > rs[j].key) { bad += 1; }
        if (rs[j - 1].key == rs[j].key && rs[j - 1].at > rs[j].at) { bad += 1; }
        j += 1;
    }
    let ns: []int;
    i = 0;
    while (i < 1000) { ns.push((i * 31) % 1000); i += 1; }
    ns.sort_by(fn (a, b) { return a > b; });
    io.println(bad, " ", len(rs), " ", ns[0], " ", ns[999]);
}' "0 5000 999 0"

expect_has "sort_by takes two elements" '#include <std/io>
fn main() { let xs = [1]; xs.sort_by(fn (a: int) { return a > 1; }); }' \
"so it takes (a: int, b: int) or just (a, b); this one takes (int)"

expect_has "sort_by returns a bool" '#include <std/io>
fn main() { let xs = ["a"]; xs.sort_by(fn (a, b) { return a; }); }' \
"answers whether a goes before b, so it returns a bool; this one returns a string"

expect_has "sort_by needs a slice to sort" '#include <std/io>
fn main() { let xs = [1, 2]; xs[0:1].sort_by(fn (a, b) { return a < b; }); }' \
".sort_by() changes the slice in place"

prelude_eq "a literal given to filter names no std::function" \
'#include <std/io>
fn main() { let xs = [1, 2]; io.println(len(xs.filter(fn (x: int) { return x > 1; }))); }' \
'#include <std/io>
fn main() { let xs = [1, 2]; io.println(len(xs)); }'

echo ""
if [ "$FAIL" -gt 0 ]; then
    echo "FAILED $FAIL test(s)"
    exit 1
else
    echo "All tests passed"
fi
