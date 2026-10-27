// GN_PhysicsDebug.cpp — 物理调试线框：形状线框 + 接触点/法线，输出 CPU 线段（无渲染依赖）。
//
// 线数量口径（DebugDraw 注释同源）：
//   球 = 3 个正交圆 × 24 段 = 72；盒 = 12 条棱；胶囊 = 2 个端圆 × 24 + 4 条侧线 = 52；
//   每个接触点 = 3 条十字 + 1 条法线 = 4。
// 渲染器消费端在 P8（`_Vulkan/vkDebugDraw.cpp`），本文件只负责几何收集。
#include <GN.h>
#include "GN_Physics.h"

#include <cmath>

namespace GN3d {

namespace {

const int kCircleSegments = 24;
const float kContactMarkerSize = 0.05f;
const float kContactNormalLength = 0.2f;
const float kTwoPi = 6.2831853072f;

struct sBodyAxes {
    GNg::sVEC3 aAxis[3];
};

GNg::sVEC3 makeVec3(float x, float y, float z) {
    GNg::sVEC3 v = {x, y, z};
    return v;
}

GNg::sVEC3 addVec(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return makeVec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

GNg::sVEC3 subVec(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return makeVec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

GNg::sVEC3 scaleVec(const GNg::sVEC3 &v, float s) {
    return makeVec3(v.x * s, v.y * s, v.z * s);
}

// 四元数 → 三个世界局部轴（旋转矩阵的列向量）。
void getBodyAxes(const sRIGID_BODY &body, sBodyAxes &out) {
    GNg::sMAT44 rotation;
    GNg::QuatToMatrix44(&rotation, &body.aRot);
    for(int i = 0; i < 3; ++i) {
        out.aAxis[i] = makeVec3(rotation.Elem(i, 0), rotation.Elem(i, 1), rotation.Elem(i, 2));
    }
}

void addCircle(sDEBUG_DRAW &draw, const GNg::sVEC3 &center, const GNg::sVEC3 &axisU,
               const GNg::sVEC3 &axisV, float radius) {
    GNg::sVEC3 previous = center;
    for(int i = 0; i <= kCircleSegments; ++i) {
        const float angle = kTwoPi * (float)i / (float)kCircleSegments;
        const float cosine = cosf(angle);
        const float sine = sinf(angle);
        const GNg::sVEC3 point = addVec(center, scaleVec(addVec(scaleVec(axisU, cosine),
                                                                scaleVec(axisV, sine)),
                                                        radius));
        if(i > 0) draw.addLine(previous, point);
        previous = point;
    }
}

void addBoxWireframe(sDEBUG_DRAW &draw, const sRIGID_BODY &body, const sBodyAxes &axes) {
    const float halfX = std::fabs(body.aHalfExtent.x);
    const float halfY = std::fabs(body.aHalfExtent.y);
    const float halfZ = std::fabs(body.aHalfExtent.z);
    GNg::sVEC3 corner[8];
    for(int i = 0; i < 8; ++i) {
        const float signX = (i & 1) ? 1.0f : -1.0f;
        const float signY = (i & 2) ? 1.0f : -1.0f;
        const float signZ = (i & 4) ? 1.0f : -1.0f;
        corner[i] = body.aPos;
        corner[i] = addVec(corner[i], scaleVec(axes.aAxis[0], signX * halfX));
        corner[i] = addVec(corner[i], scaleVec(axes.aAxis[1], signY * halfY));
        corner[i] = addVec(corner[i], scaleVec(axes.aAxis[2], signZ * halfZ));
    }
    static const int edges[12][2] = {{0, 1}, {0, 2}, {0, 4}, {1, 3}, {1, 5}, {2, 3},
                                     {2, 6}, {3, 7}, {4, 5}, {4, 6}, {5, 7}, {6, 7}};
    for(int i = 0; i < 12; ++i) draw.addLine(corner[edges[i][0]], corner[edges[i][1]]);
}

void addCapsuleWireframe(sDEBUG_DRAW &draw, const sRIGID_BODY &body, const sBodyAxes &axes) {
    const float halfHeight = (body.aHalfExtent.y > 0.0f) ? body.aHalfExtent.y : 0.0f;
    const GNg::sVEC3 top = addVec(body.aPos, scaleVec(axes.aAxis[1], halfHeight));
    const GNg::sVEC3 bottom = subVec(body.aPos, scaleVec(axes.aAxis[1], halfHeight));
    addCircle(draw, top, axes.aAxis[0], axes.aAxis[2], body.aRadius);
    addCircle(draw, bottom, axes.aAxis[0], axes.aAxis[2], body.aRadius);
    const GNg::sVEC3 directions[4] = {axes.aAxis[0], scaleVec(axes.aAxis[0], -1.0f), axes.aAxis[2],
                                      scaleVec(axes.aAxis[2], -1.0f)};
    for(int i = 0; i < 4; ++i) {
        const GNg::sVEC3 offset = scaleVec(directions[i], body.aRadius);
        draw.addLine(addVec(bottom, offset), addVec(top, offset));
    }
}

} // namespace

void cPHYSICS_WORLD::DebugDraw(sDEBUG_DRAW &draw) const {
    draw.clear();
    for(size_t i = 0; i < aBodies.size(); ++i) {
        if(!aUsed[i]) continue;
        const sRIGID_BODY &body = aBodies[i];
        sBodyAxes axes;
        getBodyAxes(body, axes);
        if(body.aShape == eSHAPE_SPHERE) {
            addCircle(draw, body.aPos, axes.aAxis[0], axes.aAxis[1], body.aRadius);
            addCircle(draw, body.aPos, axes.aAxis[0], axes.aAxis[2], body.aRadius);
            addCircle(draw, body.aPos, axes.aAxis[1], axes.aAxis[2], body.aRadius);
        } else if(body.aShape == eSHAPE_BOX) {
            addBoxWireframe(draw, body, axes);
        } else if(body.aShape == eSHAPE_CAPSULE) {
            addCapsuleWireframe(draw, body, axes);
        }
    }
    // 上一步的接触：每个点画世界轴十字 + 沿法线的短线（法线 A → B）。
    for(size_t c = 0; c < aConstraints.size(); ++c) {
        const sCONTACT_MANIFOLD &manifold = aConstraints[c].aManifold;
        const uint32_t pointCount = manifold.aCount > 4u ? 4u : manifold.aCount;
        for(uint32_t p = 0; p < pointCount; ++p) {
            const GNg::sVEC3 &point = manifold.aPoints[p].aPoint;
            draw.addLine(subVec(point, makeVec3(kContactMarkerSize, 0.0f, 0.0f)),
                         addVec(point, makeVec3(kContactMarkerSize, 0.0f, 0.0f)));
            draw.addLine(subVec(point, makeVec3(0.0f, kContactMarkerSize, 0.0f)),
                         addVec(point, makeVec3(0.0f, kContactMarkerSize, 0.0f)));
            draw.addLine(subVec(point, makeVec3(0.0f, 0.0f, kContactMarkerSize)),
                         addVec(point, makeVec3(0.0f, 0.0f, kContactMarkerSize)));
            draw.addLine(point, addVec(point, scaleVec(manifold.aNormal, kContactNormalLength)));
        }
    }
}

} // namespace GN3d
