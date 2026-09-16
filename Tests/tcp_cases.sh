#!/bin/sh
# std/tcp cover (BOB-44).
#
# std/tcp is one implementation over two socket APIs -- Winsock on Windows,
# POSIX everywhere else -- and only one of them is reachable from any given
# machine. The cover is therefore layered, cheapest and most portable first, so
# every machine runs as much of it as it can:
#
#   codegen   Transpile only (--source). Pins the call each tcp.* form emits,
#            including the 64K cap NexaC fills in for a tcp.recv that left one
#            off, and which half of the runtime a program carries -- tcp.connect
#            reaches name resolution, tcp.listen/accept reach bind/listen, and
#            neither drags in the other. Never invokes the C++ compiler or a
#            socket, so it runs anywhere NexaC itself runs.
#
#   slices    The Windows, macOS and wasm emissions, not just this machine's.
#            Each is checked for the backend it should be standing on, and the
#            Windows one is compiled for real when a mingw-capable clang is
#            around. The wasm slice is the interesting one: a page has no
#            socket, so every call there is a stub, and this layer is what
#            keeps them in step with the surface the other three implement.
#
#   compile   The emitted C++ still builds warning-clean for the native
#            backend. Skipped without a C++ compiler.
#
#   loopback  Build Tests/tcp_loopback_test.nxa and run it against
#            Tests/tcp_peer.py on 127.0.0.1, diffing the output against
#            tcp_loopback_test.expected. The peer is plain Python sockets, so
#            this is the layer that proves a Nexa program speaks TCP and not
#            just its own conventions: a line exchange, 120KB through
#            tcp.send's write-until-done loop, a NUL-bearing read, and a
#            hang-up seen as "". Needs a C++ compiler, python3 and a loopback
#            socket.
#
#   server    Build Tests/tcp_server_test.nxa and run it: one program that
#            binds a port, answers two connections from a worker thread, and
#            is the client that makes them. This is the layer that covers the
#            listening half -- the port the OS picked, accept, serving more
#            than one connection, and close actually closing. Needs a C++
#            compiler and a loopback socket.
#
# Unknown-function and arity diagnostics live in Tests/Lang/errors/tcp_*.nxa,
# where the run_tests.sh error phase already checks that NexaC does the
# complaining and clang never gets to speak.
#
# Skips are reported but do not fail the run. Note that run_tests.sh only
# prints a suite's output when it fails, so run this directly to see which
# layers a machine actually ran:
#
#   sh Tests/tcp_cases.sh            (from the repo root)
#
# Usage: Tests/tcp_cases.sh [path-to-NexaC]

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
# Writes a one-function tcp program and transpiles it. Reports the failure
# itself and returns non-zero if NexaC refuses.
transpile() {
    name=$1
    body=$2
    shift 2
    printf '#include <std/tcp>\n#include <std/io>\nfn main() {\n%s\n}\n' "$body" > "$WORK/$name.nxa"
    if ! "$NEXAC" "$WORK/$name.nxa" "$@" --source "$WORK/$name.cpp" \
            > "$WORK/$name.log" 2>&1; then
        echo "FAIL $name: NexaC could not transpile"
        sed 's/^/  /' "$WORK/$name.log"
        fails=$((fails + 1))
        return 1
    fi
    return 0
}

# --- codegen: the call each tcp.* form emits --------------------------------

# emits <label> <body> <expected substring>
emits() {
    label=$1
    transpile "$label" "$2" || return
    if ! grep -qF "$3" "$WORK/$label.cpp"; then
        echo "FAIL $label: expected to emit"
        echo "  $3"
        grep -n '__nexa_tcp_' "$WORK/$label.cpp" | sed 's/^/  got /'
        fails=$((fails + 1))
        return
    fi
    echo "ok $label"
}

echo "-- codegen: the call each tcp verb emits"

emits "connect" \
    '    let c = tcp.connect("h", 9);' \
    '__nexa_tcp_connect("h", 9)'
emits "listen" \
    '    let l = tcp.listen(9);' \
    '__nexa_tcp_listen(9)'
emits "accept" \
    '    let l = tcp.listen(9);
    let a = tcp.accept(l);' \
    '__nexa_tcp_accept('
emits "send" \
    '    let c = tcp.connect("h", 9);
    let n = tcp.send(c, "hi");' \
    ', "hi")'
emits "port" \
    '    let l = tcp.listen(0);
    io.println(tcp.port(l));' \
    '__nexa_tcp_port('
emits "close" \
    '    let c = tcp.connect("h", 9);
    tcp.close(c);' \
    '__nexa_tcp_close('

# The cap on tcp.recv is NexaC's to fill in when a program leaves it off: 64K,
# which is a whole TCP window. A cap the program did give is passed through.
emits "recv_default_cap" \
    '    let c = tcp.connect("h", 9);
    let s = tcp.recv(c);' \
    ', 65536)'
emits "recv_given_cap" \
    '    let c = tcp.connect("h", 9);
    let s = tcp.recv(c, 12);' \
    ', 12)'

# A call written for what it does, with nobody keeping the count or the 1/0 it
# hands back, still has to reach the emitted C++.
emits "send_as_a_statement_is_not_dropped" \
    '    let c = tcp.connect("h", 9);
    tcp.send(c, "x");' \
    '(void)(__nexa_tcp_send('

# A handle is an int and 0 is failure, so the handle is its own condition.
emits "handle_is_its_own_condition" \
    '    if (tcp.connect("h", 9)) { io.println("up"); }' \
    'if (__nexa_tcp_connect('

# tcp.recv is the one call that hands back a string, so it concatenates as text
# rather than going through std::to_string.
emits "recv_is_a_string" \
    '    let c = tcp.connect("h", 9);
    io.println("got " + tcp.recv(c));' \
    'std::string("got ") + __nexa_tcp_recv('

echo "-- codegen: a program carries only the half it calls"

# carries <label> <body> <yes|no connect> <yes|no listen>
# Name resolution rides with tcp.connect and bind/listen with tcp.listen, so
# each is checked through the call and through the OS machinery under it.
carries() {
    label=$1
    transpile "$label" "$2" || return
    for pair in "__nexa_tcp_connect:$3" "getaddrinfo:$3" \
                "__nexa_tcp_listen:$4" "__nexa_tcp_accept:$4"; do
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

carries "client_only_carries_no_listener" \
    '    let c = tcp.connect("h", 9);
    io.println(tcp.send(c, "x"));
    tcp.close(c);' yes no
carries "server_only_carries_no_resolver" \
    '    let l = tcp.listen(9);
    let a = tcp.accept(l);
    io.println(len(tcp.recv(a)));
    tcp.close(a);' no yes
carries "both_carries_both" \
    '    let c = tcp.connect("h", 9);
    let l = tcp.listen(9);
    let a = tcp.accept(l);
    io.println(tcp.send(c, "x") + len(tcp.recv(a)));' yes yes

# A program that includes std/tcp and never calls it pays nothing at all.
if transpile "include_without_calls" '    io.println("hi");'; then
    if grep -q '__nexa_tcp_\|socket(' "$WORK/include_without_calls.cpp"; then
        echo "FAIL include_without_calls: the runtime was emitted anyway"
        fails=$((fails + 1))
    else
        echo "ok include_without_calls"
    fi
fi

# --- slices: the other three platforms, not just this machine's -------------

echo "-- slices: every platform emission, not just this machine's"

ALL_VERBS='    let c = tcp.connect("127.0.0.1", 9);
    let l = tcp.listen(0);
    let a = tcp.accept(l);
    io.println(tcp.port(l));
    io.println(tcp.send(c, "x"));
    io.println(len(tcp.recv(a)));
    io.println(len(tcp.recv(a, 8)));
    io.println(tcp.close(c) + tcp.close(a) + tcp.close(l));'

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

slice "slice_native" "" "__nexa_tcp_connect" "!winsock2.h"
slice "slice_win" "--win" "winsock2.h" "WSAStartup" "closesocket" "!sys/socket.h"
slice "slice_macos" "--macos" "sys/socket.h" "!winsock2.h" "!WSAStartup"
# The wasm slice is stubs: the surface is all there, the sockets are not.
slice "slice_wasm" "--wasm" \
    "__nexa_tcp_connect" "__nexa_tcp_listen" "__nexa_tcp_accept" \
    "__nexa_tcp_send" "__nexa_tcp_recv" "__nexa_tcp_port" "__nexa_tcp_close" \
    "!sys/socket.h" "!winsock2.h" "!getaddrinfo"

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
    # type-checks anywhere -- no emsdk needed, unlike std/http's FETCH backend.
    if "$CXX" -std=c++17 -Wall -Wextra -D__EMSCRIPTEN__ -c "$WORK/slice_wasm.cpp" \
            -o "$WORK/wasm.o" > "$WORK/wasm.cc.log" 2>&1 \
            && ! grep -q 'warning:' "$WORK/wasm.cc.log"; then
        echo "ok wasm_build"
    else
        echo "FAIL wasm_build: the --wasm emission does not build clean"
        sed 's/^/  /' "$WORK/wasm.cc.log" | head -n 20
        fails=$((fails + 1))
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
fi

# --- loopback: std/tcp against a peer that is not Nexa ----------------------

echo "-- loopback: the dialling half, against plain Python sockets"

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
elif ! "$NEXAC" "$SUITE/tcp_loopback_test.nxa" -o "$WORK/loopback" \
        > "$WORK/loopback.log" 2>&1; then
    echo "FAIL loopback: could not build tcp_loopback_test.nxa"
    sed 's/^/  /' "$WORK/loopback.log" | head -n 20
    fails=$((fails + 1))
else
    # The peer writes the port it bound to argv[1], so no layer of this suite
    # ever picks a port and hopes it was free.
    "$PY" "$SUITE/tcp_peer.py" "$WORK/port" > "$WORK/peer.log" 2>&1 &
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
        if diff -u "$SUITE/tcp_loopback_test.expected" "$WORK/loopback.out" \
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

# --- server: the listening half, both ends in one program -------------------

echo "-- server: tcp.listen answering this program's own tcp.connect"

if [ -z "$CXX" ]; then
    echo "skip server: no C++ compiler on this machine"
    skips=$((skips + 1))
elif ! "$NEXAC" "$SUITE/tcp_server_test.nxa" -o "$WORK/server_test" \
        > "$WORK/server_test.log" 2>&1; then
    echo "FAIL server: could not build tcp_server_test.nxa"
    sed 's/^/  /' "$WORK/server_test.log" | head -n 20
    fails=$((fails + 1))
else
    "$WORK/server_test" > "$WORK/server_test.out" 2>&1
    if diff -u "$SUITE/tcp_server_test.expected" "$WORK/server_test.out" \
            > "$WORK/server_test.diff" 2>&1; then
        echo "ok server"
    else
        echo "FAIL server: output moved"
        sed 's/^/  /' "$WORK/server_test.diff" | head -n 30
        fails=$((fails + 1))
    fi
fi

# --- report -----------------------------------------------------------------

if [ $fails -eq 0 ]; then
    if [ $skips -gt 0 ]; then
        echo "tcp ok ($skips layer(s) skipped: see above)"
    else
        echo "tcp ok"
    fi
    exit 0
fi
echo "tcp: $fails failure(s)"
exit 1
