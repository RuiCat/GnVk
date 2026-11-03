// tests/cornellBox/cornellGeometry.cpp — Cornell Box 四边形/球/材质/位姿
//
// 职责：唯一一份 Cornell 几何与材质定义；两个目标（引擎实时 / CPU 路径追踪）都从这里取。
// 纯 CPU：不包含 GN.h / Vulkan，可被 cornellPt 单独链接。
#include "cornellGeometry.h"

#include <cmath>

namespace Cornell {

namespace {

sVec3 vec3(float x, float y, float z) {
    sVec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

sVec3 cross(const sVec3 &a, const sVec3 &b) {
    return vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x);
}

sVec3 normalize(const sVec3 &a) {
    const float length = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    if (length <= 1.0e-8f) return vec3(0.0f, 0.0f, 0.0f);
    return vec3(a.x / length, a.y / length, a.z / length);
}

void addQuad(sCornellGeometry *geometry, const sVec3 &p0, const sVec3 &u, const sVec3 &v,
             const sVec3 &normal, int material) {
    sQuad quad;
    quad.aP0 = p0;
    quad.aU = u;
    quad.aV = v;
    quad.aNormal = normalize(normal);
    quad.aMaterial = material;
    geometry->aQuads.push_back(quad);
}

void addSphere(sCornellGeometry *geometry, const sVec3 &center, float radius, int material) {
    sSphere sphere;
    sphere.aCenter = center;
    sphere.aRadius = radius;
    sphere.aMaterial = material;
    geometry->aSpheres.push_back(sphere);
}

} // namespace

sMaterial::sMaterial() : aRoughness(1.0f), aMetallic(0.0f) {
    aAlbedo[0] = aAlbedo[1] = aAlbedo[2] = 0.73f;
    aEmission[0] = aEmission[1] = aEmission[2] = 0.0f;
}

sCornellGeometry::sCornellGeometry() : aLightQuad(-1) {}

const sVec3 SUN_DIRECTION = vec3(0.25f, -1.0f, -0.35f);

const sPose CORNELL_POSES[4] = {
    // 正面：从开口看后墙
    {vec3(ROOM_WIDTH * 0.5f, ROOM_HEIGHT * 0.50f, ROOM_DEPTH - 0.55f),
     vec3(ROOM_WIDTH * 0.5f, ROOM_HEIGHT * 0.45f, ROOM_DEPTH * 0.5f)},
    // 左前 3/4：红墙 + 两球
    {vec3(ROOM_WIDTH * 0.78f, ROOM_HEIGHT * 0.55f, ROOM_DEPTH - 0.75f),
     vec3(ROOM_WIDTH * 0.5f, ROOM_HEIGHT * 0.45f, ROOM_DEPTH * 0.5f)},
    // 右前 3/4：绿墙 + 金属球
    {vec3(ROOM_WIDTH * 0.22f, ROOM_HEIGHT * 0.55f, ROOM_DEPTH - 0.75f),
     vec3(ROOM_WIDTH * 0.5f, ROOM_HEIGHT * 0.45f, ROOM_DEPTH * 0.5f)},
    // 高位俯视
    {vec3(ROOM_WIDTH * 0.5f, ROOM_HEIGHT * 0.86f, ROOM_DEPTH - 0.55f),
     vec3(ROOM_WIDTH * 0.5f, ROOM_HEIGHT * 0.45f, ROOM_DEPTH * 0.5f)}};

void buildCornellGeometry(bool areaLight, sCornellGeometry *out) {
    if (!out) return;
    out->aMaterials.clear();
    out->aQuads.clear();
    out->aSpheres.clear();
    out->aLightQuad = -1;

    sMaterial white;
    white.aAlbedo[0] = 0.73f;
    white.aAlbedo[1] = 0.73f;
    white.aAlbedo[2] = 0.73f;
    out->aMaterials.push_back(white);

    sMaterial red;
    red.aAlbedo[0] = 0.65f;
    red.aAlbedo[1] = 0.05f;
    red.aAlbedo[2] = 0.05f;
    out->aMaterials.push_back(red);

    sMaterial green;
    green.aAlbedo[0] = 0.12f;
    green.aAlbedo[1] = 0.45f;
    green.aAlbedo[2] = 0.12f;
    out->aMaterials.push_back(green);

    sMaterial metal;
    metal.aAlbedo[0] = 0.95f;
    metal.aAlbedo[1] = 0.95f;
    metal.aAlbedo[2] = 0.95f;
    metal.aRoughness = 0.18f;
    metal.aMetallic = 1.0f;
    out->aMaterials.push_back(metal);

    sMaterial light;
    light.aAlbedo[0] = light.aAlbedo[1] = light.aAlbedo[2] = 0.0f;
    light.aEmission[0] = light.aEmission[1] = light.aEmission[2] = 18.0f;
    out->aMaterials.push_back(light);

    const float W = ROOM_WIDTH;
    const float H = ROOM_HEIGHT;
    const float D = ROOM_DEPTH;

    // 地板（+Y）
    addQuad(out, vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, D), vec3(W, 0.0f, 0.0f),
            vec3(0.0f, 1.0f, 0.0f), eMAT_WHITE);
    // 天花板（-Y）。面光模式下留 1×1 开口，面光放在开口上方：
    // 这样天花板不会被近在咫尺的光源照爆（否则 PT 的间接光会出现大量 firefly）。
    if (areaLight) {
        const float lx = W * 0.5f - 0.5f;
        const float rx = W * 0.5f + 0.5f;
        const float dz = D * 0.5f - 0.5f;
        const float fz = D * 0.5f + 0.5f;
        addQuad(out, vec3(0.0f, H, 0.0f), vec3(lx, 0.0f, 0.0f), vec3(0.0f, 0.0f, D),
                vec3(0.0f, -1.0f, 0.0f), eMAT_WHITE);
        addQuad(out, vec3(rx, H, 0.0f), vec3(W - rx, 0.0f, 0.0f), vec3(0.0f, 0.0f, D),
                vec3(0.0f, -1.0f, 0.0f), eMAT_WHITE);
        addQuad(out, vec3(lx, H, 0.0f), vec3(rx - lx, 0.0f, 0.0f), vec3(0.0f, 0.0f, dz),
                vec3(0.0f, -1.0f, 0.0f), eMAT_WHITE);
        addQuad(out, vec3(lx, H, fz), vec3(rx - lx, 0.0f, 0.0f), vec3(0.0f, 0.0f, D - fz),
                vec3(0.0f, -1.0f, 0.0f), eMAT_WHITE);
    } else {
        addQuad(out, vec3(0.0f, H, 0.0f), vec3(W, 0.0f, 0.0f), vec3(0.0f, 0.0f, D),
                vec3(0.0f, -1.0f, 0.0f), eMAT_WHITE);
    }
    // 后墙（+Z）
    addQuad(out, vec3(0.0f, 0.0f, 0.0f), vec3(W, 0.0f, 0.0f), vec3(0.0f, H, 0.0f),
            vec3(0.0f, 0.0f, 1.0f), eMAT_WHITE);
    // 左墙（+X，红）
    addQuad(out, vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, H, 0.0f), vec3(0.0f, 0.0f, D),
            vec3(1.0f, 0.0f, 0.0f), eMAT_RED);
    // 右墙（-X，绿）
    addQuad(out, vec3(W, 0.0f, 0.0f), vec3(0.0f, 0.0f, D), vec3(0.0f, H, 0.0f),
            vec3(-1.0f, 0.0f, 0.0f), eMAT_GREEN);

    addSphere(out, vec3(W * 0.33f, 0.55f, D * 0.42f), 0.55f, eMAT_WHITE);
    addSphere(out, vec3(W * 0.68f, 0.45f, D * 0.62f), 0.45f, eMAT_METAL);

    if (areaLight) {
        // 1×1 面光放在天花板开口上方（y=H+0.02，法线 -Y），只向下照进盒子。
        addQuad(out, vec3(W * 0.5f - 0.5f, H + 0.02f, D * 0.5f - 0.5f),
                vec3(1.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f),
                vec3(0.0f, -1.0f, 0.0f), eMAT_LIGHT);
        out->aLightQuad = static_cast<int>(out->aQuads.size()) - 1;
    }
}

} // namespace Cornell
