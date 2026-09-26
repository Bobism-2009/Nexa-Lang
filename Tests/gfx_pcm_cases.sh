#!/bin/sh
# Cover for the Linux audio backend behind gfx.audio() (BOB-46, BOB-47).
#
# There is one way a Nexa program reaches a speaker on Linux: the kernel's own
# PCM interface, driven through ioctls on /dev/snd with the uapi structs written
# out by hand in include/GfxRuntime.hpp. Nothing is dlopened and nothing is
# linked, so nothing has to be installed on the machine that runs the binary.
#
# That hand-writing is the thing worth testing, and a kernel PCM has no shared
# library to substitute, so there is no fake device to point the program at.
# What there is instead is two halves:
#
#   abi   -- always runs. Cuts the [nexa:kernel-pcm-*] range out of a generated
#            program, compiles it on its own, and checks every struct size and
#            ioctl number against the kernel's. This is the failure that hides:
#            a struct one field out still builds, still runs, and silently
#            never makes a sound, because the size is part of the ioctl number
#            and the kernel simply refuses a request it does not recognise.
#
#   iec958 -- always runs where a shared library can be preloaded. A card that
#            takes IEC958 subframes and no PCM -- the Raspberry Pi's HDMI
#            ports -- is played through a stand-in: an LD_PRELOADed ioctl that
#            refuses 16-bit PCM, takes subframes a hundred frames at a time,
#            and keeps every word. Each word is checked against an encoder
#            written here from the IEC958 layout, not copied from the backend.
#
#   live  -- runs on any machine with a sound card. The program has to actually
#            play, and the clock is what proves it: a stream that opened but
#            never started hands three thousand samples back instantly, and a
#            real card cannot.
#
# Linux only, and quiet -- the live half feeds silence.
#
# Usage: Tests/gfx_pcm_cases.sh [path-to-NexaC]      (run from the repo root)

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")

SUITE=$(cd "$(dirname "$0")" && pwd)

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM

fails=0
skips=0

if [ "$(uname -s)" != "Linux" ]; then
    echo "SKIP gfx_pcm: the kernel PCM backend is Linux only (this is $(uname -s))"
    echo "gfx_pcm ok"
    exit 0
fi

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
if [ -z "$CXX" ]; then
    echo "SKIP gfx_pcm: no C++ compiler on this machine"
    echo "gfx_pcm ok"
    exit 0
fi

say_ok() { echo "ok $1"; }
say_fail() {
    echo "FAIL $1"
    fails=$((fails + 1))
}

# --- the generated program ---------------------------------------------------

if ! "$NEXAC" "$SUITE/gfx_pcm_test.nxa" --source "$WORK/prog.cpp" > "$WORK/transpile.log" 2>&1; then
    echo "FAIL build: NexaC could not transpile Tests/gfx_pcm_test.nxa"
    sed 's/^/  /' "$WORK/transpile.log"
    exit 1
fi

if grep -q '/dev/snd/pcmC%dD%dp' "$WORK/prog.cpp"; then
    say_ok "slice: the kernel PCM backend survived into the generated program"
else
    say_fail "slice: the generated program has no kernel PCM backend in it"
fi

# The whole point of the backend: a program that plays a sound owes nothing to
# a library. Neither a link to libasound nor a dlopen looking for one belongs
# anywhere in the audio it carries. Comments are cut first, because the design
# note at the top of the backend says the word "libasound" to explain what is
# deliberately not there.
grep -vE '^[[:space:]]*//' "$WORK/prog.cpp" > "$WORK/code.cpp"
if grep -nE 'libasound|snd_pcm_open|dlfcn|dlopen' "$WORK/code.cpp" > "$WORK/dep.out"; then
    say_fail "nodep: the generated program still reaches for a sound library"
    sed 's/^/  /' "$WORK/dep.out" | head -n 5
else
    say_ok "nodep: the generated program reaches no sound library, only the kernel"
fi

# The backend belongs to the sound slice, so a gfx program that never makes a
# noise must not be carrying it.
if ! "$NEXAC" "$SUITE/gfx_shapes_test.nxa" --source "$WORK/quiet.cpp" > "$WORK/quiet.log" 2>&1; then
    echo "FAIL slice: NexaC could not transpile Tests/gfx_shapes_test.nxa"
    sed 's/^/  /' "$WORK/quiet.log"
    fails=$((fails + 1))
elif grep -q '/dev/snd/' "$WORK/quiet.cpp"; then
    say_fail "slice: a gfx program with no sound in it still carries /dev/snd"
else
    say_ok "slice: a gfx program with no sound in it carries no /dev/snd"
fi

# --- abi: the hand-written uapi against the kernel's -------------------------

awk '
    /\[nexa:kernel-pcm-begin\]/ { on = 1; next }
    /\[nexa:kernel-pcm-end\]/   { on = 0; next }
    on                          { print }
' "$WORK/prog.cpp" > "$WORK/uapi.h"

if [ ! -s "$WORK/uapi.h" ]; then
    echo "FAIL abi: the [nexa:kernel-pcm-*] markers no longer bracket the declarations"
    exit 1
fi

cat > "$WORK/abi.cpp" <<'ABI'
#include <sys/ioctl.h>
#include <cstring>
#include <cstdio>
#include "uapi.h"
int main() {
    std::printf("hw=%zu sw=%zu xferi=%zu\n",
                sizeof(struct __nexa_snd_hw_params),
                sizeof(struct __nexa_snd_sw_params),
                sizeof(struct __nexa_snd_xferi));
    std::printf("hw_params=0x%lx sw_params=0x%lx writei=0x%lx delay=0x%lx\n",
                (unsigned long)NEXA_SND_IOCTL_HW_PARAMS,
                (unsigned long)NEXA_SND_IOCTL_SW_PARAMS,
                (unsigned long)NEXA_SND_IOCTL_WRITEI,
                (unsigned long)NEXA_SND_IOCTL_DELAY);
    std::printf("prepare=0x%lx drop=0x%lx drain=0x%lx resume=0x%lx\n",
                (unsigned long)NEXA_SND_IOCTL_PREPARE,
                (unsigned long)NEXA_SND_IOCTL_DROP,
                (unsigned long)NEXA_SND_IOCTL_DRAIN,
                (unsigned long)NEXA_SND_IOCTL_RESUME);
    std::printf("access=%d format=%d subformat=%d channels=%d rate=%d period=%d periods=%d buffer=%d first=%d\n",
                NEXA_SND_PARAM_ACCESS, NEXA_SND_PARAM_FORMAT, NEXA_SND_PARAM_SUBFORMAT,
                NEXA_SND_PARAM_CHANNELS, NEXA_SND_PARAM_RATE, NEXA_SND_PARAM_PERIOD_SIZE,
                NEXA_SND_PARAM_PERIODS, NEXA_SND_PARAM_BUFFER_SIZE,
                NEXA_SND_PARAM_FIRST_INTERVAL);
    std::printf("rw_interleaved=%d s16_le=%d std=%d\n",
                NEXA_SND_ACCESS_RW_INTERLEAVED, NEXA_SND_FORMAT_S16_LE, NEXA_SND_SUBFORMAT_STD);
    std::printf("iec958_subframe_le=%d\n", NEXA_SND_FORMAT_IEC958_SUBFRAME_LE);
    return 0;
}
ABI

if ! "$CXX" -std=c++17 -O1 -Wall -Wextra -I "$WORK" "$WORK/abi.cpp" -o "$WORK/abi" \
        > "$WORK/abi.log" 2>&1; then
    echo "FAIL abi: the hand-written uapi declarations do not compile on their own"
    grep -E 'error' "$WORK/abi.log" | head -n 5 | sed 's/^/  /'
    exit 1
fi
"$WORK/abi" > "$WORK/abi.out"

# The parameter numbers and the enum values are the same everywhere. The sizes
# and the ioctl numbers are not -- a 32-bit kernel has smaller structs, so it
# has different request numbers -- and the values below are the LP64 ones, taken
# from /usr/include/sound/asound.h and from what strace prints for an ALSA
# program. Checking them anywhere else would be checking an arithmetic we have
# not been given the truth for, so that half is skipped there with a word.
abi_says() {
    if grep -qx "$2" "$WORK/abi.out"; then
        say_ok "abi: $1"
    else
        echo "FAIL abi: $1"
        echo "  wanted: $2"
        echo "  got:    $(grep -E "^$(printf '%s' "$2" | cut -d' ' -f1 | cut -d= -f1)" "$WORK/abi.out")"
        fails=$((fails + 1))
    fi
}

abi_says "the parameter numbers are the kernel's SNDRV_PCM_HW_PARAM_*" \
    'access=0 format=1 subformat=2 channels=10 rate=11 period=13 periods=15 buffer=17 first=8'
abi_says "interleaved read/write, S16_LE and the standard subformat" \
    'rw_interleaved=3 s16_le=2 std=0'
abi_says "IEC958_SUBFRAME_LE is the kernel's SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE" \
    'iec958_subframe_le=18'

if [ "$(getconf LONG_BIT 2>/dev/null || echo 0)" = "64" ]; then
    abi_says "the structs are the size the kernel's are" 'hw=608 sw=136 xferi=24'
    abi_says "hw_params, sw_params, writei and delay are the kernel's ioctls" \
        'hw_params=0xc2604111 sw_params=0xc0884113 writei=0x40184150 delay=0x80084121'
    abi_says "and so are prepare, drop, drain and resume" \
        'prepare=0x4140 drop=0x4143 drain=0x4144 resume=0x4147'
else
    echo "SKIP abi: the struct sizes and ioctl numbers checked here are the"
    echo "     64-bit ones (this is $(getconf LONG_BIT 2>/dev/null)-bit)"
    skips=$((skips + 1))
fi

# --- the whole program, built the way the other gfx suites build one ---------

# -Wno-unused-function on purpose: gfx.open/close/poll/present are emitted into
# every gfx program whether or not it calls them (see GfxNeed in
# include/GfxRuntime.hpp), so a program that only opens an audio stream carries
# window helpers nothing reaches. That is the slicing design, not a defect, and
# it is the only warning class this build is excused from.
if ! "$CXX" -std=c++17 -O1 -Wall -Wextra -Wno-unused-function -I "$SUITE/gfx_x11_stub" \
        "$WORK/prog.cpp" "$SUITE/gfx_x11_stub/x11_stub.cpp" \
        -o "$WORK/prog" > "$WORK/build.log" 2>&1; then
    if grep -qE 'X11|error: .*\.h.* file not found' "$WORK/build.log"; then
        echo "SKIP gfx_pcm: this machine cannot build a gfx program"
        grep -E 'error' "$WORK/build.log" | head -n 3 | sed 's/^/       /'
        echo "gfx_pcm ok"
        exit 0
    fi
    echo "FAIL build: the generated program does not build"
    grep -E 'error:|undefined' "$WORK/build.log" | head -n 5 | sed 's/^/  /'
    exit 1
fi

diags=$(grep -E '\.cpp:[0-9]+:[0-9]+: (warning|error):' "$WORK/build.log")
if [ -n "$diags" ]; then
    echo "FAIL build: the C++ compiler had something to say about the kernel backend"
    printf '%s\n' "$diags" | head -n 5 | sed 's/^/  /'
    fails=$((fails + 1))
else
    say_ok "build: the kernel PCM backend compiles clean under -Wall -Wextra"
fi

# --- iec958: a card that takes subframes and nothing else --------------------

iec_half() {
    if ! "$NEXAC" "$SUITE/gfx_pcm_iec_test.nxa" --source "$WORK/iec.cpp" > "$WORK/iec.log" 2>&1; then
        say_fail "iec958: NexaC could not transpile Tests/gfx_pcm_iec_test.nxa"
        sed 's/^/  /' "$WORK/iec.log"
        return
    fi
    if ! "$CXX" -std=c++17 -O1 -Wno-unused-function -I "$SUITE/gfx_x11_stub" \
            "$WORK/iec.cpp" "$SUITE/gfx_x11_stub/x11_stub.cpp" -o "$WORK/iecprog" > "$WORK/iec.log" 2>&1; then
        say_fail "iec958: the generated program does not build"
        grep -E 'error' "$WORK/iec.log" | head -n 5 | sed 's/^/  /'
        return
    fi

    # The stand-in card, built on the backend's own declarations so the two
    # agree on the structs (the abi half above holds those to the kernel's).
    cat > "$WORK/fakecard.cpp" <<'FAKE'
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "uapi.h"

static int is_card(int fd) {
    const char* want = getenv("FAKE_CARD");
    char link[64], path[PATH_MAX];
    snprintf(link, sizeof(link), "/proc/self/fd/%d", fd);
    ssize_t n = readlink(link, path, sizeof(path) - 1);
    if (!want || n <= 0) return 0;
    path[n] = 0;
    return strcmp(path, want) == 0;
}

static int has(const struct __nexa_snd_hw_params* p, int which, unsigned bit) {
    return (p->masks[which].bits[bit >> 5] >> (bit & 31)) & 1;
}

static struct __nexa_snd_interval* iv(struct __nexa_snd_hw_params* p, int param) {
    return &p->intervals[param - NEXA_SND_PARAM_FIRST_INTERVAL];
}

static int fail(int e) { errno = e; return -1; }

extern "C" int ioctl(int fd, unsigned long req, ...) {
    va_list ap;
    va_start(ap, req);
    void* arg = va_arg(ap, void*);
    va_end(ap);
    if (!is_card(fd)) {
        typedef int (*real_t)(int, unsigned long, ...);
        static real_t real = (real_t)dlsym(RTLD_NEXT, "ioctl");
        return real(fd, req, arg);
    }
    if (req == NEXA_SND_IOCTL_HW_PARAMS) {
        struct __nexa_snd_hw_params* p = (struct __nexa_snd_hw_params*)arg;
        if (has(p, NEXA_SND_PARAM_FORMAT, NEXA_SND_FORMAT_S16_LE)) return fail(EINVAL);
        if (!has(p, NEXA_SND_PARAM_FORMAT, NEXA_SND_FORMAT_IEC958_SUBFRAME_LE)) return fail(EINVAL);
        struct __nexa_snd_interval* ch = iv(p, NEXA_SND_PARAM_CHANNELS);
        struct __nexa_snd_interval* rate = iv(p, NEXA_SND_PARAM_RATE);
        if (ch->min > 2 || ch->max < 2) return fail(EINVAL);
        if (rate->min > 48000 || rate->max < 48000) return fail(EINVAL);
        ch->min = ch->max = 2;
        rate->min = rate->max = 48000;
        iv(p, NEXA_SND_PARAM_PERIOD_SIZE)->min = 960;
        iv(p, NEXA_SND_PARAM_BUFFER_SIZE)->min = 4800;
        FILE* f = fopen(getenv("FAKE_LOG"), "a");
        if (f) { fprintf(f, "hw_params iec958 2ch 48000\n"); fclose(f); }
        return 0;
    }
    if (req == NEXA_SND_IOCTL_WRITEI) {
        struct __nexa_snd_xferi* x = (struct __nexa_snd_xferi*)arg;
        unsigned long n = x->frames > 100 ? 100 : x->frames;  // a partial write each time
        FILE* f = fopen(getenv("FAKE_DUMP"), "ab");
        if (!f) return fail(EIO);
        fwrite(x->buf, 8, n, f);
        fclose(f);
        x->result = (long)n;
        return 0;
    }
    if (req == NEXA_SND_IOCTL_DELAY) {
        *(long*)arg = 0;
        return 0;
    }
    return 0;  // sw_params, prepare, drain, drop
}
FAKE

    # The reference: every word from the IEC958 layout, one bit at a time.
    cat > "$WORK/iecref.cpp" <<'REF'
#include <stdio.h>
int main(int argc, char** argv) {
    if (argc < 2) return 2;
    FILE* f = fopen(argv[1], "rb");
    if (!f) { printf("no dump\n"); return 1; }
    // Consumer, not copyrighted, original PCM, 48 kHz, 16-bit words.
    const unsigned char status[24] = {0x04, 0x82, 0x00, 0x02, 0x02};
    unsigned int got[2];
    int frame = 0;
    while (fread(got, 4, 2, f) == 2) {
        short sample = (short)((frame * 131) % 65536 - 32768);
        for (int ch = 0; ch < 2; ch++) {
            int block = frame % 192;
            unsigned int w = 0;
            w |= ch == 1 ? 0x4u : (block == 0 ? 0x8u : 0x2u);          // preamble Y, Z or X
            w |= (unsigned int)(unsigned short)sample << 12;           // 16 bits of a 24-bit field
            if ((status[block / 8] >> (block % 8)) & 1) w |= 1u << 30;  // channel status
            int ones = 0;
            for (int b = 4; b < 31; b++) ones += (w >> b) & 1;
            if (ones & 1) w |= 1u << 31;                                // even parity, 4-31
            if (got[ch] != w) {
                printf("frame %d channel %d: got %08x, wanted %08x\n", frame, ch, got[ch], w);
                return 1;
            }
        }
        frame++;
    }
    printf("frames=%d\n", frame);
    return 0;
}
REF

    if ! "$CXX" -std=c++17 -shared -fPIC -I "$WORK" "$WORK/fakecard.cpp" -o "$WORK/fakecard.so" -ldl \
            > "$WORK/iec.log" 2>&1 ||
       ! "$CXX" -std=c++17 "$WORK/iecref.cpp" -o "$WORK/iecref" >> "$WORK/iec.log" 2>&1; then
        echo "SKIP iec958: cannot build a preloadable stand-in card here"
        grep -E 'error' "$WORK/iec.log" | head -n 3 | sed 's/^/       /'
        skips=$((skips + 1))
        return
    fi

    : > "$WORK/fake.card"
    rm -f "$WORK/fake.dump" "$WORK/fake.log"
    if ! FAKE_CARD="$WORK/fake.card" FAKE_DUMP="$WORK/fake.dump" FAKE_LOG="$WORK/fake.log" \
            NEXA_PCM_DEVICE="$WORK/fake.card" LD_PRELOAD="$WORK/fakecard.so" \
            timeout 60 "$WORK/iecprog" > "$WORK/out" 2>&1; then
        say_fail "iec958: the program exited non-zero"
        sed 's/^/  /' "$WORK/out"
        return
    fi
    if grep -qx "audio=1" "$WORK/out" && grep -qx "fed=500" "$WORK/out" && grep -qx "done" "$WORK/out" &&
            grep -q "hw_params iec958 2ch 48000" "$WORK/fake.log" 2>/dev/null; then
        say_ok "iec958: a card that refuses 16-bit PCM is opened in IEC958 subframes"
    else
        say_fail "iec958: a card that takes only IEC958 subframes did not open"
        sed 's/^/  /' "$WORK/out"
        return
    fi
    if "$WORK/iecref" "$WORK/fake.dump" > "$WORK/ref.out" && grep -qx "frames=500" "$WORK/ref.out"; then
        say_ok "iec958: all 500 frames are the subframes IEC958 says, across partial writes"
    else
        say_fail "iec958: the subframes are not what IEC958 says"
        sed 's/^/  /' "$WORK/ref.out"
    fi
}
iec_half

# --- live: on any machine with a card ----------------------------------------

has_pcm=no
for n in /dev/snd/pcmC*D*p; do
    [ -c "$n" ] && has_pcm=yes && break
done

if [ "$has_pcm" != "yes" ]; then
    echo "SKIP live: no /dev/snd playback node on this machine, so there is no"
    echo "     card for the backend to open"
    skips=$((skips + 1))
else
    echo "-- a real card, reached without a library"

    # A device node that is not a PCM at all. The backend has to give up on it
    # and say so the way it says everything else: a quiet 0 and a program that
    # finishes. /dev/null opens, so this gets past the open and fails where it
    # matters, on the first ioctl.
    if NEXA_PCM_DEVICE=/dev/null timeout 60 "$WORK/prog" > "$WORK/out" 2>&1; then
        if grep -qx "audio=0" "$WORK/out" && grep -qx "fed=0" "$WORK/out" &&
                grep -qx "done" "$WORK/out"; then
            say_ok "notapcm: a node that is not a PCM is a quiet 0"
        else
            echo "FAIL notapcm: a node that is not a PCM did not come back as a quiet 0"
            sed 's/^/  /' "$WORK/out"
            fails=$((fails + 1))
        fi
    else
        echo "FAIL notapcm: the program exited non-zero"
        sed 's/^/  /' "$WORK/out"
        fails=$((fails + 1))
    fi

    # A path with nothing behind it: the open itself fails.
    if NEXA_PCM_DEVICE="$WORK/no-such-device" timeout 60 "$WORK/prog" > "$WORK/out" 2>&1 &&
            grep -qx "audio=0" "$WORK/out" && grep -qx "done" "$WORK/out"; then
        say_ok "nodevice: a device that is not there is a quiet 0"
    else
        echo "FAIL nodevice: a missing device did not come back as a quiet 0"
        sed 's/^/  /' "$WORK/out"
        fails=$((fails + 1))
    fi

    # And the card itself.
    if ! timeout 60 "$WORK/prog" > "$WORK/out" 2>&1; then
        echo "FAIL live: the program exited non-zero"
        sed 's/^/  /' "$WORK/out"
        fails=$((fails + 1))
    else
        for want in "audio=1" "fed=3000" "drained=0" "after=0" "queued_after=0" "done"; do
            if grep -qx "$want" "$WORK/out"; then
                say_ok "live: $want"
            else
                echo "FAIL live: the program never printed '$want'"
                sed 's/^/  /' "$WORK/out"
                fails=$((fails + 1))
            fi
        done

        # The card took samples and had not played them all yet, which only a
        # prepared, running stream reports.
        held=$(sed -n 's/^held=//p' "$WORK/out")
        if [ -n "$held" ] && [ "$held" -gt 0 ] && [ "$held" -le 3000 ]; then
            say_ok "live: the card was holding $held of the 3000 samples"
        else
            echo "FAIL live: held=$held, which no running stream reports"
            sed 's/^/  /' "$WORK/out"
            fails=$((fails + 1))
        fi

        # Three thousand samples cannot come back faster than a card can play
        # them: 31ms at 96000, 62ms at 48000, 68ms at 44100. A stream that never
        # started gives them back in no time at all, which is the whole point of
        # measuring. The ceiling is only there to catch a stream that stalled.
        elapsed=$(sed -n 's/^elapsed_ms=//p' "$WORK/out")
        verdict=$(awk -v e="$elapsed" 'BEGIN { print (e + 0 >= 25 && e + 0 <= 4000) ? "ok" : "no" }')
        if [ "$verdict" = "ok" ]; then
            say_ok "live: the samples drained in ${elapsed}ms, the speed of a real card"
        else
            echo "FAIL live: 3000 samples drained in ${elapsed}ms, which is not playback"
            sed 's/^/  /' "$WORK/out"
            fails=$((fails + 1))
        fi
    fi
fi

if [ $fails -eq 0 ]; then
    if [ $skips -gt 0 ]; then
        echo "gfx_pcm ok ($skips skipped)"
    else
        echo "gfx_pcm ok"
    fi
    exit 0
fi
echo "gfx_pcm: $fails failure(s)"
exit 1
