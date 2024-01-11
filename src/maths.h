#ifndef MATHS_H
#define MATHS_H

#include "platform.h"
#include "core.h"

extern "C" double fabs( double arg ) NOTHROW;
extern "C" double floor (double x) NOTHROW;
extern "C" float floorf(float x) NOTHROW;
extern "C" float powf( float base, float exponent ) NOTHROW;
extern "C" double ceil (double x) NOTHROW;
extern "C" float ceilf (float x) NOTHROW;
extern "C" float fabsf( float arg ) NOTHROW;
extern "C" float cosf( float arg ) NOTHROW;
extern "C" float tanf( float arg ) NOTHROW;
extern "C" float sinf( float arg ) NOTHROW;
extern "C" double sin( double arg ) NOTHROW;
extern "C" float sqrtf( float arg ) NOTHROW;
extern "C" double sqrt( double arg ) NOTHROW;
extern "C" float copysignf(float x, float y) NOTHROW;
extern "C" float fmodf( float x, float y ) NOTHROW;

#ifndef M_BOUNDS_CHECK
#define M_BOUNDS_CHECK 0
#endif

#if M_BOUNDS_CHECK
#define M_SUBSCRIPT_OPS(T, values)\
    constexpr T& operator[](i32 i) { ASSERT((i) < ARRAY_COUNT((values))); return values[i]; }\
    constexpr const T& operator[](i32 i) const { ASSERT((i) < ARRAY_COUNT((values))); return values[i]; }
#else
#define M_SUBSCRIPT_OPS(T, values)\
    constexpr T& operator[](i32 i) { return values[i]; }\
    constexpr const T& operator[](i32 i) const { return values[i]; }
#endif

#define M_ADD_OPS(R, T_l, T_r)\
    R operator+(T_l lhs,   T_l rhs);\
    R operator+=(T_l &lhs, T_l rhs)\

#define M_SUB_OPS(R, T_l, T_r)\
    R operator-(T_l lhs,   T_l rhs);\
    R operator-=(T_l &lhs, T_l rhs)

#define M_MUL_OPS(R, T_l, T_r)\
    R operator*(T_l lhs,   T_r rhs);\
    R operator*=(T_l &lhs, T_r rhs)

#define M_DIV_OPS(R, T_l, T_r)\
    R operator/(T_l lhs,   T_r rhs);\
    R operator/=(T_l &lhs, T_r rhs)

#define M_CMP_OPS(T)\
    bool operator==(T lhs, T rhs);\
    bool operator!=(T lhs, T rhs)

#define M_UNARY_OPS(T)\
    T operator-(T v);\
    T operator+(T v)


#define M_VECTOR_OPS(T)\
    M_ADD_OPS(T, T, T);\
    M_SUB_OPS(T, T, T);\
    M_MUL_OPS(T, T, T);\
    M_MUL_OPS(T, T, f32);\
    M_MUL_OPS(T, f32, T);\
    M_DIV_OPS(T, T, T);\
    M_DIV_OPS(T, T, f32);\
    M_CMP_OPS(T);\
    M_UNARY_OPS(T)

#define M_MATRIX_OPS(T, V)\
    M_ADD_OPS(T, T, T);\
    M_SUB_OPS(T, T, T);\
    M_MUL_OPS(T, T, T);\
    M_MUL_OPS(V, T, V);\
    M_DIV_OPS(T, T, T)



f32 lerp(f32 a, f32 b, f32 t);
f32 round_to(f32 value, f32 multiple);


struct Vector2 {
    union {
        struct { f32 x, y; };
        struct { f32 w, h; };
        f32 data[2];
    };

    M_SUBSCRIPT_OPS(f32, data);
};

struct Vector2i {
    union {
        struct { i32 x, y; };
        struct { i32 w, h; };
        i32 data[2];
    };

    M_SUBSCRIPT_OPS(i32, data);
};

M_VECTOR_OPS(Vector2);

Vector2 abs(Vector2 v);
Vector2 lerp(Vector2 a, Vector2 b, f32 t);
Vector2 normalise(Vector2 v);
f32 length(Vector2 v);
f32 length_sq(Vector2 v);

f32 dot(Vector2 lhs, Vector2 rhs);

struct Vector3 {
    union {
        struct { f32 x, y, z; };
        struct { f32 r, g, b; };
        struct { f32 w, h, d; };
        struct { Vector2 xy; f32 _z; };
        struct { Vector2 rg; f32 _b; };
        struct { Vector2 wh; f32 _d; };
        f32 data[3];
    };

    M_SUBSCRIPT_OPS(f32, data);
};

M_VECTOR_OPS(Vector3);

Vector3 abs(Vector3 v);
Vector3 lerp(Vector3 a, Vector3 b, f32 t);
Vector3 normalise(Vector3 v);
f32 length(Vector3 v);
f32 length_sq(Vector3 v);

Vector3 cross(Vector3 lhs, Vector3 rhs);
f32 dot(Vector3 lhs, Vector3 rhs);

struct Vector4 {
    union {
        struct { f32 x, y, z, w; };
        struct { f32 r, g, b, a; };
        struct { Vector3 rgb; f32 _a; };
        struct { Vector3 xyz; f32 _w; };
        f32 data[4];
    };

    M_SUBSCRIPT_OPS(f32, data);
};

M_VECTOR_OPS(Vector4);

Vector4 abs(Vector4 v);
Vector4 lerp(Vector4 a, Vector4 b, f32 t);
Vector4 normalise(Vector4 v);
f32 length(Vector4 v);
f32 length_sq(Vector4 v);

Vector4 cross(Vector4 lhs, Vector4 rhs);
f32 dot(Vector4 lhs, Vector4 rhs);

struct Matrix3 {
    union {
        struct { Vector3 col0, col1, col2; };
        Vector3 columns[3];
        f32 data[9];
    };

    M_SUBSCRIPT_OPS(Vector3, columns);
};

M_MATRIX_OPS(Matrix3, Vector3);

Matrix3 matrix3_identity();

Matrix3 inverse(Matrix3 m);
Matrix3 orthographic3(f32 left, f32 right, f32 bottom, f32 top, f32 horiz_ratio);
Matrix3 transpose(Matrix3 m);

union Matrix4 {
    Vector4 columns[4];
    f32 data[12];

    M_SUBSCRIPT_OPS(Vector4, columns);
};

M_MATRIX_OPS(Matrix4, Vector4);

Matrix4 matrix4_identity();

Matrix4 inverse(Matrix4 m);
Matrix4 orthographic3(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far, f32 horiz_ratio);
Matrix4 transpose(Matrix4 m);


struct Rect {
    Vector2 tl;
    Vector2 br;
    constexpr Vector2 size() const { return br - tl; }
};

bool operator==(const Rect &lhs, const Rect &rhs);

struct Contact {
    Vector2 point;
    Vector2 normal;
    f32 depth;

    operator bool() const { return depth != f32_MAX; }
};

constexpr Contact INVALID_CONTACT = { .depth = f32_MAX };

struct AABB2 {
    Vector2 position;
    Vector2 extents;
};

struct Triangle2 {
    Vector2 p0, p1, p2;
};

struct Line2 {
    Vector2 p0, p1;
};

struct XORShift128 {
    u32 state[4];
};



u32 rand_u32(XORShift128 *series);
f32 rand_f32(XORShift128 *series);
f32 rand_f32(XORShift128 *series, f32 min, f32 max);


u32 bgra_pack(f32 r, f32 g, f32 b, f32 a);
u32 rgb_pack(Vector3 c);
u32 bgr_pack(Vector3 c);
Vector3 bgr_unpack(u32 argb);
Vector4 bgra_unpack(u32 argb);
Vector3 rgb_from_hsl(f32 h, f32 s, f32 l);
Vector3 rgb_mul(Vector3 rgb, f32 v);


bool point_in_aabb(Vector2 p, Vector2 aabb_pos, Vector2 aabb_half_size, f32 epsilon = 0.0f);

#endif // MATHS_H
