// Semantics cover for gfx3d's model transform: the arithmetic, executed.
//
// Everything else gfx3d draws has to be looked at to be checked -- it goes to
// the GPU, and there is no gfx3d.get to read a pixel back with, which is why
// Tests/gfx3d_cases.sh says plainly that it cannot catch a shape wound
// inside-out. The transform is the exception. Where a point lands is pure
// arithmetic, it needs no window, no driver and no display, and it is exactly
// the part where a sign or a multiplication order is easy to get wrong and
// impossible to notice: a rotation with its handedness backwards still spins,
// just the wrong way, and a composition in the wrong order still moves things,
// just around the wrong centre.
//
// So the runtime is compiled in and driven directly. No window is opened; none
// of this touches one.
//
// Built and run by Tests/gfx3d_cases.sh. NEXA_GEN is the generated .cpp.

#include <cstdio>
#include <cmath>

#ifndef NEXA_GEN
#error "define NEXA_GEN to the generated C++ file"
#endif

// The generated program has its own main(); this driver supplies the real one.
#define main __nexa_program_main
#include NEXA_GEN
#undef main

static int failures = 0;

static void expect(const char* what, float gx, float gy, float gz,
                   float wx, float wy, float wz) {
    const float eps = 0.0005f;
    if (std::fabs(gx - wx) > eps || std::fabs(gy - wy) > eps || std::fabs(gz - wz) > eps) {
        std::printf("FAIL %s: want %.2f %.2f %.2f, got %.2f %.2f %.2f\n",
                    what, wx, wy, wz, gx, gy, gz);
        failures++;
    }
}

static void point(float* x, float* y, float* z, float px, float py, float pz) {
    *x = px; *y = py; *z = pz;
    __nexa_g3_xf_point(x, y, z);
}

static void dir(float* x, float* y, float* z, float px, float py, float pz) {
    *x = px; *y = py; *z = pz;
    __nexa_g3_xf_dir(x, y, z);
}

int main() {
    float x, y, z;

    // Nothing asked for, nothing changed.
    __nexa_gfx3d_reset();
    point(&x, &y, &z, 7, 8, 9);
    expect("reset is identity", x, y, z, 7, 8, 9);

    __nexa_gfx3d_reset();
    __nexa_gfx3d_translate(5, -2, 3);
    point(&x, &y, &z, 1, 1, 1);
    expect("translate", x, y, z, 6, -1, 4);

    // Degrees, right-hand rule: looking back down an axis towards the origin,
    // a positive angle turns counter-clockwise. Each of the three is checked
    // on its own, because a sign error in one is invisible in a scene that
    // only ever turns about another.
    __nexa_gfx3d_reset();
    __nexa_gfx3d_rotate(0, 90, 0);
    point(&x, &y, &z, 1, 0, 0);
    expect("rotate Y 90 of +X", x, y, z, 0, 0, -1);

    __nexa_gfx3d_reset();
    __nexa_gfx3d_rotate(90, 0, 0);
    point(&x, &y, &z, 0, 1, 0);
    expect("rotate X 90 of +Y", x, y, z, 0, 0, 1);

    __nexa_gfx3d_reset();
    __nexa_gfx3d_rotate(0, 0, 90);
    point(&x, &y, &z, 1, 0, 0);
    expect("rotate Z 90 of +X", x, y, z, 0, 1, 0);

    // A full turn is where an accumulated sign error shows up as nothing.
    __nexa_gfx3d_reset();
    __nexa_gfx3d_rotate(0, 360, 0);
    point(&x, &y, &z, 1, 2, 3);
    expect("rotate Y 360", x, y, z, 1, 2, 3);

    __nexa_gfx3d_reset();
    __nexa_gfx3d_scale(3);
    point(&x, &y, &z, 2, -1, 0);
    expect("scale", x, y, z, 6, -3, 0);

    // The order the calls are written in is the order they read in: move a
    // shape somewhere and then turn it, and it turns where it stands.
    __nexa_gfx3d_reset();
    __nexa_gfx3d_translate(5, 0, 0);
    __nexa_gfx3d_rotate(0, 90, 0);
    point(&x, &y, &z, 1, 0, 0);
    expect("move then turn", x, y, z, 5, 0, -1);

    // Written the other way it swings around the origin instead, which is the
    // whole reason both orders have to be available.
    __nexa_gfx3d_reset();
    __nexa_gfx3d_rotate(0, 90, 0);
    __nexa_gfx3d_translate(5, 0, 0);
    point(&x, &y, &z, 0, 0, 0);
    expect("turn then move", x, y, z, 0, 0, -5);

    // A direction has no position, so a translation must leave it alone --
    // this is what keeps a shifted shape lit the same as an unshifted one.
    __nexa_gfx3d_reset();
    __nexa_gfx3d_translate(100, 50, -7);
    dir(&x, &y, &z, 1, 0, 0);
    expect("direction ignores translation", x, y, z, 1, 0, 0);

    // But a rotation must turn it, or a turned shape is lit as though it had
    // not moved.
    __nexa_gfx3d_reset();
    __nexa_gfx3d_rotate(0, 90, 0);
    dir(&x, &y, &z, 1, 0, 0);
    expect("direction follows rotation", x, y, z, 0, 0, -1);

    // gfx3d.clear starts a frame, and the transform with it, so a forgotten
    // reset costs one frame rather than compounding over every later one.
    __nexa_gfx3d_reset();
    __nexa_gfx3d_translate(9, 9, 9);
    __nexa_gfx3d_clear(0, 0, 0);
    point(&x, &y, &z, 1, 2, 3);
    expect("clear resets the transform", x, y, z, 1, 2, 3);

    if (failures == 0) {
        std::printf("gfx3d transform semantics ok\n");
        return 0;
    }
    std::printf("gfx3d transform semantics: %d failure(s)\n", failures);
    return 1;
}
