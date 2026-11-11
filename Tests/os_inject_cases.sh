#!/bin/sh
# os.inject_dll and os.alloc_console: slice emission and basic behaviour.
#
# Usage: Tests/os_inject_cases.sh [path-to-NexaC]

set -u

NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*) ;;
    *) NEXAC="$(pwd)/$NEXAC" ;;
esac
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexainject)
trap 'rm -rf "$WORK"' EXIT
FAIL=0

ok() { echo "ok   $1"; }
bad() { FAIL=$((FAIL + 1)); echo "FAIL $1: $2"; }

WINDOWS=0
case "$(uname -s 2>/dev/null)" in
    MINGW*|MSYS*|CYGWIN*) WINDOWS=1 ;;
esac
EXE=""
[ "$WINDOWS" = 1 ] && EXE=".exe"

cat > "$WORK/slice.nxa" <<'EOF'
#include <std/io>
fn main() {
    io.println("hi");
}
EOF
if ! "$NEXAC" "$WORK/slice.nxa" --source "$WORK/slice.cpp" > "$WORK/slice.log" 2>&1; then
    bad "slice transpile" "$(cat "$WORK/slice.log")"
elif grep -q __nexa_os_inject_dll "$WORK/slice.cpp" 2>/dev/null; then
    bad "slice" "inject helper emitted without os.inject_dll"
elif grep -q __nexa_os_alloc_console "$WORK/slice.cpp" 2>/dev/null; then
    bad "slice" "alloc_console helper emitted without os.alloc_console"
else
    ok "programs without inject omit helpers"
fi

cat > "$WORK/use.nxa" <<'EOF'
#include <std/os>
fn main() {
    let a = os.inject_dll(0, "x.dll");
    let b = os.alloc_console();
}
EOF
if ! "$NEXAC" "$WORK/use.nxa" --source "$WORK/use.cpp" > "$WORK/use.log" 2>&1; then
    bad "use transpile" "$(cat "$WORK/use.log")"
elif ! grep -q __nexa_os_inject_dll "$WORK/use.cpp" 2>/dev/null; then
    bad "use" "missing __nexa_os_inject_dll"
elif ! grep -q __nexa_os_inject_dll_path "$WORK/use.cpp" 2>/dev/null; then
    bad "use" "missing relative dll path helper"
elif ! grep -q __nexa_os_alloc_console "$WORK/use.cpp" 2>/dev/null; then
    bad "use" "missing __nexa_os_alloc_console"
else
    ok "inject and alloc_console helpers emitted when used"
fi

cat > "$WORK/simple.nxa" <<'EOF'
#include <std/os>
fn main() {
    let ok = os.inject("notepad.exe", "hook.dll");
}
EOF
if ! "$NEXAC" "$WORK/simple.nxa" --source "$WORK/simple.cpp" > "$WORK/simple.log" 2>&1; then
    bad "simple transpile" "$(cat "$WORK/simple.log")"
elif ! grep -q __nexa_os_inject "$WORK/simple.cpp" 2>/dev/null; then
    bad "simple" "missing __nexa_os_inject wrapper"
else
    ok "os.inject emits one-call wrapper"
fi

# A still-running LoadLibrary leaves exit code STILL_ACTIVE (259). That must
# not count as a loaded module, and the path buffer must stay until the thread exits.
if ! grep -q 'waited == WAIT_OBJECT_0' "$WORK/use.cpp" 2>/dev/null; then
    bad "inject wait" "path is freed without waiting for LoadLibrary to finish"
elif grep -q 'return mod ? 1 : 0' "$WORK/use.cpp" 2>/dev/null; then
    bad "inject wait" "STILL_ACTIVE still counts as success"
else
    ok "inject frees the path only after LoadLibrary returns"
fi

cat > "$WORK/dllinit.nxa" <<'EOF'
fn __init__() {
}
EOF
if ! "$NEXAC" "$WORK/dllinit.nxa" --dll --source "$WORK/dllinit.cpp" > "$WORK/dllinit.log" 2>&1; then
    bad "dll init transpile" "$(cat "$WORK/dllinit.log")"
elif ! grep -q __nexa_dll_init_thread "$WORK/dllinit.cpp" 2>/dev/null; then
    bad "dll init" "DllMain calls __init__ on the loader lock"
elif ! awk '
    /BOOL WINAPI DllMain/ { inside = 1 }
    inside && /__init__\(\)/ { hit = 1 }
    inside && /return TRUE/ { inside = 0 }
    END { exit hit ? 1 : 0 }
' "$WORK/dllinit.cpp"; then
    bad "dll init" "DllMain still calls __init__ directly"
else
    ok "DllMain starts __init__ on its own thread"
fi

if [ "$WINDOWS" = 1 ]; then
    cat > "$WORK/console.nxa" <<'EOF'
#include <std/io>
#include <std/os>
fn main() {
    if (os.alloc_console() != 1) {
        io.println("no console");
        return;
    }
    io.println("console ok");
}
EOF
    if ! "$NEXAC" "$WORK/console.nxa" -o "$WORK/console$EXE" > "$WORK/console.log" 2>&1; then
        bad "console build" "$(cat "$WORK/console.log")"
    else
        out=$("$WORK/console$EXE" 2>&1)
        if [ "$out" = "console ok" ]; then
            ok "alloc_console enables io.println"
        else
            bad "console run" "expected 'console ok', got '$out'"
        fi
    fi

    # Joining a thread from DllMain never returns: the new thread cannot run
    # until the loader lock is released. __init__ has to run after DllMain.
    cat > "$WORK/initlib.nxa" <<'EOF'
#include <std/thread>
#include <std/file>
fn mark() {
    file.write("init_ok.txt", "1");
}
fn __init__() {
    let t = thread.spawn(mark);
    thread.join(t);
}
EOF
    cat > "$WORK/inithost.nxa" <<'EOF'
#include <std/dll>
#include <std/file>
#include <std/time>
fn main() {
    let h = dll.load("initlib.dll");
    if (h < 0) { return 2; }
    let i = 0;
    while (i < 50) {
        if (file.read("init_ok.txt") == "1") { return 0; }
        time.sleep(20);
        i = i + 1;
    }
    return 3;
}
EOF
    if ! "$NEXAC" "$WORK/initlib.nxa" --dll -o "$WORK/initlib.dll" > "$WORK/initlib.log" 2>&1; then
        bad "dll init build" "$(cat "$WORK/initlib.log")"
    elif ! "$NEXAC" "$WORK/inithost.nxa" -o "$WORK/inithost$EXE" > "$WORK/inithost.log" 2>&1; then
        bad "dll init host" "$(cat "$WORK/inithost.log")"
    else
        rm -f "$WORK/init_ok.txt"
        ( cd "$WORK" && "./inithost$EXE" > host.out 2>&1 ) &
        hp=$!
        n=0
        while [ "$n" -lt 25 ]; do
            if ! kill -0 "$hp" 2>/dev/null; then
                break
            fi
            sleep 0.2
            n=$((n + 1))
        done
        if kill -0 "$hp" 2>/dev/null; then
            kill "$hp" 2>/dev/null
            wait "$hp" 2>/dev/null
            bad "dll init run" "host hung; __init__ still on the loader lock"
        else
            wait "$hp"
            ec=$?
            if [ "$ec" -eq 0 ] && [ "$(cat "$WORK/init_ok.txt" 2>/dev/null)" = "1" ]; then
                ok "fn __init__ runs off the loader lock"
            else
                bad "dll init run" "exit $ec, file '$(cat "$WORK/init_ok.txt" 2>/dev/null)' $(cat "$WORK/host.out" 2>/dev/null)"
            fi
        fi
    fi
fi

if [ "$FAIL" -eq 0 ]; then
    echo "os_inject_cases: all passed"
    exit 0
fi
echo "os_inject_cases: $FAIL failed"
exit 1
