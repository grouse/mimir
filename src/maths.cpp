#include "maths.h"
#include "core.h"

bool operator==(Vector3 lhs, Vector3 rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

Vector2 operator*(Vector2 lhs, f32 rhs)
{
    Vector2 r;
    r.x = lhs.x*rhs;
    r.y = lhs.y*rhs;
    return r;
}

Vector2 operator*(f32 lhs, Vector2 rhs)
{
    Vector2 r;
    r.x = lhs * rhs.x;
    r.y = lhs * rhs.y;
    return r;
}

Vector2 abs(Vector2 v)
{
    // TODO(jesper): cmath dependency
    Vector2 r;
    r.x = fabsf(v.x);
    r.y = fabsf(v.y);
    return r;
}

Vector2 operator+(Vector2 lhs, Vector2 rhs)
{
    Vector2 r;
    r.x = lhs.x + rhs.x;
    r.y = lhs.y + rhs.y;
    return r;
}

Vector2 operator+(Vector2 lhs, f32 rhs)
{
    Vector2 r;
    r.x = lhs.x + rhs;
    r.y = lhs.y + rhs;
    return r;
}

Vector2 operator+=(Vector2 &lhs, Vector2 rhs)
{
    lhs.x += rhs.x;
    lhs.y += rhs.y;
    return lhs;
}

Vector2 operator-=(Vector2 &lhs, Vector2 rhs)
{
    lhs.x -= rhs.x;
    lhs.y -= rhs.y;
    return lhs;
}

Vector3 operator-=(Vector3 &lhs, Vector3 rhs)
{
    lhs.x -= rhs.x;
    lhs.y -= rhs.y;
    lhs.z -= rhs.z;
    return lhs;
}

Vector2 operator-(Vector2 lhs, Vector2 rhs)
{
    Vector2 r;
    r.x = lhs.x - rhs.x;
    r.y = lhs.y - rhs.y;
    return r;
}

Vector2 operator-(Vector2 v)
{
    return { -v.x, -v.y };
}

Vector3 operator-(Vector3 v)
{
    return { -v.x, -v.y, -v.z };
}


bool operator==(Vector2 lhs, Vector2 rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool operator!=(Vector2 lhs, Vector2 rhs)
{
    return lhs.x != rhs.x || lhs.y != rhs.y;
}

Vector2 normalise(Vector2 v)
{
    f32 length = sqrtf(v.x*v.x + v.y*v.y);
    Vector2 r;
    r.x = v.x / length;
    r.y = v.y / length;
    return r;
}

Vector3 v3(Vector2 xy, f32 z)
{
    return Vector3{ xy.x, xy.y, z };
}

Matrix3 matrix3_identity()
{
    Matrix3 r{};
    r.columns[0].data[0] = 1.0f;
    r.columns[1].data[1] = 1.0f;
    r.columns[2].data[2] = 1.0f;
    return r;
}

Matrix3 transpose(Matrix3 m)
{
    Matrix3 r;
    r[0][0] = m[0][0];
    r[0][1] = m[1][0];
    r[0][2] = m[2][0];
    r[0][3] = m[3][0];

    r[1][0] = m[0][1];
    r[1][1] = m[1][1];
    r[1][2] = m[2][1];
    r[1][3] = m[3][1];

    r[2][0] = m[0][2];
    r[2][1] = m[1][2];
    r[2][2] = m[2][2];
    r[2][3] = m[3][2];

    r[3][0] = m[0][3];
    r[3][1] = m[1][3];
    r[3][2] = m[2][3];
    r[3][3] = m[3][3];
    return r;
}

Matrix4 matrix4_identity()
{
    Matrix4 r{};
    r.columns[0].data[0] = 1.0f;
    r.columns[1].data[1] = 1.0f;
    r.columns[2].data[2] = 1.0f;
    r.columns[3].data[3] = 1.0f;
    return r;
}


Vector3 operator*(Matrix3 m, Vector3 v)
{
    Vector3 r;
    r[0] = dot(m[0], v);
    r[1] = dot(m[1], v);
    r[2] = dot(m[2], v);
    return r;
}

Vector4 operator*(Matrix4 m, Vector4 v)
{
    Vector4 r;
    r[0] = dot(m[0], v);
    r[1] = dot(m[1], v);
    r[2] = dot(m[2], v);
    r[3] = dot(m[3], v);
    return r;
}

Vector3 operator+(Vector3 lhs, Vector3 rhs)
{
    Vector3 r;
    r.x = lhs.x + rhs.x;
    r.y = lhs.y + rhs.y;
    r.z = lhs.z + rhs.z;
    return r;
}

Vector3 operator+(f32 lhs, Vector3 rhs)
{
    Vector3 r;
    r.x = lhs + rhs.x;
    r.y = lhs + rhs.y;
    r.z = lhs + rhs.z;
    return r;
}

Vector3 operator*(f32 lhs, Vector3 rhs)
{
    Vector3 r;
    r.x = lhs * rhs.x;
    r.y = lhs * rhs.y;
    r.z = lhs * rhs.z;
    return r;
}

Vector3 hadamard(Vector3 lhs, Vector3 rhs)
{
    Vector3 v;
    v.x = lhs.x * rhs.x;
    v.y = lhs.y * rhs.y;
    v.z = lhs.z * rhs.z;
    return v;
}

Vector2 hadamard(Vector2 lhs, Vector2 rhs)
{
    Vector2 v;
    v.x = lhs.x * rhs.x;
    v.y = lhs.y * rhs.y;
    return v;
}

f32 dot(Vector2 lhs, Vector2 rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

f32 dot(Vector3 lhs, Vector3 rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

f32 dot(Vector4 lhs, Vector4 rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
}

Vector3 cross(Vector3 lhs, Vector3 rhs)
{
    Vector3 r;
    r.x = lhs.y * rhs.z - lhs.z * rhs.y;
    r.y = lhs.z * rhs.x - lhs.x * rhs.z;
    r.z = lhs.x * rhs.y - lhs.y * rhs.x;
    return r;
}


f32 length_sq(Vector2 a, Vector2 b)
{
    Vector2 p = b - a;
    return dot(p, p);
}

f32 length_sq(Vector2 v)
{
    return dot(v, v);
}

f32 length(Vector2 a, Vector2 b)
{
    Vector2 p = b - a;
    return sqrtf(dot(p, p));
}

bool point_in_aabb(Vector2 p, Vector2 aabb_pos, Vector2 aabb_half_size, f32 epsilon /*= 0.000001f*/)
{
    return 
        p.x <= aabb_pos.x + aabb_half_size.x + epsilon &&
        p.x >= aabb_pos.x - aabb_half_size.x - epsilon &&
        p.y <= aabb_pos.y + aabb_half_size.y + epsilon &&
        p.y >= aabb_pos.y - aabb_half_size.y - epsilon;
}

bool point_in_circle(Vector2 p, Vector2 c, f32 radius_sq)
{
    Vector2 pc = p - c;
    return dot(pc, pc) <= radius_sq;
}

Vector2 point_clamp_aabb(Vector2 p, Vector2 aabb_pos, Vector2 aabb_half_size)
{
    Vector2 r;
    r.x = CLAMP(p.x, aabb_pos.x - aabb_half_size.x, aabb_pos.x + aabb_half_size.x);
    r.y = CLAMP(p.y, aabb_pos.y - aabb_half_size.y, aabb_pos.y + aabb_half_size.y);
    return r;
}


Vector2 point_clamp_aabb_circle(
    Vector2 p, 
    Vector2 aabb_pos, 
    Vector2 aabb_half_size, 
    Vector2 ray_pos, 
    f32 ray_radius)
{
    Vector2 r = point_clamp_aabb(p, aabb_pos, aabb_half_size);
    r = ray_pos + ray_radius * normalise(r - ray_pos);
    r = point_clamp_aabb(p, aabb_pos, aabb_half_size);

    return r;
}

bool point_in_triangle(Vector2 p, Vector2 p0, Vector2 p1, Vector2 p2)
{
    f32 area = 0.5f * (-p1.y * p2.x + p0.y * (-p1.x + p2.x) + p0.x * (p1.y - p2.y) + p1.x * p2.y);
    f32 sign = area < 0.0f ? -1.0f: 1.0f;
    f32 s = (p0.y * p2.x - p0.x * p2.y + (p2.y - p0.y) * p.x + (p0.x - p2.x) * p.y) * sign;
    f32 t = (p0.x * p1.y - p0.y * p1.x + (p0.y - p1.y) * p.x + (p1.x - p0.x) * p.y) * sign;
    return s > 0.0f && t > 0.0f && (s + t) < 2.0f * area * sign;
}

bool point_in_rect(Vector2 p, Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3)
{
    f32 left = MIN4(p0.x, p1.x, p2.x, p3.x);
    f32 right = MAX4(p0.x, p1.x, p2.x, p3.x);
    f32 top = MIN4(p0.y, p1.y, p2.y, p3.y);
    f32 bot = MAX4(p0.y, p1.y, p2.y, p3.y);
    
    if (p.x < left || p.x > right || p.y < top || p.y > bot) return false;
    return true;
}

bool point_in_rect(Vector2 p, Vector2 center, Vector2 size)
{
    Vector2 hs = 0.5f*size;
    Vector2 tl = center - hs;
    Vector2 br = center + hs;
    Vector2 tr{ br.x, tl.y };
    Vector2 bl{ tl.x, br.y };
    return point_in_rect(p, tl, tr, br, bl);
}

bool line_intersect_line(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector2 *intersect_point)
{
    Vector2 s1 = p1 - p0;
    Vector2 s2 = p3 - p2;

    Vector2 u = p0 - p2;

    f32 ip = 1.0f / (-s2.x * s1.y + s1.x * s2.y);

    f32 s = (-s1.y * u.x + s1.x * u.y) * ip;
    f32 t = ( s2.x * u.y - s2.y * u.x) * ip;

    if (s >= 0.0f && s <= 1.0f && t >= 0.0f && t <= 1.0f) {
        if (intersect_point) *intersect_point = p0 + (s1 * t);
        return true;
    }

    return false;
}

bool line_intersect_aabb(
    Vector2 l_p0, 
    Vector2 l_p1, 
    Vector2 pos, 
    Vector2 half_size,
    Vector2 *out_intersect_point,
    Vector2 *out_normal)
{
    f32 min_length_sq = f32_MAX;
    Vector2 intersect_point;
    Vector2 normal;
    
    using Line = Vector2[2];
    
    Line lines[] = {
        { pos.x + half_size.x, pos.y - half_size.y },
        { pos.x - half_size.x, pos.y - half_size.y },
        { pos.x - half_size.x, pos.y + half_size.y },
        { pos.x + half_size.x, pos.y + half_size.y }
    };

    for (i32 i = 0; i < ARRAY_COUNT(lines); i++) {
        Vector2 p0 = lines[i][0];
        Vector2 p1 = lines[i][1];

        Vector2 p;
        if (line_intersect_line(l_p0, l_p1, p0, p1, &p)) {
            f32 ll = length_sq(p, pos);
            if (ll < min_length_sq) {
                min_length_sq = ll;
                intersect_point = p;

                Vector2 p0p1 = p1 - p0;
                normal = { p0p1.y, -p0p1.x };
            }
        }
    }

    if (min_length_sq != f32_MAX) {
        if (out_intersect_point) *out_intersect_point = intersect_point;
        if (out_normal) *out_normal = normal;
        return true;
    }

    return false;
}


bool aabb_intersect_aabb(
    Vector2 pos_a, 
    Vector2 half_size_a, 
    Vector2 pos_b, 
    Vector2 half_size_b,
    Vector2 *out_delta,
    Vector2 *out_normal)
{
    Vector2 dv = pos_b - pos_a;
    Vector2 pv = half_size_b + half_size_a - abs(dv);
    
    if (pv.x <= 0.0f && pv.y <= 0.0f) return false;
    
    
    f32 sign;
    Vector2 delta;
    Vector2 normal;
    Vector2 pos;

    if (pv.x < pv.y) {
        sign = dv.x > 0.0f ? 1.0f : -1.0f;
        delta = { pv.x, 0.0f };
        normal = { sign, 0.0f };
        pos = { pos_a.x + half_size_a.x*sign, pos_a.y };
    } else {
        sign = dv.y > 0.0f ? 1.0f : -1.0f;
        delta = { 0.0f, pv.y };
        normal = { 0.0f, sign };
        pos = { pos_a.x, pos_a.y + half_size_a.y*sign };
    }
    
    if (out_delta) *out_delta = sign*delta;
    if (out_normal) *out_normal = normal;
    return true;
}

bool aabb_intersect_line_swept(
    Vector2 l_p0, 
    Vector2 l_p1, 
    Vector2 pos, 
    Vector2 half_size,
    Vector2 delta,
    Vector2 *out_intersect_point,
    Vector2 *out_normal)
{
    if (l_p0.y > l_p1.y) SWAP(l_p0, l_p1);
    
    Line ls0[] = {
        { 
            { l_p0.x - half_size.x, l_p0.y - half_size.y }, 
            { l_p0.x + half_size.x, l_p0.y - half_size.y },
        },
        { 
            { l_p0.x + half_size.x, l_p0.y - half_size.y }, 
            { l_p1.x + half_size.x, l_p1.y - half_size.y },
        },
        { 
            { l_p1.x + half_size.x, l_p1.y - half_size.y }, 
            { l_p1.x + half_size.x, l_p1.y + half_size.y },
        },
        { 
            { l_p1.x + half_size.x, l_p1.y + half_size.y }, 
            { l_p1.x - half_size.x, l_p1.y + half_size.y },
        },
        { 
            { l_p1.x - half_size.x, l_p1.y + half_size.y }, 
            { l_p0.x - half_size.x, l_p0.y + half_size.y },
        },
        { 
            { l_p0.x - half_size.x, l_p0.y + half_size.y }, 
            { l_p0.x - half_size.x, l_p0.y - half_size.y },
        }
    };
    
    Line ls1[] = {
        { 
            { l_p0.x - half_size.x, l_p0.y - half_size.y }, 
            { l_p0.x + half_size.x, l_p0.y - half_size.y },
        },
        { 
            { l_p0.x + half_size.x, l_p0.y - half_size.y }, 
            { l_p0.x + half_size.x, l_p0.y + half_size.y },
        },
        { 
            { l_p0.x + half_size.x, l_p0.y + half_size.y },
            { l_p1.x + half_size.x, l_p1.y + half_size.y },
        },
        { 
            { l_p1.x + half_size.x, l_p1.y + half_size.y }, 
            { l_p1.x - half_size.x, l_p1.y + half_size.y },
        },
        { 
            { l_p1.x - half_size.x, l_p1.y + half_size.y }, 
            { l_p1.x - half_size.x, l_p1.y - half_size.y },
        },
        { 
            { l_p1.x - half_size.x, l_p1.y - half_size.y },
            { l_p0.x - half_size.x, l_p0.y - half_size.y },
        }
    };
    
    i32 line_count = l_p0.x < l_p1.x ? ARRAY_COUNT(ls0) : ARRAY_COUNT(ls1);
    Line *lines = l_p0.x < l_p1.x ? ls0 : ls1;

    f32 min_length_sq = f32_MAX;
    Vector2 intersect_point;
    Vector2 normal;

    for (i32 i = 0; i < line_count; i++) {
        Vector2 p0 = lines[i].p0;
        Vector2 p1 = lines[i].p1;

        Vector2 p;
        if (line_intersect_line(p0, p1, pos, pos + delta, &p)) {
            f32 ll = length_sq(p, pos);
            if (ll < min_length_sq) {
                min_length_sq = ll;
                intersect_point = p;

                Vector2 p0p1 = p1 - p0;
                normal = { p0p1.y, -p0p1.x };
            }
        }
    }

    if (min_length_sq != f32_MAX) {
        if (out_intersect_point) *out_intersect_point = intersect_point;
        if (out_normal) *out_normal = normal;
        return true;
    }

    return false;
}

bool aabb_intersect_aabb_swept(
    Vector2 pos_a, 
    Vector2 half_size_a, 
    Vector2 pos_b, 
    Vector2 half_size_b, 
    Vector2 delta_b,
    Vector2 *out_intersect_point,
    Vector2 *out_normal)
{
    Vector2 half_size = half_size_a + half_size_b;

    Vector2 minkowski_lines[][2] = {
        { // top
            { pos_a.x - half_size.x, pos_a.y - half_size.y }, 
            { pos_a.x + half_size.x, pos_a.y - half_size.y }, 
        },
        { // right
            { pos_a.x + half_size.x, pos_a.y - half_size.y }, 
            { pos_a.x + half_size.x, pos_a.y + half_size.y }
        },
        { // bottom
            { pos_a.x + half_size.x, pos_a.y + half_size.y }, 
            { pos_a.x - half_size.x, pos_a.y + half_size.y }
        },
        { // left
            { pos_a.x - half_size.x, pos_a.y + half_size.y }, 
            { pos_a.x - half_size.x, pos_a.y - half_size.y }
        }
    };

    f32 min_length_sq = f32_MAX;
    Vector2 intersect_point;
    Vector2 normal;
    for (i32 i = 0; i < ARRAY_COUNT(minkowski_lines); i++) {
        Vector2 p0 = minkowski_lines[i][0];
        Vector2 p1 = minkowski_lines[i][1];

        Vector2 p;
        if (line_intersect_line(p0, p1, pos_b, pos_b + delta_b, &p)) {
            f32 ll = length_sq(p, pos_b);
            if (ll < min_length_sq) {
                min_length_sq = ll;
                intersect_point = p;

                Vector2 p0p1 = p1-p0;
                normal = { p0p1.y, -p0p1.x };
            }
        }
    }

    if (min_length_sq != f32_MAX) {
        if (out_intersect_point) *out_intersect_point = intersect_point;
        if (out_normal) *out_normal = normal;
        return true;
    }
    return false;
}

Vector2 absv(Vector2 v)
{
    Vector2 r;
    r.x = fabsf(v.x);
    r.y = fabsf(v.y);
    return r;
}

f32 round_to(f32 value, f32 multiple)
{
    f32 hm = 0.5f*multiple;
    f32 sign = value < 0.0f ? -1.0f : 1.0f;
    f32 abs_value = fabsf(value);
    return sign*((abs_value+hm) - fmodf(abs_value+hm, multiple));
}

f32 lerp(f32 a, f32 b, f32 t)
{
    return (1.0f-t) * a + t*b;
}

Vector3 lerp(Vector3 a, Vector3 b, f32 t)
{
    Vector3 v;
    v.x = lerp(a.x, b.x, t);
    v.y = lerp(a.y, b.y, t);
    v.z = lerp(a.z, b.z, t);
    return v;
}

Vector3 operator*(Vector3 lhs, f32 rhs)
{
    return Vector3{ lhs.x*rhs, lhs.y*rhs, lhs.z*rhs };
}

Vector3 operator/(Vector3 lhs, f32 rhs)
{
    return Vector3{ lhs.x/rhs, lhs.y/rhs, lhs.z/rhs };
}

Vector3 operator-(Vector3 lhs, Vector3 rhs)
{
    return Vector3{ lhs.x-rhs.x, lhs.y-rhs.y, lhs.z-rhs.z };
}

Vector3 operator+=(Vector3 &lhs, Vector3 rhs)
{
    lhs.x += rhs.x;
    lhs.y += rhs.y;
    lhs.z += rhs.z;
    return lhs;
}

f32 length(Vector3 v)
{
    return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
}

Vector3 normalise(Vector3 v)
{
    Vector3 r;
    f32 l = length(v);
    r.x = v.x / l;
    r.y = v.y / l;
    r.z = v.z / l;
    return r;
}


u32 rand_u32(XORShift128 *series)
{
    u32 t = series->state[3];
    u32 s = series->state[0];
    series->state[3] = series->state[2];
    series->state[2] = series->state[1];
    series->state[1] = s;

    t ^= t << 11;
    t ^= t >> 8;
    series->state[0] = t ^ s ^ (s >> 19);
    return series->state[0];
}

f32 rand_f32(XORShift128 *series)
{
    u32 r = rand_u32(series);
    return (f32)(r >> 1) / (f32)(u32_MAX >> 1);
}

f32 rand_f32(XORShift128 *series, f32 min, f32 max)
{
    return min + (max-min)*rand_f32(series);
}

Vector3 rand_sphere(XORShift128 *series)
{
    Vector3 v;
    for (;;) {
        v.x = rand_f32(series, -1.0f, 1.0f);
        v.y = rand_f32(series, -1.0f, 1.0f);
        v.z = rand_f32(series, -1.0f, 1.0f);
        if (dot(v, v) < 1.0f) return v;
    }
}

Vector3 rand_color3(XORShift128 *series)
{
    Vector3 v;
    v.x = rand_f32(series);
    v.y = rand_f32(series);
    v.z = rand_f32(series);
    return v;
}

Vector3 rand_hemisphere(Vector3 normal, XORShift128 *series)
{
    Vector3 s = rand_sphere(series);
    if (dot(s, normal) > 0.0f) return s;
    return -s;
}

Vector3 rand_disc(XORShift128 *series)
{
    for (;;) {
        Vector3 v;
        v.x = rand_f32(series, -1.0f, 1.0f);
        v.y = rand_f32(series, -1.0f, 1.0f);
        v.z = 0.0f;
        if (dot(v, v) < 1.0) return v;
    }
}

f32 ray_hit_sphere(Vector3 ray_o, Vector3 ray_d, Vector3 sphere_p, f32 sphere_r)
{
    Vector3 oc = ray_o - sphere_p;
    f32 a = dot(ray_d, ray_d);
    f32 half_b = dot(oc, ray_d);
    f32 c = dot(oc, oc) - sphere_r*sphere_r;
    f32 disc = half_b*half_b - a*c;

    if (disc < 0.0f) return -1.0f;
    return (-half_b - sqrtf(disc)) / a;
}

u32 BGRA_pack(f32 r, f32 g, f32 b, f32 a)
{
    return u32(b*255.0) | u32(g*255.0f) << 8 | u32(r*255.0f) << 16 | u32(a*255.0f) << 24;
}

f32 sRGB_from_linear(f32 l)
{
    if (l > 1.0f) return 1.0f;
    else if (l < 0.0f) return 0.0f;
    return l > 0.0031308f ? 1.055f*powf(l, 1.0f/2.4f) - 0.055f : l*12.92f;
}

Vector3 sRGB_from_linear(Vector3 l)
{
    Vector3 s;
    s.r = sRGB_from_linear(l.r);
    s.g = sRGB_from_linear(l.g);
    s.b = sRGB_from_linear(l.b);
    return s;
}

Vector3 reflect(Vector3 v, Vector3 n)
{
    return v - 2.0f*dot(v, n)*n;
    
}

Vector3 refract(Vector3 v, Vector3 n, f32 etai_over_etat)
{
    f32 cos_theta = MIN(dot(-v, n), 1.0f);
    Vector3 d_perp = etai_over_etat * (v + cos_theta*n);
    Vector3 d_parallel = -sqrtf(fabs(1.0f-dot(d_perp, d_perp))) * n;
    return d_perp + d_parallel;
}

f32 reflectance(f32 cosine, f32 ref_idx)
{
    // Schlick's approximation
    f32 r0 = (1.0f-ref_idx) / (1.0f+ref_idx);
    r0 = r0*r0;
    return r0 + (1.0f-r0)*powf((1.0f-cosine), 5);
}

f32 radians_from_degrees(f32 theta)
{
    return theta * f32_PI / 180.0f;
}
                   

bool operator==(const Rect &lhs, const Rect &rhs)
{
    return lhs.pos == rhs.pos && lhs.size == rhs.size;
}


bool operator!=(const Rect &lhs, const Rect &rhs)
{
    return lhs.pos != rhs.pos || lhs.size != rhs.size;
}

Vector3 rgb_mul(Vector3 rgb, f32 v)
{
    return { 
        CLAMP(rgb.x * v, 0, 1), 
        CLAMP(rgb.y * v, 0, 1), 
        CLAMP(rgb.z * v, 0, 1) 
   };
}