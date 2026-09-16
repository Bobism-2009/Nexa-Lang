#!/bin/sh
# std/http cover (BOB-35).
#
# std/http reaches the network, and the four backends it is built on -- WinHTTP,
# CFNetwork, Emscripten FETCH, POSIX sockets -- are each reachable from exactly
# one kind of machine. The cover is therefore layered, cheapest and most
# portable first, so every machine runs as much of it as it can:
#
#   codegen   Transpile only (--source). Pins the call each http.* form emits:
#             the verb string, the empty body and empty header list NexaC fills
#             in for the arguments a program left off, and which half of the
#             runtime a program carries -- get/post/put/patch/delete reach
#             __nexa_http_simple, http.request reaches the response struct, and
#             neither drags in the other. Never invokes the C++ compiler or the
#             network, so it runs anywhere NexaC itself runs.
#
#   compile   The emitted C++ still builds, for the native backend and for
#             --wasm. The wasm half needs Emscripten's <emscripten/fetch.h>;
#             point NEXA_EM_INCLUDE at an include directory holding it (an
#             emsdk sysroot, say) to run it. Skipped without a C++ compiler,
#             and the wasm half skipped without those headers.
#
#   loopback  Build Tests/http_loopback_test.nxa and run it against
#             Tests/http_server.nxa on 127.0.0.1, diffing the output against
#             http_loopback_test.expected. This is the only layer that
#             exercises the client transport itself: chunked framing, redirect
#             following and its bound, request headers, Content-Length, the
#             non-2xx split between the simple verbs and http.request. Both
#             ends are Nexa -- the server used to be a Python script, and is
#             now http.localhost() answering with http.reply and http.raw.
#             Needs a C++ compiler and a loopback socket.
#
#   server    Build Tests/http_server_test.nxa and run it: one program that
#             binds a loopback port, answers four requests from a worker
#             thread, and asks them with the client verbs. This is the layer
#             that covers the server half -- the bound port, the request the
#             server sees, reply's framing, raw's bytes, and close actually
#             closing. Needs a C++ compiler and a loopback socket.
#
# Unknown-verb and arity diagnostics live in Tests/Lang/errors/http_*.nxa,
# where the run_tests.sh error phase already checks that NexaC does the
# complaining and clang never gets to speak.
#
# Skips are reported but do not fail the run. Note that run_tests.sh only
# prints a suite's output when it fails, so run this directly to see which
# layers a machine actually ran:
#
#   sh Tests/http_cases.sh           (from the repo root)
#
# Usage: Tests/http_cases.sh [path-to-NexaC]

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
    [ -n "${SRV_PID:-}" ] && kill "$SRV_PID" 2>/dev/null
    rm -rf "$WORK"
}
trap cleanup EXIT INT TERM

fails=0
skips=0
SRV_PID=""

# transpile <name> <body> [extra NexaC flags...]
# Writes a one-function http program and transpiles it. Reports the failure
# itself and returns non-zero if NexaC refuses.
transpile() {
    name=$1
    body=$2
    shift 2
    printf '#include <std/http>\n#include <std/io>\nfn main() {\n%s\n}\n' "$body" > "$WORK/$name.nxa"
    if ! "$NEXAC" "$WORK/$name.nxa" "$@" --source "$WORK/$name.cpp" \
            > "$WORK/$name.log" 2>&1; then
        echo "FAIL $name: NexaC could not transpile"
        sed 's/^/  /' "$WORK/$name.log"
        fails=$((fails + 1))
        return 1
    fi
    return 0
}

# --- codegen: the call each http.* form emits -------------------------------

# emits <label> <body> <expected substring>
emits() {
    label=$1
    transpile "$label" "$2" || return
    if ! grep -qF "$3" "$WORK/$label.cpp"; then
        echo "FAIL $label: expected to emit"
        echo "  $3"
        grep -n '__nexa_http_simple\|__nexa_http_request(' "$WORK/$label.cpp" | sed 's/^/  got /'
        fails=$((fails + 1))
        return
    fi
    echo "ok $label"
}

echo "-- codegen: the call each http verb emits"

emits "get" \
    '    let r = http.get("http://h/p");' \
    '__nexa_http_simple("GET", "http://h/p", std::string(), std::vector<std::string>())'
emits "get_headers" \
    '    let r = http.get("http://h/p", ["A: 1"]);' \
    '__nexa_http_simple("GET", "http://h/p", std::string(), std::vector<std::string>{"A: 1"})'
emits "post" \
    '    let r = http.post("http://h/p", "b");' \
    '__nexa_http_simple("POST", "http://h/p", "b", std::vector<std::string>())'
emits "post_headers" \
    '    let r = http.post("http://h/p", "b", ["A: 1"]);' \
    '__nexa_http_simple("POST", "http://h/p", "b", std::vector<std::string>{"A: 1"})'
emits "put" \
    '    let r = http.put("http://h/p", "b");' \
    '__nexa_http_simple("PUT", "http://h/p", "b", std::vector<std::string>())'
emits "patch" \
    '    let r = http.patch("http://h/p", "b");' \
    '__nexa_http_simple("PATCH", "http://h/p", "b", std::vector<std::string>())'
emits "delete" \
    '    let r = http.delete("http://h/p");' \
    '__nexa_http_simple("DELETE", "http://h/p", std::string(), std::vector<std::string>())'
emits "delete_headers" \
    '    let r = http.delete("http://h/p", ["A: 1"]);' \
    '__nexa_http_simple("DELETE", "http://h/p", std::string(), std::vector<std::string>{"A: 1"})'
emits "request" \
    '    let r = http.request("HEAD", "http://h/p", "");' \
    '__nexa_http_request("HEAD", "http://h/p", "", std::vector<std::string>())'
emits "request_headers" \
    '    let r = http.request("HEAD", "http://h/p", "", ["A: 1"]);' \
    '__nexa_http_request("HEAD", "http://h/p", "", std::vector<std::string>{"A: 1"})'

# HttpResponse is the runtime's struct: a local holding one gets that C++ type
# rather than a definition NexaC emitted alongside it.
emits "response_is_the_runtime_struct" \
    '    let r = http.request("GET", "http://h/p", "");
    let v = r.value();
    io.println(v.status);
    io.println(v.body);
    io.println(len(v.headers));' \
    '__nexa_http_response __nexa_var_'
# And .field reads the field'\''s type -- a string field concatenates, it does
# not go through std::to_string.
emits "response_fields_keep_their_types" \
    '    let r = http.request("GET", "http://h/p", "");
    io.println("b " + r.value().body + " s " + r.value().status);' \
    'std::string("b ") + __nexa_var_0.value().body'

echo "-- codegen: the call each http server verb emits"

emits "localhost" \
    '    let s = http.localhost();' \
    '__nexa_http_localhost(0)'
emits "localhost_port" \
    '    let s = http.localhost(8080);' \
    '__nexa_http_localhost(8080)'
emits "accept" \
    '    let s = http.localhost();
    let r = http.accept(s.value());' \
    '__nexa_http_accept(__nexa_var_0.value())'
emits "reply" \
    '    let s = http.localhost();
    let r = http.accept(s.value());
    http.reply(r.value(), 200, "b");' \
    '__nexa_http_reply(__nexa_var_1.value(), 200, "b", std::vector<std::string>())'
emits "reply_headers" \
    '    let s = http.localhost();
    let r = http.accept(s.value());
    http.reply(r.value(), 200, "b", ["A: 1"]);' \
    '__nexa_http_reply(__nexa_var_1.value(), 200, "b", std::vector<std::string>{"A: 1"})'
emits "raw" \
    '    let s = http.localhost();
    let r = http.accept(s.value());
    http.raw(r.value(), "HTTP/1.1 200 OK\r\n\r\n");' \
    '__nexa_http_raw(__nexa_var_1.value(), "HTTP/1.1 200 OK'
emits "close" \
    '    let s = http.localhost();
    http.close(s.value());' \
    '__nexa_http_close(__nexa_var_0.value())'

# A server call written for what it does, with nobody keeping the 1/0 it hands
# back, still has to reach the emitted C++.
emits "reply_as_a_statement_is_not_dropped" \
    '    let s = http.localhost();
    let r = http.accept(s.value());
    http.reply(r.value(), 204, "");' \
    '(void)(__nexa_http_reply('

# HttpServer and HttpRequest are the runtime's structs, the same way
# HttpResponse is.
emits "server_structs_are_the_runtime_structs" \
    '    let s = http.localhost();
    let srv = s.value();
    let r = http.accept(srv);
    let req = r.value();
    io.println(srv.port);
    io.println(req.method + req.path + req.body);
    io.println(len(req.headers));' \
    '__nexa_http_incoming __nexa_var_'

echo "-- codegen: a program carries only the half it calls"

# carries <label> <body> <yes|no simple> <yes|no response> <yes|no server>
# The client transport under the first two is checked too: a program that only
# serves carries no __nexa_http_perform, and so none of the TLS machinery.
carries() {
    label=$1
    transpile "$label" "$2" || return
    case "$3$4" in *yes*) client=yes ;; *) client=no ;; esac
    for pair in "__nexa_http_simple:$3" "__nexa_http_response:$4" \
                "__nexa_http_localhost:$5" "__nexa_http_perform:$client"; do
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

carries "simple_only_has_no_response_struct" \
    '    let r = http.get("http://h/p");
    io.println(r.ok());' yes no no
carries "request_only_has_no_simple_verb" \
    '    let r = http.request("GET", "http://h/p", "");
    io.println(r.value().status);' no yes no
carries "both_carries_both" \
    '    let a = http.get("http://h/p");
    let b = http.request("GET", "http://h/p", "");
    io.println(a.ok());
    io.println(b.value().status);' yes yes no
carries "server_only_carries_no_client" \
    '    let s = http.localhost();
    let r = http.accept(s.value());
    http.reply(r.value(), 200, "hi");' no no yes
carries "client_only_carries_no_server" \
    '    let r = http.get("http://h/p");
    io.println(r.ok());' yes no no

# The wasm target emits the same calls; only the backend under them differs.
echo "-- codegen: --wasm emits the same surface over the FETCH backend"

ALL_VERBS='    let a = http.get("http://h/p", ["A: 1"]);
    let b = http.post("http://h/p", "b");
    let c = http.put("http://h/p", "b");
    let d = http.patch("http://h/p", "b");
    let e = http.delete("http://h/p");
    let f = http.request("GET", "http://h/p", "");
    let s = http.localhost();
    let q = http.accept(s.value());
    http.reply(q.value(), 200, "b", ["A: 1"]);
    http.raw(q.value(), "x");
    http.close(s.value());
    io.println(a.ok() + b.ok() + c.ok() + d.ok() + e.ok() + f.value().status);'

if transpile "wasm_surface" "$ALL_VERBS" --wasm; then
    missing=""
    for sym in emscripten_fetch_attr_t requestHeaders \
               emscripten_fetch_get_response_headers __nexa_http_simple \
               __nexa_http_request __nexa_http_response __nexa_http_localhost \
               __nexa_http_incoming; do
        grep -q "$sym" "$WORK/wasm_surface.cpp" || missing="$missing $sym"
    done
    if [ -n "$missing" ]; then
        echo "FAIL wasm_surface: missing$missing"
        fails=$((fails + 1))
    else
        echo "ok wasm_surface"
    fi
fi

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
    if transpile "native_build" "$ALL_VERBS"; then
        if "$CXX" -std=c++17 -Wall -Wextra -c "$WORK/native_build.cpp" \
                -o "$WORK/native_build.o" > "$WORK/native_build.cc.log" 2>&1 \
                && ! grep -q 'warning:' "$WORK/native_build.cc.log"; then
            echo "ok native_build"
        else
            echo "FAIL native_build: the emitted C++ does not build clean"
            sed 's/^/  /' "$WORK/native_build.cc.log" | head -n 20
            fails=$((fails + 1))
        fi
    fi

    # The FETCH backend needs Emscripten's headers to type-check. They are not
    # something NexaC ships, so this half runs only when pointed at them.
    if [ -n "${NEXA_EM_INCLUDE:-}" ] && [ -f "$NEXA_EM_INCLUDE/emscripten/fetch.h" ]; then
        if "$CXX" -std=c++17 -Wall -Wextra -D__EMSCRIPTEN__ -I"$NEXA_EM_INCLUDE" \
                -c "$WORK/wasm_surface.cpp" -o "$WORK/wasm_surface.o" \
                > "$WORK/wasm_surface.cc.log" 2>&1 \
                && ! grep -q 'warning:' "$WORK/wasm_surface.cc.log"; then
            echo "ok wasm_build"
        else
            echo "FAIL wasm_build: the --wasm emission does not build clean"
            sed 's/^/  /' "$WORK/wasm_surface.cc.log" | head -n 20
            fails=$((fails + 1))
        fi
    else
        echo "skip wasm_build: set NEXA_EM_INCLUDE to a dir holding emscripten/fetch.h"
        skips=$((skips + 1))
    fi
fi

# --- loopback: the client transport, against a Nexa server ------------------

echo "-- loopback: the client transport, against a server written in Nexa"

if [ -z "$CXX" ]; then
    echo "skip loopback: no C++ compiler on this machine"
    skips=$((skips + 1))
elif ! "$NEXAC" "$SUITE/http_server.nxa" -o "$WORK/server" > "$WORK/server.log" 2>&1; then
    echo "FAIL loopback: could not build http_server.nxa"
    sed 's/^/  /' "$WORK/server.log" | head -n 20
    fails=$((fails + 1))
else
    # The server writes the port it was given to argv[1], so no layer of this
    # suite ever picks a port and hopes it was free.
    "$WORK/server" "$WORK/port" > "$WORK/server.run.log" 2>&1 &
    SRV_PID=$!
    tries=0
    while [ ! -s "$WORK/port" ] && [ $tries -lt 50 ]; do
        sleep 0.2 2>/dev/null || sleep 1
        tries=$((tries + 1))
    done
    if [ ! -s "$WORK/port" ]; then
        echo "skip loopback: could not bind a loopback port"
        sed 's/^/  /' "$WORK/server.run.log"
        skips=$((skips + 1))
    elif ! "$NEXAC" "$SUITE/http_loopback_test.nxa" -o "$WORK/loopback" \
            > "$WORK/loopback.log" 2>&1; then
        echo "FAIL loopback: could not build http_loopback_test.nxa"
        sed 's/^/  /' "$WORK/loopback.log" | head -n 20
        fails=$((fails + 1))
    else
        "$WORK/loopback" "http://127.0.0.1:$(cat "$WORK/port")" > "$WORK/loopback.out" 2>&1
        if diff -u "$SUITE/http_loopback_test.expected" "$WORK/loopback.out" \
                > "$WORK/loopback.diff" 2>&1; then
            echo "ok loopback"
        else
            echo "FAIL loopback: output moved"
            sed 's/^/  /' "$WORK/loopback.diff" | head -n 30
            fails=$((fails + 1))
        fi
    fi
fi

# --- server: the listening half, both ends in one program -------------------

echo "-- server: http.localhost() answering this program's own client calls"

if [ -z "$CXX" ]; then
    echo "skip server: no C++ compiler on this machine"
    skips=$((skips + 1))
elif ! "$NEXAC" "$SUITE/http_server_test.nxa" -o "$WORK/server_test" \
        > "$WORK/server_test.log" 2>&1; then
    echo "FAIL server: could not build http_server_test.nxa"
    sed 's/^/  /' "$WORK/server_test.log" | head -n 20
    fails=$((fails + 1))
else
    "$WORK/server_test" > "$WORK/server_test.out" 2>&1
    if diff -u "$SUITE/http_server_test.expected" "$WORK/server_test.out" \
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
        echo "http ok ($skips layer(s) skipped: see above)"
    else
        echo "http ok"
    fi
    exit 0
fi
echo "http: $fails failure(s)"
exit 1
