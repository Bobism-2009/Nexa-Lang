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

if [ "$FAIL" -eq 0 ]; then
    echo "physics_cases: all passed"
    exit 0
fi
echo "physics_cases: $FAIL failed"
exit 1
