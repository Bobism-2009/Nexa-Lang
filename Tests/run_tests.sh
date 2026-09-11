#!/bin/sh
# Nexa regression harness (BOB-14).
#
# Four phases, each of which can fail the run:
#
#   run     Tests/Lang/*.nxa are compiled with NexaC, executed, and their
#           stdout compared byte-for-byte against the sibling *.expected.
#           These pin observable language behaviour: if a NexaC change moves
#           a result, the diff says exactly which line moved.
#
#   error   Tests/Lang/errors/*.nxa must NOT compile. Each has a sibling
#           *.expected holding one expected substring per line; every line
#           must appear in NexaC's output, NexaC must exit non-zero, and no
#           raw C++ compiler diagnostic may leak through to the user.
#           These only transpile (--source), so they never invoke clang++.
#
#   xfail   Tests/Lang/known_bugs/*.nxa are compiled and run the same way as
#           the `run` phase, but their .expected holds what the language is
#           SUPPOSED to produce. They are expected to fail; a pass is reported
#           as XPASS and fails the run, because it means a filed NexaC bug is
#           fixed and the case should be promoted into Tests/Lang.
#
#   script  The pre-existing Tests/*_cases.sh suites are run as-is.
#
# Usage: Tests/run_tests.sh [options]        (run from the repo root)
#   --nexac PATH    NexaC to test            (default: ./NexaC)
#   --jobs N        parallel compiles        (default: CPUs, capped at 8)
#   --filter PAT    only tests whose name matches the glob PAT
#   --phase P       only run phase P (run|error|xfail|script); repeatable
#   --keep          keep the work directory and print its path
#   -h, --help      this text

set -u

NEXAC="./NexaC"
JOBS=""
FILTER="*"
PHASES=""
KEEP=0

while [ $# -gt 0 ]; do
    case $1 in
        --nexac) NEXAC=$2; shift 2 ;;
        --jobs) JOBS=$2; shift 2 ;;
        --filter) FILTER=$2; shift 2 ;;
        --phase) PHASES="$PHASES $2"; shift 2 ;;
        --keep) KEEP=1; shift ;;
        -h|--help) sed -n '2,31p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

[ -n "$PHASES" ] || PHASES="run error xfail script"

if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC" >&2
    echo "      build it first (make) or pass --nexac PATH" >&2
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")

SUITE=$(cd "$(dirname "$0")" && pwd)
LANG_DIR="$SUITE/Lang"
if [ ! -d "$LANG_DIR" ]; then
    echo "FAIL: $LANG_DIR not found; run this from the repo root" >&2
    exit 1
fi

if [ -z "$JOBS" ]; then
    JOBS=$(nproc 2>/dev/null || echo 4)
    [ "$JOBS" -gt 8 ] 2>/dev/null && JOBS=8
fi

WORK=$(mktemp -d)
if [ "$KEEP" -eq 1 ]; then
    echo "work dir: $WORK"
else
    trap 'rm -rf "$WORK"' EXIT INT TERM
fi
mkdir -p "$WORK/res"

passed=0
failed=0
skipped=0

# --- phase helpers ----------------------------------------------------------

# A phase worker writes "<status>\n<log>" to $WORK/res/<slot>. Reporting is
# serialized in the parent so parallel output never interleaves.
report() {
    slot=$1
    label=$2
    status=$(head -n 1 "$WORK/res/$slot")
    case $status in
        ok)   passed=$((passed + 1)); echo "ok   $label" ;;
        skip) skipped=$((skipped + 1)); echo "SKIP $label"; tail -n +2 "$WORK/res/$slot" ;;
        *)    failed=$((failed + 1)); echo "FAIL $label"; tail -n +2 "$WORK/res/$slot" | sed 's/^/       /' ;;
    esac
}

# Cap concurrency by draining every $JOBS launches. Crude next to a real job
# pool, but it is POSIX sh and a compile is slow enough that the barrier costs
# far less than the compiles it overlaps.
inflight=0
throttle() {
    inflight=$((inflight + 1))
    if [ "$inflight" -ge "$JOBS" ]; then
        wait
        inflight=0
    fi
}

# --- phase: run -------------------------------------------------------------

do_run_case() {
    src=$1
    name=$2
    slot=$3
    out="$WORK/$name"
    log=$("$NEXAC" "$src" -o "$out" 2>&1)
    if [ $? -ne 0 ] || [ ! -x "$out" ]; then
        { echo fail
          echo "NexaC failed to build $name"
          printf '%s\n' "$log"
        } > "$WORK/res/$slot"
        return
    fi
    # Generated code must also be clean: a warning here is a codegen bug.
    diags=$(printf '%s\n' "$log" | grep -E '\.cpp:[0-9]+:[0-9]+: (warning|error):')
    if [ -n "$diags" ]; then
        { echo fail
          echo "the C++ compiler had something to say about the generated code"
          printf '%s\n' "$diags"
        } > "$WORK/res/$slot"
        return
    fi
    got=$("$out" 2>&1)
    rc=$?
    if [ $rc -ne 0 ]; then
        { echo fail
          echo "program exited $rc"
          printf '%s\n' "$got"
        } > "$WORK/res/$slot"
        return
    fi
    want=$(cat "${src%.nxa}.expected")
    if [ "$got" != "$want" ]; then
        { echo fail
          echo "output differs from ${name}.expected (-want +got):"
          printf '%s\n' "$want" > "$WORK/$name.want"
          printf '%s\n' "$got" > "$WORK/$name.got"
          diff -u "$WORK/$name.want" "$WORK/$name.got" | tail -n +3
        } > "$WORK/res/$slot"
        return
    fi
    echo ok > "$WORK/res/$slot"
}

phase_run() {
    echo "== run: compile, execute, compare stdout =="
    inflight=0
    slot=0
    names=""
    for src in "$LANG_DIR"/*.nxa; do
        [ -e "$src" ] || continue
        name=$(basename "$src" .nxa)
        case $name in $FILTER) ;; *) continue ;; esac
        if [ ! -f "${src%.nxa}.expected" ]; then
            { echo fail; echo "no ${name}.expected next to ${name}.nxa"; } > "$WORK/res/r$slot"
            names="$names r$slot:$name"
            slot=$((slot + 1))
            continue
        fi
        throttle
        do_run_case "$src" "$name" "r$slot" &
        names="$names r$slot:$name"
        slot=$((slot + 1))
    done
    wait
    for entry in $names; do
        report "${entry%%:*}" "${entry#*:}"
    done
}

# --- phase: error -----------------------------------------------------------

do_error_case() {
    src=$1
    name=$2
    slot=$3
    out=$("$NEXAC" "$src" --source "$WORK/$name.cpp" 2>&1)
    status=$?
    if [ $status -eq 0 ]; then
        { echo fail
          echo "NexaC exited 0; this program must not compile"
          printf '%s\n' "$out"
        } > "$WORK/res/$slot"
        return
    fi
    missing=""
    # Each non-blank, non-# line of the .expected must appear in the output.
    while IFS= read -r want; do
        case $want in ''|'#'*) continue ;; esac
        case $out in *"$want"*) ;; *) missing="$missing$want
" ;;
        esac
    done < "${src%.nxa}.expected"
    if [ -n "$missing" ]; then
        { echo fail
          echo "diagnostic did not contain:"
          printf '%s' "$missing"
          echo "got: $out"
        } > "$WORK/res/$slot"
        return
    fi
    # The point of diagnosing in Nexa is that clang never gets to speak.
    case $out in
        *"error:"*)
            { echo fail
              echo "a raw C++ compiler error leaked through"
              echo "got: $out"
            } > "$WORK/res/$slot"
            return ;;
    esac
    case $out in
        *"$name.nxa"*) ;;
        *)  { echo fail
              echo "diagnostic does not name the source file"
              echo "got: $out"
            } > "$WORK/res/$slot"
            return ;;
    esac
    echo ok > "$WORK/res/$slot"
}

phase_error() {
    echo "== error: must not compile, with the right diagnostic =="
    [ -d "$LANG_DIR/errors" ] || return 0
    inflight=0
    slot=0
    names=""
    for src in "$LANG_DIR"/errors/*.nxa; do
        [ -e "$src" ] || continue
        name=$(basename "$src" .nxa)
        case $name in $FILTER) ;; *) continue ;; esac
        if [ ! -f "${src%.nxa}.expected" ]; then
            { echo fail; echo "no ${name}.expected next to ${name}.nxa"; } > "$WORK/res/e$slot"
            names="$names e$slot:$name"
            slot=$((slot + 1))
            continue
        fi
        throttle
        do_error_case "$src" "$name" "e$slot" &
        names="$names e$slot:$name"
        slot=$((slot + 1))
    done
    wait
    for entry in $names; do
        report "${entry%%:*}" "${entry#*:}"
    done
}

# --- phase: xfail -----------------------------------------------------------

# Tests/Lang/known_bugs holds cases that document a NexaC bug we have filed but
# not fixed. Each states what the language is supposed to do, so the .expected
# is the CORRECT output and the test is expected to fail. A pass here is news:
# the bug is fixed, and the case should move up into Tests/Lang.
phase_xfail() {
    echo "== xfail: filed NexaC bugs, expected to fail until fixed =="
    [ -d "$LANG_DIR/known_bugs" ] || return 0
    for src in "$LANG_DIR"/known_bugs/*.nxa; do
        [ -e "$src" ] || continue
        name=$(basename "$src" .nxa)
        case $name in $FILTER) ;; *) continue ;; esac
        do_run_case "$src" "$name" "x_$name"
        if [ "$(head -n 1 "$WORK/res/x_$name")" = ok ]; then
            failed=$((failed + 1))
            echo "XPASS $name"
            echo "       this known bug now behaves correctly — move the case into"
            echo "       Tests/Lang/ and close the issue it was filed under"
        else
            skipped=$((skipped + 1))
            echo "xfail $name (known bug)"
        fi
    done
}

# --- phase: script ----------------------------------------------------------

phase_script() {
    echo "== script: existing Tests/*_cases.sh suites =="
    for s in "$SUITE"/*_cases.sh; do
        [ -e "$s" ] || continue
        name=$(basename "$s" .sh)
        case $name in $FILTER) ;; *) continue ;; esac
        log=$(sh "$s" "$NEXAC" 2>&1)
        if [ $? -eq 0 ]; then
            passed=$((passed + 1))
            echo "ok   $name"
        else
            failed=$((failed + 1))
            echo "FAIL $name"
            printf '%s\n' "$log" | grep -v '^ok ' | sed 's/^/       /'
        fi
    done
}

# --- drive ------------------------------------------------------------------

for p in $PHASES; do
    case $p in
        run) phase_run ;;
        error) phase_error ;;
        xfail) phase_xfail ;;
        script) phase_script ;;
        *) echo "unknown phase: $p" >&2; exit 2 ;;
    esac
done

echo
echo "$passed passed, $failed failed, $skipped skipped"
[ "$failed" -eq 0 ] || exit 1
