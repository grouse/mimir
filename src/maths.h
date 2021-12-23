#ifndef MATHS_H
#define MATHS_H

#include "platform.h"

extern "C" double fabs( double arg );
extern "C" double floor (double x);
extern "C" float powf( float base, float exponent );
extern "C" double ceil (double x);
extern "C" float fabsf( float arg );
extern "C" float cosf( float arg );
extern "C" float tanf( float arg );
extern "C" float sinf( float arg );
extern "C" double sin( double arg );
extern "C" float sqrtf( float arg );
extern "C" double sqrt( double arg );
extern "C" float cosf( float arg );
extern "C" float copysignf(float x, float y);
extern "C" float fmodf( float x, float y );

union Vector2 {
    struct { f32 x, y; };
    f32 data[2];

    constexpr f32& operator[](i32 i)  { return data[i]; }
};

union Vector3 {
    struct { f32 x, y, z; };
    struct { f32 r, g, b; };
    struct { Vector2 xy; f32 _z; };
    struct { Vector2 rg; f32 _b; };
    f32 data[3];

    constexpr f32& operator[](i32 i) { return data[i]; }
};

union Vector4 {
    struct { f32 x, y, z, w; };
    struct { f32 r, g, b, a; };
    struct { Vector3 rgb; f32 _a; };
    struct { Vector3 xyz; f32 _w; };
    f32 data[4];

    constexpr f32& operator[](i32 i) { return data[i]; }
};

union Matrix3 {
    Vector3 columns[3];
    f32 data[9];

    constexpr Vector3& operator[](i32 i) { return columns[i]; }
};

union Matrix4 {
    Vector4 columns[4];
    f32 data[12];

    constexpr Vector4& operator[](i32 i) { return columns[i]; }
};

struct Rect {
    Vector2 pos;
    Vector2 size;
};

struct AABB {
    Vector2 position;
    Vector2 half_size;
};

struct Triangle {
    Vector2 p0, p1, p2;
};

struct Line {
    Vector2 p0, p1;
};

struct XORShift128 {
    u32 state[4];
};

u32 rand_u32(XORShift128 *series);
f32 rand_f32(XORShift128 *series);
f32 rand_f32(XORShift128 *series, f32 min, f32 max);

f32 dot(Vector2 lhs, Vector2 rhs);
f32 dot(Vector3 lhs, Vector3 rhs);
f32 dot(Vector4 lhs, Vector4 rhs);
Vector3 cross(Vector3 lhs, Vector3 rhs);
Vector3 lerp(Vector3 a, Vector3 b, f32 t);

Matrix4 matrix4_identity();

Vector2 operator*(Vector2 lhs, f32 rhs);
Vector2 operator*(f32 lhs, Vector2 rhs);
Vector2 abs(Vector2 v);
Vector2 operator+(Vector2 lhs, Vector2 rhs);
Vector2 operator+(Vector2 lhs, f32 rhs);
Vector2 operator+=(Vector2 &lhs, Vector2 rhs);
Vector2 operator-=(Vector2 &lhs, Vector2 rhs);
Vector3 operator-=(Vector3 &lhs, Vector3 rhs);
Vector2 operator-(Vector2 lhs, Vector2 rhs);
Vector2 operator-(Vector2 v);
Vector3 operator-(Vector3 v);
bool operator==(Vector2 lhs, Vector2 rhs);
bool operator!=(Vector2 lhs, Vector2 rhs);
Vector2 normalise(Vector2 v);
Vector3 v3(Vector2 xy, f32 z);
Matrix3 matrix3_identity();
Matrix3 transpose(Matrix3 m);
Vector3 operator*(Matrix3 m, Vector3 v);
Vector4 operator*(Matrix4 m, Vector4 v);
Vector3 operator+(Vector3 lhs, Vector3 rhs);

Vector3 operator+(f32 lhs, Vector3 rhs);
Vector3 operator*(f32 lhs, Vector3 rhs);
Vector3 hadamard(Vector3 lhs, Vector3 rhs);
Vector2 hadamard(Vector2 lhs, Vector2 rhs);
f32 dot(Vector2 lhs, Vector2 rhs);
f32 dot(Vector3 lhs, Vector3 rhs);
f32 dot(Vector4 lhs, Vector4 rhs);
Vector3 cross(Vector3 lhs, Vector3 rhs);
f32 length_sq(Vector2 a, Vector2 b);
f32 length_sq(Vector2 v);
f32 length(Vector2 a, Vector2 b);
bool point_in_aabb(Vector2 p, Vector2 aabb_pos, Vector2 aabb_half_size, f32 epsilon = 0.000001f);
bool point_in_circle(Vector2 p, Vector2 c, f32 radius_sq);
Vector2 point_clamp_aabb(Vector2 p, Vector2 aabb_pos, Vector2 aabb_half_size);
Vector2 point_clamp_aabb_circle(Vector2 p, Vector2 aabb_pos, Vector2 aabb_half_size, Vector2 ray_pos, f32 ray_radius);
bool point_in_triangle(Vector2 p, Vector2 p0, Vector2 p1, Vector2 p2);
bool point_in_rect(Vector2 p, Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3);
bool point_in_rect(Vector2 p, Vector2 center, Vector2 size);
bool line_intersect_line(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, Vector2 *intersect_point);
bool line_intersect_aabb(Vector2 l_p0, Vector2 l_p1, Vector2 pos, Vector2 half_size, Vector2 *out_intersect_point, Vector2 *out_normal);

Vector2 absv(Vector2 v);
f32 round_to(f32 value, f32 multiple);

#endif // MATHS_H