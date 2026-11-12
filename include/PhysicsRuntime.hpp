#pragma once

// std/physics. One world, fixed storage, no heap on the step.
//
// Queries (overlap and ray) are pure. Bodies are spheres, axis-aligned boxes
// and capsules that translate and do not rotate. A capsule is the same shape
// gfx3d.capsule draws: a radius around the segment between two points. A mesh
// collider is the triangles of a gfx3d.model, at the same centre and scale as
// gfx3d.draw, and that code is emitted later, only if the program asks for it.
// Up to 256 bodies. A program that never calls physics carries none of this.
//
// The step is sequential impulses on a fixed array. A hit changes velocity in
// proportion to inverse mass, so two spheres of the same weight trade speed
// instead of one shoving the other aside. A sphere already touching another
// is not moved deeper into it; whatever overlap remains is taken back out,
// split by mass, after the impulse. Restitution applies only when the hit is
// faster than 1 m/s. Two spheres barely grip, so one slides off the other; a
// box keeps the grip that stops a body on the floor. A fast sphere is swept
// against the floor and against other spheres, and a step is split into at
// most four.

#include <string>

namespace nexa {

inline std::string physicsRuntimeCpp(bool query, bool body, bool mesh = false, bool squish = false) {
    if (!query && !body) return "";
    std::string out;
    out += "#include <cmath>\n";
    out += "static float __nexa_phys_hx, __nexa_phys_hy, __nexa_phys_hz;\n";
    out += "static float __nexa_phys_nx, __nexa_phys_ny, __nexa_phys_nz;\n";
    out += R"PHYS(
static float __nexa_phys_clampf(float v, float a, float b) {
    return v < a ? a : (v > b ? b : v);
}
static float __nexa_phys_len(float x, float y, float z) {
    return sqrtf(x * x + y * y + z * z);
}
static void __nexa_phys_hit_clear() {
    __nexa_phys_hx = __nexa_phys_hy = __nexa_phys_hz = 0.f;
    __nexa_phys_nx = __nexa_phys_ny = __nexa_phys_nz = 0.f;
}
static void __nexa_phys_hit_set(float hx, float hy, float hz, float nx, float ny, float nz) {
    float L = __nexa_phys_len(nx, ny, nz);
    __nexa_phys_hx = hx; __nexa_phys_hy = hy; __nexa_phys_hz = hz;
    if (L > 1e-8f) { __nexa_phys_nx = nx / L; __nexa_phys_ny = ny / L; __nexa_phys_nz = nz / L; }
    else { __nexa_phys_nx = 0.f; __nexa_phys_ny = 1.f; __nexa_phys_nz = 0.f; }
}
static void __nexa_phys_aabb_closest(float px, float py, float pz,
    float cx, float cy, float cz, float hx, float hy, float hz,
    float* ox, float* oy, float* oz) {
    *ox = __nexa_phys_clampf(px, cx - hx, cx + hx);
    *oy = __nexa_phys_clampf(py, cy - hy, cy + hy);
    *oz = __nexa_phys_clampf(pz, cz - hz, cz + hz);
}
static int __nexa_phys_sphere_sphere(float ax, float ay, float az, float ar,
    float bx, float by, float bz, float br,
    float* nx, float* ny, float* nz, float* pen) {
    float dx = ax - bx, dy = ay - by, dz = az - bz;
    float d2 = dx * dx + dy * dy + dz * dz;
    float rad = ar + br;
    if (d2 > rad * rad) return 0;
    float dist = sqrtf(d2);
    *pen = rad - dist;
    if (dist > 1e-8f) { *nx = dx / dist; *ny = dy / dist; *nz = dz / dist; }
    else { *nx = 0.f; *ny = 1.f; *nz = 0.f; *pen = rad; }
    return 1;
}
static int __nexa_phys_sphere_box(float sx, float sy, float sz, float sr,
    float cx, float cy, float cz, float w, float h, float d,
    float* nx, float* ny, float* nz, float* pen) {
    float hx = w < 0.f ? -w * 0.5f : w * 0.5f;
    float hy = h < 0.f ? -h * 0.5f : h * 0.5f;
    float hz = d < 0.f ? -d * 0.5f : d * 0.5f;
    if (sr < 0.f) sr = -sr;
    float qx, qy, qz;
    __nexa_phys_aabb_closest(sx, sy, sz, cx, cy, cz, hx, hy, hz, &qx, &qy, &qz);
    float dx = sx - qx, dy = sy - qy, dz = sz - qz;
    float d2 = dx * dx + dy * dy + dz * dz;
    if (d2 > sr * sr) return 0;
    if (d2 > 1e-12f) {
        float dist = sqrtf(d2);
        *pen = sr - dist;
        *nx = dx / dist; *ny = dy / dist; *nz = dz / dist;
        return 1;
    }
    float px = hx - fabsf(sx - cx);
    float py = hy - fabsf(sy - cy);
    float pz = hz - fabsf(sz - cz);
    if (py <= px && py <= pz) {
        *pen = sr + py; *nx = 0.f; *ny = (sy >= cy) ? 1.f : -1.f; *nz = 0.f;
    } else if (pz <= px) {
        *pen = sr + pz; *nx = 0.f; *ny = 0.f; *nz = (sz >= cz) ? 1.f : -1.f;
    } else {
        *pen = sr + px; *nx = (sx >= cx) ? 1.f : -1.f; *ny = 0.f; *nz = 0.f;
    }
    return 1;
}
static int __nexa_phys_box_box(float ax, float ay, float az, float aw, float ah, float ad,
    float bx, float by, float bz, float bw, float bh, float bd,
    float* nx, float* ny, float* nz, float* pen) {
    float hx = fabsf(aw) * 0.5f + fabsf(bw) * 0.5f;
    float hy = fabsf(ah) * 0.5f + fabsf(bh) * 0.5f;
    float hz = fabsf(ad) * 0.5f + fabsf(bd) * 0.5f;
    float dx = ax - bx, dy = ay - by, dz = az - bz;
    float ox = hx - fabsf(dx), oy = hy - fabsf(dy), oz = hz - fabsf(dz);
    if (ox <= 0.f || oy <= 0.f || oz <= 0.f) return 0;
    if (ox <= oy && ox <= oz) { *pen = ox; *nx = dx >= 0.f ? 1.f : -1.f; *ny = 0.f; *nz = 0.f; }
    else if (oy <= oz) { *pen = oy; *nx = 0.f; *ny = dy >= 0.f ? 1.f : -1.f; *nz = 0.f; }
    else { *pen = oz; *nx = 0.f; *ny = 0.f; *nz = dz >= 0.f ? 1.f : -1.f; }
    return 1;
}
static void __nexa_phys_seg_aabb(float ax, float ay, float az, float bx, float by, float bz,
    float cx, float cy, float cz, float hx, float hy, float hz,
    float* spx, float* spy, float* spz, float* qpx, float* qpy, float* qpz) {
    float lo = 0.f, hi = 1.f;
    for (int i = 0; i < 24; i++) {
        float t1 = lo + (hi - lo) / 3.f;
        float t2 = hi - (hi - lo) / 3.f;
        float p1x = ax + (bx - ax) * t1, p1y = ay + (by - ay) * t1, p1z = az + (bz - az) * t1;
        float p2x = ax + (bx - ax) * t2, p2y = ay + (by - ay) * t2, p2z = az + (bz - az) * t2;
        float c1x, c1y, c1z, c2x, c2y, c2z;
        __nexa_phys_aabb_closest(p1x, p1y, p1z, cx, cy, cz, hx, hy, hz, &c1x, &c1y, &c1z);
        __nexa_phys_aabb_closest(p2x, p2y, p2z, cx, cy, cz, hx, hy, hz, &c2x, &c2y, &c2z);
        float d1x = p1x - c1x, d1y = p1y - c1y, d1z = p1z - c1z;
        float d2x = p2x - c2x, d2y = p2y - c2y, d2z = p2z - c2z;
        float d1 = d1x * d1x + d1y * d1y + d1z * d1z;
        float d2 = d2x * d2x + d2y * d2y + d2z * d2z;
        if (d1 < d2) hi = t2; else lo = t1;
    }
    float t = (lo + hi) * 0.5f;
    *spx = ax + (bx - ax) * t; *spy = ay + (by - ay) * t; *spz = az + (bz - az) * t;
    __nexa_phys_aabb_closest(*spx, *spy, *spz, cx, cy, cz, hx, hy, hz, qpx, qpy, qpz);
}
static void __nexa_phys_seg_seg(float ax, float ay, float az, float bx, float by, float bz,
    float cx, float cy, float cz, float dx, float dy, float dz,
    float* px, float* py, float* pz, float* qx, float* qy, float* qz) {
    float d1x = bx - ax, d1y = by - ay, d1z = bz - az;
    float d2x = dx - cx, d2y = dy - cy, d2z = dz - cz;
    float rx = ax - cx, ry = ay - cy, rz = az - cz;
    float a = d1x * d1x + d1y * d1y + d1z * d1z;
    float e = d2x * d2x + d2y * d2y + d2z * d2z;
    float f = d2x * rx + d2y * ry + d2z * rz;
    float s = 0.f, t = 0.f;
    const float eps = 1e-8f;
    if (a <= eps && e <= eps) {
        s = t = 0.f;
    } else if (a <= eps) {
        s = 0.f;
        t = (e > eps) ? f / e : 0.f;
        t = __nexa_phys_clampf(t, 0.f, 1.f);
    } else {
        float c = d1x * rx + d1y * ry + d1z * rz;
        if (e <= eps) {
            t = 0.f;
            s = __nexa_phys_clampf(-c / a, 0.f, 1.f);
        } else {
            float b = d1x * d2x + d1y * d2y + d1z * d2z;
            float denom = a * e - b * b;
            s = (denom != 0.f) ? (b * f - c * e) / denom : 0.f;
            s = __nexa_phys_clampf(s, 0.f, 1.f);
            t = (b * s + f) / e;
            if (t < 0.f) {
                t = 0.f;
                s = __nexa_phys_clampf(-c / a, 0.f, 1.f);
            } else if (t > 1.f) {
                t = 1.f;
                s = __nexa_phys_clampf((b - c) / a, 0.f, 1.f);
            }
        }
    }
    *px = ax + d1x * s; *py = ay + d1y * s; *pz = az + d1z * s;
    *qx = cx + d2x * t; *qy = cy + d2y * t; *qz = cz + d2z * t;
}
static int __nexa_phys_sphere_capsule(float sx, float sy, float sz, float sr,
    float x1, float y1, float z1, float x2, float y2, float z2, float cr,
    float* nx, float* ny, float* nz, float* pen) {
    float abx = x2 - x1, aby = y2 - y1, abz = z2 - z1;
    float ab2 = abx * abx + aby * aby + abz * abz;
    float t = 0.f;
    if (ab2 > 1e-12f) {
        t = ((sx - x1) * abx + (sy - y1) * aby + (sz - z1) * abz) / ab2;
        t = __nexa_phys_clampf(t, 0.f, 1.f);
    }
    float qx = x1 + abx * t, qy = y1 + aby * t, qz = z1 + abz * t;
    return __nexa_phys_sphere_sphere(sx, sy, sz, sr, qx, qy, qz, cr, nx, ny, nz, pen);
}
static double __nexa_phys_ray_sphere_t(float ox, float oy, float oz, float dx, float dy, float dz,
    float cx, float cy, float cz, float r, int write) {
    if (r < 0.f) r = -r;
    float ocx = ox - cx, ocy = oy - cy, ocz = oz - cz;
    float a = dx * dx + dy * dy + dz * dz;
    float b = 2.f * (ocx * dx + ocy * dy + ocz * dz);
    float c = ocx * ocx + ocy * ocy + ocz * ocz - r * r;
    float disc = b * b - 4.f * a * c;
    if (a < 1e-12f || disc < 0.f) {
        if (write) __nexa_phys_hit_clear();
        return -1.0;
    }
    float s = sqrtf(disc);
    float t0 = (-b - s) / (2.f * a);
    float t1 = (-b + s) / (2.f * a);
    float t = t0 >= 0.f ? t0 : t1;
    if (t < 0.f) {
        if (write) __nexa_phys_hit_clear();
        return -1.0;
    }
    if (write) {
        float hx = ox + dx * t, hy = oy + dy * t, hz = oz + dz * t;
        __nexa_phys_hit_set(hx, hy, hz, hx - cx, hy - cy, hz - cz);
    }
    return (double)t;
}
)PHYS";

    if (query) {
        out += R"PHYS(
static int __nexa_phys_overlap_spheres(double x1, double y1, double z1, double r1,
    double x2, double y2, double z2, double r2) {
    float nx, ny, nz, pen;
    return __nexa_phys_sphere_sphere((float)x1, (float)y1, (float)z1, (float)r1,
        (float)x2, (float)y2, (float)z2, (float)r2, &nx, &ny, &nz, &pen);
}
static int __nexa_phys_overlap_boxes(double ax, double ay, double az, double aw, double ah, double ad,
    double bx, double by, double bz, double bw, double bh, double bd) {
    float nx, ny, nz, pen;
    return __nexa_phys_box_box((float)ax, (float)ay, (float)az, (float)aw, (float)ah, (float)ad,
        (float)bx, (float)by, (float)bz, (float)bw, (float)bh, (float)bd, &nx, &ny, &nz, &pen);
}
static int __nexa_phys_overlap_sphere_box(double sx, double sy, double sz, double sr,
    double bx, double by, double bz, double bw, double bh, double bd) {
    float nx, ny, nz, pen;
    return __nexa_phys_sphere_box((float)sx, (float)sy, (float)sz, (float)sr,
        (float)bx, (float)by, (float)bz, (float)bw, (float)bh, (float)bd, &nx, &ny, &nz, &pen);
}
static int __nexa_phys_overlap_sphere_capsule(double sx, double sy, double sz, double sr,
    double x1, double y1, double z1, double x2, double y2, double z2, double cr) {
    float nx, ny, nz, pen;
    return __nexa_phys_sphere_capsule((float)sx, (float)sy, (float)sz, (float)sr,
        (float)x1, (float)y1, (float)z1, (float)x2, (float)y2, (float)z2, (float)cr, &nx, &ny, &nz, &pen);
}
static double __nexa_phys_ray_sphere(double ox, double oy, double oz, double dx, double dy, double dz,
    double cx, double cy, double cz, double r) {
    return __nexa_phys_ray_sphere_t((float)ox, (float)oy, (float)oz, (float)dx, (float)dy, (float)dz,
        (float)cx, (float)cy, (float)cz, (float)r, 1);
}
static double __nexa_phys_ray_box(double ox, double oy, double oz, double dx, double dy, double dz,
    double cx, double cy, double cz, double w, double h, double d) {
    float fx = (float)ox, fy = (float)oy, fz = (float)oz;
    float fdx = (float)dx, fdy = (float)dy, fdz = (float)dz;
    float hx = fabsf((float)w) * 0.5f, hy = fabsf((float)h) * 0.5f, hz = fabsf((float)d) * 0.5f;
    float minx = (float)cx - hx, maxx = (float)cx + hx;
    float miny = (float)cy - hy, maxy = (float)cy + hy;
    float minz = (float)cz - hz, maxz = (float)cz + hz;
    float tmin = 0.f, tmax = 1e30f;
    int axis = -1, sign = 1;
    float o[3] = {fx, fy, fz}, dir[3] = {fdx, fdy, fdz};
    float mn[3] = {minx, miny, minz}, mx[3] = {maxx, maxy, maxz};
    for (int a = 0; a < 3; a++) {
        if (fabsf(dir[a]) < 1e-8f) {
            if (o[a] < mn[a] || o[a] > mx[a]) { __nexa_phys_hit_clear(); return -1.0; }
            continue;
        }
        float inv = 1.f / dir[a];
        float t1 = (mn[a] - o[a]) * inv;
        float t2 = (mx[a] - o[a]) * inv;
        int sn = -1;
        if (t1 > t2) { float s = t1; t1 = t2; t2 = s; sn = 1; }
        if (t1 > tmin) { tmin = t1; axis = a; sign = sn; }
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) { __nexa_phys_hit_clear(); return -1.0; }
    }
    float t = tmin >= 0.f ? tmin : (tmax >= 0.f ? 0.f : -1.f);
    if (t < 0.f || tmax < 0.f) { __nexa_phys_hit_clear(); return -1.0; }
    float hx0 = fx + fdx * t, hy0 = fy + fdy * t, hz0 = fz + fdz * t;
    float nx = 0.f, ny = 0.f, nz = 0.f;
    if (axis == 0) nx = (float)sign;
    else if (axis == 1) ny = (float)sign;
    else if (axis == 2) nz = (float)sign;
    else ny = 1.f;
    __nexa_phys_hit_set(hx0, hy0, hz0, nx, ny, nz);
    return (double)t;
}
static double __nexa_phys_ray_capsule(double ox, double oy, double oz, double dx, double dy, double dz,
    double x1, double y1, double z1, double x2, double y2, double z2, double radius) {
    float r = (float)radius; if (r < 0.f) r = -r;
    float best = -1.f;
    float bhx = 0, bhy = 0, bhz = 0, bnx = 0, bny = 1, bnz = 0;
    float ends[2][3] = {{(float)x1, (float)y1, (float)z1}, {(float)x2, (float)y2, (float)z2}};
    for (int e = 0; e < 2; e++) {
        double t = __nexa_phys_ray_sphere_t((float)ox, (float)oy, (float)oz, (float)dx, (float)dy, (float)dz,
            ends[e][0], ends[e][1], ends[e][2], r, 0);
        if (t >= 0.0 && (best < 0.f || (float)t < best)) {
            best = (float)t;
            bhx = (float)ox + (float)dx * best;
            bhy = (float)oy + (float)dy * best;
            bhz = (float)oz + (float)dz * best;
            bnx = bhx - ends[e][0]; bny = bhy - ends[e][1]; bnz = bhz - ends[e][2];
        }
    }
    float ax = (float)x1, ay = (float)y1, az = (float)z1;
    float abx = (float)x2 - ax, aby = (float)y2 - ay, abz = (float)z2 - az;
    float ab2 = abx * abx + aby * aby + abz * abz;
    float fdx = (float)dx, fdy = (float)dy, fdz = (float)dz;
    if (ab2 > 1e-12f) {
        float inv = 1.f / sqrtf(ab2);
        float nx = abx * inv, ny = aby * inv, nz = abz * inv;
        float ocx = (float)ox - ax, ocy = (float)oy - ay, ocz = (float)oz - az;
        float ddn = fdx * nx + fdy * ny + fdz * nz;
        float ocn = ocx * nx + ocy * ny + ocz * nz;
        float pdx = fdx - ddn * nx, pdy = fdy - ddn * ny, pdz = fdz - ddn * nz;
        float pcx = ocx - ocn * nx, pcy = ocy - ocn * ny, pcz = ocz - ocn * nz;
        float A = pdx * pdx + pdy * pdy + pdz * pdz;
        float B = 2.f * (pcx * pdx + pcy * pdy + pcz * pdz);
        float C = pcx * pcx + pcy * pcy + pcz * pcz - r * r;
        float disc = B * B - 4.f * A * C;
        if (A > 1e-12f && disc >= 0.f) {
            float s = sqrtf(disc);
            float ts[2] = {(-B - s) / (2.f * A), (-B + s) / (2.f * A)};
            float L = sqrtf(ab2);
            for (int k = 0; k < 2; k++) {
                float t = ts[k];
                if (t < 0.f) continue;
                float along = ocn + t * ddn;
                if (along < 0.f || along > L) continue;
                if (best >= 0.f && t >= best) continue;
                best = t;
                bhx = (float)ox + fdx * t; bhy = (float)oy + fdy * t; bhz = (float)oz + fdz * t;
                float qx = ax + nx * along, qy = ay + ny * along, qz = az + nz * along;
                bnx = bhx - qx; bny = bhy - qy; bnz = bhz - qz;
            }
        }
    }
    if (best < 0.f) { __nexa_phys_hit_clear(); return -1.0; }
    __nexa_phys_hit_set(bhx, bhy, bhz, bnx, bny, bnz);
    return (double)best;
}
static double __nexa_phys_hitx() { return (double)__nexa_phys_hx; }
static double __nexa_phys_hity() { return (double)__nexa_phys_hy; }
static double __nexa_phys_hitz() { return (double)__nexa_phys_hz; }
static double __nexa_phys_hitnx() { return (double)__nexa_phys_nx; }
static double __nexa_phys_hitny() { return (double)__nexa_phys_ny; }
static double __nexa_phys_hitnz() { return (double)__nexa_phys_nz; }
)PHYS";
    }

    if (body) {
        out += R"PHYS(
struct __nexa_phys_body {
    int kind;
    float inv;
    float bounce;
    float x, y, z;
    float vx, vy, vz;
    float r, w, h, d;
    float ax, ay, az, bx, by, bz;
)PHYS";
        if (mesh) {
            out += "    int model;\n    float scl;\n";
        }
        if (squish) {
            out += "    float sq, snx, sny, snz, hit;\n";
        }
        out += R"PHYS(};
static __nexa_phys_body __nexa_phys_bodies[257];
static int __nexa_phys_hi = 0;
static float __nexa_phys_gx = 0.f, __nexa_phys_gy = -9.81f, __nexa_phys_gz = 0.f;
)PHYS";
        if (squish) {
            out += R"PHYS(static float __nexa_phys_squish = 0.f;
static float __nexa_phys_body_rad(const __nexa_phys_body& b) {
    float rad = b.r;
    if (b.kind == 2) {
        rad = fabsf(b.w);
        if (fabsf(b.h) < rad) rad = fabsf(b.h);
        if (fabsf(b.d) < rad) rad = fabsf(b.d);
        rad *= 0.5f;
    }
    if (rad < 0.f) rad = -rad;
    return rad;
}
static float __nexa_phys_soft_rad(const __nexa_phys_body& a, const __nexa_phys_body& b) {
    float rad = __nexa_phys_body_rad(a);
    if (b.inv > 0.f) {
        float rb = __nexa_phys_body_rad(b);
        if (a.inv <= 0.f || (rb > 0.05f && rb < rad)) rad = rb;
    }
    if (rad < 0.05f) rad = 0.05f;
    return rad;
}
)PHYS";
        }
        out += R"PHYS(static int __nexa_phys_alive(int id) {
    return id >= 1 && id <= 256 && __nexa_phys_bodies[id].kind != 0;
}
static void __nexa_phys_ends(const __nexa_phys_body& b, float* x1, float* y1, float* z1, float* x2, float* y2, float* z2) {
    *x1 = b.x + b.ax; *y1 = b.y + b.ay; *z1 = b.z + b.az;
    *x2 = b.x + b.bx; *y2 = b.y + b.by; *z2 = b.z + b.bz;
}
static int __nexa_phys_alloc(int kind) {
    for (int i = 1; i <= 256; i++) {
        if (__nexa_phys_bodies[i].kind == 0) {
            __nexa_phys_body& b = __nexa_phys_bodies[i];
            b = __nexa_phys_body{};
            b.kind = kind;
            b.inv = 1.f;
            if (i > __nexa_phys_hi) __nexa_phys_hi = i;
            return i;
        }
    }
    return 0;
}
static int __nexa_phys_sphere(double x, double y, double z, double r) {
    int id = __nexa_phys_alloc(1);
    if (!id) return 0;
    __nexa_phys_body& b = __nexa_phys_bodies[id];
    b.x = (float)x; b.y = (float)y; b.z = (float)z; b.r = (float)r; if (b.r < 0.f) b.r = -b.r;
    return id;
}
static int __nexa_phys_box(double x, double y, double z, double w, double h, double d) {
    int id = __nexa_phys_alloc(2);
    if (!id) return 0;
    __nexa_phys_body& b = __nexa_phys_bodies[id];
    b.x = (float)x; b.y = (float)y; b.z = (float)z;
    b.w = (float)w; b.h = (float)h; b.d = (float)d;
    return id;
}
static int __nexa_phys_capsule(double x1, double y1, double z1, double x2, double y2, double z2, double r) {
    int id = __nexa_phys_alloc(3);
    if (!id) return 0;
    __nexa_phys_body& b = __nexa_phys_bodies[id];
    b.x = (float)((x1 + x2) * 0.5); b.y = (float)((y1 + y2) * 0.5); b.z = (float)((z1 + z2) * 0.5);
    b.ax = (float)x1 - b.x; b.ay = (float)y1 - b.y; b.az = (float)z1 - b.z;
    b.bx = (float)x2 - b.x; b.by = (float)y2 - b.y; b.bz = (float)z2 - b.z;
    b.r = (float)r; if (b.r < 0.f) b.r = -b.r;
    return id;
}
static int __nexa_phys_mass(int id, double m) {
    if (!__nexa_phys_alive(id)) return 0;
    __nexa_phys_body& b = __nexa_phys_bodies[id];
    if (m <= 0.0) { b.inv = 0.f; b.vx = b.vy = b.vz = 0.f; }
    else b.inv = (float)(1.0 / m);
    return 1;
}
static int __nexa_phys_bounce(int id, double e) {
    if (!__nexa_phys_alive(id)) return 0;
    float f = (float)e;
    if (f < 0.f) f = 0.f;
    if (f > 1.f) f = 1.f;
    __nexa_phys_bodies[id].bounce = f;
    return 1;
}
static int __nexa_phys_velocity(int id, double vx, double vy, double vz) {
    if (!__nexa_phys_alive(id)) return 0;
    __nexa_phys_body& b = __nexa_phys_bodies[id];
    if (b.inv == 0.f) return 0;
    b.vx = (float)vx; b.vy = (float)vy; b.vz = (float)vz;
    return 1;
}
static int __nexa_phys_set(int id, double x, double y, double z) {
    if (!__nexa_phys_alive(id)) return 0;
    __nexa_phys_bodies[id].x = (float)x;
    __nexa_phys_bodies[id].y = (float)y;
    __nexa_phys_bodies[id].z = (float)z;
    return 1;
}
static int __nexa_phys_remove(int id) {
    if (!__nexa_phys_alive(id)) return 0;
    __nexa_phys_bodies[id].kind = 0;
    if (id == __nexa_phys_hi) {
        while (__nexa_phys_hi > 0 && __nexa_phys_bodies[__nexa_phys_hi].kind == 0) __nexa_phys_hi--;
    }
    return 1;
}
static int __nexa_phys_gravity(double x, double y, double z) {
    __nexa_phys_gx = (float)x; __nexa_phys_gy = (float)y; __nexa_phys_gz = (float)z;
    return 1;
}
static double __nexa_phys_coord(int id, int which) {
    if (!__nexa_phys_alive(id)) return 0.0;
    const __nexa_phys_body& b = __nexa_phys_bodies[id];
    float x1, y1, z1, x2, y2, z2;
    __nexa_phys_ends(b, &x1, &y1, &z1, &x2, &y2, &z2);
    switch (which) {
        case 0: return (double)b.x;
        case 1: return (double)b.y;
        case 2: return (double)b.z;
        case 3: return (double)b.vx;
        case 4: return (double)b.vy;
        case 5: return (double)b.vz;
        case 6: return (double)x1;
        case 7: return (double)y1;
        case 8: return (double)z1;
        case 9: return (double)x2;
        case 10: return (double)y2;
        default: return (double)z2;
    }
}
)PHYS";
        if (mesh) {
            out += "static int __nexa_phys_mesh_hit(int mesh, int other, float* nx, float* ny, float* nz, float* pen);\n";
            out += "static int __nexa_phys_mesh_mesh(int i, int j, float* nx, float* ny, float* nz, float* pen);\n";
            out += "static int __nexa_phys_sphere_mesh_toi(float ox, float oy, float oz, float dx, float dy, float dz, float r, int mesh, float* tout);\n";
        }
        out += R"PHYS(static int __nexa_phys_pair(int i, int j, float* nx, float* ny, float* nz, float* pen) {
    const __nexa_phys_body& a = __nexa_phys_bodies[i];
    const __nexa_phys_body& b = __nexa_phys_bodies[j];
    int ka = a.kind, kb = b.kind;
    if (ka == 1 && kb == 1)
        return __nexa_phys_sphere_sphere(a.x, a.y, a.z, a.r, b.x, b.y, b.z, b.r, nx, ny, nz, pen);
    if ((ka == 1 && kb == 2) || (ka == 2 && kb == 1)) {
        const __nexa_phys_body& s = ka == 1 ? a : b;
        const __nexa_phys_body& box = ka == 1 ? b : a;
        int hit = __nexa_phys_sphere_box(s.x, s.y, s.z, s.r, box.x, box.y, box.z, box.w, box.h, box.d, nx, ny, nz, pen);
        if (hit && ka != 1) { *nx = -*nx; *ny = -*ny; *nz = -*nz; }
        return hit;
    }
    if (ka == 2 && kb == 2)
        return __nexa_phys_box_box(a.x, a.y, a.z, a.w, a.h, a.d, b.x, b.y, b.z, b.w, b.h, b.d, nx, ny, nz, pen);
    if ((ka == 1 && kb == 3) || (ka == 3 && kb == 1)) {
        const __nexa_phys_body& s = ka == 1 ? a : b;
        const __nexa_phys_body& c = ka == 1 ? b : a;
        float x1, y1, z1, x2, y2, z2;
        __nexa_phys_ends(c, &x1, &y1, &z1, &x2, &y2, &z2);
        int hit = __nexa_phys_sphere_capsule(s.x, s.y, s.z, s.r, x1, y1, z1, x2, y2, z2, c.r, nx, ny, nz, pen);
        if (hit && ka != 1) { *nx = -*nx; *ny = -*ny; *nz = -*nz; }
        return hit;
    }
    if ((ka == 2 && kb == 3) || (ka == 3 && kb == 2)) {
        const __nexa_phys_body& box = ka == 2 ? a : b;
        const __nexa_phys_body& c = ka == 2 ? b : a;
        float x1, y1, z1, x2, y2, z2;
        __nexa_phys_ends(c, &x1, &y1, &z1, &x2, &y2, &z2);
        float hx = fabsf(box.w) * 0.5f, hy = fabsf(box.h) * 0.5f, hz = fabsf(box.d) * 0.5f;
        float spx, spy, spz, qpx, qpy, qpz;
        __nexa_phys_seg_aabb(x1, y1, z1, x2, y2, z2, box.x, box.y, box.z, hx, hy, hz, &spx, &spy, &spz, &qpx, &qpy, &qpz);
        float dx = spx - qpx, dy = spy - qpy, dz = spz - qpz;
        float d2 = dx * dx + dy * dy + dz * dz;
        int hit;
        if (d2 > 1e-10f) {
            float dist = sqrtf(d2);
            if (dist > c.r) return 0;
            *pen = c.r - dist;
            *nx = dx / dist; *ny = dy / dist; *nz = dz / dist;
            hit = 1;
        } else {
            hit = __nexa_phys_sphere_box((x1 + x2) * 0.5f, (y1 + y2) * 0.5f, (z1 + z2) * 0.5f, c.r,
                box.x, box.y, box.z, box.w, box.h, box.d, nx, ny, nz, pen);
        }
        if (hit && ka != 3) { *nx = -*nx; *ny = -*ny; *nz = -*nz; }
        return hit;
    }
    if (ka == 3 && kb == 3) {
        float a1x, a1y, a1z, a2x, a2y, a2z, b1x, b1y, b1z, b2x, b2y, b2z;
        __nexa_phys_ends(a, &a1x, &a1y, &a1z, &a2x, &a2y, &a2z);
        __nexa_phys_ends(b, &b1x, &b1y, &b1z, &b2x, &b2y, &b2z);
        float px, py, pz, qx, qy, qz;
        __nexa_phys_seg_seg(a1x, a1y, a1z, a2x, a2y, a2z, b1x, b1y, b1z, b2x, b2y, b2z, &px, &py, &pz, &qx, &qy, &qz);
        return __nexa_phys_sphere_sphere(px, py, pz, a.r, qx, qy, qz, b.r, nx, ny, nz, pen);
    }
)PHYS";
        if (mesh) {
            out += R"PHYS(
    if (ka == 4 || kb == 4) {
        if (ka == 4 && kb == 4) return __nexa_phys_mesh_mesh(i, j, nx, ny, nz, pen);
        int hit = __nexa_phys_mesh_hit(ka == 4 ? i : j, ka == 4 ? j : i, nx, ny, nz, pen);
        if (hit && ka == 4) { *nx = -*nx; *ny = -*ny; *nz = -*nz; }
        return hit;
    }
)PHYS";
        }
        out += R"PHYS(    return 0;
}
static int __nexa_phys_sweep_aabb(float ox, float oy, float oz, float dx, float dy, float dz,
    float minx, float miny, float minz, float maxx, float maxy, float maxz, float* tout) {
    float tmin = 0.f, tmax = 1.f;
    float o[3] = {ox, oy, oz}, dir[3] = {dx, dy, dz};
    float mn[3] = {minx, miny, minz}, mx[3] = {maxx, maxy, maxz};
    for (int a = 0; a < 3; a++) {
        if (fabsf(dir[a]) < 1e-8f) {
            if (o[a] < mn[a] || o[a] > mx[a]) return 0;
            continue;
        }
        float inv = 1.f / dir[a];
        float t1 = (mn[a] - o[a]) * inv;
        float t2 = (mx[a] - o[a]) * inv;
        if (t1 > t2) { float s = t1; t1 = t2; t2 = s; }
        if (t1 > tmin) tmin = t1;
        if (t2 < tmax) tmax = t2;
        if (tmin > tmax) return 0;
    }
    if (tmax < 0.f) return 0;
    *tout = tmin >= 0.f ? tmin : 0.f;
    return 1;
}
static int __nexa_phys_sphere_toi(float ax, float ay, float az, float bx, float by, float bz,
    float adx, float ady, float adz, float bdx, float bdy, float bdz,
    float ra, float rb, float* tout) {
    float rx = ax - bx, ry = ay - by, rz = az - bz;
    float vx = adx - bdx, vy = ady - bdy, vz = adz - bdz;
    float rad = ra + rb;
    float c = rx * rx + ry * ry + rz * rz - rad * rad;
    if (c <= 0.f) return 0;
    float a = vx * vx + vy * vy + vz * vz;
    float b = 2.f * (rx * vx + ry * vy + rz * vz);
    if (a < 1e-12f || b >= 0.f) return 0;
    float disc = b * b - 4.f * a * c;
    if (disc < 0.f) return 0;
    float t = (-b - sqrtf(disc)) / (2.f * a);
    if (t <= 0.f || t > 1.f) return 0;
    *tout = t;
    return 1;
}
// Drop the part of the step that would push two spheres deeper together.
// Split by inverse mass, so a heavy one gives up less of its movement.
static void __nexa_phys_clip_spheres(int hi, float* ox, float* oy, float* oz,
    float* mx, float* my, float* mz) {
    for (int i = 1; i <= hi; i++) {
        if (__nexa_phys_bodies[i].kind != 1) continue;
        for (int j = i + 1; j <= hi; j++) {
            if (__nexa_phys_bodies[j].kind != 1) continue;
            float dx = ox[i] - ox[j], dy = oy[i] - oy[j], dz = oz[i] - oz[j];
            float rad = __nexa_phys_bodies[i].r + __nexa_phys_bodies[j].r;
            float d2 = dx * dx + dy * dy + dz * dz;
            if (d2 >= rad * rad || d2 < 1e-12f) continue;
            float dist = sqrtf(d2);
            float nx = dx / dist, ny = dy / dist, nz = dz / dist;
            float rel = (mx[i] - mx[j]) * nx + (my[i] - my[j]) * ny + (mz[i] - mz[j]) * nz;
            if (rel >= 0.f) continue;
            float invi = __nexa_phys_bodies[i].inv, invj = __nexa_phys_bodies[j].inv;
            float inv = invi + invj;
            if (inv <= 0.f) continue;
            float alpha = -rel / inv;
            mx[i] += nx * alpha * invi; my[i] += ny * alpha * invi; mz[i] += nz * alpha * invi;
            mx[j] -= nx * alpha * invj; my[j] -= ny * alpha * invj; mz[j] -= nz * alpha * invj;
        }
    }
}
static void __nexa_phys_integrate(float h) {
    int hi = __nexa_phys_hi;
    float ox[257], oy[257], oz[257];
    float mx[257], my[257], mz[257];
    unsigned char dyn[257];
    for (int i = 1; i <= hi; i++) {
        __nexa_phys_body& b = __nexa_phys_bodies[i];
        dyn[i] = 0;
        ox[i] = b.x; oy[i] = b.y; oz[i] = b.z;
        mx[i] = my[i] = mz[i] = 0.f;
        if (b.kind == 0 || b.inv == 0.f) continue;
        b.vx += __nexa_phys_gx * h;
        b.vy += __nexa_phys_gy * h;
        b.vz += __nexa_phys_gz * h;
        mx[i] = b.vx * h; my[i] = b.vy * h; mz[i] = b.vz * h;
        dyn[i] = 1;
    }
    __nexa_phys_clip_spheres(hi, ox, oy, oz, mx, my, mz);
    for (int i = 1; i <= hi; i++) {
        if (!dyn[i]) continue;
        __nexa_phys_body& b = __nexa_phys_bodies[i];
        float best = 1.f;
        float move = __nexa_phys_len(mx[i], my[i], mz[i]);
        if (b.kind == 1 && move > b.r * 0.5f) {
            for (int j = 1; j <= hi; j++) {
                __nexa_phys_body& box = __nexa_phys_bodies[j];
                if (box.kind != 2 || box.inv != 0.f) continue;
                float ex = fabsf(box.w) * 0.5f + b.r;
                float ey = fabsf(box.h) * 0.5f + b.r;
                float ez = fabsf(box.d) * 0.5f + b.r;
                float t;
                if (!__nexa_phys_sweep_aabb(ox[i], oy[i], oz[i], mx[i], my[i], mz[i],
                        box.x - ex, box.y - ey, box.z - ez,
                        box.x + ex, box.y + ey, box.z + ez, &t)) continue;
                if (t > 0.f && t < best) best = t;
            }
        }
        if (b.kind == 1 && move > 1e-6f) {
            for (int j = 1; j <= hi; j++) {
                if (j == i || __nexa_phys_bodies[j].kind != 1) continue;
                float t;
                if (!__nexa_phys_sphere_toi(ox[i], oy[i], oz[i], ox[j], oy[j], oz[j],
                        mx[i], my[i], mz[i], mx[j], my[j], mz[j],
                        b.r, __nexa_phys_bodies[j].r, &t)) continue;
                if (t < best) best = t;
            }
        }
)PHYS";
        if (mesh) {
            out += R"PHYS(
        if (b.kind == 1 && move > 1e-6f) {
            for (int j = 1; j <= hi; j++) {
                if (__nexa_phys_bodies[j].kind != 4 || __nexa_phys_bodies[j].inv != 0.f) continue;
                float t;
                float rdx = mx[i] - mx[j], rdy = my[i] - my[j], rdz = mz[i] - mz[j];
                if (!__nexa_phys_sphere_mesh_toi(ox[i], oy[i], oz[i], rdx, rdy, rdz, b.r, j, &t)) continue;
                if (t >= 0.f && t < best) best = t;
            }
        }
)PHYS";
        }
        out += R"PHYS(        if (best < 1.f && move > 1e-8f) {
            float adv = best * move + 0.002f;
            if (adv > move) adv = move;
            float s = adv / move;
            b.x = ox[i] + mx[i] * s; b.y = oy[i] + my[i] * s; b.z = oz[i] + mz[i] * s;
        } else {
            b.x = ox[i] + mx[i]; b.y = oy[i] + my[i]; b.z = oz[i] + mz[i];
        }
    }
}
static void __nexa_phys_solve(float h, unsigned char* touch) {
    int hi = __nexa_phys_hi;
    if (h < 1e-6f) h = 1e-6f;
    for (int iter = 0; iter < 8; iter++) {
        int fwd = (iter & 1) == 0;
        for (int i = fwd ? 1 : hi; fwd ? (i <= hi) : (i >= 1); i += fwd ? 1 : -1) {
            if (__nexa_phys_bodies[i].kind == 0) continue;
            for (int j = i + 1; j <= hi; j++) {
                if (__nexa_phys_bodies[j].kind == 0) continue;
                __nexa_phys_body& a = __nexa_phys_bodies[i];
                __nexa_phys_body& b = __nexa_phys_bodies[j];
                if (a.inv == 0.f && b.inv == 0.f) continue;
                float nx, ny, nz, pen;
                if (!__nexa_phys_pair(i, j, &nx, &ny, &nz, &pen)) continue;
                float inv = a.inv + b.inv;
                if (inv <= 0.f) continue;
                float rvx = a.vx - b.vx, rvy = a.vy - b.vy, rvz = a.vz - b.vz;
                float rel = rvx * nx + rvy * ny + rvz * nz;
                // Wanted speed along the normal. Positive separates. Bounce
                // replaces the bias on a hard hit; a slow overlap only creeps
                // apart, shared by mass, so nobody is teleported aside.
)PHYS";
        if (!squish) {
            out += R"PHYS(                const float slop = 0.005f;
                float want = 0.f;
                if (pen > slop) {
                    want = 2.f * (pen - slop) / h;
                    if (want > 2.f) want = 2.f;
                }
                if (iter == 0 && rel < -1.f) {
                    float e = a.bounce > b.bounce ? a.bounce : b.bounce;
                    float bounce = -e * rel;
                    if (bounce > want) want = bounce;
                }
)PHYS";
        } else {
            out += R"PHYS(                const float slop = 0.005f;
                float want = 0.f;
                float __soft = __nexa_phys_squish;
                if (__soft <= 0.f) {
                    if (pen > slop) {
                        want = 2.f * (pen - slop) / h;
                        if (want > 2.f) want = 2.f;
                    }
                    if (iter == 0 && rel < -1.f) {
                        float e = a.bounce > b.bounce ? a.bounce : b.bounce;
                        float bounce = -e * rel;
                        if (bounce > want) want = bounce;
                    }
                } else {
                    // The overlap is the compression. A fast hit is pushed in
                    // once, to the depth the spring will hold, and the closing
                    // speed is absorbed. Later frames ease that overlap back
                    // to a small sag. Nothing here launches the body back out.
                    float __rad = __nexa_phys_soft_rad(a, b);
                    float __sag = __soft * __rad * 0.16f;
                    float __maxp = __soft * __rad * 0.45f;
                    if (iter == 0 && rel < -1.2f && pen < __maxp) {
                        float __need = (-rel) * __soft * 0.04f;
                        if (__need > __maxp) __need = __maxp;
                        if (pen < __need) {
                            float __sink = __need - pen;
                            float __sa = __sink * (a.inv / inv);
                            float __sb = __sink * (b.inv / inv);
                            a.x -= nx * __sa; a.y -= ny * __sa; a.z -= nz * __sa;
                            b.x += nx * __sb; b.y += ny * __sb; b.z += nz * __sb;
                            pen = __need;
                        }
                    }
                    if (pen > __sag) {
                        want = (pen - __sag) * 1.6f;
                        if (want > 0.5f) want = 0.5f;
                    }
                    if (iter == 0 && rel < -1.f) {
                        float e = a.bounce > b.bounce ? a.bounce : b.bounce;
                        float bounce = -e * rel * (1.f - __soft);
                        if (bounce > want) want = bounce;
                    }
                }
)PHYS";
        }
        out += R"PHYS(                float jn = 0.f;
                if (rel < want) {
                    jn = (want - rel) / inv;
                    a.vx += nx * jn * a.inv; a.vy += ny * jn * a.inv; a.vz += nz * jn * a.inv;
                    b.vx -= nx * jn * b.inv; b.vy -= ny * jn * b.inv; b.vz -= nz * jn * b.inv;
                    rvx = a.vx - b.vx; rvy = a.vy - b.vy; rvz = a.vz - b.vz;
                    rel = rvx * nx + rvy * ny + rvz * nz;
                }
                float tx = rvx - rel * nx, ty = rvy - rel * ny, tz = rvz - rel * nz;
                float tl = __nexa_phys_len(tx, ty, tz);
                if (tl > 1e-6f) {
                    float limit = jn;
                    if (iter == 0) {
                        float gn = __nexa_phys_gx * nx + __nexa_phys_gy * ny + __nexa_phys_gz * nz;
                        float into = 0.f;
                        if (a.inv > 0.f && gn < 0.f) into += (-gn) * (1.f / a.inv) * h;
                        if (b.inv > 0.f && gn > 0.f) into += gn * (1.f / b.inv) * h;
                        if (into > limit) limit = into;
                    }
)PHYS";
        if (mesh) {
            out += "                    float mu = (a.kind == 2 || b.kind == 2 || a.kind == 4 || b.kind == 4) ? 0.6f : 0.02f;\n";
        } else {
            out += "                    float mu = (a.kind == 2 || b.kind == 2) ? 0.6f : 0.02f;\n";
        }
        out += R"PHYS(                    float maxf = mu * limit;
                    float mag = tl / inv;
                    if (mag > maxf) mag = maxf;
                    if (mag > 0.f) {
                        float itx = tx / tl, ity = ty / tl, itz = tz / tl;
                        a.vx -= itx * mag * a.inv; a.vy -= ity * mag * a.inv; a.vz -= itz * mag * a.inv;
                        b.vx += itx * mag * b.inv; b.vy += ity * mag * b.inv; b.vz += itz * mag * b.inv;
                    }
                }
                if (pen > 0.02f) { touch[i] = touch[j] = 2; }
                else {
                    if (touch[i] != 2) touch[i] = 1;
                    if (touch[j] != 2) touch[j] = 1;
                }
            }
        }
    }
)PHYS";
        if (squish) {
            // A soft pair shares the hit, so the lower body is left moving
            // into the floor. Stop that leftover speed after the iterations,
            // or the next step carries it through the static shape.
            out += R"PHYS(    if (__nexa_phys_squish > 0.f) {
        for (int i = 1; i <= hi; i++) {
            if (__nexa_phys_bodies[i].kind == 0) continue;
            for (int j = i + 1; j <= hi; j++) {
                if (__nexa_phys_bodies[j].kind == 0) continue;
                __nexa_phys_body& a = __nexa_phys_bodies[i];
                __nexa_phys_body& b = __nexa_phys_bodies[j];
                if ((a.inv == 0.f) == (b.inv == 0.f)) continue;
                float nx, ny, nz, pen;
                if (!__nexa_phys_pair(i, j, &nx, &ny, &nz, &pen) || pen <= 0.f) continue;
                __nexa_phys_body& d = (a.inv > 0.f) ? a : b;
                float sign = (a.inv > 0.f) ? 1.f : -1.f;
                float vn = (d.vx * nx + d.vy * ny + d.vz * nz) * sign;
                float __rad = __nexa_phys_soft_rad(a, b);
                float __sag = __nexa_phys_squish * __rad * 0.16f;
                float hold = 0.f;
                if (pen > __sag) {
                    hold = (pen - __sag) * 1.6f;
                    if (hold > 0.5f) hold = 0.5f;
                }
                if (vn < hold) {
                    float dv = hold - vn;
                    d.vx += nx * sign * dv;
                    d.vy += ny * sign * dv;
                    d.vz += nz * sign * dv;
                }
            }
        }
    }
)PHYS";
        }
        out += R"PHYS(}
// Take the overlap back out. The impulse has already shared the speed, so
// this only undoes the intersection, split the same way by mass.
static void __nexa_phys_separate() {
    int hi = __nexa_phys_hi;
    for (int iter = 0; iter < 8; iter++) {
        int any = 0;
        for (int i = 1; i <= hi; i++) {
            if (__nexa_phys_bodies[i].kind == 0) continue;
            for (int j = i + 1; j <= hi; j++) {
                if (__nexa_phys_bodies[j].kind == 0) continue;
                __nexa_phys_body& a = __nexa_phys_bodies[i];
                __nexa_phys_body& b = __nexa_phys_bodies[j];
                float inv = a.inv + b.inv;
                if (inv <= 0.f) continue;
                float nx, ny, nz, pen;
                if (!__nexa_phys_pair(i, j, &nx, &ny, &nz, &pen) || pen <= 0.001f) continue;
                any = 1;
)PHYS";
        if (!squish) {
            out += "                float corr = (pen - 0.001f) * 0.85f;\n";
        } else {
            out += R"PHYS(                float corr;
                if (__nexa_phys_squish <= 0.f) corr = (pen - 0.001f) * 0.85f;
                else {
                    float __rad = __nexa_phys_soft_rad(a, b);
                    float __sag = __nexa_phys_squish * __rad * 0.16f;
                    float __maxp = __nexa_phys_squish * __rad * 0.45f;
                    if (pen > __maxp) corr = (pen - __maxp) * 0.7f;
                    else {
                        float __use = pen - __sag;
                        if (__use < 0.f) __use = 0.f;
                        corr = __use * 0.012f;
                    }
                }
)PHYS";
        }
        out += R"PHYS(                float sa = corr * (a.inv / inv), sb = corr * (b.inv / inv);
                a.x += nx * sa; a.y += ny * sa; a.z += nz * sa;
                b.x -= nx * sb; b.y -= ny * sb; b.z -= nz * sb;
            }
        }
        if (!any) break;
    }
)PHYS";
        if (squish) {
            // The pair above can shove a body into a static shape while it
            // eases two soft bodies apart. Put it back on the near side.
            out += R"PHYS(    if (__nexa_phys_squish > 0.f) {
        for (int i = 1; i <= hi; i++) {
            if (__nexa_phys_bodies[i].kind == 0) continue;
            for (int j = i + 1; j <= hi; j++) {
                if (__nexa_phys_bodies[j].kind == 0) continue;
                __nexa_phys_body& a = __nexa_phys_bodies[i];
                __nexa_phys_body& b = __nexa_phys_bodies[j];
                if ((a.inv == 0.f) == (b.inv == 0.f)) continue;
                float inv = a.inv + b.inv;
                if (inv <= 0.f) continue;
                float nx, ny, nz, pen;
                if (!__nexa_phys_pair(i, j, &nx, &ny, &nz, &pen)) continue;
                float __maxp = __nexa_phys_squish * __nexa_phys_soft_rad(a, b) * 0.45f;
                if (pen <= __maxp) continue;
                float corr = pen - __maxp;
                float sa = corr * (a.inv / inv), sb = corr * (b.inv / inv);
                a.x += nx * sa; a.y += ny * sa; a.z += nz * sa;
                b.x -= nx * sb; b.y -= ny * sb; b.z -= nz * sb;
            }
        }
    }
)PHYS";
        }
        out += R"PHYS(}
static int __nexa_phys_step(double dtin) {
    float dt = (float)dtin;
    if (dt <= 0.f) return 0;
    if (dt > 0.05f) dt = 0.05f;
    int subs = 1;
    int hi = __nexa_phys_hi;
    for (int i = 1; i <= hi; i++) {
        __nexa_phys_body& b = __nexa_phys_bodies[i];
        if (b.kind == 0 || b.inv == 0.f) continue;
        float vx = b.vx + __nexa_phys_gx * dt;
        float vy = b.vy + __nexa_phys_gy * dt;
        float vz = b.vz + __nexa_phys_gz * dt;
        float rad = (b.kind == 2) ? fabsf(b.w) : b.r;
        if (b.kind == 2) {
            if (fabsf(b.h) < rad) rad = fabsf(b.h);
            if (fabsf(b.d) < rad) rad = fabsf(b.d);
            rad *= 0.5f;
        }
        if (rad < 0.05f) rad = 0.05f;
        int need = (int)(__nexa_phys_len(vx, vy, vz) * dt / (rad * 0.5f)) + 1;
        if (need > subs) subs = need;
    }
    if (subs > 4) subs = 4;
    float h = dt / (float)subs;
    unsigned char touch[257];
    for (int i = 1; i <= 256; i++) touch[i] = 0;
    for (int s = 0; s < subs; s++) {
        __nexa_phys_integrate(h);
        __nexa_phys_solve(h, touch);
        __nexa_phys_separate();
    }
    hi = __nexa_phys_hi;
    for (int i = 1; i <= hi; i++) {
        __nexa_phys_body& b = __nexa_phys_bodies[i];
        if (b.kind == 0 || b.inv == 0.f || touch[i] != 1) continue;
        float sp = b.vx * b.vx + b.vy * b.vy + b.vz * b.vz;
        if (sp < 0.0009f) b.vx = b.vy = b.vz = 0.f;
    }
)PHYS";
        if (squish) {
            out += R"PHYS(
    if (__nexa_phys_squish > 0.f) {
        for (int i = 1; i <= hi; i++) __nexa_phys_bodies[i].hit = 0.f;
        for (int i = 1; i <= hi; i++) {
            if (__nexa_phys_bodies[i].kind == 0) continue;
            for (int j = i + 1; j <= hi; j++) {
                if (__nexa_phys_bodies[j].kind == 0) continue;
                __nexa_phys_body& a = __nexa_phys_bodies[i];
                __nexa_phys_body& b = __nexa_phys_bodies[j];
                float inv = a.inv + b.inv;
                if (inv <= 0.f) continue;
                float nx, ny, nz, pen;
                if (!__nexa_phys_pair(i, j, &nx, &ny, &nz, &pen) || pen <= 0.001f) continue;
                float pa = pen * (a.inv / inv);
                float pb = pen * (b.inv / inv);
                if (a.inv > 0.f && pa > a.hit) { a.hit = pa; a.snx = nx; a.sny = ny; a.snz = nz; }
                if (b.inv > 0.f && pb > b.hit) { b.hit = pb; b.snx = -nx; b.sny = -ny; b.snz = -nz; }
            }
        }
        for (int i = 1; i <= hi; i++) {
            __nexa_phys_body& b = __nexa_phys_bodies[i];
            if (b.kind == 0 || b.inv == 0.f) continue;
            float rad = __nexa_phys_body_rad(b);
            if (rad < 0.05f) rad = 0.05f;
            float target = b.hit / rad;
            if (target > 0.72f) target = 0.72f;
            float k = (target > b.sq) ? 0.85f : 0.7f;
            b.sq += (target - b.sq) * k;
            if (b.sq < 0.f) b.sq = 0.f;
            if (b.sq > 0.72f) b.sq = 0.72f;
            b.hit = 0.f;
            float L2 = b.snx * b.snx + b.sny * b.sny + b.snz * b.snz;
            if (L2 < 1e-8f) { b.snx = 0.f; b.sny = 1.f; b.snz = 0.f; }
            else {
                float invn = 1.f / sqrtf(L2);
                b.snx *= invn; b.sny *= invn; b.snz *= invn;
            }
        }
    }
    return 1;
}
static int __nexa_phys_squishy(double amount) {
    float s = (float)amount;
    if (s < 0.f) s = 0.f;
    if (s > 1.f) s = 1.f;
    __nexa_phys_squish = s;
    return 1;
}
static double __nexa_phys_squash(int id) {
    if (!__nexa_phys_alive(id)) return 0.0;
    return (double)__nexa_phys_bodies[id].sq;
}
static double __nexa_phys_sn(int id, int w) {
    if (!__nexa_phys_alive(id)) return w == 1 ? 1.0 : 0.0;
    const __nexa_phys_body& b = __nexa_phys_bodies[id];
    float L2 = b.snx * b.snx + b.sny * b.sny + b.snz * b.snz;
    if (L2 < 1e-8f) return w == 1 ? 1.0 : 0.0;
    if (w == 0) return (double)b.snx;
    if (w == 1) return (double)b.sny;
    return (double)b.snz;
}
)PHYS";
        } else {
            out += "    return 1;\n}\n";
        }
    }
    return out;
}

// Emitted only when a program calls physics.mesh or physics.ray_mesh, and only
// after gfx3d's model block: the triangles live in __nexa_g3_models. The same
// centre and scale as gfx3d.draw, so a drawn model and its collider coincide.
inline std::string physicsMeshStubCpp() {
    return R"MESH(
static int __nexa_phys_mesh(double, double, double, double, double) { return 0; }
static double __nexa_phys_ray_mesh(double, double, double, double, double, double,
    double, double, double, double, double) {
    __nexa_phys_hit_clear();
    return -1.0;
}
)MESH";
}

inline std::string physicsMeshCpp(bool withBody) {
    std::string out = R"MESH(
static int __nexa_phys_model_ok(int id) {
    return id >= 1 && (size_t)id < __nexa_g3_models.size() && __nexa_g3_models[(size_t)id].p.size() >= 9;
}
static void __nexa_phys_tri_vert(const __nexa_G3Model& m, float s, float bx, float by, float bz, int i,
    float* x, float* y, float* z) {
    *x = (m.p[i] - m.cx) * s + bx;
    *y = (m.p[i + 1] - m.cy) * s + by;
    *z = (m.p[i + 2] - m.cz) * s + bz;
}
static double __nexa_phys_ray_mesh(double ox, double oy, double oz, double dx, double dy, double dz,
    double model, double x, double y, double z, double scale) {
    __nexa_phys_hit_clear();
    int id = (int)model;
    if (!__nexa_phys_model_ok(id)) return -1.0;
    const __nexa_G3Model& m = __nexa_g3_models[(size_t)id];
    float s = (float)scale * m.inv;
    float fx = (float)x, fy = (float)y, fz = (float)z;
    float best = -1.f;
    float bhx = 0.f, bhy = 0.f, bhz = 0.f, bnx = 0.f, bny = 1.f, bnz = 0.f;
    int n = (int)(m.p.size() / 9);
    for (int ti = 0; ti < n; ti++) {
        float ax, ay, az, bx, by, bz, cx, cy, cz;
        __nexa_phys_tri_vert(m, s, fx, fy, fz, ti * 9, &ax, &ay, &az);
        __nexa_phys_tri_vert(m, s, fx, fy, fz, ti * 9 + 3, &bx, &by, &bz);
        __nexa_phys_tri_vert(m, s, fx, fy, fz, ti * 9 + 6, &cx, &cy, &cz);
        float e1x = bx - ax, e1y = by - ay, e1z = bz - az;
        float e2x = cx - ax, e2y = cy - ay, e2z = cz - az;
        float px = (float)dy * e2z - (float)dz * e2y;
        float py = (float)dz * e2x - (float)dx * e2z;
        float pz = (float)dx * e2y - (float)dy * e2x;
        float det = e1x * px + e1y * py + e1z * pz;
        if (det > -1e-8f && det < 1e-8f) continue;
        float inv = 1.f / det;
        float tx = (float)ox - ax, ty = (float)oy - ay, tz = (float)oz - az;
        float u = (tx * px + ty * py + tz * pz) * inv;
        if (u < 0.f || u > 1.f) continue;
        float qx = ty * e1z - tz * e1y, qy = tz * e1x - tx * e1z, qz = tx * e1y - ty * e1x;
        float v = ((float)dx * qx + (float)dy * qy + (float)dz * qz) * inv;
        if (v < 0.f || u + v > 1.f) continue;
        float t = (e2x * qx + e2y * qy + e2z * qz) * inv;
        if (t < 0.f || (best >= 0.f && t >= best)) continue;
        float nx = e1y * e2z - e1z * e2y, ny = e1z * e2x - e1x * e2z, nz = e1x * e2y - e1y * e2x;
        if (nx * (float)dx + ny * (float)dy + nz * (float)dz > 0.f) { nx = -nx; ny = -ny; nz = -nz; }
        best = t;
        bhx = (float)ox + (float)dx * t; bhy = (float)oy + (float)dy * t; bhz = (float)oz + (float)dz * t;
        bnx = nx; bny = ny; bnz = nz;
    }
    if (best < 0.f) return -1.0;
    __nexa_phys_hit_set(bhx, bhy, bhz, bnx, bny, bnz);
    return (double)best;
}
)MESH";
    if (!withBody) return out;
    out += R"MESH(
struct __nexa_phys_tri { float x0, y0, z0, x1, y1, z1, x2, y2, z2; };
static int __nexa_phys_in_tri(float px, float py, float pz, const __nexa_phys_tri& t) {
    float abx = t.x1 - t.x0, aby = t.y1 - t.y0, abz = t.z1 - t.z0;
    float acx = t.x2 - t.x0, acy = t.y2 - t.y0, acz = t.z2 - t.z0;
    float apx = px - t.x0, apy = py - t.y0, apz = pz - t.z0;
    float d00 = abx * abx + aby * aby + abz * abz;
    float d01 = abx * acx + aby * acy + abz * acz;
    float d11 = acx * acx + acy * acy + acz * acz;
    float d20 = apx * abx + apy * aby + apz * abz;
    float d21 = apx * acx + apy * acy + apz * acz;
    float den = d00 * d11 - d01 * d01;
    if (den > -1e-12f && den < 1e-12f) return 0;
    float v = (d11 * d20 - d01 * d21) / den;
    float w = (d00 * d21 - d01 * d20) / den;
    return v >= -1e-3f && w >= -1e-3f && (v + w) <= 1.001f;
}
static void __nexa_phys_closest_tri(float px, float py, float pz, const __nexa_phys_tri& t,
    float* ox, float* oy, float* oz) {
    float abx = t.x1 - t.x0, aby = t.y1 - t.y0, abz = t.z1 - t.z0;
    float acx = t.x2 - t.x0, acy = t.y2 - t.y0, acz = t.z2 - t.z0;
    float apx = px - t.x0, apy = py - t.y0, apz = pz - t.z0;
    float d1 = abx * apx + aby * apy + abz * apz;
    float d2 = acx * apx + acy * apy + acz * apz;
    if (d1 <= 0.f && d2 <= 0.f) { *ox = t.x0; *oy = t.y0; *oz = t.z0; return; }
    float bpx = px - t.x1, bpy = py - t.y1, bpz = pz - t.z1;
    float d3 = abx * bpx + aby * bpy + abz * bpz;
    float d4 = acx * bpx + acy * bpy + acz * bpz;
    if (d3 >= 0.f && d4 <= d3) { *ox = t.x1; *oy = t.y1; *oz = t.z1; return; }
    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f) {
        float v = d1 / (d1 - d3);
        *ox = t.x0 + abx * v; *oy = t.y0 + aby * v; *oz = t.z0 + abz * v;
        return;
    }
    float cpx = px - t.x2, cpy = py - t.y2, cpz = pz - t.z2;
    float d5 = abx * cpx + aby * cpy + abz * cpz;
    float d6 = acx * cpx + acy * cpy + acz * cpz;
    if (d6 >= 0.f && d5 <= d6) { *ox = t.x2; *oy = t.y2; *oz = t.z2; return; }
    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f) {
        float w = d2 / (d2 - d6);
        *ox = t.x0 + acx * w; *oy = t.y0 + acy * w; *oz = t.z0 + acz * w;
        return;
    }
    float va = d3 * d6 - d5 * d4;
    if (va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        *ox = t.x1 + (t.x2 - t.x1) * w; *oy = t.y1 + (t.y2 - t.y1) * w; *oz = t.z1 + (t.z2 - t.z1) * w;
        return;
    }
    float denom = va + vb + vc;
    if (denom > -1e-12f && denom < 1e-12f) { *ox = t.x0; *oy = t.y0; *oz = t.z0; return; }
    float v = vb / denom, w = vc / denom;
    *ox = t.x0 + abx * v + acx * w;
    *oy = t.y0 + aby * v + acy * w;
    *oz = t.z0 + abz * v + acz * w;
}
static int __nexa_phys_load_tri(const __nexa_G3Model& m, float s, float bx, float by, float bz, int ti, __nexa_phys_tri* t) {
    __nexa_phys_tri_vert(m, s, bx, by, bz, ti * 9, &t->x0, &t->y0, &t->z0);
    __nexa_phys_tri_vert(m, s, bx, by, bz, ti * 9 + 3, &t->x1, &t->y1, &t->z1);
    __nexa_phys_tri_vert(m, s, bx, by, bz, ti * 9 + 6, &t->x2, &t->y2, &t->z2);
    return 1;
}
static int __nexa_phys_near_cube(float x, float y, float z, float bx, float by, float bz, float half, float pad) {
    return x >= bx - half - pad && x <= bx + half + pad &&
           y >= by - half - pad && y <= by + half + pad &&
           z >= bz - half - pad && z <= bz + half + pad;
}
static void __nexa_phys_sphere_one(float sx, float sy, float sz, float sr, const __nexa_phys_tri& t,
    int* hit, float* best, float* nx, float* ny, float* nz) {
    float qx, qy, qz;
    __nexa_phys_closest_tri(sx, sy, sz, t, &qx, &qy, &qz);
    float dx = sx - qx, dy = sy - qy, dz = sz - qz;
    float d2 = dx * dx + dy * dy + dz * dz;
    if (d2 > sr * sr) return;
    float dist = sqrtf(d2);
    float pen = sr - dist;
    if (*hit && pen <= *best) return;
    *hit = 1;
    *best = pen;
    if (dist > 1e-5f) { *nx = dx / dist; *ny = dy / dist; *nz = dz / dist; return; }
    float e1x = t.x1 - t.x0, e1y = t.y1 - t.y0, e1z = t.z1 - t.z0;
    float e2x = t.x2 - t.x0, e2y = t.y2 - t.y0, e2z = t.z2 - t.z0;
    float fnx = e1y * e2z - e1z * e2y, fny = e1z * e2x - e1x * e2z, fnz = e1x * e2y - e1y * e2x;
    if (fnx * (sx - t.x0) + fny * (sy - t.y0) + fnz * (sz - t.z0) < 0.f) { fnx = -fnx; fny = -fny; fnz = -fnz; }
    float L = sqrtf(fnx * fnx + fny * fny + fnz * fnz);
    if (L > 1e-8f) { *nx = fnx / L; *ny = fny / L; *nz = fnz / L; }
    else { *nx = 0.f; *ny = 1.f; *nz = 0.f; }
}
static void __nexa_phys_capsule_one(float x1, float y1, float z1, float x2, float y2, float z2, float rad,
    const __nexa_phys_tri& t, int* hit, float* best, float* nx, float* ny, float* nz) {
    float qx = 0.f, qy = 0.f, qz = 0.f, px = 0.f, py = 0.f, pz = 0.f;
    float best2 = 1e30f;
    float ends[2][3] = {{x1, y1, z1}, {x2, y2, z2}};
    for (int e = 0; e < 2; e++) {
        float cx, cy, cz;
        __nexa_phys_closest_tri(ends[e][0], ends[e][1], ends[e][2], t, &cx, &cy, &cz);
        float dx = ends[e][0] - cx, dy = ends[e][1] - cy, dz = ends[e][2] - cz;
        float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < best2) { best2 = d2; qx = cx; qy = cy; qz = cz; px = ends[e][0]; py = ends[e][1]; pz = ends[e][2]; }
    }
    float edge[3][6] = {
        {t.x0, t.y0, t.z0, t.x1, t.y1, t.z1},
        {t.x1, t.y1, t.z1, t.x2, t.y2, t.z2},
        {t.x2, t.y2, t.z2, t.x0, t.y0, t.z0}
    };
    for (int e = 0; e < 3; e++) {
        float spx, spy, spz, epx, epy, epz;
        __nexa_phys_seg_seg(x1, y1, z1, x2, y2, z2, edge[e][0], edge[e][1], edge[e][2], edge[e][3], edge[e][4], edge[e][5],
            &spx, &spy, &spz, &epx, &epy, &epz);
        float dx = spx - epx, dy = spy - epy, dz = spz - epz;
        float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < best2) { best2 = d2; qx = epx; qy = epy; qz = epz; px = spx; py = spy; pz = spz; }
    }
    float e1x = t.x1 - t.x0, e1y = t.y1 - t.y0, e1z = t.z1 - t.z0;
    float e2x = t.x2 - t.x0, e2y = t.y2 - t.y0, e2z = t.z2 - t.z0;
    float fnx = e1y * e2z - e1z * e2y, fny = e1z * e2x - e1x * e2z, fnz = e1x * e2y - e1y * e2x;
    float nl = sqrtf(fnx * fnx + fny * fny + fnz * fnz);
    if (nl > 1e-8f) {
        fnx /= nl; fny /= nl; fnz /= nl;
        float d1 = (x1 - t.x0) * fnx + (y1 - t.y0) * fny + (z1 - t.z0) * fnz;
        float d2 = (x2 - t.x0) * fnx + (y2 - t.y0) * fny + (z2 - t.z0) * fnz;
        if (d1 * d2 <= 0.f) {
            float den = d1 - d2;
            float u = (den > -1e-8f && den < 1e-8f) ? 0.f : d1 / den;
            if (u < 0.f) u = 0.f; if (u > 1.f) u = 1.f;
            float hx = x1 + (x2 - x1) * u, hy = y1 + (y2 - y1) * u, hz = z1 + (z2 - z1) * u;
            if (__nexa_phys_in_tri(hx, hy, hz, t)) {
                float mx = (x1 + x2) * 0.5f - hx, my = (y1 + y2) * 0.5f - hy, mz = (z1 + z2) * 0.5f - hz;
                if (mx * fnx + my * fny + mz * fnz < 0.f) { fnx = -fnx; fny = -fny; fnz = -fnz; }
                if (!*hit || rad > *best) { *hit = 1; *best = rad; *nx = fnx; *ny = fny; *nz = fnz; }
                return;
            }
        }
    }
    if (best2 > rad * rad) return;
    float dist = sqrtf(best2);
    float pen = rad - dist;
    if (*hit && pen <= *best) return;
    *hit = 1;
    *best = pen;
    float dx = px - qx, dy = py - qy, dz = pz - qz;
    if (dist > 1e-6f) { *nx = dx / dist; *ny = dy / dist; *nz = dz / dist; }
    else { *nx = 0.f; *ny = 1.f; *nz = 0.f; }
}
static int __nexa_phys_sat_axis(float ax, float ay, float az, const __nexa_phys_tri& t, float hx, float hy, float hz) {
    float len = sqrtf(ax * ax + ay * ay + az * az);
    if (len < 1e-8f) return 1;
    ax /= len; ay /= len; az /= len;
    float p0 = t.x0 * ax + t.y0 * ay + t.z0 * az;
    float p1 = t.x1 * ax + t.y1 * ay + t.z1 * az;
    float p2 = t.x2 * ax + t.y2 * ay + t.z2 * az;
    float tmin = p0, tmax = p0;
    if (p1 < tmin) tmin = p1; if (p1 > tmax) tmax = p1;
    if (p2 < tmin) tmin = p2; if (p2 > tmax) tmax = p2;
    float rad = fabsf(ax) * hx + fabsf(ay) * hy + fabsf(az) * hz;
    if (tmax < -rad || tmin > rad) return 0;
    return 1;
}
static void __nexa_phys_box_one(const __nexa_phys_tri& world, float cx, float cy, float cz, float hx, float hy, float hz,
    int* hit, float* best, float* nx, float* ny, float* nz) {
    __nexa_phys_tri t = world;
    t.x0 -= cx; t.y0 -= cy; t.z0 -= cz;
    t.x1 -= cx; t.y1 -= cy; t.z1 -= cz;
    t.x2 -= cx; t.y2 -= cy; t.z2 -= cz;
    float e1x = t.x1 - t.x0, e1y = t.y1 - t.y0, e1z = t.z1 - t.z0;
    float e2x = t.x2 - t.x0, e2y = t.y2 - t.y0, e2z = t.z2 - t.z0;
    float fnx = e1y * e2z - e1z * e2y, fny = e1z * e2x - e1x * e2z, fnz = e1x * e2y - e1y * e2x;
    float nl = sqrtf(fnx * fnx + fny * fny + fnz * fnz);
    if (nl < 1e-8f) return;
    fnx /= nl; fny /= nl; fnz /= nl;
    if (!__nexa_phys_sat_axis(1.f, 0.f, 0.f, t, hx, hy, hz)) return;
    if (!__nexa_phys_sat_axis(0.f, 1.f, 0.f, t, hx, hy, hz)) return;
    if (!__nexa_phys_sat_axis(0.f, 0.f, 1.f, t, hx, hy, hz)) return;
    if (!__nexa_phys_sat_axis(fnx, fny, fnz, t, hx, hy, hz)) return;
    float ex[3] = {t.x1 - t.x0, t.x2 - t.x1, t.x0 - t.x2};
    float ey[3] = {t.y1 - t.y0, t.y2 - t.y1, t.y0 - t.y2};
    float ez[3] = {t.z1 - t.z0, t.z2 - t.z1, t.z0 - t.z2};
    for (int e = 0; e < 3; e++) {
        if (!__nexa_phys_sat_axis(0.f, -ez[e], ey[e], t, hx, hy, hz)) return;
        if (!__nexa_phys_sat_axis(ez[e], 0.f, -ex[e], t, hx, hy, hz)) return;
        if (!__nexa_phys_sat_axis(-ey[e], ex[e], 0.f, t, hx, hy, hz)) return;
    }
    float dist = -(t.x0 * fnx + t.y0 * fny + t.z0 * fnz);
    if (dist < 0.f) { fnx = -fnx; fny = -fny; fnz = -fnz; dist = -dist; }
    float rad = fabsf(fnx) * hx + fabsf(fny) * hy + fabsf(fnz) * hz;
    float pen = rad - dist;
    if (pen < 0.f) return;
    if (*hit && pen <= *best) return;
    *hit = 1; *best = pen; *nx = fnx; *ny = fny; *nz = fnz;
}
static int __nexa_phys_bind(const __nexa_phys_body& b, const __nexa_G3Model** m, float* s, float* half) {
    if (!__nexa_phys_model_ok(b.model)) return 0;
    *m = &__nexa_g3_models[(size_t)b.model];
    *s = b.scl * (*m)->inv;
    *half = fabsf(b.scl) * 0.5f;
    return 1;
}
static int __nexa_phys_mesh_hit(int mesh, int other, float* nx, float* ny, float* nz, float* pen) {
    const __nexa_phys_body& m = __nexa_phys_bodies[mesh];
    const __nexa_phys_body& o = __nexa_phys_bodies[other];
    const __nexa_G3Model* model = 0;
    float s = 0.f, half = 0.f;
    if (!__nexa_phys_bind(m, &model, &s, &half)) return 0;
    int n = (int)(model->p.size() / 9);
    int hit = 0;
    float best = 0.f;
    if (o.kind == 1) {
        if (!__nexa_phys_near_cube(o.x, o.y, o.z, m.x, m.y, m.z, half, o.r)) return 0;
        for (int ti = 0; ti < n; ti++) {
            __nexa_phys_tri t;
            __nexa_phys_load_tri(*model, s, m.x, m.y, m.z, ti, &t);
            __nexa_phys_sphere_one(o.x, o.y, o.z, o.r, t, &hit, &best, nx, ny, nz);
        }
    } else if (o.kind == 3) {
        float x1, y1, z1, x2, y2, z2;
        __nexa_phys_ends(o, &x1, &y1, &z1, &x2, &y2, &z2);
        float minx = x1 < x2 ? x1 : x2, maxx = x1 > x2 ? x1 : x2;
        float miny = y1 < y2 ? y1 : y2, maxy = y1 > y2 ? y1 : y2;
        float minz = z1 < z2 ? z1 : z2, maxz = z1 > z2 ? z1 : z2;
        if (maxx < m.x - half - o.r || minx > m.x + half + o.r) return 0;
        if (maxy < m.y - half - o.r || miny > m.y + half + o.r) return 0;
        if (maxz < m.z - half - o.r || minz > m.z + half + o.r) return 0;
        for (int ti = 0; ti < n; ti++) {
            __nexa_phys_tri t;
            __nexa_phys_load_tri(*model, s, m.x, m.y, m.z, ti, &t);
            __nexa_phys_capsule_one(x1, y1, z1, x2, y2, z2, o.r, t, &hit, &best, nx, ny, nz);
        }
    } else if (o.kind == 2) {
        float hx = fabsf(o.w) * 0.5f, hy = fabsf(o.h) * 0.5f, hz = fabsf(o.d) * 0.5f;
        if (!__nexa_phys_near_cube(o.x, o.y, o.z, m.x, m.y, m.z, half, hx > hy ? (hx > hz ? hx : hz) : (hy > hz ? hy : hz))) return 0;
        for (int ti = 0; ti < n; ti++) {
            __nexa_phys_tri t;
            __nexa_phys_load_tri(*model, s, m.x, m.y, m.z, ti, &t);
            __nexa_phys_box_one(t, o.x, o.y, o.z, hx, hy, hz, &hit, &best, nx, ny, nz);
        }
    }
    if (!hit) return 0;
    *pen = best;
    return 1;
}
static int __nexa_phys_mesh_aabb(const __nexa_phys_body& b, float* cx, float* cy, float* cz, float* w, float* h, float* d) {
    const __nexa_G3Model* model = 0;
    float s = 0.f, half = 0.f;
    if (!__nexa_phys_bind(b, &model, &s, &half)) return 0;
    float minx = 1e30f, miny = 1e30f, minz = 1e30f, maxx = -1e30f, maxy = -1e30f, maxz = -1e30f;
    int nv = (int)(model->p.size() / 3);
    for (int i = 0; i < nv; i++) {
        float x, y, z;
        __nexa_phys_tri_vert(*model, s, b.x, b.y, b.z, i * 3, &x, &y, &z);
        if (x < minx) minx = x; if (x > maxx) maxx = x;
        if (y < miny) miny = y; if (y > maxy) maxy = y;
        if (z < minz) minz = z; if (z > maxz) maxz = z;
    }
    *cx = (minx + maxx) * 0.5f; *cy = (miny + maxy) * 0.5f; *cz = (minz + maxz) * 0.5f;
    *w = maxx - minx; *h = maxy - miny; *d = maxz - minz;
    return 1;
}
static int __nexa_phys_mesh_mesh(int i, int j, float* nx, float* ny, float* nz, float* pen) {
    float ax, ay, az, aw, ah, ad, bx, by, bz, bw, bh, bd;
    if (!__nexa_phys_mesh_aabb(__nexa_phys_bodies[i], &ax, &ay, &az, &aw, &ah, &ad)) return 0;
    if (!__nexa_phys_mesh_aabb(__nexa_phys_bodies[j], &bx, &by, &bz, &bw, &bh, &bd)) return 0;
    return __nexa_phys_box_box(ax, ay, az, aw, ah, ad, bx, by, bz, bw, bh, bd, nx, ny, nz, pen);
}
static void __nexa_phys_face_toi(float ox, float oy, float oz, float dx, float dy, float dz, float rad,
    const __nexa_phys_tri& t, float* best) {
    float e1x = t.x1 - t.x0, e1y = t.y1 - t.y0, e1z = t.z1 - t.z0;
    float e2x = t.x2 - t.x0, e2y = t.y2 - t.y0, e2z = t.z2 - t.z0;
    float nx = e1y * e2z - e1z * e2y, ny = e1z * e2x - e1x * e2z, nz = e1x * e2y - e1y * e2x;
    float nl = sqrtf(nx * nx + ny * ny + nz * nz);
    if (nl < 1e-8f) return;
    nx /= nl; ny /= nl; nz /= nl;
    float dist = (ox - t.x0) * nx + (oy - t.y0) * ny + (oz - t.z0) * nz;
    if (dist < 0.f) { nx = -nx; ny = -ny; nz = -nz; dist = -dist; }
    float denom = dx * nx + dy * ny + dz * nz;
    if (denom >= -1e-8f) return;
    float tt;
    float qx, qy, qz;
    if (dist >= rad) {
        tt = (rad - dist) / denom;
        if (tt < 0.f || tt > 1.f || tt >= *best) return;
        qx = ox + dx * tt - nx * rad;
        qy = oy + dy * tt - ny * rad;
        qz = oz + dz * tt - nz * rad;
    } else {
        tt = 0.f;
        if (tt >= *best) return;
        qx = ox - nx * dist; qy = oy - ny * dist; qz = oz - nz * dist;
    }
    if (!__nexa_phys_in_tri(qx, qy, qz, t)) return;
    *best = tt;
}
static void __nexa_phys_vert_toi(float ox, float oy, float oz, float dx, float dy, float dz, float rad,
    float x, float y, float z, float* best) {
    float vx = ox - x, vy = oy - y, vz = oz - z;
    float d2 = vx * vx + vy * vy + vz * vz;
    if (d2 <= rad * rad) {
        if (vx * dx + vy * dy + vz * dz < 0.f && 0.f < *best) *best = 0.f;
        return;
    }
    double t = __nexa_phys_ray_sphere_t(ox, oy, oz, dx, dy, dz, x, y, z, rad, 0);
    if (t >= 0.0 && t <= 1.0 && (float)t < *best) *best = (float)t;
}
static void __nexa_phys_edge_toi(float ox, float oy, float oz, float dx, float dy, float dz, float rad,
    float ax, float ay, float az, float bx, float by, float bz, float* best) {
    float ex = bx - ax, ey = by - ay, ez = bz - az;
    float el = ex * ex + ey * ey + ez * ez;
    if (el < 1e-12f) return;
    float wx = ox - ax, wy = oy - ay, wz = oz - az;
    float dE = dx * ex + dy * ey + dz * ez;
    float wE = wx * ex + wy * ey + wz * ez;
    float inv = 1.f / el;
    float drx = dx - ex * (dE * inv), dry = dy - ey * (dE * inv), drz = dz - ez * (dE * inv);
    float wrx = wx - ex * (wE * inv), wry = wy - ey * (wE * inv), wrz = wz - ez * (wE * inv);
    float A = drx * drx + dry * dry + drz * drz;
    float B = 2.f * (wrx * drx + wry * dry + wrz * drz);
    float C = wrx * wrx + wry * wry + wrz * wrz - rad * rad;
    float tt = -1.f;
    if (A < 1e-12f) {
        if (C < 0.f && B < 0.f) tt = 0.f;
    } else {
        float disc = B * B - 4.f * A * C;
        if (disc < 0.f) return;
        if (C < 0.f) { if (B < 0.f) tt = 0.f; }
        else {
            float t0 = (-B - sqrtf(disc)) / (2.f * A);
            if (t0 >= 0.f && t0 <= 1.f) tt = t0;
        }
    }
    if (tt < 0.f || tt >= *best) return;
    float u = (wE + tt * dE) * inv;
    if (u < 0.f || u > 1.f) return;
    *best = tt;
}
static int __nexa_phys_sphere_mesh_toi(float ox, float oy, float oz, float dx, float dy, float dz, float r, int mesh, float* tout) {
    const __nexa_phys_body& b = __nexa_phys_bodies[mesh];
    const __nexa_G3Model* model = 0;
    float s = 0.f, half = 0.f;
    if (!__nexa_phys_bind(b, &model, &s, &half)) return 0;
    float best = 2.f;
    int n = (int)(model->p.size() / 9);
    for (int ti = 0; ti < n; ti++) {
        __nexa_phys_tri t;
        __nexa_phys_load_tri(*model, s, b.x, b.y, b.z, ti, &t);
        __nexa_phys_face_toi(ox, oy, oz, dx, dy, dz, r, t, &best);
        __nexa_phys_vert_toi(ox, oy, oz, dx, dy, dz, r, t.x0, t.y0, t.z0, &best);
        __nexa_phys_vert_toi(ox, oy, oz, dx, dy, dz, r, t.x1, t.y1, t.z1, &best);
        __nexa_phys_vert_toi(ox, oy, oz, dx, dy, dz, r, t.x2, t.y2, t.z2, &best);
        __nexa_phys_edge_toi(ox, oy, oz, dx, dy, dz, r, t.x0, t.y0, t.z0, t.x1, t.y1, t.z1, &best);
        __nexa_phys_edge_toi(ox, oy, oz, dx, dy, dz, r, t.x1, t.y1, t.z1, t.x2, t.y2, t.z2, &best);
        __nexa_phys_edge_toi(ox, oy, oz, dx, dy, dz, r, t.x2, t.y2, t.z2, t.x0, t.y0, t.z0, &best);
    }
    if (best > 1.f) return 0;
    *tout = best;
    return 1;
}
static int __nexa_phys_mesh(double model, double x, double y, double z, double scale) {
    int id = (int)model;
    if (!__nexa_phys_model_ok(id)) return 0;
    int body = __nexa_phys_alloc(4);
    if (!body) return 0;
    __nexa_phys_body& b = __nexa_phys_bodies[body];
    b.x = (float)x; b.y = (float)y; b.z = (float)z;
    b.model = id;
    b.scl = (float)scale;
    return body;
}
)MESH";
    return out;
}

}  // namespace nexa
