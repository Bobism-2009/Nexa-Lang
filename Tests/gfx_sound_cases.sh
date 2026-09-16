#!/bin/sh
# Cover for the gfx sound engine (BOB-39): gfx.sound, gfx.play, gfx.loop,
# gfx.stop, gfx.volume.
#
# Two halves, both headless -- nothing here opens a window or needs a display,
# and nothing here needs an audio device either.
#
#   codegen   Which call each gfx.* form reaches and what NexaC fills in for
#             the arguments left off: the default volume, gfx.stop()'s "every
#             voice", and which of gfx.play and gfx.loop sets the loop bit.
#             These are decisions that live in the transpiler rather than the
#             runtime, so no behavioural test can see them. Transpile only
#             (--source), so this half runs anywhere NexaC does.
#
#   semantics Tests/gfx_sound_semantics.cpp builds the generated runtime and
#             drives the WAV reader and the mixer directly. Skipped, loudly,
#             when there is no C++ compiler.
#
# The semantics half has one piece of scaffolding in it. The mixer only runs
# with the audio stream open, and today only winmm and Web Audio open one -- on
# macOS and Linux gfx.audio() returns 0 and not a sample is ever mixed, so a
# test that went through the public API on this machine would asserts nothing
# about the mixing. The three-line macOS/Linux audio stub in the generated file
# is therefore swapped below for one that opens a fake stream and keeps every
# sample handed to it. Everything above that seam -- the RIFF reader, the voice
# table, the gain arithmetic, the saturation -- is the same code all four
# backends run, so checking it here checks it everywhere.
#
# Emit slicing (a program that never plays a sound carries none of this) is
# covered in Tests/gfx_emit_cases.sh, and the arity diagnostics in
# Tests/Lang/errors/gfx_*.nxa.
#
# Usage: Tests/gfx_sound_cases.sh [path-to-NexaC]      (run from the repo root)

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")

HERE=$(cd "$(dirname "$0")" && pwd)

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM

fails=0

# --- codegen ----------------------------------------------------------------

# expect_source <label> <grep-ERE> <body>
# Asserts the transpiled main contains a line matching the pattern.
expect_source() {
    label=$1
    want=$2
    body=$3

    printf '#include <std/gfx>\nfn main() {\n%s\n}\n' "$body" > "$WORK/case.nxa"
    if ! "$NEXAC" "$WORK/case.nxa" --source "$WORK/case.cpp" > "$WORK/case.log" 2>&1; then
        echo "FAIL $label: NexaC could not transpile"
        sed 's/^/  /' "$WORK/case.log"
        fails=$((fails + 1))
        return
    fi
    if ! awk '/^int main\(/ { inmain = 1 } inmain { print } inmain && /^\}/ { exit }' \
            "$WORK/case.cpp" | grep -Eq "$want"; then
        echo "FAIL $label: main has no line matching /$want/"
        awk '/^int main\(/ { inmain = 1 } inmain { print } inmain && /^\}/ { exit }' \
            "$WORK/case.cpp" | sed 's/^/  /'
        fails=$((fails + 1))
        return
    fi
    echo "ok codegen: $label"
}

echo "-- codegen: what each gfx sound call emits"

expect_source "gfx.sound loads by path" \
    '__nexa_gfx_sound\("beep\.wav"\)' \
    '    let s: int = gfx.sound("beep.wav");'

# play and loop are one runtime call apart in the last argument.
expect_source "gfx.play defaults to full volume" \
    '__nexa_gfx_voice_start\(1, 255, 0\)' \
    '    let v: int = gfx.play(1);'

expect_source "gfx.play passes a volume through" \
    '__nexa_gfx_voice_start\(1, 64, 0\)' \
    '    let v: int = gfx.play(1, 64);'

expect_source "gfx.loop sets the loop bit" \
    '__nexa_gfx_voice_start\(1, 255, 1\)' \
    '    let v: int = gfx.loop(1);'

expect_source "gfx.loop passes a volume through" \
    '__nexa_gfx_voice_start\(1, 64, 1\)' \
    '    let v: int = gfx.loop(1, 64);'

# gfx.stop() with nothing to stop in particular is voice 0, which the runtime
# reads as "all of them".
expect_source "gfx.stop() is every voice" \
    '__nexa_gfx_stop\(0\)' \
    '    let n: int = gfx.stop();'

expect_source "gfx.stop(v) is one voice" \
    '__nexa_gfx_stop\(7\)' \
    '    let n: int = gfx.stop(7);'

# The two forms of gfx.volume are two different functions, the way gfx.alpha is.
expect_source "gfx.volume() reads" \
    '__nexa_gfx_volume_get\(\)' \
    '    let v: int = gfx.volume();'

expect_source "gfx.volume(v) writes" \
    '__nexa_gfx_volume_set\(128\)' \
    '    let v: int = gfx.volume(128);'

# gfx.audio_flush() means "start playing what is queued", so the mixer gets its
# turn before the partial buffer goes out.
expect_source "gfx.audio_flush pumps the mixer first" \
    '__nexa_gfx_mix_pump\(\), __nexa_gfx_audio_flush\(\)' \
    '    let n: int = gfx.audio_flush();'

# --- semantics --------------------------------------------------------------

pick_cxx() {
    if [ -n "${NEXA_CXX:-}" ]; then
        echo "$NEXA_CXX"
        return
    fi
    for c in clang++ g++ c++; do
        if command -v "$c" > /dev/null 2>&1; then
            echo "$c"
            return
        fi
    done
    echo ""
}

CXX=$(pick_cxx)

echo "-- semantics: the WAV reader and the mixer"

if [ -z "$CXX" ]; then
    echo "SKIP semantics: no C++ compiler on this machine"
else
    # One call to each, so the generated file carries the whole sound runtime.
    printf '%s' '#include <std/gfx>
fn main() {
    let s: int = gfx.sound("beep.wav");
    let v: int = gfx.play(s);
    let l: int = gfx.loop(s, 128);
    let a: int = gfx.stop(v);
    let b: int = gfx.stop();
    let m: int = gfx.volume();
    let n: int = gfx.volume(200);
    gfx.poll();
}
' > "$WORK/gen.nxa"

    if ! "$NEXAC" "$WORK/gen.nxa" --source "$WORK/gen.cpp" > "$WORK/gen.log" 2>&1; then
        echo "FAIL semantics: NexaC could not transpile the driver program"
        sed 's/^/  /' "$WORK/gen.log"
        fails=$((fails + 1))
    else
        # The seam described at the top of this file: give macOS/Linux a stream
        # that opens and a speaker that remembers, so the mixer above it runs.
        sed \
            -e 's|^static int __nexa_gfx_audio(int rate) { (void)rate; return 0; }$|static int __nexa_gfx_audio(int rate) { __nexa_audio_rate = rate; return 1; }|' \
            -e 's|^static int __nexa_gfx_sample(int s) { (void)s; return 0; }$|static std::vector<int> __nexa_cap;\nstatic int __nexa_gfx_sample(int s) { __nexa_cap.push_back(s); return 1; }|' \
            -e 's|^static int __nexa_gfx_audio_queued() { return 0; }$|static int __nexa_gfx_audio_queued() { return (int)__nexa_cap.size(); }|' \
            "$WORK/gen.cpp" > "$WORK/patched.cpp"

        # If the swap stopped matching, say so rather than testing nothing: the
        # likeliest reason is that macOS or Linux grew a real audio backend, and
        # then this is the file that has to decide what to do about it.
        if ! grep -q '__nexa_cap' "$WORK/patched.cpp"; then
            echo "FAIL semantics: the macOS/Linux audio stub no longer matches"
            echo "  (the sed above expects the three one-line stubs in GfxRuntime.hpp)"
            fails=$((fails + 1))
        elif ! "$CXX" -std=c++17 -O1 -I "$HERE/gfx_x11_stub" \
                -DNEXA_GEN="\"$WORK/patched.cpp\"" \
                -DNEXA_WAV_DIR="\"$HERE\"" \
                "$HERE/gfx_sound_semantics.cpp" "$HERE/gfx_x11_stub/x11_stub.cpp" \
                -o "$WORK/semantics" > "$WORK/build.log" 2>&1; then
            echo "FAIL semantics: could not build the driver"
            sed 's/^/  /' "$WORK/build.log"
            fails=$((fails + 1))
        else
            out=$("$WORK/semantics" 2>&1)
            status=$?
            printf '%s\n' "$out" | grep '^ok ' | sed 's/^ok /ok semantics: /'
            if [ $status -ne 0 ]; then
                printf '%s\n' "$out" | grep -v '^ok ' | sed 's/^/  /'
                fails=$((fails + 1))
            fi
        fi
    fi
fi

if [ $fails -eq 0 ]; then
    echo "gfx_sound ok"
    exit 0
fi
echo "gfx_sound: $fails failure(s)"
exit 1
