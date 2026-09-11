#!/bin/sh
# std/gfx shape rasterizers (BOB-19).
#
# Two things have to be true for gfx.rect / gfx.circle / gfx.poly / ... to work,
# and they fail in different ways, so they are checked separately:
#
#   dispatch    A .nxa call reaches the right runtime function with the right
#               arguments, and a malformed call is refused in Nexa terms rather
#               than by the C++ compiler. Checked by transpiling (--source),
#               which needs no display and no X11 headers.
#
#   raster      The shapes actually land on the pixels the documentation says
#               they do. A gfx program cannot be *run* on a headless box -- the
#               window backend is linked in statically -- so this suite lifts
#               the rasterizer block straight out of include/GfxRuntime.hpp
#               (it is marked off there, and touches nothing but the shared
#               framebuffer), compiles it against a stub framebuffer, and
#               compares the drawing to ASCII art written out by hand from
#               SYNTAX/Modules.txt. No window, no display, no X11.
#
# Usage: Tests/gfx_shapes_cases.sh [path-to-NexaC]      (run from the repo root)

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

# expect_emit <label> <nexa-body> <substring>...
# Transpiles a program and asserts each substring shows up in the generated C++.
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

# expect_reject <label> <nexa-body> <expected-substring>
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

expect_emit "dispatch: every shape call" \
'#include <std/gfx>
#include <std/io>
fn main() {
    let xs: []int = [1, 9, 5];
    let ys: []int = [1, 2, 9];
    gfx.rect(1, 2, 3, 4, 10, 11, 12);
    gfx.circle(1, 2, 3, 10, 11, 12);
    gfx.fill_circle(1, 2, 3, 10, 11, 12);
    gfx.ellipse(1, 2, 3, 4, 10, 11, 12);
    gfx.fill_ellipse(1, 2, 3, 4, 10, 11, 12);
    gfx.tri(1, 2, 3, 4, 5, 6, 10, 11, 12);
    gfx.fill_tri(1, 2, 3, 4, 5, 6, 10, 11, 12);
    gfx.line(1, 2, 3, 4, 10, 11, 12);
    gfx.line(1, 2, 3, 4, 10, 11, 12, 7);
    io.println(gfx.poly(xs, ys, 10, 11, 12));
    io.println(gfx.fill_poly(xs, ys, 10, 11, 12));
}
' \
    "__nexa_gfx_rect(1, 2, 3, 4, 10, 11, 12)" \
    "__nexa_gfx_circle(1, 2, 3, 10, 11, 12)" \
    "__nexa_gfx_fill_circle(1, 2, 3, 10, 11, 12)" \
    "__nexa_gfx_ellipse(1, 2, 3, 4, 10, 11, 12)" \
    "__nexa_gfx_fill_ellipse(1, 2, 3, 4, 10, 11, 12)" \
    "__nexa_gfx_tri(1, 2, 3, 4, 5, 6, 10, 11, 12)" \
    "__nexa_gfx_fill_tri(1, 2, 3, 4, 5, 6, 10, 11, 12)" \
    "__nexa_gfx_line(1, 2, 3, 4, 10, 11, 12)" \
    "__nexa_gfx_line_thick(1, 2, 3, 4, 10, 11, 12, 7)"

# gfx.poly / gfx.fill_poly report whether they drew, so they have to be usable
# as a value, not only as a statement.
expect_emit "dispatch: poly is an int expression" \
'#include <std/gfx>
#include <std/io>
fn main() {
    let xs: []int = [1, 9, 5];
    let ys: []int = [1, 2, 9];
    let drew: int = gfx.fill_poly(xs, ys, 1, 2, 3);
    if (gfx.poly(xs, ys, 1, 2, 3) == 1) {
        io.println(drew);
    }
}
' \
    "__nexa_gfx_fill_poly(" \
    "__nexa_gfx_poly("

expect_reject "reject: gfx.rect short one colour" \
'#include <std/gfx>
fn main() {
    gfx.rect(1, 2, 3, 4, 10, 11);
}
' \
    "gfx.rect(x, y, w, h, r, g, b)"

expect_reject "reject: gfx.circle with ellipse arguments" \
'#include <std/gfx>
fn main() {
    gfx.circle(1, 2, 3, 4, 10, 11, 12);
}
' \
    "gfx.circle(cx, cy, rad, r, g, b)"

expect_reject "reject: gfx.line with two thicknesses" \
'#include <std/gfx>
fn main() {
    gfx.line(1, 2, 3, 4, 10, 11, 12, 7, 8);
}
' \
    "gfx.line(x1, y1, x2, y2, r, g, b[, t])"

expect_reject "reject: gfx.poly on a string slice" \
'#include <std/gfx>
fn main() {
    let xs: []string = ["a", "b", "c"];
    let ys: []int = [1, 2, 3];
    gfx.poly(xs, ys, 1, 2, 3);
}
' \
    "expects []int point lists"

expect_reject "reject: gfx.fill_poly on a plain int" \
'#include <std/gfx>
fn main() {
    let ys: []int = [1, 2, 3];
    gfx.fill_poly(7, ys, 1, 2, 3);
}
' \
    "expects []int point lists"

# The display-level test and the demo cannot run here -- linking std/gfx pulls
# in the window backend -- but they can still be held to compiling, so neither
# rots into a program that no longer parses.
for src in "$SUITE/gfx_shapes_test.nxa" "$ROOT/Examples/shapes_demo.nxa"; do
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

# --- raster: the shapes themselves, against a stub framebuffer ---------------

CXX=${CXX:-}
if [ -z "$CXX" ]; then
    for c in clang++ g++ c++; do
        if command -v "$c" >/dev/null 2>&1; then CXX=$c; break; fi
    done
fi
if [ -z "$CXX" ]; then
    echo "SKIP raster: no C++ compiler found (set CXX)"
    if [ $fails -eq 0 ]; then
        echo "gfx_shapes ok (raster skipped)"
        exit 0
    fi
    echo "gfx_shapes: $fails failure(s)"
    exit 1
fi

awk '/\[nexa:rasterizers-begin\]/ { taking = 1; next }
     /\[nexa:rasterizers-end\]/   { taking = 0 }
     taking' "$HEADER" > "$WORK/raster.inc"

if ! grep -q "__nexa_gfx_fill_poly_pts" "$WORK/raster.inc"; then
    echo "FAIL raster: could not lift the rasterizer block out of include/GfxRuntime.hpp"
    echo "     (the [nexa:rasterizers-begin] / [nexa:rasterizers-end] markers moved)"
    echo "gfx_shapes: $((fails + 1)) failure(s)"
    exit 1
fi

# The stub carries only the framebuffer fields the rasterizers read. If a shape
# ever reaches for a window handle, this will not compile -- which is the point.
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
    cat "$WORK/raster.inc"
    cat <<'EPILOGUE'

// --- the test itself --------------------------------------------------------

static const int WHITE = 255;

static void fb_open(int w, int h) {
    delete[] __nexa_g.fb;
    __nexa_g.fb = new unsigned char[(size_t)w * (size_t)h * 4];
    __nexa_g.w = w;
    __nexa_g.h = h;
    __nexa_g.ready = 1;
    __nexa_gfx_clear(0, 0, 0);
}

static void fb_close() {
    delete[] __nexa_g.fb;
    __nexa_g.fb = nullptr;
    __nexa_g.w = 0;
    __nexa_g.h = 0;
    __nexa_g.ready = 0;
}

static void art(const char* label) {
    std::printf("%s\n", label);
    for (int y = 0; y < __nexa_g.h; y++) {
        for (int x = 0; x < __nexa_g.w; x++) {
            std::putchar(__nexa_gfx_get(x, y) == 0 ? '.' : '#');
        }
        std::putchar('\n');
    }
}

static std::vector<unsigned char> snapshot() {
    size_t n = (size_t)__nexa_g.w * (size_t)__nexa_g.h * 4;
    return std::vector<unsigned char>(__nexa_g.fb, __nexa_g.fb + n);
}

static void same(const char* label, const std::vector<unsigned char>& a,
                 const std::vector<unsigned char>& b) {
    std::printf("%s=%s\n", label, a == b ? "yes" : "no");
}

int main() {
    // The stub framebuffer behaves like the real one before anything is drawn
    // into it: the shapes below are only interesting if this much is true.
    fb_open(4, 4);
    __nexa_gfx_plot(1, 1, WHITE, WHITE, WHITE);
    __nexa_gfx_plot(-1, 9, WHITE, WHITE, WHITE);
    std::printf("plot=%d,%d\n", __nexa_gfx_get(1, 1), __nexa_gfx_get(0, 0));

    // --- rectangle outline --------------------------------------------------
    fb_open(12, 8);
    __nexa_gfx_rect(2, 1, 6, 4, WHITE, WHITE, WHITE);
    art("rect");
    std::vector<unsigned char> positive = snapshot();

    // Negative w/h flip exactly as gfx.fill flips them: the corner they are
    // measured from is the one *past* the rectangle, so (8,5,-6,-4) is the same
    // six-by-four box as (2,1,6,4) and not the one starting at 7,4.
    __nexa_gfx_clear(0, 0, 0);
    __nexa_gfx_rect(8, 5, -6, -4, WHITE, WHITE, WHITE);
    same("rect_negative_matches", positive, snapshot());

    // A one-pixel rectangle must not draw its single row or column twice over
    // the edge of the framebuffer, and must not vanish.
    __nexa_gfx_clear(0, 0, 0);
    __nexa_gfx_rect(3, 2, 1, 1, WHITE, WHITE, WHITE);
    std::printf("rect_1x1=%d,%d\n", __nexa_gfx_get(3, 2), __nexa_gfx_get(4, 2));
    __nexa_gfx_clear(0, 0, 0);
    __nexa_gfx_rect(1, 1, 4, 0, WHITE, WHITE, WHITE);
    std::printf("rect_zero_h=%d\n", __nexa_gfx_get(1, 1));

    fb_open(6, 6);
    __nexa_gfx_rect(-2, -2, 6, 6, WHITE, WHITE, WHITE);
    art("rect_clipped");

    // --- circles ------------------------------------------------------------
    fb_open(12, 12);
    __nexa_gfx_circle(5, 5, 3, WHITE, WHITE, WHITE);
    art("circle");

    fb_open(12, 12);
    __nexa_gfx_fill_circle(5, 5, 3, WHITE, WHITE, WHITE);
    art("fill_circle");

    fb_open(5, 5);
    __nexa_gfx_circle(2, 2, 0, WHITE, WHITE, WHITE);
    std::printf("circle_r0=%d,%d\n", __nexa_gfx_get(2, 2), __nexa_gfx_get(3, 2));
    __nexa_gfx_clear(0, 0, 0);
    __nexa_gfx_circle(2, 2, -1, WHITE, WHITE, WHITE);
    __nexa_gfx_fill_circle(2, 2, -1, WHITE, WHITE, WHITE);
    std::printf("circle_negative=%d\n", __nexa_gfx_get(2, 2));

    // --- ellipses -----------------------------------------------------------
    fb_open(13, 7);
    __nexa_gfx_fill_ellipse(6, 3, 5, 2, WHITE, WHITE, WHITE);
    art("fill_ellipse");

    fb_open(13, 7);
    __nexa_gfx_ellipse(6, 3, 5, 2, WHITE, WHITE, WHITE);
    art("ellipse");

    // --- triangles ----------------------------------------------------------
    fb_open(12, 8);
    __nexa_gfx_fill_tri(1, 1, 9, 1, 5, 6, WHITE, WHITE, WHITE);
    art("fill_tri");

    fb_open(12, 8);
    __nexa_gfx_tri(1, 1, 9, 1, 5, 6, WHITE, WHITE, WHITE);
    art("tri");

    // --- polygons -----------------------------------------------------------
    // A bow tie: the two loops cross, so the even-odd rule is what decides the
    // inside, and the waist where the edges meet is empty.
    fb_open(10, 8);
    {
        std::vector<int> xs = {0, 8, 0, 8};
        std::vector<int> ys = {0, 6, 6, 0};
        std::printf("fill_poly_drew=%d\n", __nexa_gfx_fill_poly(xs, ys, WHITE, WHITE, WHITE));
    }
    art("fill_poly_bowtie");

    fb_open(12, 8);
    {
        // Point lists reach the runtime as whatever integer vector held them,
        // so a wider slice has to work exactly like an int one.
        std::vector<int> xs = {1, 9, 5};
        std::vector<long long> ys = {1, 1, 6};
        std::printf("poly_drew=%d\n", __nexa_gfx_poly(xs, ys, WHITE, WHITE, WHITE));
    }
    art("poly_outline");

    // The filled polygon of an axis-aligned rectangle has to be the rectangle
    // gfx.fill draws -- same half-open edges, no fencepost of its own.
    fb_open(12, 8);
    __nexa_gfx_fill(2, 1, 6, 4, WHITE, WHITE, WHITE);
    std::vector<unsigned char> filled = snapshot();
    __nexa_gfx_clear(0, 0, 0);
    {
        std::vector<int> xs = {2, 8, 8, 2};
        std::vector<int> ys = {1, 1, 5, 5};
        __nexa_gfx_fill_poly(xs, ys, WHITE, WHITE, WHITE);
    }
    same("fill_poly_matches_fill", filled, snapshot());

    // Refused input: unequal lists, too few points, and a closed window.
    {
        std::vector<int> three = {0, 1, 2};
        std::vector<int> two = {0, 1};
        std::printf("poly_mismatched=%d\n", __nexa_gfx_poly(three, two, WHITE, WHITE, WHITE));
        std::printf("fill_poly_mismatched=%d\n", __nexa_gfx_fill_poly(three, two, WHITE, WHITE, WHITE));
        std::printf("poly_two_points=%d\n", __nexa_gfx_poly(two, two, WHITE, WHITE, WHITE));
        std::printf("fill_poly_two_points=%d\n", __nexa_gfx_fill_poly(two, two, WHITE, WHITE, WHITE));
    }

    // --- thick lines --------------------------------------------------------
    fb_open(12, 8);
    __nexa_gfx_line_thick(2, 4, 9, 4, WHITE, WHITE, WHITE, 3);
    art("thick_horizontal");

    fb_open(12, 8);
    __nexa_gfx_line_thick(5, 1, 5, 6, WHITE, WHITE, WHITE, 3);
    art("thick_vertical");

    fb_open(12, 12);
    __nexa_gfx_line_thick(2, 2, 9, 9, WHITE, WHITE, WHITE, 3);
    art("thick_diagonal");

    // t of 1 or less is the plain one-pixel line, to the pixel.
    fb_open(12, 8);
    __nexa_gfx_line(1, 1, 10, 6, WHITE, WHITE, WHITE);
    std::vector<unsigned char> thin = snapshot();
    __nexa_gfx_clear(0, 0, 0);
    __nexa_gfx_line_thick(1, 1, 10, 6, WHITE, WHITE, WHITE, 1);
    same("thick_t1_matches_line", thin, snapshot());
    __nexa_gfx_clear(0, 0, 0);
    __nexa_gfx_line_thick(1, 1, 10, 6, WHITE, WHITE, WHITE, 0);
    same("thick_t0_matches_line", thin, snapshot());

    // A thick "line" of zero length is the round cap on its own: a dot.
    fb_open(9, 9);
    __nexa_gfx_line_thick(4, 4, 4, 4, WHITE, WHITE, WHITE, 5);
    art("thick_dot");

    // --- colour clamping ----------------------------------------------------
    fb_open(4, 4);
    __nexa_gfx_rect(0, 0, 4, 4, 300, -5, 128);
    std::printf("colour_clamped=%d\n", __nexa_gfx_get(0, 0));

    // --- coordinates far outside the framebuffer ----------------------------
    // Clipping happens per row, so these cost nothing and, more to the point,
    // finish: a shape is never allowed to loop over its own coordinate space.
    fb_open(16, 16);
    __nexa_gfx_rect(-2000000000, -2000000000, 2000000000, 2000000000, WHITE, WHITE, WHITE);
    __nexa_gfx_rect(2000000000, 2000000000, 2000000000, 2000000000, WHITE, WHITE, WHITE);
    __nexa_gfx_circle(8, 8, 2000000000, WHITE, WHITE, WHITE);
    __nexa_gfx_fill_circle(8, 8, 2000000000, WHITE, WHITE, WHITE);
    __nexa_gfx_ellipse(8, 8, 2000000000, 2000000000, WHITE, WHITE, WHITE);
    __nexa_gfx_fill_tri(-2000000000, -2000000000, 2000000000, -2000000000, 0, 2000000000,
                        WHITE, WHITE, WHITE);
    __nexa_gfx_line_thick(-2000000000, 8, 2000000000, 8, WHITE, WHITE, WHITE, 100000);
    std::printf("extremes=done\n");

    // --- with no framebuffer at all (window closed or never opened) ---------
    fb_close();
    __nexa_gfx_rect(0, 0, 4, 4, WHITE, WHITE, WHITE);
    __nexa_gfx_circle(0, 0, 2, WHITE, WHITE, WHITE);
    __nexa_gfx_fill_circle(0, 0, 2, WHITE, WHITE, WHITE);
    __nexa_gfx_ellipse(0, 0, 2, 2, WHITE, WHITE, WHITE);
    __nexa_gfx_fill_ellipse(0, 0, 2, 2, WHITE, WHITE, WHITE);
    __nexa_gfx_tri(0, 0, 1, 1, 2, 2, WHITE, WHITE, WHITE);
    __nexa_gfx_fill_tri(0, 0, 1, 1, 2, 2, WHITE, WHITE, WHITE);
    __nexa_gfx_line_thick(0, 0, 4, 4, WHITE, WHITE, WHITE, 3);
    {
        std::vector<int> xs = {0, 4, 2};
        std::vector<int> ys = {0, 0, 4};
        std::printf("closed_poly=%d\n", __nexa_gfx_poly(xs, ys, WHITE, WHITE, WHITE));
        std::printf("closed_fill_poly=%d\n", __nexa_gfx_fill_poly(xs, ys, WHITE, WHITE, WHITE));
    }
    std::printf("closed=no-op\n");
    return 0;
}
EPILOGUE
} > "$WORK/raster.cpp"

build=$("$CXX" -std=c++17 -O1 -Wall -Wextra "$WORK/raster.cpp" -o "$WORK/raster" 2>&1)
status=$?
if [ $status -ne 0 ]; then
    echo "FAIL raster: the rasterizer block does not compile on its own"
    printf '%s\n' "$build" | sed 's/^/  /'
    fails=$((fails + 1))
elif [ -n "$build" ]; then
    echo "FAIL raster: the compiler had something to say about the rasterizers"
    printf '%s\n' "$build" | sed 's/^/  /'
    fails=$((fails + 1))
else
    "$WORK/raster" > "$WORK/raster.got" 2>&1
    rc=$?
    # Expected art, written from the documented semantics: '#' is a drawn pixel.
    cat > "$WORK/raster.want" <<'WANT'
plot=16777215,0
rect
............
..######....
..#....#....
..#....#....
..######....
............
............
............
rect_negative_matches=yes
rect_1x1=16777215,0
rect_zero_h=0
rect_clipped
...#..
...#..
...#..
####..
......
......
circle
............
............
.....#......
...##.##....
...#...#....
..#.....#...
...#...#....
...##.##....
.....#......
............
............
............
fill_circle
............
............
.....#......
...#####....
...#####....
..#######...
...#####....
...#####....
.....#......
............
............
............
circle_r0=16777215,0
circle_negative=0
fill_ellipse
.............
......#......
..#########..
.###########.
..#########..
......#......
.............
ellipse
.............
......#......
..####.####..
.#.........#.
..####.####..
......#......
.............
fill_tri
............
.########...
..#######...
...#####....
....###.....
.....#......
............
............
tri
............
.#########..
..#.....#...
...#...#....
...#...#....
....#.#.....
.....#......
............
fill_poly_drew=1
fill_poly_bowtie
########..
..#####...
...###....
..........
...###....
..#####...
..........
..........
poly_drew=1
poly_outline
............
.#########..
..#.....#...
...#...#....
...#...#....
....#.#.....
.....#......
............
fill_poly_matches_fill=yes
poly_mismatched=0
fill_poly_mismatched=0
poly_two_points=0
fill_poly_two_points=0
thick_horizontal
............
............
............
.##########.
.##########.
.##########.
............
............
thick_vertical
....###.....
....###.....
....###.....
....###.....
....###.....
....###.....
....###.....
....###.....
thick_diagonal
............
.###........
.####.......
.#####......
..#####.....
...#####....
....#####...
.....#####..
......#####.
.......####.
........###.
............
thick_t1_matches_line=yes
thick_t0_matches_line=yes
thick_dot
.........
.........
...###...
..#####..
..#####..
..#####..
...###...
.........
.........
colour_clamped=16711808
extremes=done
closed_poly=0
closed_fill_poly=0
closed=no-op
WANT
    if [ $rc -ne 0 ]; then
        echo "FAIL raster: the harness exited $rc"
        sed 's/^/  /' "$WORK/raster.got"
        fails=$((fails + 1))
    elif ! diff -u "$WORK/raster.want" "$WORK/raster.got" > "$WORK/raster.diff"; then
        echo "FAIL raster: shapes differ from the documented pixels (-want +got):"
        tail -n +3 "$WORK/raster.diff" | sed 's/^/  /'
        fails=$((fails + 1))
    else
        echo "ok raster: shapes land on the documented pixels"
    fi
fi

if [ $fails -eq 0 ]; then
    echo "gfx_shapes ok"
    exit 0
fi
echo "gfx_shapes: $fails failure(s)"
exit 1
