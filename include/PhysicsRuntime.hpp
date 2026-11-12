#pragma once

// std/physics. One world, fixed storage, no heap on the step.
//
// Queries (overlap and ray) are pure. Bodies are spheres, axis-aligned boxes
// and capsules that translate and do not rotate. A capsule is the same shape
// gfx3d.capsule draws: a radius around the segment between two points. Up to
// 256 bodies. A program that never calls physics carries none of this.

#include <string>

namespace nexa {

inline std::string physicsRuntimeCpp(bool query, bool body) {
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
};
static __nexa_phys_body __nexa_phys_bodies[257];
static float __nexa_phys_gx = 0.f, __nexa_phys_gy = -9.81f, __nexa_phys_gz = 0.f;
static int __nexa_phys_alive(int id) {
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
static int __nexa_phys_pair(int i, int j, float* nx, float* ny, float* nz, float* pen) {
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
    return 0;
}
static int __nexa_phys_step(double dtin) {
    float dt = (float)dtin;
    if (dt <= 0.f) return 0;
    if (dt > 0.05f) dt = 0.05f;
    unsigned char touch[257];
    for (int i = 1; i <= 256; i++) touch[i] = 0;
    for (int i = 1; i <= 256; i++) {
        __nexa_phys_body& b = __nexa_phys_bodies[i];
        if (b.kind == 0 || b.inv == 0.f) continue;
        b.vx += __nexa_phys_gx * dt;
        b.vy += __nexa_phys_gy * dt;
        b.vz += __nexa_phys_gz * dt;
        b.x += b.vx * dt;
        b.y += b.vy * dt;
        b.z += b.vz * dt;
    }
    for (int iter = 0; iter < 4; iter++) {
        for (int i = 1; i <= 256; i++) {
            if (__nexa_phys_bodies[i].kind == 0) continue;
            for (int j = i + 1; j <= 256; j++) {
                if (__nexa_phys_bodies[j].kind == 0) continue;
                __nexa_phys_body& a = __nexa_phys_bodies[i];
                __nexa_phys_body& b = __nexa_phys_bodies[j];
                if (a.inv == 0.f && b.inv == 0.f) continue;
                float nx, ny, nz, pen;
                if (!__nexa_phys_pair(i, j, &nx, &ny, &nz, &pen)) continue;
                float inv = a.inv + b.inv;
                if (inv <= 0.f) continue;
                float slop = 0.001f;
                if (pen > slop) {
                    float corr = (pen - slop) * 0.8f;
                    a.x += nx * corr * (a.inv / inv);
                    a.y += ny * corr * (a.inv / inv);
                    a.z += nz * corr * (a.inv / inv);
                    b.x -= nx * corr * (b.inv / inv);
                    b.y -= ny * corr * (b.inv / inv);
                    b.z -= nz * corr * (b.inv / inv);
                }
                float rvx = a.vx - b.vx, rvy = a.vy - b.vy, rvz = a.vz - b.vz;
                float rel = rvx * nx + rvy * ny + rvz * nz;
                if (rel < 0.f) {
                    float e = a.bounce > b.bounce ? a.bounce : b.bounce;
                    float jn = -(1.f + e) * rel / inv;
                    a.vx += nx * jn * a.inv; a.vy += ny * jn * a.inv; a.vz += nz * jn * a.inv;
                    b.vx -= nx * jn * b.inv; b.vy -= ny * jn * b.inv; b.vz -= nz * jn * b.inv;
                    rvx = a.vx - b.vx; rvy = a.vy - b.vy; rvz = a.vz - b.vz;
                    rel = rvx * nx + rvy * ny + rvz * nz;
                    float tx = rvx - rel * nx, ty = rvy - rel * ny, tz = rvz - rel * nz;
                    float tl = __nexa_phys_len(tx, ty, tz);
                    if (tl > 1e-6f) {
                        float mag = tl / inv;
                        float maxf = 0.6f * jn;
                        if (mag > maxf) mag = maxf;
                        float itx = tx / tl, ity = ty / tl, itz = tz / tl;
                        a.vx -= itx * mag * a.inv; a.vy -= ity * mag * a.inv; a.vz -= itz * mag * a.inv;
                        b.vx += itx * mag * b.inv; b.vy += ity * mag * b.inv; b.vz += itz * mag * b.inv;
                    }
                }
                touch[i] = touch[j] = 1;
            }
        }
    }
    for (int i = 1; i <= 256; i++) {
        __nexa_phys_body& b = __nexa_phys_bodies[i];
        if (b.kind == 0 || b.inv == 0.f || !touch[i]) continue;
        float sp = b.vx * b.vx + b.vy * b.vy + b.vz * b.vz;
        if (sp < 0.0025f) b.vx = b.vy = b.vz = 0.f;
    }
    return 1;
}
)PHYS";
    }
    return out;
}

}  // namespace nexa
