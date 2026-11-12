#!/bin/sh
# std/physics: queries, a resting body, and the slice that keeps it out of
# a program that never calls it.
#
# Usage: Tests/physics_cases.sh [path-to-NexaC]

set -u

NEXAC="${1:-./NexaC}"
case "$NEXAC" in
    /*) ;;
    *) NEXAC="$(pwd)/$NEXAC" ;;
esac
WORK=$(mktemp -d 2>/dev/null || mktemp -d -t nexaphys)
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

cat > "$WORK/plain.nxa" <<'EOF'
#include <std/io>
fn main() {
    io.println("hi");
}
EOF
if ! "$NEXAC" "$WORK/plain.nxa" --source "$WORK/plain.cpp" > "$WORK/plain.log" 2>&1; then
    bad "plain transpile" "$(cat "$WORK/plain.log")"
elif grep -q __nexa_phys "$WORK/plain.cpp"; then
    bad "slice" "physics runtime emitted with no physics call"
else
    ok "a program without physics carries none of it"
fi

cat > "$WORK/rayonly.nxa" <<'EOF'
#include <std/physics>
fn main() {
    let t = physics.ray_sphere(0, 0, -5, 0, 0, 1, 0, 0, 0, 1);
    return t < 0;
}
EOF
if ! "$NEXAC" "$WORK/rayonly.nxa" --source "$WORK/ray.cpp" > "$WORK/ray.log" 2>&1; then
    bad "ray transpile" "$(cat "$WORK/ray.log")"
elif ! grep -q __nexa_phys_ray_sphere "$WORK/ray.cpp"; then
    bad "ray slice" "a ray did not emit the query"
elif grep -q __nexa_phys_step "$WORK/ray.cpp"; then
    bad "ray slice" "a ray-only program carries the solver"
elif grep -q __nexa_phys_mesh "$WORK/ray.cpp"; then
    bad "ray slice" "a ray carries mesh colliders"
else
    ok "a ray carries the query and not the solver"
fi

cat > "$WORK/use.nxa" <<'EOF'
#include <std/io>
#include <std/physics>
fn main() {
    if (physics.overlap_spheres(0, 0, 0, 1, 1, 0, 0, 1) != 1) { io.println("fail overlap"); return 1; }
    if (physics.overlap_spheres(0, 0, 0, 1, 3, 0, 0, 1) != 0) { io.println("fail gap"); return 1; }
    if (physics.overlap_sphere_capsule(2, 0, 0, 0.5, 0, 0, 0, 1, 0, 0, 0.6) != 1) { io.println("fail capsule"); return 1; }
    let t = physics.ray_sphere(0, 0, -5, 0, 0, 1, 0, 0, 0, 1);
    if (t < 3.9 || t > 4.1) { io.println("fail ray"); return 1; }
    if (physics.hz() > -0.9 || physics.hz() < -1.1) { io.println("fail hit"); return 1; }
    let tc = physics.ray_capsule(0, 0, -5, 0, 0, 1, -1, 0, 0, 1, 0, 0, 0.5);
    if (tc < 4.4 || tc > 4.6) { io.println("fail capsule ray"); return 1; }
    let ground = physics.box(0, 0, 0, 10, 1, 10);
    physics.mass(ground, 0);
    let ball = physics.sphere(0, 5, 0, 0.5);
    let i = 0;
    while (i < 300) {
        physics.step(0.016);
        i = i + 1;
    }
    let y = physics.y(ball);
    if (y < 0.98 || y > 1.04) { io.println("fail rest"); io.println(y); return 1; }

    // A sphere dropped onto the side of another slides off. It does not stay stuck.
    let base = physics.sphere(2, 1, 0, 0.5);
    let top = physics.sphere(2.35, 3.0, 0, 0.5);
    i = 0;
    while (i < 400) { physics.step(0.016); i = i + 1; }
    let ybase = physics.y(base);
    let ytop = physics.y(top);
    if (ybase < 0.95 || ybase > 1.2) { io.println("fail slideoff base"); io.println(ybase); return 1; }
    if (ytop > 1.35) { io.println("fail slideoff"); io.println(ytop); return 1; }

    // Horizontal speed on the floor dies under friction, and the ball stays put.
    let slide = physics.sphere(0, 1, 4, 0.5);
    physics.velocity(slide, 4, 0, 0);
    i = 0;
    while (i < 200) { physics.step(0.016); i = i + 1; }
    let sv = physics.vx(slide);
    let sy = physics.y(slide);
    if (sv > 0.08 || sv < -0.08) { io.println("fail slide"); io.println(sv); return 1; }
    if (sy < 0.98 || sy > 1.06) { io.println("fail slide y"); io.println(sy); return 1; }

    // A long step from high up used to jump a thin floor. The centre stays on it.
    let thin = physics.box(0, 0, 8, 8, 0.1, 8);
    physics.mass(thin, 0);
    let drop = physics.sphere(0, 25, 8, 0.25);
    i = 0;
    while (i < 80) { physics.step(0.05); i = i + 1; }
    let dy = physics.y(drop);
    if (dy < 0.22 || dy > 0.45) { io.println("fail tunnel"); io.println(dy); return 1; }

    // Two spheres of the same mass. The one that was sitting takes the hit;
    // the one that was moving does not plough on through.
    physics.gravity(0, 0, 0);
    let still = physics.sphere(0, 6, 0, 0.5);
    let mover = physics.sphere(-2, 6, 0, 0.5);
    physics.velocity(mover, 4, 0, 0);
    i = 0;
    while (i < 40) { physics.step(0.016); i = i + 1; }
    let vs = physics.vx(still);
    let vm = physics.vx(mover);
    if (vs < 1.5) { io.println("fail share"); io.println(vs); return 1; }
    if (vm > vs) { io.println("fail heavier"); io.println(vm); return 1; }
    let sx = physics.x(still) - physics.x(mover);
    let sy2 = physics.y(still) - physics.y(mover);
    let sz = physics.z(still) - physics.z(mover);
    if (sx * sx + sy2 * sy2 + sz * sz < 0.90) { io.println("fail inside"); return 1; }
    physics.gravity(0, -9.81, 0);

    // Eight spheres dropped through each other have to end up separated.
    let pile: []int;
    let k = 0;
    while (k < 8) {
        pile.push(physics.sphere(-3, 1.2 + k * 0.25, 0, 0.5));
        k = k + 1;
    }
    k = 0;
    while (k < 300) { physics.step(0.016); k = k + 1; }
    let p = 0;
    while (p < 8) {
        let q = p + 1;
        while (q < 8) {
            let dx = physics.x(pile[p]) - physics.x(pile[q]);
            let dy2 = physics.y(pile[p]) - physics.y(pile[q]);
            let dz = physics.z(pile[p]) - physics.z(pile[q]);
            let d2 = dx * dx + dy2 * dy2 + dz * dz;
            if (d2 < 0.90) { io.println("fail pile"); io.println(d2); return 1; }
            q = q + 1;
        }
        p = p + 1;
    }

    io.println("ok");
    return 0;
}
EOF
if ! "$NEXAC" "$WORK/use.nxa" -o "$WORK/use$EXE" > "$WORK/use.log" 2>&1; then
    bad "build" "$(tail -20 "$WORK/use.log")"
else
    out=$("$WORK/use$EXE" 2>&1 | tr -d '\r')
    if [ "$out" = "ok" ]; then
        ok "overlaps, rays, rest, slide-off, friction, and no tunnel"
    else
        bad "run" "got [$out]"
    fi
fi

if ! "$NEXAC" "$WORK/use.nxa" --source "$WORK/use.cpp" > "$WORK/use.src.log" 2>&1; then
    bad "body source" "$(cat "$WORK/use.src.log")"
elif grep -q __nexa_phys_mesh "$WORK/use.cpp"; then
    bad "body slice" "a sphere world carries mesh colliders"
elif grep -q __nexa_phys_squish "$WORK/use.cpp"; then
    bad "body slice" "a rigid world carries squishy mode"
else
    ok "a body world carries no mesh colliders"
fi

# A floor of two triangles. Span 2, centred, so scale 4 is a floor on y = 0
# covering x and z in [-2, 2]. Run from $WORK so the .obj path stays relative.
cat > "$WORK/floor.obj" <<'EOF'
v -1 0 -1
v 1 0 -1
v 1 0 1
v -1 0 1
f 1 2 3
f 1 3 4
EOF
cat > "$WORK/mesh.nxa" <<'EOF'
#include <std/io>
#include <std/physics>
#include <std/gfx3d>
fn main() {
    let model = gfx3d.model("floor.obj");
    if (model == 0) { io.println("fail model"); return 1; }
    let floor = physics.mesh(model, 0, 0, 0, 4);
    if (floor == 0) { io.println("fail mesh"); return 1; }
    physics.mass(floor, 0);
    let ball = physics.sphere(0, 2, 0, 0.5);
    let crate = physics.box(1.5, 3, -1.5, 1, 1, 1);
    let post = physics.capsule(-1.5, 2, -1.2, -1.5, 3, -1.2, 0.25);
    let i = 0;
    while (i < 300) {
        physics.step(0.016);
        i = i + 1;
    }
    let y = physics.y(ball);
    if (y < 0.42 || y > 0.65) { io.println("fail mesh rest"); io.println(y); return 1; }
    let cy = physics.y(crate);
    if (cy < 0.40 || cy > 0.70) { io.println("fail mesh box"); io.println(cy); return 1; }
    let py = physics.y1(post);
    if (py < 0.15 || py > 0.45) { io.println("fail mesh capsule"); io.println(py); return 1; }
    let drop = physics.sphere(-1.5, 25, 1.5, 0.5);
    i = 0;
    while (i < 80) { physics.step(0.05); i = i + 1; }
    let dy = physics.y(drop);
    if (dy < 0.35 || dy > 0.75) { io.println("fail mesh tunnel"); io.println(dy); return 1; }
    let t = physics.ray_mesh(0, 2, 0, 0, -1, 0, model, 0, 0, 0, 4);
    if (t < 1.9 || t > 2.1) { io.println("fail mesh ray"); io.println(t); return 1; }
    let hy = physics.hy();
    if (hy < -0.05 || hy > 0.05) { io.println("fail mesh hit"); io.println(hy); return 1; }
    io.println("ok");
    return 0;
}
EOF
if ! "$NEXAC" "$WORK/mesh.nxa" --source "$WORK/mesh.cpp" > "$WORK/mesh.src.log" 2>&1; then
    bad "mesh transpile" "$(cat "$WORK/mesh.src.log")"
elif ! grep -q __nexa_phys_mesh "$WORK/mesh.cpp"; then
    bad "mesh slice" "physics.mesh did not emit a mesh collider"
elif ! grep -q __nexa_phys_ray_mesh "$WORK/mesh.cpp"; then
    bad "mesh slice" "physics.ray_mesh did not emit a mesh ray"
elif ! "$NEXAC" "$WORK/mesh.nxa" -o "$WORK/use_mesh$EXE" > "$WORK/mesh.log" 2>&1; then
    bad "mesh build" "$(tail -30 "$WORK/mesh.log")"
else
    out=$(cd "$WORK" && "./use_mesh$EXE" 2>&1 | tr -d '\r')
    if [ "$out" = "ok" ]; then
        ok "a loaded mesh is a collider"
    else
        bad "mesh run" "got [$out]"
    fi
fi

cat > "$WORK/squish.nxa" <<'EOF'
#include <std/io>
#include <std/physics>
fn main() {
    physics.squishy(0);
    let ground = physics.box(0, 0, 0, 10, 1, 10);
    physics.mass(ground, 0);
    let rigid = physics.sphere(0, 5, 0, 0.5);
    let i = 0;
    while (i < 300) { physics.step(0.016); i = i + 1; }
    let y0 = physics.y(rigid);
    if (y0 < 0.98 || y0 > 1.04) { io.println("fail squish off"); io.println(y0); return 1; }

    physics.squishy(0.9);
    let ball = physics.sphere(3, 5, 0, 0.5);
    let peak = 0.0;
    i = 0;
    while (i < 90) {
        physics.step(0.016);
        let s = physics.squash(ball);
        if (s > peak) { peak = s; }
        i = i + 1;
    }
    if (peak < 0.18) { io.println("fail squash"); io.println(peak); return 1; }
    let y = physics.y(ball);
    if (y < 0.75 || y > 1.25) { io.println("fail squish y"); io.println(y); return 1; }
    i = 0;
    while (i < 180) { physics.step(0.016); i = i + 1; }
    y = physics.y(ball);
    let rest = physics.squash(ball);
    if (y < 0.84 || y > 1.00) { io.println("fail rest y"); io.println(y); return 1; }
    if (rest < 0.08 || rest > 0.32) { io.println("fail rest squash"); io.println(rest); return 1; }
    let low = physics.sphere(-2, 1.0, 0, 0.5);
    let high = physics.sphere(-2, 3.2, 0, 0.5);
    i = 0;
    while (i < 180) { physics.step(0.016); i = i + 1; }
    let ly = physics.y(low);
    let hy = physics.y(high);
    let dx = physics.x(high) - physics.x(low);
    let dy = hy - ly;
    let dz = physics.z(high) - physics.z(low);
    let d2 = dx * dx + dy * dy + dz * dz;
    if (ly < 0.75 || ly > 1.08) { io.println("fail stack low"); io.println(ly); return 1; }
    if (hy < ly + 0.7) { io.println("fail stack high"); io.println(hy); return 1; }
    if (d2 < 0.64) { io.println("fail stack"); io.println(d2); return 1; }
    io.println("ok");
    return 0;
}
EOF
if ! "$NEXAC" "$WORK/squish.nxa" -o "$WORK/squish$EXE" > "$WORK/squish.log" 2>&1; then
    bad "squish build" "$(tail -20 "$WORK/squish.log")"
else
    out=$("$WORK/squish$EXE" 2>&1 | tr -d '\r')
    if [ "$out" = "ok" ]; then
        ok "squishy mode compresses on impact and eases back"
    else
        bad "squish run" "got [$out]"
    fi
fi

if ! "$NEXAC" Examples/squish_demo.nxa --source "$WORK/squish_demo.cpp" > "$WORK/squish_demo.log" 2>&1; then
    bad "squish demo" "$(tail -20 "$WORK/squish_demo.log")"
elif ! grep -q __nexa_gfx3d_ellipsoid "$WORK/squish_demo.cpp"; then
    bad "squish demo" "the demo does not draw a flattened sphere"
else
    ok "squish demo transpiles"
fi

if [ "$FAIL" -eq 0 ]; then
    echo "physics_cases: all passed"
    exit 0
fi
echo "physics_cases: $FAIL failed"
exit 1
