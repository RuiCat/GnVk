#pragma once
#ifndef _GN_3DMATH_H_
#define _GN_3DMATH_H_
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 3D 数学库 (向量/矩阵/四元数)                 ║
// ║  配合 Vulkan 后端，矩阵采用列主序存储                     ║
// ╚══════════════════════════════════════════════════════════╝

#include"GN_2Dmath.h"   // 依赖 sVEC2, sVEC3 基础定义及 GN_API 宏

namespace GNg {

// ═══════════════════════════════════════════════════════════
//  sVEC4 — 4D 齐次向量
// ═══════════════════════════════════════════════════════════
struct sVEC4
{
    float x, y, z, w;

    inline sVEC4(){}
    inline sVEC4(float _x, float _y, float _z, float _w):x(_x), y(_y), z(_z), w(_w){}
    inline sVEC4(const sVEC3& v3, float _w=1.0f):x(v3.x), y(v3.y), z(v3.z), w(_w){}
    inline void Init(float _x, float _y, float _z, float _w){ x=_x; y=_y; z=_z; w=_w; }

    inline sVEC4& operator +=(const sVEC4& v){ x+=v.x; y+=v.y; z+=v.z; w+=v.w; return *this; }
    inline sVEC4& operator -=(const sVEC4& v){ x-=v.x; y-=v.y; z-=v.z; w-=v.w; return *this; }
    inline sVEC4& operator *=(float f){ x*=f; y*=f; z*=f; w*=f; return *this; }
    inline sVEC4& operator /=(float f){ x/=f; y/=f; z/=f; w/=f; return *this; }

    inline sVEC4 operator +(const sVEC4& v)const{ return sVEC4(x+v.x, y+v.y, z+v.z, w+v.w); }
    inline sVEC4 operator -(const sVEC4& v)const{ return sVEC4(x-v.x, y-v.y, z-v.z, w-v.w); }
    inline sVEC4 operator *(float f)const{ return sVEC4(x*f, y*f, z*f, w*f); }
    inline sVEC4 operator /(float f)const{ return sVEC4(x/f, y/f, z/f, w/f); }
    inline sVEC4 operator -()const{ return sVEC4(-x, -y, -z, -w); }

    inline bool operator ==(const sVEC4& v)const{ return x==v.x && y==v.y && z==v.z && w==v.w; }
    inline bool operator !=(const sVEC4& v)const{ return x!=v.x || y!=v.y || z!=v.z || w!=v.w; }

    inline operator float*(){ return (float*)this; }
    inline operator const float*()const{ return (const float*)this; }

    inline float Length()const{ return sqrtf(x*x + y*y + z*z + w*w); }
    inline float Dot(const sVEC4& v)const{ return x*v.x + y*v.y + z*v.z + w*v.w; }
    inline sVEC3 XYZ()const{ return sVEC3{x, y, z}; }
};

// ═══════════════════════════════════════════════════════════
//  sMAT44 — 4x4 矩阵 (列主序, 与 Vulkan/GLSL 内存布局一致)
// ═══════════════════════════════════════════════════════════
struct sMAT44
{
    float m[16];  // 列主序存储

    inline sMAT44(){}
    inline void Init(bool is_unit);  // 实现放在所有函数声明之后
    inline sMAT44(bool is_unit){ Init(is_unit); }

    // 逐元素构造 (列主序: 先列0, 再列1, 再列2, 再列3)
    inline sMAT44(
        float _00, float _10, float _20, float _30,
        float _01, float _11, float _21, float _31,
        float _02, float _12, float _22, float _32,
        float _03, float _13, float _23, float _33
    ){
        m[0]=_00; m[1]=_10; m[2]=_20; m[3]=_30;
        m[4]=_01; m[5]=_11; m[6]=_21; m[7]=_31;
        m[8]=_02; m[9]=_12; m[10]=_22; m[11]=_32;
        m[12]=_03; m[13]=_13; m[14]=_23; m[15]=_33;
    }

    inline operator float*(){ return m; }
    inline operator const float*()const{ return m; }
    inline float& Elem(int col, int row){ return m[col*4 + row]; }
    inline float  Elem(int col, int row)const{ return m[col*4 + row]; }
};

// ═══════════════════════════════════════════════════════════
//  sQUAT — 四元数
// ═══════════════════════════════════════════════════════════
struct sQUAT
{
    float x, y, z, w;

    inline sQUAT():x(0),y(0),z(0),w(1){}
    inline sQUAT(float _x, float _y, float _z, float _w):x(_x),y(_y),z(_z),w(_w){}
    inline void Init(float _x, float _y, float _z, float _w){ x=_x; y=_y; z=_z; w=_w; }

    inline sQUAT operator *(const sQUAT& q)const{
        return sQUAT(
            w*q.x + x*q.w + y*q.z - z*q.y,
            w*q.y - x*q.z + y*q.w + z*q.x,
            w*q.z + x*q.y - y*q.x + z*q.w,
            w*q.w - x*q.x - y*q.y - z*q.z
        );
    }
    inline sQUAT operator +(const sQUAT& q)const{ return sQUAT(x+q.x, y+q.y, z+q.z, w+q.w); }
    inline sQUAT operator *(float s)const{ return sQUAT(x*s, y*s, z*s, w*s); }
    inline sQUAT& operator *=(const sQUAT& q){ *this = *this * q; return *this; }

    inline float Length()const{ return sqrtf(x*x + y*y + z*z + w*w); }
    inline float Dot(const sQUAT& q)const{ return x*q.x + y*q.y + z*q.z + w*q.w; }
};

// ═══════════════════════════════════════════════════════════
//  全局默认实例
// ═══════════════════════════════════════════════════════════
extern sVEC4  Vector4D;
extern sMAT44 Matrix44;
extern sQUAT  Quaternion;

// ═══════════════════════════════════════════════════════════
//  3D 向量函数 (sVEC3 已在 GN_2Dmath.h 中定义)
// ═══════════════════════════════════════════════════════════
inline float Vec3Length(const sVEC3 *v){
    return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}
inline float Vec3LengthSq(const sVEC3 *v){
    return v->x * v->x + v->y * v->y + v->z * v->z;
}
GN_API void Vec3Normalize(sVEC3 *out, const sVEC3 *in);
inline float Vec3Dot(const sVEC3 *a, const sVEC3 *b){
    return a->x * b->x + a->y * b->y + a->z * b->z;
}
GN_API void Vec3Cross(sVEC3 *out, const sVEC3 *a, const sVEC3 *b);
inline void Vec3Add(sVEC3 *out, const sVEC3 *a, const sVEC3 *b){
    out->x = a->x + b->x; out->y = a->y + b->y; out->z = a->z + b->z;
}
inline void Vec3Sub(sVEC3 *out, const sVEC3 *a, const sVEC3 *b){
    out->x = a->x - b->x; out->y = a->y - b->y; out->z = a->z - b->z;
}
inline void Vec3Scale(sVEC3 *out, const sVEC3 *in, float s){
    out->x = in->x * s; out->y = in->y * s; out->z = in->z * s;
}
GN_API void Vec3Reflect(sVEC3 *out, const sVEC3 *ray, const sVEC3 *normal);
GN_API void Vec3Lerp(sVEC3 *out, const sVEC3 *a, const sVEC3 *b, float t);
inline float Vec3Angle(const sVEC3 *a, const sVEC3 *b){
    sVEC3 na, nb;
    Vec3Normalize(&na, a);
    Vec3Normalize(&nb, b);
    float d = Vec3Dot(&na, &nb);
    if(d >  1.0f) d =  1.0f;
    if(d < -1.0f) d = -1.0f;
    return acosf(d);
}

GN_API void Vec4Normalize(sVEC4 *out, const sVEC4 *in);

// ═══════════════════════════════════════════════════════════
//  矩阵函数
// ═══════════════════════════════════════════════════════════
GN_API void Mat44Identity(sMAT44 *out);
GN_API void Mat44Mul(sMAT44 *out, const sMAT44 *a, const sMAT44 *b);
GN_API void Mat44Transpose(sMAT44 *out, const sMAT44 *in);
GN_API void Mat44Inverse(sMAT44 *out, const sMAT44 *in);
GN_API void Mat44TransformCoord(sVEC3 *out, const sVEC3 *in, const sMAT44 *m);
GN_API void Mat44TransformNormal(sVEC3 *out, const sVEC3 *in, const sMAT44 *m);
GN_API void Mat44TransformV4(sVEC4 *out, const sVEC4 *in, const sMAT44 *m);

GN_API void Mat44Translate(sMAT44 *out, float x, float y, float z);
GN_API void Mat44Scale(sMAT44 *out, float sx, float sy, float sz);
GN_API void Mat44RotateX(sMAT44 *out, float rad);
GN_API void Mat44RotateY(sMAT44 *out, float rad);
GN_API void Mat44RotateZ(sMAT44 *out, float rad);
GN_API void Mat44RotateAxis(sMAT44 *out, const sVEC3 *axis, float rad);
GN_API void Mat44RotateYawPitchRoll(sMAT44 *out, float yaw, float pitch, float roll);

GN_API void Mat44PerspectiveVK(sMAT44 *out, float fovY_rad, float aspect, float nearZ, float farZ);
GN_API void Mat44PerspectiveGL(sMAT44 *out, float fovY_rad, float aspect, float nearZ, float farZ);
GN_API void Mat44OrthoVK(sMAT44 *out, float left, float right, float bottom, float top, float nearZ, float farZ);
GN_API void Mat44OrthoGL(sMAT44 *out, float left, float right, float bottom, float top, float nearZ, float farZ);
GN_API void Mat44LookAtRH(sMAT44 *out, const sVEC3 *eye, const sVEC3 *center, const sVEC3 *up);

GN_API void Mat44GetTranslation(sVEC3 *out, const sMAT44 *m);
GN_API void Mat44GetScale(sVEC3 *out, const sMAT44 *m);
GN_API void Mat44Lerp(sMAT44 *out, const sMAT44 *a, const sMAT44 *b, float t);

// ═══════════════════════════════════════════════════════════
//  四元数函数
// ═══════════════════════════════════════════════════════════
GN_API void QuatIdentity(sQUAT *out);
GN_API void QuatNormalize(sQUAT *out, const sQUAT *in);
GN_API void QuatFromAxisAngle(sQUAT *out, const sVEC3 *axis, float rad);
GN_API void QuatFromEuler(sQUAT *out, float yaw, float pitch, float roll);
GN_API void QuatToMatrix44(sMAT44 *out, const sQUAT *q);
GN_API void QuatSlerp(sQUAT *out, const sQUAT *a, const sQUAT *b, float t);
GN_API void QuatRotateVec3(sVEC3 *out, const sVEC3 *v, const sQUAT *q);
GN_API void QuatInverse(sQUAT *out, const sQUAT *in);

GN_API void Mat44ComposeTRS(sMAT44 *out, const sVEC3 *translation, const sQUAT *rotation, const sVEC3 *scale);
GN_API void Mat44DecomposeTRS(const sMAT44 *m, sVEC3 *translation_out, sQUAT *rotation_out, sVEC3 *scale_out);

// ═══════════════════════════════════════════════════════════
//  延迟内联实现 (依赖上述函数声明)
// ═══════════════════════════════════════════════════════════
inline void sMAT44::Init(bool is_unit){
    if(is_unit) Mat44Identity(this);
    else        memset(m, 0, sizeof(m));
}

} // end namespace GNg
#endif // _GN_3DMATH_H_
