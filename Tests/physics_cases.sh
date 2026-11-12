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
cat > "$WORK/wobble.nxa" <<'EOF'
// A soft body has to ring, not just flatten. The deformation used to follow the
// contact depth with a lag: it arrived at the depth the contact asked for and
// stopped, so a body only ever looked pressed. It is a spring now -- it carries
// past, comes back through round into a stretch, and rings down.
#include <std/io>
#include <std/physics>
fn main(): int {
    physics.squishy(0.9);
    let floor = physics.box(0, -0.5, 0, 24, 1, 24);
    physics.mass(floor, 0);
    let b = physics.sphere(0, 0.5, 0, 0.5);
    let i = 0;
    while (i < 60) { physics.step(1.0 / 60.0); i = i + 1; }
    // Thrown up off a resting squash, it must swing through round and draw out.
    physics.velocity(b, 0, 7, 0);
    let thinnest = 0.0;
    let fattest = 0.0;
    let crossings = 0;
    let was = physics.squash(b);
    i = 0;
    while (i < 60) {
        physics.step(1.0 / 60.0);
        let q = physics.squash(b);
        if (q < thinnest) { thinnest = q; }
        if (q > fattest) { fattest = q; }
        if ((was > 0.0 && q < 0.0) || (was < 0.0 && q > 0.0)) { crossings = crossings + 1; }
        was = q;
        i = i + 1;
    }
    // A stretch at all, which was impossible before: both the solver and the
    // shape clamped the deformation at zero.
    if (thinnest > -0.02) {
        io.println("fail never stretched");
        io.println(thinnest);
        return 1;
    }
    // And it has to come back, not sit stretched.
    if (fattest < 0.005) { io.println("fail never came back"); io.println(fattest); return 1; }
    // More than one swing through round is the ring.
    if (crossings < 2) { io.println("fail did not ring"); io.println(crossings); return 1; }
    // It must settle, not ring for ever.
    i = 0;
    while (i < 400) { physics.step(1.0 / 60.0); i = i + 1; }
    let settled = physics.squash(b);
    if (settled < 0.08 || settled > 0.25) {
        io.println("fail did not settle on the floor");
        io.println(settled);
        return 1;
    }
    io.println("ok");
    return 0;
}
EOF
if ! "$NEXAC" "$WORK/wobble.nxa" -o "$WORK/wobble$EXE" > "$WORK/wobble.log" 2>&1; then
    bad "wobble build" "$(tail -20 "$WORK/wobble.log")"
else
    out=$("$WORK/wobble$EXE" 2>&1 | tr -d '\r')
    if [ "$out" = "ok" ]; then
        ok "a soft body rings through round into a stretch, then settles"
    else
        bad "wobble" "$out"
    fi
fi

cat > "$WORK/nosink.nxa" <<'EOF'
// The ringing must not let the shape be drawn rounder than the contact is deep,
// or the body is simply buried in the floor -- measured 16cm into it on a
// half-metre ball while the spring caught up.
#include <std/io>
#include <std/physics>
fn main(): int {
    physics.squishy(0.9);
    let floor = physics.box(0, -0.5, 0, 24, 1, 24);
    physics.mass(floor, 0);
    let b = physics.sphere(0, 3, 0, 0.5);
    let worst = 0.0;
    let i = 0;
    while (i < 300) {
        physics.step(1.0 / 60.0);
        // where the drawn underside sits: centre less the flattened half-height
        let under = physics.y(b) - 0.5 * (1.0 - physics.squash(b));
        if (under < worst) { worst = under; }
        i = i + 1;
    }
    if (worst < -0.02) {
        io.println("fail drawn shape sank into the floor");
        io.println(worst);
        return 1;
    }
    io.println("ok");
    return 0;
}
EOF
if ! "$NEXAC" "$WORK/nosink.nxa" -o "$WORK/nosink$EXE" > "$WORK/nosink.log" 2>&1; then
    bad "no sink build" "$(tail -20 "$WORK/nosink.log")"
else
    out=$("$WORK/nosink$EXE" 2>&1 | tr -d '\r')
    if [ "$out" = "ok" ]; then
        ok "and the shape still sits on its contact rather than sinking in"
    else
        bad "no sink" "$out"
    fi
fi

cat > "$WORK/softbody.nxa" <<'EOF'
// physics.soft sets one body apart from the rest. Before it there was one knob
// for the whole world, so every body was as soft as every other: a firm ball
// asked for against the floor still squashed like jelly, and physics.bounce did
// nothing at all on a soft world, since softness scales restitution away.
#include <std/io>
#include <std/physics>
fn main(): int {
    physics.squishy(0.9);
    let floor = physics.box(0, -0.5, 0, 24, 1, 24);
    physics.mass(floor, 0);
    // Same drop, three characters.
    let jelly = physics.sphere(-3, 3, 0, 0.5);
    let firm = physics.sphere(0, 3, 0, 0.5);
    physics.soft(firm, 0.08);
    let rubber = physics.sphere(3, 3, 0, 0.5);
    physics.soft(rubber, 0.08);
    physics.bounce(rubber, 0.75);
    let hr = 0.0;
    let touched = 0;
    let i = 0;
    while (i < 300) {
        physics.step(1.0 / 60.0);
        if (physics.y(rubber) < 0.55) { touched = 1; }
        if (touched == 1 && physics.y(rubber) > hr) { hr = physics.y(rubber); }
        i = i + 1;
    }
    let qj = physics.squash(jelly);
    let qf = physics.squash(firm);
    // The firm one has to land firmly, not like the jelly beside it.
    if (qf > qj * 0.5) {
        io.println("fail firm squashed like jelly");
        io.println(qj);
        io.println(qf);
        return 1;
    }
    if (qj < 0.08) { io.println("fail jelly too firm"); io.println(qj); return 1; }
    // And a firm body with bounce set has to leave the floor.
    if (hr < 1.0) {
        io.println("fail bounce on a firm body");
        io.println(hr);
        return 1;
    }
    // A firm ball dropped on a soft one dents the soft one, not itself.
    let cushion = physics.sphere(8, 0.5, 0, 0.5);
    let press = physics.sphere(8, 3, 0, 0.5);
    physics.soft(press, 0.05);
    i = 0;
    while (i < 300) { physics.step(1.0 / 60.0); i = i + 1; }
    let qc = physics.squash(cushion);
    let qp = physics.squash(press);
    if (qc < 0.2) { io.println("fail cushion not dented"); io.println(qc); return 1; }
    if (qp > 0.05) { io.println("fail presser deformed"); io.println(qp); return 1; }
    io.println("ok");
    return 0;
}
EOF
if ! "$NEXAC" "$WORK/softbody.nxa" -o "$WORK/softbody$EXE" > "$WORK/softbody.log" 2>&1; then
    bad "soft body build" "$(tail -20 "$WORK/softbody.log")"
else
    out=$("$WORK/softbody$EXE" 2>&1 | tr -d '\r')
    if [ "$out" = "ok" ]; then
        ok "physics.soft sets one body's softness apart from the world's"
    else
        bad "soft body" "$out"
    fi
fi

cat > "$WORK/softonly.nxa" <<'EOF'
// physics.soft on its own, with the world left rigid: the soft paths still have
// to be emitted and to run. They used to be gated on physics.squishy alone.
#include <std/io>
#include <std/physics>
fn main(): int {
    let floor = physics.box(0, -0.5, 0, 24, 1, 24);
    physics.mass(floor, 0);
    let b = physics.sphere(0, 3, 0, 0.5);
    physics.soft(b, 0.9);
    let i = 0;
    while (i < 300) { physics.step(1.0 / 60.0); i = i + 1; }
    let q = physics.squash(b);
    if (q < 0.08 || q > 0.25) { io.println("fail soft alone"); io.println(q); return 1; }
    io.println("ok");
    return 0;
}
EOF
if ! "$NEXAC" "$WORK/softonly.nxa" -o "$WORK/softonly$EXE" > "$WORK/softonly.log" 2>&1; then
    bad "soft only build" "$(tail -20 "$WORK/softonly.log")"
else
    out=$("$WORK/softonly$EXE" 2>&1 | tr -d '\r')
    if [ "$out" = "ok" ]; then
        ok "physics.soft works with no physics.squishy call at all"
    else
        bad "soft only" "$out"
    fi
fi

cat > "$WORK/squishhead.nxa" <<'EOF'
// Two soft bodies with no bounce, thrown at each other, have to absorb the
// collision rather than trade it back. They did not: the recovery was driven by
// asking the solver for a separation speed, that ask was made while the overlap
// was deepest, and the pair kept the speed long after the contact was over --
// 1.7 m/s apart, forever, from a head-on hit at 4. Getting the overlap out by
// moving them instead cannot hand energy back.
#include <std/io>
#include <std/physics>
fn main(): int {
    physics.squishy(0.9);
    physics.gravity(0, 0, 0);
    let a = physics.sphere(-2, 0, 0, 0.5);
    let b = physics.sphere(2, 0, 0, 0.5);
    physics.velocity(a, 4, 0, 0);
    physics.velocity(b, -4, 0, 0);
    let touched = 0;
    let i = 0;
    while (i < 150) {
        physics.step(1.0 / 60.0);
        if (physics.squash(a) > 0.02) { touched = 1; }
        i = i + 1;
    }
    if (touched == 0) { io.println("fail no contact"); return 1; }
    let sep = physics.vx(b) - physics.vx(a);
    if (sep < 0.0) { sep = 0.0 - sep; }
    if (sep > 0.25) {
        io.println("fail soft pair bounced apart");
        io.println(sep);
        return 1;
    }
    // And they must not be left inside one another either.
    let gap = physics.x(b) - physics.x(a);
    if (gap < 0.0) { gap = 0.0 - gap; }
    if (gap < 0.6) {
        io.println("fail left overlapping");
        io.println(gap);
        return 1;
    }
    io.println("ok");
    return 0;
}
EOF
if ! "$NEXAC" "$WORK/squishhead.nxa" -o "$WORK/squishhead$EXE" > "$WORK/squishhead.log" 2>&1; then
    bad "squish headon build" "$(tail -20 "$WORK/squishhead.log")"
else
    out=$("$WORK/squishhead$EXE" 2>&1 | tr -d '\r')
    if [ "$out" = "ok" ]; then
        ok "two soft bodies absorb a head-on hit instead of trading it back"
    else
        bad "squish headon" "$out"
    fi
fi

cat > "$WORK/squishload.nxa" <<'EOF'
// A soft body should flatten more when something rests on it. It did not: a
// ball carrying another squashed 0.148 where it squashed 0.146 on its own, and
// a ball resting on a ball squashed half what one resting on the floor did.
// The resting depth follows the load now, so the bottom of a stack is squatter
// than the top.
#include <std/io>
#include <std/physics>
fn main(): int {
    physics.squishy(0.9);
    let floor = physics.box(0, -0.5, 0, 24, 1, 24);
    physics.mass(floor, 0);
    let low = physics.sphere(0, 0.5, 0, 0.5);
    let i = 0;
    while (i < 180) { physics.step(1.0 / 60.0); i = i + 1; }
    let alone = physics.squash(low);
    if (alone < 0.08 || alone > 0.25) { io.println("fail alone"); io.println(alone); return 1; }
    let high = physics.sphere(0, 3, 0, 0.5);
    i = 0;
    while (i < 300) { physics.step(1.0 / 60.0); i = i + 1; }
    let loaded = physics.squash(low);
    let upper = physics.squash(high);
    // Carrying another of its own mass has to show, not by a rounding error.
    if (loaded < alone * 1.6) {
        io.println("fail load squash");
        io.println(alone);
        io.println(loaded);
        return 1;
    }
    // And the one on top is the rounder of the two, since less rests on it.
    if (upper >= loaded) {
        io.println("fail upper rounder");
        io.println(upper);
        io.println(loaded);
        return 1;
    }
    // The flattened ball still sits on the floor rather than hovering or
    // sinking: half its radius, less what is pressed in.
    let under = physics.y(low) - 0.5 * (1.0 - loaded);
    if (under < -0.03 || under > 0.03) {
        io.println("fail sits on floor");
        io.println(under);
        return 1;
    }
    io.println("ok");
    return 0;
}
EOF
if ! "$NEXAC" "$WORK/squishload.nxa" -o "$WORK/squishload$EXE" > "$WORK/squishload.log" 2>&1; then
    bad "squish load build" "$(tail -20 "$WORK/squishload.log")"
else
    out=$("$WORK/squishload$EXE" 2>&1 | tr -d '\r')
    if [ "$out" = "ok" ]; then
        ok "a soft body flattens more when another rests on it"
    else
        bad "squish load" "$out"
    fi
fi

cat > "$WORK/squishfps.nxa" <<'EOF'
// The same drop, stepped at 30 frames and at 240, has to come to rest in the
// same place. It did not: the squash it settled at ranged over 47% across that
// span and the ball sat 6.6cm lower at 30 than at 240, because the tracking
// moved by a fixed share of the gap per step and a resting body was held up by
// a separation speed rather than by where it was.
#include <std/io>
#include <std/physics>
fn rest_squash(dt: float, steps: int): float {
    physics.squishy(0.9);
    let floor = physics.box(0, -0.5, 0, 24, 1, 24);
    physics.mass(floor, 0);
    let ball = physics.sphere(0, 3, 0, 0.5);
    let i = 0;
    while (i < steps) { physics.step(dt); i = i + 1; }
    let q = physics.squash(ball);
    physics.remove(ball);
    physics.remove(floor);
    return q;
}
fn rest_y(dt: float, steps: int): float {
    physics.squishy(0.9);
    let floor = physics.box(0, -0.5, 0, 24, 1, 24);
    physics.mass(floor, 0);
    let ball = physics.sphere(0, 3, 0, 0.5);
    let i = 0;
    while (i < steps) { physics.step(dt); i = i + 1; }
    let y = physics.y(ball);
    physics.remove(ball);
    physics.remove(floor);
    return y;
}
fn main(): int {
    let coarse = rest_squash(1.0 / 30.0, 90);
    let fine = rest_squash(1.0 / 240.0, 720);
    let spread = coarse - fine;
    if (spread < 0.0) { spread = 0.0 - spread; }
    if (spread > 0.02) {
        io.println("fail squash spread");
        io.println(coarse);
        io.println(fine);
        return 1;
    }
    let yc = rest_y(1.0 / 30.0, 90);
    let yf = rest_y(1.0 / 240.0, 720);
    let dy = yc - yf;
    if (dy < 0.0) { dy = 0.0 - dy; }
    if (dy > 0.01) {
        io.println("fail rest height spread");
        io.println(yc);
        io.println(yf);
        return 1;
    }
    io.println("ok");
    return 0;
}
EOF
if ! "$NEXAC" "$WORK/squishfps.nxa" -o "$WORK/squishfps$EXE" > "$WORK/squishfps.log" 2>&1; then
    bad "squish fps build" "$(tail -20 "$WORK/squishfps.log")"
else
    out=$("$WORK/squishfps$EXE" 2>&1 | tr -d '\r')
    if [ "$out" = "ok" ]; then
        ok "a soft body rests in the same place at 30 frames and at 240"
    else
        bad "squish fps" "$out"
    fi
fi

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
