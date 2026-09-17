#!/bin/sh
# std/network cover: the umbrella, and udp.* under it (BOB-45).
#
# Tests/http_cases.sh and Tests/tcp_cases.sh already cover their own protocols
# through the new include. This suite covers the two things that are new: that
# one include really is one include -- three protocols behind it, and a program
# paying only for the ones it calls -- and udp.* itself.
#
# Layered the way tcp_cases.sh is, cheapest and most portable first, so every
# machine runs as much of it as it can:
#
#   umbrella  Transpile only (--source). One include, three namespaces, and
#            nothing emitted for a protocol nobody called: a udp program
#            carries no HTTP transport and no TCP, a tcp program carries no
#            udp, and a program that includes std/network and calls nothing
#            carries none of it. Also the retired includes, which must name
#            std/network rather than failing at the first http.get.
#
#   codegen   Pins the call each udp.* form emits, including the 64K cap NexaC
#            fills in for a udp.recv that left one off, and that a program
#            which never asks who sent a packet carries no bookkeeping for the
#            answer. Never invokes the C++ compiler or a socket.
#
#   slices    The Windows, macOS and wasm emissions, not just this machine's.
#            The Windows one is compiled for real when a mingw-capable clang is
#            around. The wasm slice is stubs -- a page has no datagram socket
#            -- and a real --wasm BUILD of a udp program is refused outright,
#            which is the layer below.
#
#   compile   The emitted C++ still builds warning-clean for the native
#            backend, and for wasm. Skipped without a C++ compiler.
#
#   loopback  Build Tests/udp_loopback_test.nxa and run it against
#            Tests/udp_peer.py on 127.0.0.1. Plain Python sockets on the other
#            end, so this is the layer that proves a Nexa program speaks UDP
#            and not just its own conventions: a packet each way, a 40000-byte
#            datagram arriving whole, a NUL-bearing payload, an empty datagram
#            that is still a datagram, and udp.sender naming a peer this
#            program never wrote down.
#
#   server    Build Tests/udp_server_test.nxa and run it: one program with a
#            socket answering two datagrams from a worker thread and the
#            client that sends them. This is where udp.sender earns its place
#            -- the reply goes where the packet came from, and the thread that
#            read it is the thread that answers.
#
#   mixed     Build Tests/network_mixed_test.nxa and run it: one include, a
#            stream socket and a datagram socket open at once, and the prefix
#            on each call the only thing saying which is which. This is the
#            founder's ask, run as a program.
#
# Unknown-function and arity diagnostics live in Tests/Lang/errors/udp_*.nxa,
# and the retired-include diagnostics in *_moved_include.nxa, where the
# run_tests.sh error phase already checks that NexaC does the complaining and
# clang never gets to speak.
#
# Skips are reported but do not fail the run. Note that run_tests.sh only
# prints a suite's output when it fails, so run this directly to see which
# layers a machine actually ran:
#
#   sh Tests/network_cases.sh        (from the repo root)
#
# Usage: Tests/network_cases.sh [path-to-NexaC]

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")

SUITE=$(cd "$(dirname "$0")" && pwd)

WORK=$(mktemp -d)
cleanup() {
    [ -n "${PEER_PID:-}" ] && kill "$PEER_PID" 2>/dev/null
    rm -rf "$WORK"
}
trap cleanup EXIT INT TERM

fails=0
skips=0
PEER_PID=""

# transpile <name> <body> [extra NexaC flags...]
# Writes a one-function std/network program and transpiles it. Reports the
# failure itself and returns non-zero if NexaC refuses.
transpile() {
    name=$1
    body=$2
    shift 2
    printf '#include <std/network>\n#include <std/io>\nfn main() {\n%s\n}\n' "$body" > "$WORK/$name.nxa"
    if ! "$NEXAC" "$WORK/$name.nxa" "$@" --source "$WORK/$name.cpp" \
            > "$WORK/$name.log" 2>&1; then
        echo "FAIL $name: NexaC could not transpile"
        sed 's/^/  /' "$WORK/$name.log"
        fails=$((fails + 1))
        return 1
    fi
    return 0
}

# --- umbrella: one include, and only what the program calls -----------------

echo "-- umbrella: three protocols behind one include, priced separately"

# carries <label> <body> <yes|no http> <yes|no tcp> <yes|no udp>
# Each protocol is checked through a symbol only its runtime defines, so a
# protocol nobody called leaves no trace at all rather than a smaller one.
carries() {
    label=$1
    transpile "$label" "$2" || return
    for pair in "__nexa_http_:$3" "__nexa_tcp_:$4" "__nexa_udp_:$5"; do
        sym=${pair%:*}
        want=${pair#*:}
        if grep -q "$sym" "$WORK/$label.cpp"; then have=yes; else have=no; fi
        if [ "$have" != "$want" ]; then
            echo "FAIL $label: $sym present=$have, expected $want"
            fails=$((fails + 1))
            return
        fi
    done
    echo "ok $label"
}

carries "udp_only_carries_no_http_or_tcp" \
    '    let s = udp.open(0);
    io.println(len(udp.recv(s)));
    udp.close(s);' no no yes
carries "tcp_only_carries_no_udp" \
    '    let c = tcp.connect("h", 9);
    io.println(len(tcp.recv(c)));
    tcp.close(c);' no yes no
carries "http_only_carries_no_sockets" \
    '    let r = http.get("http://example.com/");
    io.println(r.ok());' yes no no
carries "two_protocols_carry_two" \
    '    let c = tcp.connect("h", 9);
    let s = udp.open(0);
    io.println(tcp.send(c, "x") + udp.send(s, "h", 9, "y"));' no yes yes

# The include alone is free. A program that reaches for none of the three pays
# for none of them -- which is the whole claim the umbrella rests on.
if transpile "include_without_calls" '    io.println("hi");'; then
    if grep -q '__nexa_http_\|__nexa_tcp_\|__nexa_udp_\|__nexa_sock_\|socket(' \
            "$WORK/include_without_calls.cpp"; then
        echo "FAIL include_without_calls: a runtime was emitted anyway"
        fails=$((fails + 1))
    else
        echo "ok include_without_calls"
    fi
fi

# tcp.* and udp.* are the same sockets under two protocols, so the platform
# preamble they share is emitted once and only once.
if transpile "socket_preamble_emitted_once" \
        '    let c = tcp.connect("h", 9);
    let s = udp.open(0);
    io.println(tcp.close(c) + udp.close(s));'; then
    n=$(grep -c '^\[\[maybe_unused\]\] static int __nexa_net_start' \
        "$WORK/socket_preamble_emitted_once.cpp")
    if [ "$n" != 1 ]; then
        echo "FAIL socket_preamble_emitted_once: __nexa_net_start defined $n time(s), expected 1"
        fails=$((fails + 1))
    else
        echo "ok socket_preamble_emitted_once"
    fi
fi

# retired <label> <old module>
# The old includes must say where the module went, by name. The error phase
# pins the wording; this pins that NexaC is the one refusing, before clang.
retired() {
    label=$1
    printf '#include <std/%s>\nfn main() { }\n' "$2" > "$WORK/$label.nxa"
    if "$NEXAC" "$WORK/$label.nxa" --source "$WORK/$label.cpp" \
            > "$WORK/$label.log" 2>&1; then
        echo "FAIL $label: std/$2 still compiles"
        fails=$((fails + 1))
        return
    fi
    if ! grep -q "std/$2 moved into std/network" "$WORK/$label.log"; then
        echo "FAIL $label: the diagnostic does not name std/network"
        sed 's/^/  /' "$WORK/$label.log" | head -n 5
        fails=$((fails + 1))
        return
    fi
    echo "ok $label"
}

retired "retired_http_include" "http"
retired "retired_tcp_include" "tcp"

# --- codegen: the call each udp.* form emits --------------------------------

# emits <label> <body> <expected substring>
emits() {
    label=$1
    transpile "$label" "$2" || return
    if ! grep -qF "$3" "$WORK/$label.cpp"; then
        echo "FAIL $label: expected to emit"
        echo "  $3"
        grep -n '__nexa_udp_' "$WORK/$label.cpp" | sed 's/^/  got /'
        fails=$((fails + 1))
        return
    fi
    echo "ok $label"
}

echo "-- codegen: the call each udp verb emits"

emits "open" \
    '    let s = udp.open(9);' \
    '__nexa_udp_open(9)'
emits "port" \
    '    let s = udp.open(0);
    io.println(udp.port(s));' \
    '__nexa_udp_port('
emits "send" \
    '    let s = udp.open(0);
    let n = udp.send(s, "h", 9, "hi");' \
    ', "h", 9, "hi")'
emits "sender" \
    '    let s = udp.open(0);
    io.println(udp.sender(s) + udp.sender_port(s));' \
    '__nexa_udp_sender('
emits "close" \
    '    let s = udp.open(0);
    udp.close(s);' \
    '__nexa_udp_close('

# The cap on udp.recv is NexaC's to fill in when a program leaves it off: 64K,
# which is past the largest datagram IPv4 can carry. A cap the program did give
# is passed through.
emits "recv_default_cap" \
    '    let s = udp.open(0);
    let m = udp.recv(s);' \
    ', 65536)'
emits "recv_given_cap" \
    '    let s = udp.open(0);
    let m = udp.recv(s, 12);' \
    ', 12)'

# A call written for what it does, with nobody keeping the count or the 1/0 it
# hands back, still has to reach the emitted C++.
emits "send_as_a_statement_is_not_dropped" \
    '    let s = udp.open(0);
    udp.send(s, "h", 9, "x");' \
    '(void)(__nexa_udp_send('

# A handle is an int and 0 is failure, so the handle is its own condition.
emits "handle_is_its_own_condition" \
    '    if (udp.open(9)) { io.println("up"); }' \
    'if (__nexa_udp_open('

# udp.recv and udp.sender are the two calls that hand back text, so they
# concatenate as strings rather than going through std::to_string.
emits "recv_is_a_string" \
    '    let s = udp.open(0);
    io.println("got " + udp.recv(s));' \
    'std::string("got ") + __nexa_udp_recv('
emits "sender_is_a_string" \
    '    let s = udp.open(0);
    io.println("from " + udp.sender(s));' \
    'std::string("from ") + __nexa_udp_sender('

# A program that never asks who sent a packet carries no table for the answer,
# and does not pay getnameinfo on every read either.
if transpile "no_sender_no_bookkeeping" \
        '    let s = udp.open(9);
    io.println(len(udp.recv(s)));'; then
    if grep -q '__nexa_udp_peers\|getnameinfo' "$WORK/no_sender_no_bookkeeping.cpp"; then
        echo "FAIL no_sender_no_bookkeeping: the sender table was emitted anyway"
        fails=$((fails + 1))
    else
        echo "ok no_sender_no_bookkeeping"
    fi
fi

# --- slices: the other three platforms, not just this machine's -------------

echo "-- slices: every platform emission, not just this machine's"

ALL_VERBS='    let s = udp.open(0);
    io.println(udp.port(s));
    io.println(udp.send(s, "127.0.0.1", 9, "x"));
    io.println(len(udp.recv(s)));
    io.println(len(udp.recv(s, 8)));
    io.println(udp.sender(s) + udp.sender_port(s));
    io.println(udp.close(s));'

# slice <label> <flag> <symbol that must be there>... ; "!sym" must not be
slice() {
    label=$1
    flag=$2
    shift 2
    if [ -n "$flag" ]; then
        transpile "$label" "$ALL_VERBS" "$flag" || return
    else
        transpile "$label" "$ALL_VERBS" || return
    fi
    for sym in "$@"; do
        case "$sym" in
            !*)
                if grep -q "${sym#!}" "$WORK/$label.cpp"; then
                    echo "FAIL $label: ${sym#!} should have been sliced away"
                    fails=$((fails + 1))
                    return
                fi
                ;;
            *)
                if ! grep -q "$sym" "$WORK/$label.cpp"; then
                    echo "FAIL $label: missing $sym"
                    fails=$((fails + 1))
                    return
                fi
                ;;
        esac
    done
    echo "ok $label"
}

slice "slice_native" "" "__nexa_udp_open" "SOCK_DGRAM" "recvfrom" "!winsock2.h"
slice "slice_win" "--win" "winsock2.h" "WSAStartup" "closesocket" "SOCK_DGRAM" "!sys/socket.h"
slice "slice_macos" "--macos" "sys/socket.h" "SOCK_DGRAM" "!winsock2.h" "!WSAStartup"
# The wasm slice is stubs: the surface is all there, the sockets are not.
slice "slice_wasm" "--wasm" \
    "__nexa_udp_open" "__nexa_udp_port" "__nexa_udp_send" "__nexa_udp_recv" \
    "__nexa_udp_sender" "__nexa_udp_sender_port" "__nexa_udp_close" \
    "!sys/socket.h" "!winsock2.h" "!recvfrom" "!getnameinfo"

# --- compile: the emitted C++ still builds ----------------------------------

echo "-- compile: the emission builds"

CXX=""
for c in clang++ g++ c++; do
    command -v "$c" >/dev/null 2>&1 && { CXX=$c; break; }
done

if [ -z "$CXX" ]; then
    echo "skip compile: no C++ compiler on this machine"
    skips=$((skips + 1))
else
    if "$CXX" -std=c++17 -Wall -Wextra -c "$WORK/slice_native.cpp" \
            -o "$WORK/native.o" > "$WORK/native.cc.log" 2>&1 \
            && ! grep -q 'warning:' "$WORK/native.cc.log"; then
        echo "ok native_build"
    else
        echo "FAIL native_build: the emitted C++ does not build clean"
        sed 's/^/  /' "$WORK/native.cc.log" | head -n 20
        fails=$((fails + 1))
    fi

    # The wasm slice is pure C++ with no socket headers under it, so it
    # type-checks anywhere -- no emsdk needed, unlike http.*'s FETCH backend.
    if "$CXX" -std=c++17 -Wall -Wextra -D__EMSCRIPTEN__ -c "$WORK/slice_wasm.cpp" \
            -o "$WORK/wasm.o" > "$WORK/wasm.cc.log" 2>&1 \
            && ! grep -q 'warning:' "$WORK/wasm.cc.log"; then
        echo "ok wasm_build"
    else
        echo "FAIL wasm_build: the --wasm emission does not build clean"
        sed 's/^/  /' "$WORK/wasm.cc.log" | head -n 20
        fails=$((fails + 1))
    fi

    # A stream and a datagram socket in one program share the platform
    # preamble, so this is the emission that would catch a duplicate typedef.
    if transpile "both_protocols" \
            '    let c = tcp.connect("h", 9);
    let s = udp.open(0);
    io.println(len(tcp.recv(c)) + len(udp.recv(s)));
    io.println(udp.send(s, "h", 9, "x") + tcp.send(c, "x"));
    io.println(tcp.close(c) + udp.close(s));'; then
        if "$CXX" -std=c++17 -Wall -Wextra -c "$WORK/both_protocols.cpp" \
                -o "$WORK/both.o" > "$WORK/both.cc.log" 2>&1 \
                && ! grep -q 'warning:' "$WORK/both.cc.log"; then
            echo "ok both_protocols_build"
        else
            echo "FAIL both_protocols_build: tcp and udp together do not build clean"
            sed 's/^/  /' "$WORK/both.cc.log" | head -n 20
            fails=$((fails + 1))
        fi
    fi

    # http.*'s listening half has a platform layer of its own, under names that
    # read like this one's. A program that serves HTTP and opens a raw socket
    # emits both, and this is the only place that would catch them colliding.
    if transpile "http_server_and_sockets" \
            '    let s = http.localhost(0);
    let u = udp.open(0);
    let c = tcp.connect("h", 9);
    io.println(s.ok());
    io.println(udp.close(u) + tcp.close(c));'; then
        if "$CXX" -std=c++17 -Wall -Wextra -c "$WORK/http_server_and_sockets.cpp" \
                -o "$WORK/hs.o" > "$WORK/hs.cc.log" 2>&1 \
                && ! grep -q 'warning:' "$WORK/hs.cc.log"; then
            echo "ok http_server_and_sockets_build"
        else
            echo "FAIL http_server_and_sockets_build: http.localhost beside tcp/udp does not build clean"
            sed 's/^/  /' "$WORK/hs.cc.log" | head -n 20
            fails=$((fails + 1))
        fi
    fi

    # The Winsock half is the one backend nobody here can run, so compiling it
    # is the only check there is. A clang that can target mingw does it.
    if "$CXX" -target x86_64-windows-gnu -std=c++17 -Wall -Wextra \
            -c "$WORK/slice_win.cpp" -o "$WORK/win.o" \
            > "$WORK/win.cc.log" 2>&1; then
        if grep -q 'warning:' "$WORK/win.cc.log"; then
            echo "FAIL win_build: the --win emission builds with warnings"
            sed 's/^/  /' "$WORK/win.cc.log" | head -n 20
            fails=$((fails + 1))
        else
            echo "ok win_build"
        fi
    else
        echo "skip win_build: $CXX cannot target x86_64-windows-gnu here"
        skips=$((skips + 1))
    fi

    # A --wasm BUILD that calls udp.* is refused: a page has no datagram socket
    # and no browser API opens one, so the stubs above exist to keep the
    # emission compiling and not to be shipped. NEXA_WASM_CXX stands in for a
    # wasm toolchain, since the refusal happens before anything is compiled.
    if NEXA_WASM_CXX="$CXX" "$NEXAC" "$WORK/slice_native.nxa" --wasm \
            -o "$WORK/udp.wasm" > "$WORK/wasmgate.log" 2>&1; then
        echo "FAIL wasm_gate: a --wasm build of a udp program was allowed"
        fails=$((fails + 1))
    elif grep -q 'udp\.\* is not available on WASM' "$WORK/wasmgate.log"; then
        echo "ok wasm_gate"
    else
        echo "skip wasm_gate: no wasm toolchain could be stood up here"
        sed 's/^/  /' "$WORK/wasmgate.log" | head -n 5
        skips=$((skips + 1))
    fi
fi

# --- loopback: udp.* against a peer that is not Nexa ------------------------

echo "-- loopback: udp.*, against plain Python sockets"

PY=""
for p in python3 python; do
    command -v "$p" >/dev/null 2>&1 && { PY=$p; break; }
done

if [ -z "$CXX" ]; then
    echo "skip loopback: no C++ compiler on this machine"
    skips=$((skips + 1))
elif [ -z "$PY" ]; then
    echo "skip loopback: no python on this machine"
    skips=$((skips + 1))
elif ! "$NEXAC" "$SUITE/udp_loopback_test.nxa" -o "$WORK/loopback" \
        > "$WORK/loopback.log" 2>&1; then
    echo "FAIL loopback: could not build udp_loopback_test.nxa"
    sed 's/^/  /' "$WORK/loopback.log" | head -n 20
    fails=$((fails + 1))
else
    # The peer writes the port it bound to argv[1], so no layer of this suite
    # ever picks a port and hopes it was free.
    "$PY" "$SUITE/udp_peer.py" "$WORK/port" > "$WORK/peer.log" 2>&1 &
    PEER_PID=$!
    tries=0
    while [ ! -s "$WORK/port" ] && [ $tries -lt 50 ]; do
        sleep 0.2 2>/dev/null || sleep 1
        tries=$((tries + 1))
    done
    if [ ! -s "$WORK/port" ]; then
        echo "skip loopback: the peer could not bind a loopback port"
        sed 's/^/  /' "$WORK/peer.log"
        skips=$((skips + 1))
    else
        "$WORK/loopback" "$(cat "$WORK/port")" > "$WORK/loopback.out" 2>&1
        if diff -u "$SUITE/udp_loopback_test.expected" "$WORK/loopback.out" \
                > "$WORK/loopback.diff" 2>&1; then
            echo "ok loopback"
        else
            echo "FAIL loopback: output moved"
            sed 's/^/  /' "$WORK/loopback.diff" | head -n 30
            sed 's/^/  peer: /' "$WORK/peer.log" | head -n 10
            fails=$((fails + 1))
        fi
    fi
    wait "$PEER_PID" 2>/dev/null
    PEER_PID=""
fi

# --- server and mixed: both ends Nexa, in one program -----------------------

# runs <label> <source> <expected> <what it proves>
runs() {
    label=$1
    if [ -z "$CXX" ]; then
        echo "skip $label: no C++ compiler on this machine"
        skips=$((skips + 1))
        return
    fi
    if ! "$NEXAC" "$SUITE/$2" -o "$WORK/$label" > "$WORK/$label.log" 2>&1; then
        echo "FAIL $label: could not build $2"
        sed 's/^/  /' "$WORK/$label.log" | head -n 20
        fails=$((fails + 1))
        return
    fi
    "$WORK/$label" > "$WORK/$label.out" 2>&1
    if diff -u "$SUITE/$3" "$WORK/$label.out" > "$WORK/$label.diff" 2>&1; then
        echo "ok $label"
    else
        echo "FAIL $label: output moved"
        sed 's/^/  /' "$WORK/$label.diff" | head -n 30
        fails=$((fails + 1))
    fi
}

echo "-- server: one socket answering this program's own datagrams"
runs "server" "udp_server_test.nxa" "udp_server_test.expected"

echo "-- mixed: one include, a stream and a datagram socket at once"
runs "mixed" "network_mixed_test.nxa" "network_mixed_test.expected"

# --- report -----------------------------------------------------------------

if [ $fails -eq 0 ]; then
    if [ $skips -gt 0 ]; then
        echo "network ok ($skips layer(s) skipped: see above)"
    else
        echo "network ok"
    fi
    exit 0
fi
echo "network: $fails failure(s)"
exit 1
