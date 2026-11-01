// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — 3D 数学库 实现                               ║
// ╚══════════════════════════════════════════════════════════╝
#include <GN.h>
#include "GN_3Dmath.h"
#include <cmath>
#include <cstring>

namespace GNg {

// 全局默认实例
sVEC4 Vector4D;
sMAT44 Matrix44(true);
sQUAT Quaternion;

// ═══════════════════════════════════════════════════════════
//  3D 向量函数
// ═══════════════════════════════════════════════════════════

void Vec3Normalize(sVEC3 *out, const sVEC3 *in) {
    float len = sqrtf(in->x * in->x + in->y * in->y + in->z * in->z);
    if(len > 0.0000001f) {
        float inv = 1.0f / len;
        out->x = in->x * inv;
        out->y = in->y * inv;
        out->z = in->z * inv;
    } else {
        out->x = out->y = out->z = 0.0f;
    }
}

void Vec3Cross(sVEC3 *out, const sVEC3 *a, const sVEC3 *b) {
    out->x = a->y * b->z - a->z * b->y;
    out->y = a->z * b->x - a->x * b->z;
    out->z = a->x * b->y - a->y * b->x;
}

void Vec3Reflect(sVEC3 *out, const sVEC3 *ray, const sVEC3 *normal) {
    // R = I - 2*(N·I)*N
    float d = Vec3Dot(ray, normal) * 2.0f;
    out->x = ray->x - normal->x * d;
    out->y = ray->y - normal->y * d;
    out->z = ray->z - normal->z * d;
}

void Vec3Lerp(sVEC3 *out, const sVEC3 *a, const sVEC3 *b, float t) {
    float t1 = 1.0f - t;
    out->x = a->x * t1 + b->x * t;
    out->y = a->y * t1 + b->y * t;
    out->z = a->z * t1 + b->z * t;
}

// ═══════════════════════════════════════════════════════════
//  4D 向量函数
// ═══════════════════════════════════════════════════════════

void Vec4Normalize(sVEC4 *out, const sVEC4 *in) {
    float len = in->Length();
    if(len > 0.0000001f) {
        float inv = 1.0f / len;
        out->x = in->x * inv;
        out->y = in->y * inv;
        out->z = in->z * inv;
        out->w = in->w * inv;
    } else {
        out->x = out->y = out->z = out->w = 0.0f;
    }
}

// ═══════════════════════════════════════════════════════════
//  4x4 矩阵函数
// ═══════════════════════════════════════════════════════════

void Mat44Identity(sMAT44 *out) {
    memset(out->m, 0, sizeof(out->m));
    out->m[0] = out->m[5] = out->m[10] = out->m[15] = 1.0f;
}

void Mat44Mul(sMAT44 *out, const sMAT44 *a, const sMAT44 *b) {
    // 列主序: C[col][row] = Σk A[k][row] * B[col][k]
    float result[16];
    for(int col = 0; col < 4; ++col) {
        for(int row = 0; row < 4; ++row) {
            result[col * 4 + row] =
                a->m[0 * 4 + row] * b->m[col * 4 + 0] +
                a->m[1 * 4 + row] * b->m[col * 4 + 1] +
                a->m[2 * 4 + row] * b->m[col * 4 + 2] +
                a->m[3 * 4 + row] * b->m[col * 4 + 3];
        }
    }
    memcpy(out->m, result, sizeof(result));
}

void Mat44Transpose(sMAT44 *out, const sMAT44 *in) {
    // 列主序转置: 行变列, 列变行
    // m[col*4+row] → m[row*4+col]
    for(int col = 0; col < 4; ++col) {
        for(int row = 0; row < 4; ++row) {
            out->m[row * 4 + col] = in->m[col * 4 + row];
        }
    }
}

void Mat44Inverse(sMAT44 *out, const sMAT44 *in) {
    // 列主序 4×4 逆（伴随矩阵法，A^{-1} = adj(A)/det(A)）。
    // 旧实现用错误的 cofactor 组合，纯平移矩阵会返回错误结果；此处按经典列主序公式重写。
    if(!out || !in) return;
    const float *m = in->m;
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
             m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
             m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
             m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
              m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
             m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
             m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
             m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
              m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
             m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
             m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
              m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
              m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
             m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
             m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
              m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
              m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    const float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if(fabsf(det) < 1e-8f) {
        Mat44Identity(out);
        return;
    }
    const float invDet = 1.0f / det;
    for(int i = 0; i < 16; ++i) {
        out->m[i] = inv[i] * invDet;
    }
}

void Mat44TransformCoord(sVEC3 *out, const sVEC3 *in, const sMAT44 *m) {
    // v' = M * v, 其中 v = (in.x, in.y, in.z, 1)
    float x = m->m[0] * in->x + m->m[4] * in->y + m->m[8] * in->z + m->m[12];
    float y = m->m[1] * in->x + m->m[5] * in->y + m->m[9] * in->z + m->m[13];
    float z = m->m[2] * in->x + m->m[6] * in->y + m->m[10] * in->z + m->m[14];
    float w = m->m[3] * in->x + m->m[7] * in->y + m->m[11] * in->z + m->m[15];

    if(fabsf(w) > 0.0000001f) {
        float invW = 1.0f / w;
        out->x = x * invW;
        out->y = y * invW;
        out->z = z * invW;
    } else {
        out->x = x;
        out->y = y;
        out->z = z;
    }
}

void Mat44TransformNormal(sVEC3 *out, const sVEC3 *in, const sMAT44 *m) {
    // 法线变换: 使用矩阵的逆转置
    // 对于正交矩阵 (纯旋转/均匀缩放), 直接使用 3x3 子矩阵即可
    // 这里采用简化方法: 用 w=0 变换再标准化
    // 注: 对非均匀缩放, 应使用逆矩阵的转置; 此处为性能权衡
    float x = m->m[0] * in->x + m->m[4] * in->y + m->m[8] * in->z;
    float y = m->m[1] * in->x + m->m[5] * in->y + m->m[9] * in->z;
    float z = m->m[2] * in->x + m->m[6] * in->y + m->m[10] * in->z;

    out->x = x;
    out->y = y;
    out->z = z;
    Vec3Normalize(out, out);
}

void Mat44TransformV4(sVEC4 *out, const sVEC4 *in, const sMAT44 *m) {
    float x = m->m[0] * in->x + m->m[4] * in->y + m->m[8] * in->z + m->m[12] * in->w;
    float y = m->m[1] * in->x + m->m[5] * in->y + m->m[9] * in->z + m->m[13] * in->w;
    float z = m->m[2] * in->x + m->m[6] * in->y + m->m[10] * in->z + m->m[14] * in->w;
    float w = m->m[3] * in->x + m->m[7] * in->y + m->m[11] * in->z + m->m[15] * in->w;
    out->x = x;
    out->y = y;
    out->z = z;
    out->w = w;
}

// ═══════════════════════════════════════════════════════════
//  变换矩阵构建
// ═══════════════════════════════════════════════════════════

void Mat44Translate(sMAT44 *out, float x, float y, float z) {
    Mat44Identity(out);
    out->m[12] = x;
    out->m[13] = y;
    out->m[14] = z;
}

void Mat44Scale(sMAT44 *out, float sx, float sy, float sz) {
    memset(out->m, 0, sizeof(out->m));
    out->m[0] = sx;
    out->m[5] = sy;
    out->m[10] = sz;
    out->m[15] = 1.0f;
}

void Mat44RotateX(sMAT44 *out, float rad) {
    float c = cosf(rad), s = sinf(rad);
    Mat44Identity(out);
    out->m[5] = c;
    out->m[9] = -s;
    out->m[6] = s;
    out->m[10] = c;
}

void Mat44RotateY(sMAT44 *out, float rad) {
    float c = cosf(rad), s = sinf(rad);
    Mat44Identity(out);
    out->m[0] = c;
    out->m[8] = s;
    out->m[2] = -s;
    out->m[10] = c;
}

void Mat44RotateZ(sMAT44 *out, float rad) {
    float c = cosf(rad), s = sinf(rad);
    Mat44Identity(out);
    out->m[0] = c;
    out->m[4] = -s;
    out->m[1] = s;
    out->m[5] = c;
}

void Mat44RotateAxis(sMAT44 *out, const sVEC3 *axis, float rad) {
    sVEC3 n;
    Vec3Normalize(&n, axis);
    float c = cosf(rad), s = sinf(rad), t = 1.0f - c;

    out->m[0] = n.x * n.x * t + c;
    out->m[1] = n.x * n.y * t + n.z * s;
    out->m[2] = n.x * n.z * t - n.y * s;
    out->m[3] = 0.0f;

    out->m[4] = n.x * n.y * t - n.z * s;
    out->m[5] = n.y * n.y * t + c;
    out->m[6] = n.y * n.z * t + n.x * s;
    out->m[7] = 0.0f;

    out->m[8] = n.x * n.z * t + n.y * s;
    out->m[9] = n.y * n.z * t - n.x * s;
    out->m[10] = n.z * n.z * t + c;
    out->m[11] = 0.0f;

    out->m[12] = out->m[13] = out->m[14] = 0.0f;
    out->m[15] = 1.0f;
}

void Mat44RotateYawPitchRoll(sMAT44 *out, float yaw, float pitch, float roll) {
    // Yaw(Y) * Pitch(X) * Roll(Z)
    float cy = cosf(yaw), sy = sinf(yaw);
    float cp = cosf(pitch), sp = sinf(pitch);
    float cr = cosf(roll), sr = sinf(roll);

    out->m[0] = cy * cr + sy * sp * sr;
    out->m[1] = cp * sr;
    out->m[2] = -sy * cr + cy * sp * sr;
    out->m[3] = 0.0f;

    out->m[4] = -cy * sr + sy * sp * cr;
    out->m[5] = cp * cr;
    out->m[6] = sy * sr + cy * sp * cr;
    out->m[7] = 0.0f;

    out->m[8] = sy * cp;
    out->m[9] = -sp;
    out->m[10] = cy * cp;
    out->m[11] = 0.0f;

    out->m[12] = out->m[13] = out->m[14] = 0.0f;
    out->m[15] = 1.0f;
}

// ═══════════════════════════════════════════════════════════
//  投影矩阵
// ═══════════════════════════════════════════════════════════

void Mat44PerspectiveVK(sMAT44 *out, float fovY_rad, float aspect,
                        float nearZ, float farZ) {
    // 输入：视图空间右手系、+Y 上、-Z 前；输出：Vulkan NDC（y 向下、z ∈ [0,1]）。
    // m[5] = -f 把 +Y 翻到 NDC 的 -y，再由正高度视口映射到屏幕上方。
    float f = 1.0f / tanf(fovY_rad * 0.5f);
    float nf = 1.0f / (nearZ - farZ);

    memset(out->m, 0, sizeof(out->m));
    out->m[0] = f / aspect;
    out->m[5] = -f; // Vulkan 的 Y 翻转
    out->m[10] = farZ * nf; // [0,1] 深度映射
    out->m[11] = -1.0f;
    out->m[14] = nearZ * farZ * nf;
}

void Mat44PerspectiveGL(sMAT44 *out, float fovY_rad, float aspect,
                        float nearZ, float farZ) {
    // OpenGL 约定: 深度 [-1,1], Y 向上, 右手坐标系
    float f = 1.0f / tanf(fovY_rad * 0.5f);
    float nf = 1.0f / (nearZ - farZ);

    memset(out->m, 0, sizeof(out->m));
    out->m[0] = f / aspect;
    out->m[5] = f;
    out->m[10] = (farZ + nearZ) * nf;
    out->m[11] = -1.0f;
    out->m[14] = 2.0f * nearZ * farZ * nf;
}

void Mat44OrthoVK(sMAT44 *out, float left, float right,
                  float bottom, float top, float nearZ, float farZ) {
    // Vulkan 约定: 深度 [0,1]
    float rl = 1.0f / (right - left);
    float tb = 1.0f / (top - bottom);
    float fn = 1.0f / (nearZ - farZ);

    memset(out->m, 0, sizeof(out->m));
    out->m[0] = 2.0f * rl;
    out->m[5] = 2.0f * tb;
    // VK [0,1]：z_view=-near → 0、-far → 1。fn=1/(near-far) 本身为负，故 m[10]=fn。
    // 旧值 -fn 会把 near/far 都映射到负 NDC（被光栅钳到 0），正交深度不可用；L3c CSM 依赖本矩阵，本轮修正。
    out->m[10] = fn;
    out->m[12] = -(right + left) * rl;
    out->m[13] = -(top + bottom) * tb;
    out->m[14] = nearZ * fn;
    out->m[15] = 1.0f;
}

void Mat44OrthoGL(sMAT44 *out, float left, float right,
                  float bottom, float top, float nearZ, float farZ) {
    // OpenGL 约定: 深度 [-1,1]
    float rl = 1.0f / (right - left);
    float tb = 1.0f / (top - bottom);
    float fn = 1.0f / (nearZ - farZ);

    memset(out->m, 0, sizeof(out->m));
    out->m[0] = 2.0f * rl;
    out->m[5] = 2.0f * tb;
    out->m[10] = -2.0f * fn;
    out->m[12] = -(right + left) * rl;
    out->m[13] = -(top + bottom) * tb;
    out->m[14] = (farZ + nearZ) * fn;
    out->m[15] = 1.0f;
}

void Mat44LookAtRH(sMAT44 *out, const sVEC3 *eye,
                   const sVEC3 *center, const sVEC3 *up) {
    // 右手坐标系 LookAt
    // forward = normalize(eye - center)  — 从目标指向摄像机
    // right   = normalize(cross(up, forward))
    // up_perp = cross(forward, right)
    sVEC3 f, r, u;
    Vec3Sub(&f, eye, center); // f = eye - center (摄像机前方)
    Vec3Normalize(&f, &f);

    Vec3Cross(&r, up, &f); // r = up × f
    Vec3Normalize(&r, &r);

    Vec3Cross(&u, &f, &r); // u = f × r

    Mat44Identity(out);
    out->m[0] = r.x;
    out->m[4] = r.y;
    out->m[8] = r.z;
    out->m[12] = -Vec3Dot(&r, eye);
    out->m[1] = u.x;
    out->m[5] = u.y;
    out->m[9] = u.z;
    out->m[13] = -Vec3Dot(&u, eye);
    out->m[2] = f.x;
    out->m[6] = f.y;
    out->m[10] = f.z;
    out->m[14] = -Vec3Dot(&f, eye);
}

// ═══════════════════════════════════════════════════════════
//  矩阵分解
// ═══════════════════════════════════════════════════════════

void Mat44GetTranslation(sVEC3 *out, const sMAT44 *m) {
    out->x = m->m[12];
    out->y = m->m[13];
    out->z = m->m[14];
}

void Mat44GetScale(sVEC3 *out, const sMAT44 *m) {
    // 各列向量的长度即为缩放因子
    out->x = sqrtf(m->m[0] * m->m[0] + m->m[1] * m->m[1] + m->m[2] * m->m[2]);
    out->y = sqrtf(m->m[4] * m->m[4] + m->m[5] * m->m[5] + m->m[6] * m->m[6]);
    out->z = sqrtf(m->m[8] * m->m[8] + m->m[9] * m->m[9] + m->m[10] * m->m[10]);
}

void Mat44Lerp(sMAT44 *out, const sMAT44 *a, const sMAT44 *b, float t) {
    float t1 = 1.0f - t;
    for(int i = 0; i < 16; ++i) {
        out->m[i] = a->m[i] * t1 + b->m[i] * t;
    }
}

// ═══════════════════════════════════════════════════════════
//  四元数函数
// ═══════════════════════════════════════════════════════════

void QuatIdentity(sQUAT *out) {
    out->x = out->y = out->z = 0.0f;
    out->w = 1.0f;
}

void QuatNormalize(sQUAT *out, const sQUAT *in) {
    float len = in->Length();
    if(len > 0.0000001f) {
        float inv = 1.0f / len;
        out->x = in->x * inv;
        out->y = in->y * inv;
        out->z = in->z * inv;
        out->w = in->w * inv;
    } else {
        QuatIdentity(out);
    }
}

void QuatFromAxisAngle(sQUAT *out, const sVEC3 *axis, float rad) {
    sVEC3 n;
    Vec3Normalize(&n, axis);
    float half = rad * 0.5f;
    float s = sinf(half);
    out->x = n.x * s;
    out->y = n.y * s;
    out->z = n.z * s;
    out->w = cosf(half);
}

void QuatFromEuler(sQUAT *out, float yaw, float pitch, float roll) {
    // Yaw(Y) * Pitch(X) * Roll(Z)
    float cy = cosf(yaw * 0.5f), sy = sinf(yaw * 0.5f);
    float cp = cosf(pitch * 0.5f), sp = sinf(pitch * 0.5f);
    float cr = cosf(roll * 0.5f), sr = sinf(roll * 0.5f);

    out->w = cy * cp * cr + sy * sp * sr;
    out->x = cy * sp * cr + sy * cp * sr;
    out->y = sy * cp * cr - cy * sp * sr;
    out->z = cy * cp * sr - sy * sp * cr;
}

void QuatToMatrix44(sMAT44 *out, const sQUAT *q) {
    float xx = q->x * q->x, yy = q->y * q->y, zz = q->z * q->z;
    float xy = q->x * q->y, xz = q->x * q->z, yz = q->y * q->z;
    float wx = q->w * q->x, wy = q->w * q->y, wz = q->w * q->z;

    out->m[0] = 1.0f - 2.0f * (yy + zz);
    out->m[1] = 2.0f * (xy + wz);
    out->m[2] = 2.0f * (xz - wy);
    out->m[3] = 0.0f;

    out->m[4] = 2.0f * (xy - wz);
    out->m[5] = 1.0f - 2.0f * (xx + zz);
    out->m[6] = 2.0f * (yz + wx);
    out->m[7] = 0.0f;

    out->m[8] = 2.0f * (xz + wy);
    out->m[9] = 2.0f * (yz - wx);
    out->m[10] = 1.0f - 2.0f * (xx + yy);
    out->m[11] = 0.0f;

    out->m[12] = out->m[13] = out->m[14] = 0.0f;
    out->m[15] = 1.0f;
}

void QuatSlerp(sQUAT *out, const sQUAT *a, const sQUAT *b, float t) {
    // 计算点积
    float dot = a->Dot(*b);
    float sign = 1.0f;
    if(dot < 0.0f) {
        dot = -dot;
        sign = -1.0f;
    }

    // 如果夹角很小，使用线性插值
    if(dot > 0.9995f) {
        out->x = a->x + (b->x * sign - a->x) * t;
        out->y = a->y + (b->y * sign - a->y) * t;
        out->z = a->z + (b->z * sign - a->z) * t;
        out->w = a->w + (b->w * sign - a->w) * t;
        QuatNormalize(out, out);
        return;
    }

    float theta = acosf(dot);
    float sinTheta = sinf(theta);
    float w1 = sinf((1.0f - t) * theta) / sinTheta;
    float w2 = sinf(t * theta) / sinTheta * sign;

    out->x = a->x * w1 + b->x * w2;
    out->y = a->y * w1 + b->y * w2;
    out->z = a->z * w1 + b->z * w2;
    out->w = a->w * w1 + b->w * w2;
}

void QuatRotateVec3(sVEC3 *out, const sVEC3 *v, const sQUAT *q) {
    // v' = q * v * q^{-1}, 其中 v 被视为纯四元数 (v.x, v.y, v.z, 0)
    sVEC3 qv{q->x, q->y, q->z};
    sVEC3 t, result;
    Vec3Cross(&t, &qv, v);
    Vec3Scale(&t, &t, 2.0f);
    Vec3Cross(&result, &qv, &t);
    Vec3Scale(&t, &t, q->w);
    out->x = v->x + t.x + result.x;
    out->y = v->y + t.y + result.y;
    out->z = v->z + t.z + result.z;
}

void QuatInverse(sQUAT *out, const sQUAT *in) {
    // 对单位四元数，逆就是共轭
    float lenSq = in->x * in->x + in->y * in->y + in->z * in->z + in->w * in->w;
    if(lenSq > 0.0000001f) {
        float inv = 1.0f / lenSq;
        out->x = -in->x * inv;
        out->y = -in->y * inv;
        out->z = -in->z * inv;
        out->w = in->w * inv;
    } else {
        QuatIdentity(out);
    }
}

// ═══════════════════════════════════════════════════════════
//  TRS 组合与分解
// ═══════════════════════════════════════════════════════════

void Mat44ComposeTRS(sMAT44 *out,
                     const sVEC3 *translation,
                     const sQUAT *rotation,
                     const sVEC3 *scale) {
    sMAT44 rotMat;
    QuatToMatrix44(&rotMat, rotation);

    // 组合: T * R * S
    // 缩放列
    rotMat.m[0] *= scale->x;
    rotMat.m[1] *= scale->x;
    rotMat.m[2] *= scale->x;
    rotMat.m[4] *= scale->y;
    rotMat.m[5] *= scale->y;
    rotMat.m[6] *= scale->y;
    rotMat.m[8] *= scale->z;
    rotMat.m[9] *= scale->z;
    rotMat.m[10] *= scale->z;

    // 平移
    rotMat.m[12] = translation->x;
    rotMat.m[13] = translation->y;
    rotMat.m[14] = translation->z;

    *out = rotMat;
}

void Mat44DecomposeTRS(const sMAT44 *m,
                       sVEC3 *translation_out,
                       sQUAT *rotation_out,
                       sVEC3 *scale_out) {
    // 提取平移
    if(translation_out) {
        translation_out->x = m->m[12];
        translation_out->y = m->m[13];
        translation_out->z = m->m[14];
    }

    // 提取缩放 (列向量长度)
    sVEC3 col0{m->m[0], m->m[1], m->m[2]};
    sVEC3 col1{m->m[4], m->m[5], m->m[6]};
    sVEC3 col2{m->m[8], m->m[9], m->m[10]};

    float sx = Vec3Length(&col0);
    float sy = Vec3Length(&col1);
    float sz = Vec3Length(&col2);

    if(scale_out) {
        scale_out->x = sx;
        scale_out->y = sy;
        scale_out->z = sz;
    }

    // 提取旋转矩阵 (归一化各列)
    if(rotation_out && sx > 0.0000001f && sy > 0.0000001f && sz > 0.0000001f) {
        sMAT44 rotMat;
        float isx = 1.0f / sx, isy = 1.0f / sy, isz = 1.0f / sz;

        rotMat.m[0] = col0.x * isx;
        rotMat.m[1] = col0.y * isx;
        rotMat.m[2] = col0.z * isx;
        rotMat.m[3] = 0.0f;

        rotMat.m[4] = col1.x * isy;
        rotMat.m[5] = col1.y * isy;
        rotMat.m[6] = col1.z * isy;
        rotMat.m[7] = 0.0f;

        rotMat.m[8] = col2.x * isz;
        rotMat.m[9] = col2.y * isz;
        rotMat.m[10] = col2.z * isz;
        rotMat.m[11] = 0.0f;

        rotMat.m[12] = rotMat.m[13] = rotMat.m[14] = 0.0f;
        rotMat.m[15] = 1.0f;

        // 从旋转矩阵转为四元数
        float trace = rotMat.m[0] + rotMat.m[5] + rotMat.m[10];
        if(trace > 0.0f) {
            float s = sqrtf(trace + 1.0f) * 2.0f;
            rotation_out->w = 0.25f * s;
            rotation_out->x = (rotMat.m[9] - rotMat.m[6]) / s;
            rotation_out->y = (rotMat.m[2] - rotMat.m[8]) / s;
            rotation_out->z = (rotMat.m[4] - rotMat.m[1]) / s;
        } else if(rotMat.m[0] > rotMat.m[5] && rotMat.m[0] > rotMat.m[10]) {
            float s = sqrtf(1.0f + rotMat.m[0] - rotMat.m[5] - rotMat.m[10]) * 2.0f;
            rotation_out->w = (rotMat.m[9] - rotMat.m[6]) / s;
            rotation_out->x = 0.25f * s;
            rotation_out->y = (rotMat.m[1] + rotMat.m[4]) / s;
            rotation_out->z = (rotMat.m[2] + rotMat.m[8]) / s;
        } else if(rotMat.m[5] > rotMat.m[10]) {
            float s = sqrtf(1.0f + rotMat.m[5] - rotMat.m[0] - rotMat.m[10]) * 2.0f;
            rotation_out->w = (rotMat.m[2] - rotMat.m[8]) / s;
            rotation_out->x = (rotMat.m[1] + rotMat.m[4]) / s;
            rotation_out->y = 0.25f * s;
            rotation_out->z = (rotMat.m[6] + rotMat.m[9]) / s;
        } else {
            float s = sqrtf(1.0f + rotMat.m[10] - rotMat.m[0] - rotMat.m[5]) * 2.0f;
            rotation_out->w = (rotMat.m[4] - rotMat.m[1]) / s;
            rotation_out->x = (rotMat.m[2] + rotMat.m[8]) / s;
            rotation_out->y = (rotMat.m[6] + rotMat.m[9]) / s;
            rotation_out->z = 0.25f * s;
        }
        QuatNormalize(rotation_out, rotation_out);
    }
}

} // end namespace GNg
