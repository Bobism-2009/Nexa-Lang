#!/bin/sh
# std/gfx alpha, mirrored blits and gfx.save (BOB-20).
#
# Same two-layer shape as Tests/gfx_shapes_cases.sh, for the same reason -- a
# gfx program cannot be run, or even linked, on a machine with no display:
#
#   dispatch    A .nxa call reaches the right runtime function with the right
#               arguments, and a malformed call is refused in Nexa terms rather
#               than by the C++ compiler. Checked by transpiling (--source).
#
#   pixels      The blend, the blit and the BMP writer do what the
#               documentation says. This suite lifts four marked blocks out of
#               include/GfxRuntime.hpp -- the rasterizers, the loaded-image
#               table, the blit and gfx.save -- compiles them against a stub
#               framebuffer, and checks the pixels and the bytes on disk. No
#               window, no display, no X11.
#
# Every expected number here was worked out from SYNTAX/Modules.txt --
# new = (draw*a + old*(255-a) + 127) / 255, per channel, in integers -- and not
# recorded from what the runtime currently produces.
#
# Usage: Tests/gfx_alpha_cases.sh [path-to-NexaC]       (run from the repo root)

set -u

NEXAC="${1:-./NexaC}"
if [ ! -x "$NEXAC" ]; then
    echo "FAIL: NexaC not found or not executable: $NEXAC"
    exit 1
fi
NEXAC=$(cd "$(dirname "$NEXAC")" && pwd)/$(basename "$NEXAC")

SUITE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$SUITE")
HEADER="$ROOT/include/GfxRuntime.hpp"

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM

fails=0

# --- dispatch: .nxa source -> the right runtime call -------------------------

expect_emit() {
    label=$1
    src=$2
    shift 2
    printf '%s' "$src" > "$WORK/case.nxa"
    log=$("$NEXAC" "$WORK/case.nxa" --source "$WORK/case.cpp" 2>&1)
    if [ $? -ne 0 ]; then
        echo "FAIL $label: NexaC refused the program"
        printf '%s\n' "$log" | sed 's/^/  /'
        fails=$((fails + 1))
        return
    fi
    for want in "$@"; do
        if ! grep -qF "$want" "$WORK/case.cpp"; then
            echo "FAIL $label: generated C++ does not contain"
            echo "  $want"
            fails=$((fails + 1))
            return
        fi
    done
    echo "ok $label"
}

# Asserts NexaC refuses the program, says why in Nexa's own terms, and does not
# let a C++ diagnostic about machine-written code reach the user.
expect_reject() {
    label=$1
    printf '%s' "$2" > "$WORK/case.nxa"
    out=$("$NEXAC" "$WORK/case.nxa" --source "$WORK/case.cpp" 2>&1)
    if [ $? -eq 0 ]; then
        echo "FAIL $label: NexaC accepted a program it must reject"
        fails=$((fails + 1))
        return
    fi
    case $out in
        *"$3"*) ;;
        *)  echo "FAIL $label: diagnostic did not mention: $3"
            printf '%s\n' "$out" | sed 's/^/  /'
            fails=$((fails + 1))
            return ;;
    esac
    case $out in
        *".cpp:"*"error:"*)
            echo "FAIL $label: a raw C++ compiler error leaked through"
            printf '%s\n' "$out" | sed 's/^/  /'
            fails=$((fails + 1))
            return ;;
    esac
    echo "ok $label"
}

expect_emit "dispatch: alpha, save and a mirrored blit" \
'#include <std/gfx>
#include <std/io>
fn main() {
    io.println(gfx.alpha());
    gfx.alpha(128);
    io.println(gfx.save("shot.bmp"));
    // A literal handle keeps the expected C++ clear of compiler-chosen
    // variable names; gfx.image is covered by Tests/gfx_open_close_test.nxa.
    gfx.blit(1, 2, 3, -8, -8);
    gfx.blit(1, 2, 3, 0, 0, 4, 4, -8, 8);
}
' \
    "__nexa_gfx_alpha_get()" \
    "__nexa_gfx_alpha_set(128)" \
    "__nexa_gfx_save(\"shot.bmp\")" \
    "__nexa_gfx_blit(1, 2, 3, -8, -8, 0, 0, 0, 0)" \
    "__nexa_gfx_blit(1, 2, 3, -8, 8, 0, 0, 4, 4)"

# Both report something, so both have to be usable as a value and not only as a
# statement.
expect_emit "dispatch: alpha and save are int expressions" \
'#include <std/gfx>
#include <std/io>
fn main() {
    let was: int = gfx.alpha(64);
    if (gfx.save("shot.bmp") == 1) {
        io.println(was + gfx.alpha());
    }
}
' \
    "__nexa_gfx_alpha_set(64)" \
    "__nexa_gfx_save(" \
    "__nexa_gfx_alpha_get()"

expect_reject "reject: gfx.alpha with two arguments" \
'#include <std/gfx>
fn main() {
    gfx.alpha(1, 2);
}
' \
    "gfx.alpha() or gfx.alpha(a)"

expect_reject "reject: gfx.save with no path" \
'#include <std/gfx>
fn main() {
    gfx.save();
}
' \
    "gfx.save(path)"

expect_reject "reject: gfx.save on a number" \
'#include <std/gfx>
fn main() {
    gfx.save(1);
}
' \
    "gfx.save(path) expects a string path"

expect_reject "reject: unknown gfx method still lists the new ones" \
'#include <std/gfx>
fn main() {
    gfx.opacity(128);
}
' \
    "blit, alpha, save"

# The display-level test and the demo cannot run here -- linking std/gfx pulls
# in the window backend -- but they can still be held to compiling, so neither
# rots into a program that no longer parses.
for src in "$SUITE/gfx_alpha_test.nxa" "$ROOT/Examples/alpha_demo.nxa"; do
    [ -f "$src" ] || continue
    name=$(basename "$src")
    log=$("$NEXAC" "$src" --source "$WORK/$name.cpp" 2>&1)
    if [ $? -ne 0 ]; then
        echo "FAIL transpile: $name"
        printf '%s\n' "$log" | sed 's/^/  /'
        fails=$((fails + 1))
    else
        echo "ok transpile: $name"
    fi
done

# --- pixels: the blend, the blit and the BMP, against a stub framebuffer ------

CXX=${CXX:-}
if [ -z "$CXX" ]; then
    for c in clang++ g++ c++; do
        if command -v "$c" >/dev/null 2>&1; then CXX=$c; break; fi
    done
fi
if [ -z "$CXX" ]; then
    echo "SKIP pixels: no C++ compiler found (set CXX)"
    if [ $fails -eq 0 ]; then
        echo "gfx_alpha ok (pixels skipped)"
        exit 0
    fi
    echo "gfx_alpha: $fails failure(s)"
    exit 1
fi

awk '/\[nexa:(rasterizers|imgstore|blit|screenshot)-begin\]/ { taking = 1; next }
     /\[nexa:(rasterizers|imgstore|blit|screenshot)-end\]/   { taking = 0 }
     taking' "$HEADER" > "$WORK/lifted.inc"

for want in __nexa_gfx_alpha_set __nexa_gfx_put_a __nexa_gfx_blit __nexa_gfx_save; do
    if ! grep -q "$want" "$WORK/lifted.inc"; then
        echo "FAIL pixels: could not lift $want out of include/GfxRuntime.hpp"
        echo "     (the [nexa:...-begin] / [nexa:...-end] markers moved)"
        echo "gfx_alpha: $((fails + 1)) failure(s)"
        exit 1
    fi
done

# The stub carries only the framebuffer fields the lifted code reads. If any of
# it ever reaches for a window handle or an image decoder, this will not
# compile -- which is the point.
{
    cat <<'PROLOGUE'
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>

struct __nexa_Gfx {
    int w;
    int h;
    int ready;
    unsigned char* fb;
};
static __nexa_Gfx __nexa_g = {};
PROLOGUE
    cat "$WORK/lifted.inc"
    cat <<'EPILOGUE'

// --- the test itself --------------------------------------------------------

static void fb_open(int w, int h) {
    delete[] __nexa_g.fb;
    __nexa_g.fb = new unsigned char[(size_t)w * (size_t)h * 4];
    __nexa_g.w = w;
    __nexa_g.h = h;
    __nexa_g.ready = 1;
    __nexa_gfx_alpha_set(255);
    __nexa_gfx_clear(0, 0, 0);
}

static void fb_close() {
    delete[] __nexa_g.fb;
    __nexa_g.fb = nullptr;
    __nexa_g.w = 0;
    __nexa_g.h = 0;
    __nexa_g.ready = 0;
}

static std::vector<unsigned char> snapshot() {
    size_t n = (size_t)__nexa_g.w * (size_t)__nexa_g.h * 4;
    return std::vector<unsigned char>(__nexa_g.fb, __nexa_g.fb + n);
}

static void same(const char* label, const std::vector<unsigned char>& a,
                 const std::vector<unsigned char>& b) {
    std::printf("%s=%s\n", label, a == b ? "yes" : "no");
}

// One row of a framebuffer, as the blue channel of each pixel: enough to read
// a gradient back without printing three numbers per pixel.
static void row(const char* label, int y) {
    std::printf("%s:", label);
    for (int x = 0; x < __nexa_g.w; x++) {
        int c = __nexa_gfx_get(x, y);
        std::printf(" %d", c < 0 ? -1 : (c & 0xFF));
    }
    std::putchar('\n');
}

static void col(const char* label, int x) {
    std::printf("%s:", label);
    for (int y = 0; y < __nexa_g.h; y++) {
        int c = __nexa_gfx_get(x, y);
        std::printf(" %d", c < 0 ? -1 : (c & 0xFF));
    }
    std::putchar('\n');
}

// Registers an image in the store the way __nexa_gfx_store_img does, so the
// blit under test is reached exactly as a decoded PNG would reach it. Pixels
// are straight (non-premultiplied) RGBA, which is what every decode path in
// GfxRuntime.hpp produces.
static int make_img(int w, int h, const unsigned char* rgba) {
    if (__nexa_imgs.empty()) {
        __nexa_GfxImg z;
        z.w = 0;
        z.h = 0;
        z.px = NULL;
        __nexa_imgs.push_back(z);
        __nexa_img_paths.push_back("");
    }
    __nexa_GfxImg im;
    im.w = w;
    im.h = h;
    im.px = new unsigned char[(size_t)w * (size_t)h * 4];
    std::memcpy(im.px, rgba, (size_t)w * (size_t)h * 4);
    __nexa_imgs.push_back(im);
    __nexa_img_paths.push_back("");
    return (int)__nexa_imgs.size() - 1;
}

static std::string slurp(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return std::string();
    std::string out;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    std::fclose(f);
    return out;
}

static unsigned int le32(const std::string& s, size_t off) {
    return (unsigned char)s[off] | ((unsigned char)s[off + 1] << 8)
         | ((unsigned char)s[off + 2] << 16) | ((unsigned char)s[off + 3] << 24);
}

static unsigned int le16(const std::string& s, size_t off) {
    return (unsigned char)s[off] | ((unsigned char)s[off + 1] << 8);
}

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : ".";

    // --- the alpha value itself ---------------------------------------------
    fb_open(8, 4);
    std::printf("alpha_default=%d\n", __nexa_gfx_alpha_get());
    int was = __nexa_gfx_alpha_set(128);
    std::printf("alpha_set=%d,%d\n", was, __nexa_gfx_alpha_get());
    std::printf("alpha_clamp=%d,%d\n", __nexa_gfx_alpha_set(-5), __nexa_gfx_alpha_set(300));

    // --- the blend, white over black, across the range ----------------------
    // (255*a + 0*(255-a) + 127) / 255 lands on a itself for every a here.
    fb_open(8, 4);
    {
        const int as[] = {0, 1, 64, 128, 192, 254, 255};
        for (int i = 0; i < 7; i++) {
            __nexa_gfx_alpha_set(as[i]);
            __nexa_gfx_plot(i, 0, 255, 255, 255);
        }
    }
    row("blend_over_black", 0);

    // Over a non-black background, and with a colour that is not white:
    // white over 100 at a=128 is (255*128 + 100*127 + 127)/255 = 178;
    // 60 over 200 at a=200 is (60*200 + 200*55 + 127)/255 = 90.
    fb_open(4, 1);
    __nexa_gfx_alpha_set(255);
    __nexa_gfx_plot(0, 0, 100, 100, 100);
    __nexa_gfx_plot(1, 0, 200, 200, 200);
    __nexa_gfx_alpha_set(128);
    __nexa_gfx_plot(0, 0, 255, 255, 255);
    __nexa_gfx_alpha_set(200);
    __nexa_gfx_plot(1, 0, 60, 60, 60);
    row("blend_over_colour", 0);

    // Channels are blended independently, not as one grey.
    fb_open(1, 1);
    __nexa_gfx_alpha_set(255);
    __nexa_gfx_plot(0, 0, 0, 0, 255);
    __nexa_gfx_alpha_set(128);
    __nexa_gfx_plot(0, 0, 255, 0, 0);
    std::printf("blend_channels=%06X\n", (unsigned)__nexa_gfx_get(0, 0));

    // --- alpha 255 is byte-for-byte the opaque draw -------------------------
    fb_open(12, 8);
    __nexa_gfx_fill_circle(5, 4, 3, 200, 30, 60);
    __nexa_gfx_line_thick(0, 0, 11, 7, 10, 240, 90, 3);
    std::vector<unsigned char> opaque = snapshot();
    fb_open(12, 8);
    __nexa_gfx_alpha_set(200);
    __nexa_gfx_alpha_set(255);
    __nexa_gfx_fill_circle(5, 4, 3, 200, 30, 60);
    __nexa_gfx_line_thick(0, 0, 11, 7, 10, 240, 90, 3);
    same("alpha255_is_the_old_path", opaque, snapshot());

    // --- alpha 0 draws nothing at all ---------------------------------------
    fb_open(12, 8);
    std::vector<unsigned char> blank = snapshot();
    __nexa_gfx_alpha_set(0);
    __nexa_gfx_fill(0, 0, 12, 8, 255, 255, 255);
    __nexa_gfx_fill_circle(5, 4, 3, 255, 255, 255);
    __nexa_gfx_line(0, 0, 11, 7, 255, 255, 255);
    same("alpha0_draws_nothing", blank, snapshot());

    // --- gfx.clear is a reset, not a draw -----------------------------------
    fb_open(4, 1);
    __nexa_gfx_alpha_set(128);
    __nexa_gfx_clear(255, 255, 255);
    row("clear_ignores_alpha", 0);

    // --- a pixel covered twice is blended twice (documented) ----------------
    // 255 over 0 at a=128 is 128; 255 over 128 at a=128 is
    // (255*128 + 128*127 + 127)/255 = 192.
    fb_open(4, 1);
    __nexa_gfx_alpha_set(128);
    __nexa_gfx_plot(0, 0, 255, 255, 255);
    __nexa_gfx_plot(1, 0, 255, 255, 255);
    __nexa_gfx_plot(1, 0, 255, 255, 255);
    row("double_blend", 0);

    // --- blits ---------------------------------------------------------------
    // A 4x1 strip: opaque blue, half-alpha white, fully transparent white, and
    // opaque white. Straight (non-premultiplied) RGBA.
    const unsigned char strip[16] = {
        0, 0, 255, 255,
        255, 255, 255, 128,
        255, 255, 255, 0,
        255, 255, 255, 255,
    };
    int img = make_img(4, 1, strip);

    // Opaque pixels copy, the transparent one leaves the background alone, the
    // half one blends: white over black at 128 is 128.
    fb_open(4, 1);
    std::printf("blit_drew=%d\n", __nexa_gfx_blit(0, 0, img, 0, 0, 0, 0, 0, 0));
    row("blit_alpha", 0);

    // The whole strip over a grey background, so the transparent pixel is
    // visibly not written: white over 100 at 128 is 178.
    fb_open(4, 1);
    __nexa_gfx_fill(0, 0, 4, 1, 100, 100, 100);
    __nexa_gfx_blit(0, 0, img, 0, 0, 0, 0, 0, 0);
    row("blit_over_grey", 0);

    // An all-opaque image is the same bytes as drawing it by hand -- the fast
    // path is still a straight copy.
    const unsigned char opaque_px[8] = {10, 20, 30, 255, 40, 50, 60, 255};
    int solid = make_img(2, 1, opaque_px);
    fb_open(2, 1);
    __nexa_gfx_plot(0, 0, 10, 20, 30);
    __nexa_gfx_plot(1, 0, 40, 50, 60);
    std::vector<unsigned char> byhand = snapshot();
    fb_open(2, 1);
    __nexa_gfx_blit(0, 0, solid, 0, 0, 0, 0, 0, 0);
    same("blit_opaque_is_a_copy", byhand, snapshot());

    // The global alpha multiplies with the image's own: an opaque pixel at
    // gfx.alpha(128) is 128, and the half-alpha pixel becomes
    // (128*128 + 127)/255 = 64, so white over black lands on 64.
    fb_open(4, 1);
    __nexa_gfx_alpha_set(128);
    __nexa_gfx_blit(0, 0, img, 0, 0, 0, 0, 0, 0);
    row("blit_global_alpha", 0);

    fb_open(4, 1);
    __nexa_gfx_alpha_set(0);
    std::vector<unsigned char> untouched = snapshot();
    __nexa_gfx_blit(0, 0, img, 0, 0, 0, 0, 0, 0);
    same("blit_alpha0_draws_nothing", untouched, snapshot());

    // --- mirrored blits ------------------------------------------------------
    // A 4x1 ramp and a 1x4 ramp, so a flip is obvious in the numbers.
    const unsigned char ramp4[16] = {
        10, 10, 10, 255,
        20, 20, 20, 255,
        30, 30, 30, 255,
        40, 40, 40, 255,
    };
    int hramp = make_img(4, 1, ramp4);
    int vramp = make_img(1, 4, ramp4);

    fb_open(4, 1);
    __nexa_gfx_blit(0, 0, hramp, 0, 0, 0, 0, 0, 0);
    row("blit_plain", 0);
    fb_open(4, 1);
    __nexa_gfx_blit(0, 0, hramp, -4, 0, 0, 0, 0, 0);
    row("blit_flip_x", 0);

    fb_open(1, 4);
    __nexa_gfx_blit(0, 0, vramp, 0, -4, 0, 0, 0, 0);
    col("blit_flip_y", 0);

    // Both axes at once, on a 2x2 so the corners swap diagonally.
    const unsigned char quad[16] = {
        1, 1, 1, 255,
        2, 2, 2, 255,
        3, 3, 3, 255,
        4, 4, 4, 255,
    };
    int q = make_img(2, 2, quad);
    fb_open(2, 2);
    __nexa_gfx_blit(0, 0, q, -2, -2, 0, 0, 0, 0);
    row("blit_flip_both_top", 0);
    row("blit_flip_both_bottom", 1);

    // A mirrored blit lands in the same box as the plain one: same eight
    // columns starting at x, only the order of the pixels differs.
    fb_open(8, 1);
    __nexa_gfx_blit(2, 0, hramp, 4, 0, 0, 0, 0, 0);
    row("blit_box_plain", 0);
    fb_open(8, 1);
    __nexa_gfx_blit(2, 0, hramp, -4, 0, 0, 0, 0, 0);
    row("blit_box_flipped", 0);

    // Mirroring composes with scaling and with a source rect: the middle two
    // pixels of the ramp, doubled, backwards.
    fb_open(4, 1);
    __nexa_gfx_blit(0, 0, hramp, -4, 1, 1, 0, 2, 1);
    row("blit_flip_scaled_subrect", 0);

    // Clipped on the left: the part of the mirrored image that is on screen is
    // the part that would be on screen unmirrored, with mirrored content.
    fb_open(4, 1);
    __nexa_gfx_blit(-2, 0, hramp, -4, 0, 0, 0, 0, 0);
    row("blit_flip_clipped", 0);

    // --- blits that must not run away ---------------------------------------
    // Destinations the size of the coordinate space, and INT_MIN, which is the
    // one value that cannot simply be negated.
    fb_open(8, 4);
    std::printf("blit_huge=%d\n", __nexa_gfx_blit(0, 0, hramp, 2000000000, 2000000000, 0, 0, 0, 0));
    std::printf("blit_huge_flipped=%d\n", __nexa_gfx_blit(0, 0, hramp, -2000000000, -2000000000, 0, 0, 0, 0));
    std::printf("blit_int_min=%d\n", __nexa_gfx_blit(0, 0, hramp, (-2147483647 - 1), (-2147483647 - 1), 0, 0, 0, 0));
    std::printf("blit_far_away=%d\n", __nexa_gfx_blit(2000000000, 2000000000, hramp, 0, 0, 0, 0, 0, 0));
    std::printf("blit_far_negative=%d\n", __nexa_gfx_blit(-2000000000, -2000000000, hramp, 0, 0, 0, 0, 0, 0));
    std::printf("blit_bad_handle=%d\n", __nexa_gfx_blit(0, 0, 9999, 0, 0, 0, 0, 0, 0));
    std::printf("extremes=done\n");

    // --- gfx.save ------------------------------------------------------------
    // 3 wide, so a BMP row is 9 bytes padded up to 12 and the padding is under
    // test too.
    fb_open(3, 2);
    __nexa_gfx_alpha_set(255);
    __nexa_gfx_plot(0, 0, 255, 0, 0);
    __nexa_gfx_plot(1, 0, 0, 255, 0);
    __nexa_gfx_plot(2, 0, 0, 0, 255);
    __nexa_gfx_plot(0, 1, 1, 2, 3);
    __nexa_gfx_plot(2, 1, 255, 255, 255);
    std::string shot = dir + "/shot.bmp";
    std::printf("save=%d\n", __nexa_gfx_save(shot));
    std::string bytes = slurp(shot);
    std::printf("save_size=%d\n", (int)bytes.size());
    if (bytes.size() >= 54) {
        std::printf("save_magic=%c%c\n", bytes[0], bytes[1]);
        std::printf("save_filesize_field=%u\n", le32(bytes, 2));
        std::printf("save_offset=%u\n", le32(bytes, 10));
        std::printf("save_dib=%u\n", le32(bytes, 14));
        std::printf("save_dims=%u,%u\n", le32(bytes, 18), le32(bytes, 22));
        std::printf("save_planes=%u\n", le16(bytes, 26));
        std::printf("save_bpp=%u\n", le16(bytes, 28));
        std::printf("save_compression=%u\n", le32(bytes, 30));
        std::printf("save_imagesize=%u\n", le32(bytes, 34));
        // Bottom-up rows of BGR, padded to 4 bytes: read it back the way any
        // other program would and compare against the framebuffer.
        int ok = 1;
        int pad = 1;
        size_t stride = 12;
        for (int y = 0; y < __nexa_g.h; y++) {
            size_t off = 54 + stride * (size_t)(__nexa_g.h - 1 - y);
            for (int x = 0; x < __nexa_g.w; x++) {
                unsigned char b = (unsigned char)bytes[off + (size_t)x * 3 + 0];
                unsigned char g = (unsigned char)bytes[off + (size_t)x * 3 + 1];
                unsigned char r = (unsigned char)bytes[off + (size_t)x * 3 + 2];
                int c = (r << 16) | (g << 8) | b;
                if (c != __nexa_gfx_get(x, y)) ok = 0;
            }
            for (size_t p = (size_t)__nexa_g.w * 3; p < stride; p++) {
                if (bytes[off + p] != 0) pad = 0;
            }
        }
        std::printf("save_roundtrip=%s\n", ok ? "yes" : "no");
        std::printf("save_padding_zeroed=%s\n", pad ? "yes" : "no");
    }

    // Refused: no path, a directory that does not exist, and a closed window.
    std::printf("save_empty_path=%d\n", __nexa_gfx_save(""));
    std::printf("save_bad_dir=%d\n", __nexa_gfx_save(dir + "/no-such-dir/shot.bmp"));

    // A window that was never opened, and one whose framebuffer went away, are
    // the same thing to every entry point under test here.
    fb_close();
    std::printf("closed_alpha=%d\n", __nexa_gfx_alpha_set(128));
    __nexa_gfx_plot(0, 0, 255, 255, 255);
    __nexa_gfx_fill(0, 0, 4, 4, 255, 255, 255);
    std::printf("closed_blit=%d\n", __nexa_gfx_blit(0, 0, img, 0, 0, 0, 0, 0, 0));
    std::printf("closed_save=%d\n", __nexa_gfx_save(dir + "/closed.bmp"));
    std::printf("closed=no-op\n");
    return 0;
}
EPILOGUE
} > "$WORK/pixels.cpp"

# -Wno-unused-function: this suite drives the alpha, blit and save paths, not
# every shape in the lifted rasterizer block. Tests/gfx_shapes_cases.sh already
# holds that same block to a completely warning-free build.
build=$("$CXX" -std=c++17 -O1 -Wall -Wextra -Wno-unused-function "$WORK/pixels.cpp" -o "$WORK/pixels" 2>&1)
status=$?
if [ $status -ne 0 ]; then
    echo "FAIL pixels: the lifted alpha/blit/save code does not compile on its own"
    printf '%s\n' "$build" | sed 's/^/  /'
    fails=$((fails + 1))
elif [ -n "$build" ]; then
    echo "FAIL pixels: the compiler had something to say about the lifted code"
    printf '%s\n' "$build" | sed 's/^/  /'
    fails=$((fails + 1))
else
    "$WORK/pixels" "$WORK" > "$WORK/pixels.got" 2>&1
    rc=$?
    cat > "$WORK/pixels.want" <<'WANT'
alpha_default=255
alpha_set=128,128
alpha_clamp=0,255
blend_over_black: 0 1 64 128 192 254 255 0
blend_over_colour: 178 90 0 0
blend_channels=80007F
alpha255_is_the_old_path=yes
alpha0_draws_nothing=yes
clear_ignores_alpha: 255 255 255 255
double_blend: 128 192 0 0
blit_drew=1
blit_alpha: 255 128 0 255
blit_over_grey: 255 178 100 255
blit_opaque_is_a_copy=yes
blit_global_alpha: 128 64 0 128
blit_alpha0_draws_nothing=yes
blit_plain: 10 20 30 40
blit_flip_x: 40 30 20 10
blit_flip_y: 40 30 20 10
blit_flip_both_top: 4 3
blit_flip_both_bottom: 2 1
blit_box_plain: 0 0 10 20 30 40 0 0
blit_box_flipped: 0 0 40 30 20 10 0 0
blit_flip_scaled_subrect: 30 30 20 20
blit_flip_clipped: 20 10 0 0
blit_huge=1
blit_huge_flipped=1
blit_int_min=1
blit_far_away=0
blit_far_negative=0
blit_bad_handle=0
extremes=done
save=1
save_size=78
save_magic=BM
save_filesize_field=78
save_offset=54
save_dib=40
save_dims=3,2
save_planes=1
save_bpp=24
save_compression=0
save_imagesize=24
save_roundtrip=yes
save_padding_zeroed=yes
save_empty_path=0
save_bad_dir=0
closed_alpha=128
closed_blit=0
closed_save=0
closed=no-op
WANT
    if [ $rc -ne 0 ]; then
        echo "FAIL pixels: the harness exited $rc"
        sed 's/^/  /' "$WORK/pixels.got"
        fails=$((fails + 1))
    elif ! diff -u "$WORK/pixels.want" "$WORK/pixels.got" > "$WORK/pixels.diff"; then
        echo "FAIL pixels: alpha/blit/save differ from the documented behaviour (-want +got):"
        tail -n +3 "$WORK/pixels.diff" | sed 's/^/  /'
        fails=$((fails + 1))
    else
        echo "ok pixels: alpha, blits and gfx.save match the documentation"
    fi
fi

if [ $fails -eq 0 ]; then
    echo "gfx_alpha ok"
    exit 0
fi
echo "gfx_alpha: $fails failure(s)"
exit 1
