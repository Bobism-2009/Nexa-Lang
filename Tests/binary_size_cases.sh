#!/bin/sh
# What a program costs to ship, and the behaviour the size work must keep.
#
# A Nexa program that cannot throw is built without the C++ exception runtime.
# On Windows the static libc++ (and on Linux the static libstdc++) used to pull
# that runtime in anyway -- with the
# C++ demangler and mingw's own printf/strtod -- through operator new, the
# prebuilt std::string members, libc++'s abort message and std::to_string:
# ~200KB on every program that used a string, a slice or a map, against 14KB
# for one that did not. NexaC now closes each of those ways in (see
# kNexaSlimRuntime in NexaC.cpp), and Result.value() / io.to_int no longer need
# exceptions at all.
#
#   size       Windows and Linux: an everyday program -- strings, a slice, a map,
#              an f-string, a Result, io.to_int, io.parse_int / parse_float /
#              to_float, io.eprintln, time.unix / format, a thread.mutex, a defer, float32 and
#              bits, std/math, all of std/random, the std/file extras, std/term colour and termios flags, an inline_asm! block,
#              range loops (a..b, a..<b, over chars), an enum taken apart by match (as a statement and as a value, with a guard), a match
#              on a Result and on ints (with ranges) and strings, and an Option with its methods, ?, m.get and xs.first,
#              xs.filter / any / all / map / find given function literals, #define / #if / # / __has_include / #pragma, s.tokens,
#              xs.sort_by (on ints, as filter/map are: a []Item costs a page on Linux), xs.sort, _Pragma, __DATE__,
#              a range loop with a step, s.trim / upper / replace / substring / index_of / count /
#              repeat and s[a:b] / xs[a:] (each one function, however often called), a string
#              built from + and one printed from it, and try / catch / throw (NexaC's own errors, with the .value()
#              calls above caught too) -- is
#              under 64KB, and prints
#              exactly what the same program built without the slimming
#              (NEXA_NO_SLIM=1) prints. It does not include std/gfx3d, so the
#              generated C++ carries no Vulkan runtime.
#
#   literal    Windows and Linux: the std::to_string rewrite leaves a string that
#              happens to spell it alone.
#
#   hello      Windows: hello world starts on NexaC's own entry point, not MinGW's
#              crt2.o, so it carries none of the C runtime's startup code: under
#              6KB (it was 13.8KB), prints, and exits 0. Linux (glibc 2.34+): the
#              same below glibc's crt1.o, under 6KB on x86-64 (it was 14.4KB) and
#              36KB on aarch64, whose 16KB pages set the floor.
#
#   value      Everywhere: .value() on an error, with no try/catch around it,
#              keeps what the program printed before it, says what the error
#              was on stderr, and exits non-zero.
#
#   catch      Everywhere: with a try/catch in the program, .value() on an
#              error is still an exception the catch receives.
#
#   uncaught   Everywhere, and in every kind of build -- the default, the
#              stock runtime (NEXA_NO_SLIM=1), --debug, and a program with
#              inline_cpp: a throw nothing catches keeps what the program
#              printed, says "Uncaught error: <text>" on stderr and exits
#              non-zero. (The stock handler dropped that output when stdout
#              was a file or a pipe.) Windows and Linux: that program, whose
#              errors are NexaC's own and not C++ exceptions, is under 16KB
#              (it was 62-68KB with the exception runtime).
#
#   sh Tests/binary_size_cases.sh           (from the repo root)
#
# Usage: Tests/binary_size_cases.sh [path-to-NexaC]

set -u

NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*|[A-Za-z]:*) ;;
    *) NEXAC="$(pwd)/$NEXAC" ;;
esac
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexasize)
trap 'rm -rf "$WORK"' EXIT
FAIL=0
PASS=0
CR=$(printf '\r')

ok() { PASS=$((PASS + 1)); echo "ok   $1"; }
bad() { FAIL=$((FAIL + 1)); echo "FAIL $1: $2"; }

WINDOWS=0
SLIM=0
case "$(uname -s 2>/dev/null)" in
    MINGW*|MSYS*|CYGWIN*) WINDOWS=1; SLIM=1 ;;
    Linux) SLIM=1 ;;
esac
EXE=""
[ "$WINDOWS" = 1 ] && EXE=".exe"

cat > "$WORK/every.nxa" <<'EOF'
#include <std/io>
#include <std/time>
#include <std/thread>
#include <std/math>
#include <std/random>
#include <std/file>
#include <std/term>
#include <std/inline>

#pragma once
_Pragma("once")
#define SCALE(x) ((x) * 2)
#define NAME_OF(x) #x
#if (defined(_WIN32) || defined(__linux__)) && __has_include(<std/io>)
#define OS_OK 1
#else
#define OS_OK 0
#endif

struct Item {
    name: string;
    qty: int = 1;
}

enum Shape {
    Circle { r: float; };
    Box { w: int; h: int; };
    Dot;
}

fn parse(s: string): Result[int] {
    let n = io.to_int(s);
    if (n == 0 && s != "0") { return err("not a number: " + s); }
    return ok(n);
}

fn twice(s: string): Result[int] {
    return ok(parse(s)? * 2);
}

fn checked(n: int): int {
    if (n > 99) { throw "too big: " + n; }
    return n;
}
fn first_long(words: []string): Option[string] {
    for (w in words) {
        if (w.len() > 1) { return Some(w); }
    }
    return None;
}

fn loud(words: []string): Option[string] {
    return Some(first_long(words)?.upper());
}

fn main() {
    let items = [Item { name: "pen", qty: 3 }, Item { name: "cup", qty: 12 }, Item { name: "map" }];
    let totals: map[string]int;
    for (it in items) { totals[it.name] += it.qty; }
    for (i in 1..<len(items)) { totals[items[i].name] += i; }
    for (c in 'a'..'c') { totals["pen"] += (int)(c) - 96; }
    let words = "7,x,-4".split(",");
    for (t in " a;; b ".tokens("; ")) { totals["pen"] += len(t); }
    for (k in 6..0 step -2) { totals["pen"] += k; }
    let sum = 0;
    for (w in words) {
        let r = parse(w);
        if (r.ok()) { sum += r.value(); } else { io.println(r.error()); }
        let t = twice(w);
        if (t.ok()) { sum += t.value(); }
    }
    io.println(f"{len(items)} items, {totals["cup"]:>4} cups, sum {sum}, {2.5 * sum:.2}");
    io.println(totals, " ", words, " ", 1 + 2 + 3);
    // The string methods and slices, one function each, and a string built in one go.
    let line = " Pen, Cup " + sum + " x" + len(words) + "!";
    io.println(line.trim().upper(), "|", line.replace(" ", "_"), "|", line.substring(1, 3), "|",
               line.index_of("Cup"), "|", line.count(" "), "|", line[1:4], "|", words[1:], "|", "ab".repeat(sum % 3 + 1));
    io.println("built " + line + " of " + len(line));
    // io.parse_int's error text once brought the exception runtime back
    // (libc++'s prebuilt "text" + s): 16KB became 90KB.
    let p = io.parse_int("12x");
    if (!p.ok()) { io.eprintln(p.error(), " ", sum); }
    io.println(io.parse_float(" 2.5 ").value() + io.to_float("0.5"));
    let half: float32 = 0.5;
    sum += (int)(from_bits32(bits(half)) * 4.0);
    let m = thread.mutex();
    thread.lock(m);
    defer thread.unlock(m);
    sum += 1;
    io.println(term.bold(term.color("ok", "green")), " ", term.underline("x"));
    // inline_asm!: nop is an instruction on every CPU, so this one builds anywhere.
    inline_asm!(inout sum) {
        nop
    }
    // termios flags: stdin is /dev/null here, so reads give -1 and writes 0.
    io.println(term.c_lflag() & term.ICANON, " ", term.set_c_lflag(term.c_lflag() & ~term.ECHO), " ",
               term.c_cc(term.VMIN), " ", term.restore(), " ", term.ISTRIP);
    random.seed(5);
    let bag = [3, 1, 2];
    random.shuffle(bag);
    sum += random.choice(bag) + (int)(random.float(0, 4));
    io.println(math.clamp(math.hypot(3, 4), 0, 4), " ", math.atan2(1, 1) < 1.0, " ", bag, " ", random.chance(0.5),
               " ", random.bool(), " ", sum);
    let nowhere = "_nexa_no_such_dir/notes.txt";
    io.println(file.stem(nowhere), " ", len(file.lines(nowhere)), " ", len(file.walk(nowhere)), " ",
               file.size(nowhere), " ", file.modified(nowhere), " ", file.remove_all(nowhere));
    let when: long = 1000000000;
    io.println(time.format_utc("%F %T", when), " ", time.unix() > when, " ", time.month(when));
    let shapes = [Shape.Circle { r: 1.5 }, Shape.Box { w: 2, h: sum }, Shape.Dot];
    for (s in shapes) {
        let area = match (s) { Shape.Circle { r } => r * r * 3.0, Shape.Box { w, h } => w * h, _ => 0 };
        match (s) {
            Shape.Box { w, h } if w == h => { io.println("square ", w); }
            Shape.Box { w } => { io.println("box ", w, " ", area); }
            _ => { io.println(match (s) { Shape.Dot => "dot", _ => "round" }, " ", area); }
        }
    }
    for (w in words) {
        match (twice(w)) {
            Ok { v } if v > 10 => { io.println("big ", v); }
            Ok { v } => { io.println(match (v) { -8 => "minus eight", 0 | 1 => "small", 2..9 => "few", ..-1 => "below", _ => "other" }); }
            Err { e } => { io.println(match (w) { "x" => "an x", _ => "?" }, ": ", e); }
        }
    }
    io.println(match (first_long(words)) { Some { w } => w, None => "none" });
    let lw = loud(words);
    io.println(lw.or("-"), " ", lw.is_some(), " ", loud([]).is_none(), " ", lw.value().len());
    io.println(totals.get("cup").or(0), " ", totals.get("nope").is_none(), " ", words.first().or(""), " ", items.last().value().name);
    bag.sort_by(fn (a, b) { return a > b; });
    bag.sort();
    let few = bag.filter(fn (x) { return x > 1; });
    io.println(len(few), " ", items.any(fn (it) { return it.qty > 10; }), " ", bag.all(fn (x: int) { return x > 0; }),
               " ", bag.map(fn (x) { return x * sum; }).sum(), " ",
               items.find(fn (it) { return it.name == "cup"; }).value().qty);
    io.println(SCALE(OS_OK), " ", NAME_OF(items[0]), " ", __LINE__, " ", len(__DATE__));
    try {
        io.println(checked(sum), " ", checked(sum * 100));
    } catch (e) {
        io.println("caught ", e);
    }
}
EOF

if [ "$SLIM" = 1 ]; then
    if "$NEXAC" "$WORK/every.nxa" -o "$WORK/every$EXE" >"$WORK/b1.log" 2>&1 &&
       NEXA_NO_SLIM=1 "$NEXAC" "$WORK/every.nxa" -o "$WORK/stock$EXE" >"$WORK/b2.log" 2>&1; then
        size=$(wc -c < "$WORK/every$EXE" | tr -d ' ')
        if [ "$size" -lt 65536 ]; then ok "size ($size bytes)"; else bad size "$size bytes, over 64KB"; fi
        a=$("$WORK/every$EXE" </dev/null | tr -d "$CR")
        b=$("$WORK/stock$EXE" </dev/null | tr -d "$CR")
        if [ "$a" = "$b" ] && [ -n "$a" ]; then ok "size_same_output"; else bad size_same_output "slim printed [$a], stock printed [$b]"; fi
        if "$NEXAC" "$WORK/every.nxa" --source "$WORK/every.cpp" >"$WORK/every.src.log" 2>&1; then
            if grep -q 'nexa_vk14' "$WORK/every.cpp" || grep -q 'vulkan-1.dll' "$WORK/every.cpp"; then
                bad vulkan "a program without std/gfx3d carries the Vulkan runtime"
            else
                ok "no vulkan without std/gfx3d"
            fi
        else
            bad vulkan "could not emit source: $(tail -3 "$WORK/every.src.log")"
        fi
    else
        bad size "build failed: $(tail -3 "$WORK/b1.log" "$WORK/b2.log" 2>/dev/null)"
    fi

    cat > "$WORK/lit.nxa" <<'EOF'
#include <std/io>
fn main() {
    let n = 5;
    io.println("std::to_string(" + n + ")");
}
EOF
    if "$NEXAC" "$WORK/lit.nxa" -o "$WORK/lit$EXE" >"$WORK/b3.log" 2>&1; then
        out=$("$WORK/lit$EXE" | tr -d "$CR")
        if [ "$out" = "std::to_string(5)" ]; then ok literal; else bad literal "printed [$out]"; fi
    else
        bad literal "build failed: $(tail -3 "$WORK/b3.log")"
    fi
else
    echo "skip size, literal (Windows and Linux only)"
fi

HELLO_MAX=
[ "$WINDOWS" = 1 ] && HELLO_MAX=6144
if [ "$(uname -s 2>/dev/null)" = Linux ]; then
    glibc=$(getconf GNU_LIBC_VERSION 2>/dev/null | sed -n 's/^glibc 2\.\([0-9]*\).*/\1/p')
    if [ -n "$glibc" ] && [ "$glibc" -ge 34 ]; then
        case "$(uname -m)" in
            x86_64) HELLO_MAX=6144 ;;
            # 16KB pages (see the aarch64 link flags): the code and the data each start a page.
            aarch64) HELLO_MAX=18432 ;;
        esac
    fi
fi
if [ -n "$HELLO_MAX" ]; then
    printf '#include <std/io>
fn main() {
    io.println("Hello, Nexa");
}
' > "$WORK/hello.nxa"
    if "$NEXAC" "$WORK/hello.nxa" -o "$WORK/hello$EXE" > "$WORK/bh.log" 2>&1; then
        size=$(wc -c < "$WORK/hello$EXE" | tr -d ' ')
        out=$("$WORK/hello$EXE" | tr -d "$CR"); rc=$?
        if [ "$size" -ge "$HELLO_MAX" ]; then bad hello "$size bytes, $HELLO_MAX or more (the startup code is back?)"
        elif [ "$out" != "Hello, Nexa" ] || [ $rc -ne 0 ]; then bad hello "printed [$out], exit $rc"
        else ok "hello ($size bytes)"; fi
    else
        bad hello "build failed: $(tail -3 "$WORK/bh.log")"
    fi
fi

# Networking. The http client was 15KB on Windows and 34KB on Linux while it put
# requests together as chains of std::string + and substr; a udp program with
# nobody's threads was 20KB on Windows because the sender table's thread_local
# kept it off NexaC's own entry point. Now 11.8KB / 22.1KB and 9.7KB (14KB on
# Linux): the ceilings leave room for a toolchain, not for those to come back.
NET_HTTP_MAX=; NET_UDP_MAX=
[ "$WINDOWS" = 1 ] && { NET_HTTP_MAX=14336; NET_UDP_MAX=12288; }
[ "$HELLO_MAX" = 6144 ] && [ "$WINDOWS" != 1 ] && { NET_HTTP_MAX=26624; NET_UDP_MAX=16384; }
if [ -n "$NET_HTTP_MAX" ]; then
    printf '#include <std/network>\n#include <std/io>\nfn main() {\n    let r = http.get("http://127.0.0.1:1/x");\n    io.println(r.ok());\n}\n' > "$WORK/hget.nxa"
    if "$NEXAC" "$WORK/hget.nxa" -o "$WORK/hget$EXE" > "$WORK/bn.log" 2>&1; then
        size=$(wc -c < "$WORK/hget$EXE" | tr -d ' ')
        if [ "$size" -ge "$NET_HTTP_MAX" ]; then bad http_client "$size bytes, $NET_HTTP_MAX or more"
        else ok "http_client ($size bytes)"; fi
    else
        bad http_client "build failed: $(tail -3 "$WORK/bn.log")"
    fi
    printf '#include <std/network>\n#include <std/io>\nfn main() {\n    let u = udp.open(0);\n    udp.send(u, "127.0.0.1", udp.port(u), "hi");\n    io.println(udp.recv(u), " ", udp.sender(u));\n}\n' > "$WORK/udp.nxa"
    if "$NEXAC" "$WORK/udp.nxa" -o "$WORK/udp$EXE" > "$WORK/bu.log" 2>&1; then
        size=$(wc -c < "$WORK/udp$EXE" | tr -d ' ')
        out=$("$WORK/udp$EXE" | tr -d "$CR")
        if [ "$size" -ge "$NET_UDP_MAX" ]; then bad udp "$size bytes, $NET_UDP_MAX or more"
        elif [ "$out" != "hi 127.0.0.1" ]; then bad udp "printed [$out]"
        else ok "udp ($size bytes)"; fi
    else
        bad udp "build failed: $(tail -3 "$WORK/bu.log")"
    fi
fi

# inline_cpp! used to cost a program the exception runtime, RTTI and NexaC's own
# entry point whatever the C++ said: one extern "C" function doubling an int made
# hello world 226KB on Windows. Now only C++ that throws, catches or asks for a
# type pays for it -- and that C++ still catches what it throws.
if [ "$SLIM" = 1 ]; then
    printf '#include <std/io>\n#include <std/inline>\ninline_cpp! {\n#include <cstdio>\nextern "C" int nt_twice(int x) { return x * 2; }\n}\nextern fn nt_twice(x: int): int;\nfn main() {\n    let s = "n=" + nt_twice(21);\n    io.println(s);\n}\n' > "$WORK/icpp.nxa"
    if "$NEXAC" "$WORK/icpp.nxa" -o "$WORK/icpp$EXE" > "$WORK/bi.log" 2>&1; then
        size=$(wc -c < "$WORK/icpp$EXE" | tr -d ' ')
        out=$("$WORK/icpp$EXE" | tr -d "$CR")
        if [ "$size" -ge 32768 ]; then bad inline_cpp "$size bytes, 32KB or more"
        elif [ "$out" != "n=42" ]; then bad inline_cpp "printed [$out]"
        else ok "inline_cpp ($size bytes)"; fi
    else
        bad inline_cpp "build failed: $(tail -3 "$WORK/bi.log")"
    fi
    printf '#include <std/io>\n#include <std/inline>\ninline_cpp! {\n#include <stdexcept>\nextern "C" int nt_safe(int x) {\n    try { if (x < 0) throw std::runtime_error("neg"); return x; }\n    catch (const std::exception&) { return -1; }\n}\n}\nextern fn nt_safe(x: int): int;\nfn main() { io.println("" + nt_safe(-5) + " " + nt_safe(3)); }\n' > "$WORK/icppt.nxa"
    if "$NEXAC" "$WORK/icppt.nxa" -o "$WORK/icppt$EXE" > "$WORK/bi.log" 2>&1; then
        out=$("$WORK/icppt$EXE" | tr -d "$CR")
        if [ "$out" != "-1 3" ]; then bad inline_cpp_throws "printed [$out]"
        else ok "inline_cpp_throws"; fi
    else
        bad inline_cpp_throws "build failed: $(tail -3 "$WORK/bi.log")"
    fi
fi

cat > "$WORK/value.nxa" <<'EOF'
#include <std/io>
fn load(): Result[string] { return err("no such file"); }
fn main() {
    io.println("before");
    let s = load().value();
    io.println("after " + s);
}
EOF
if "$NEXAC" "$WORK/value.nxa" -o "$WORK/value$EXE" >"$WORK/b4.log" 2>&1; then
    "$WORK/value$EXE" >"$WORK/v.out" 2>"$WORK/v.err" </dev/null
    code=$?
    out=$(tr -d "$CR" < "$WORK/v.out")
    if [ "$code" -ne 0 ] && [ "$out" = "before" ] && grep -q "no such file" "$WORK/v.err"; then
        ok value
    else
        bad value "exit $code, stdout [$out], stderr [$(cat "$WORK/v.err")]"
    fi
else
    bad value "build failed: $(tail -3 "$WORK/b4.log")"
fi

cat > "$WORK/catch.nxa" <<'EOF'
#include <std/io>
fn load(): Result[string] { return err("no such file"); }
fn main() {
    try {
        let s = load().value();
        io.println("after " + s);
    } catch (e) {
        io.println("caught: " + e);
    }
}
EOF
if "$NEXAC" "$WORK/catch.nxa" -o "$WORK/catch$EXE" >"$WORK/b5.log" 2>&1; then
    out=$("$WORK/catch$EXE" </dev/null 2>&1 | tr -d "$CR")
    if [ "$out" = "caught: no such file" ]; then ok catch; else bad catch "printed [$out]"; fi
else
    bad catch "build failed: $(tail -3 "$WORK/b5.log")"
fi

cat > "$WORK/uncaught.nxa" <<'EOF'
#include <std/io>
fn main() {
    let s = "a";
    try {
        s += "b";
        io.println(s);
    } catch (e) {
        io.println(e);
    }
    throw "boom: " + s;
}
EOF
# The same with C++ of its own in the program, which keeps the stock runtime.
cat > "$WORK/uncaught_cpp.nxa" <<'EOF'
#include <std/io>
#include <std/inline>
fn main() {
    let s = "ab";
    inline_cpp! {
        (void)0;
    }
    io.println(s);
    throw "boom: " + s;
}
EOF

# check NAME SOURCE [ENV=VALUE] [flags...]: built that way, an uncaught throw keeps
# the program's output ("ab"), names what was thrown and exits non-zero.
check_uncaught() {
    name=$1; src=$2; shift 2
    envset=""
    case "${1:-}" in *=*) envset=$1; shift ;; esac
    exe="$WORK/$name$EXE"
    if env $envset "$NEXAC" "$src" "$@" -o "$exe" >"$WORK/$name.log" 2>&1; then
        "$exe" >"$WORK/$name.out" 2>"$WORK/$name.err" </dev/null
        code=$?
        out=$(tr -d "$CR" < "$WORK/$name.out")
        if [ "$code" -ne 0 ] && [ "$out" = "ab" ] && grep -q "Uncaught error: boom: ab" "$WORK/$name.err"; then
            ok "$name"
        else
            bad "$name" "exit $code, stdout [$out], stderr [$(head -3 "$WORK/$name.err")]"
        fi
    else
        bad "$name" "build failed: $(tail -3 "$WORK/$name.log")"
    fi
}

check_uncaught uncaught "$WORK/uncaught.nxa"
check_uncaught uncaught_stock "$WORK/uncaught.nxa" NEXA_NO_SLIM=1
check_uncaught uncaught_debug "$WORK/uncaught.nxa" NEXA_NO_SLIM= --debug
check_uncaught uncaught_inline_cpp "$WORK/uncaught_cpp.nxa"
rm -f "$WORK"/*.debug.cpp 2>/dev/null

if [ "$SLIM" = 1 ] && [ -f "$WORK/uncaught$EXE" ]; then
    size=$(wc -c < "$WORK/uncaught$EXE" | tr -d ' ')
    if [ "$size" -lt 16384 ]; then ok "uncaught_size ($size bytes)"; else bad uncaught_size "$size bytes, over 16KB"; fi
fi

echo
echo "$PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
