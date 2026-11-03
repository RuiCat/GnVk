// tests/cornellBox/cornellScene.cpp — Cornell Box 几何/材质/光源（引擎侧）
//
// 职责：把 cornellGeometry 的四边形/球三角化为 G-Buffer 网格，建立 PBR 材质与两种光照变体，
//       并提交引擎的延迟 G-Buffer 绘制。几何/材质的唯一定义在 cornellGeometry.cpp。
#include "cornellShared.h"

#include <cmath>
#include <cstdio>

namespace Cornell {

namespace {

const int SPHERE_LATITUDES = 24;
const int SPHERE_LONGITUDES = 32;

sVec3 vec3(float x, float y, float z) {
    sVec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

sVec3 add(const sVec3 &a, const sVec3 &b) {
    return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

sVec3 scale(const sVec3 &a, float s) {
    return vec3(a.x * s, a.y * s, a.z * s);
}

void pushVertex(sMesh &mesh, const sVec3 &p, const sVec3 &n) {
    GNg::VkVertex3D vertex;
    vertex.px = p.x;
    vertex.py = p.y;
    vertex.pz = p.z;
    vertex.nx = n.x;
    vertex.ny = n.y;
    vertex.nz = n.z;
    vertex.tx = 0.0f;
    vertex.ty = 0.0f;
    vertex.tz = 0.0f;
    vertex.u = 0.0f;
    vertex.v = 0.0f;
    mesh.aVertices.push_back(vertex);
}

// 四边形 → 2 三角形；p0 + a*u + b*v，从 +normal 侧看为 CCW。
void tessellateQuad(const sQuad &quad, sMesh &mesh) {
    const uint32_t base = static_cast<uint32_t>(mesh.aVertices.size());
    pushVertex(mesh, quad.aP0, quad.aNormal);
    pushVertex(mesh, add(quad.aP0, quad.aU), quad.aNormal);
    pushVertex(mesh, add(add(quad.aP0, quad.aU), quad.aV), quad.aNormal);
    pushVertex(mesh, add(quad.aP0, quad.aV), quad.aNormal);
    const uint32_t indices[6] = {base, base + 1, base + 2, base, base + 2, base + 3};
    mesh.aIndices.insert(mesh.aIndices.end(), indices, indices + 6);
}

// 经纬球：外法线 + 外向 CCW 绕序（球在盒内，从盒内看是正面）。
void tessellateSphere(const sSphere &sphere, sMesh &mesh) {
    const uint32_t base = static_cast<uint32_t>(mesh.aVertices.size());
    for (int lat = 0; lat <= SPHERE_LATITUDES; ++lat) {
        const float theta = 3.14159265359f * static_cast<float>(lat) /
                            static_cast<float>(SPHERE_LATITUDES);
        const float sinTheta = std::sin(theta);
        const float cosTheta = std::cos(theta);
        for (int lon = 0; lon <= SPHERE_LONGITUDES; ++lon) {
            const float phi = 2.0f * 3.14159265359f * static_cast<float>(lon) /
                              static_cast<float>(SPHERE_LONGITUDES);
            const sVec3 n = vec3(std::cos(phi) * sinTheta, cosTheta,
                                 std::sin(phi) * sinTheta);
            pushVertex(mesh, add(sphere.aCenter, scale(n, sphere.aRadius)), n);
        }
    }
    for (int lat = 0; lat < SPHERE_LATITUDES; ++lat) {
        for (int lon = 0; lon < SPHERE_LONGITUDES; ++lon) {
            const uint32_t first = base + static_cast<uint32_t>(lat * (SPHERE_LONGITUDES + 1) + lon);
            const uint32_t second = first + static_cast<uint32_t>(SPHERE_LONGITUDES + 1);
            // 与 CPU 验证一致：这两个三角形的几何法线与顶点外法线同向。
            const uint32_t indices[6] = {first, second + 1, second,
                                         first, first + 1, second + 1};
            mesh.aIndices.insert(mesh.aIndices.end(), indices, indices + 6);
        }
    }
}

} // namespace

sMesh::sMesh() : aRoughness(1.0f), aMetallic(0.0f), aAo(1.0f) {
    aAlbedo[0] = 0.73f;
    aAlbedo[1] = 0.73f;
    aAlbedo[2] = 0.73f;
}

std::vector<sMesh> gMeshes;
GN3d::cLIGHT *gLight = nullptr;
GNg::sIMAGE *gFlatNormal = nullptr;
int gLightMode = 0;

bool initScene() {
    gMeshes.clear();

    sCornellGeometry geometry;
    buildCornellGeometry(false, &geometry); // 引擎侧不绘制面光；point 模式用点光近似

    for (size_t materialIndex = 0; materialIndex < geometry.aMaterials.size(); ++materialIndex) {
        if (static_cast<int>(materialIndex) == eMAT_LIGHT) continue;
        const sMaterial &material = geometry.aMaterials[materialIndex];
        sMesh mesh;
        mesh.aAlbedo[0] = material.aAlbedo[0];
        mesh.aAlbedo[1] = material.aAlbedo[1];
        mesh.aAlbedo[2] = material.aAlbedo[2];
        mesh.aRoughness = material.aRoughness;
        mesh.aMetallic = material.aMetallic;
        mesh.aAo = 1.0f;
        for (size_t i = 0; i < geometry.aQuads.size(); ++i) {
            if (geometry.aQuads[i].aMaterial == static_cast<int>(materialIndex)) {
                tessellateQuad(geometry.aQuads[i], mesh);
            }
        }
        for (size_t i = 0; i < geometry.aSpheres.size(); ++i) {
            if (geometry.aSpheres[i].aMaterial == static_cast<int>(materialIndex)) {
                tessellateSphere(geometry.aSpheres[i], mesh);
            }
        }
        if (!mesh.aVertices.empty()) gMeshes.push_back(mesh);
    }

    rebuildLights(gLightMode);
    // 1×1 平面法线（R=0.5,G=0.5,B=1）：G-Buffer 默认白图会被当成 (1,1,1) 法线贴图，
    // 从而改写顶点法线；显式给平面法线，保证墙面/球面法线正确。
    if (!gFlatNormal) {
        gFlatNormal = new GNg::sIMAGE();
        unsigned char px[4] = {128, 128, 255, 255};
        gFlatNormal->DataFill(px, GNg::CR_RGBA, 1, 1);
        if (gFlatNormal->vkImage == VK_NULL_HANDLE) {
            std::fprintf(stderr, "[cornellBox] 平面法线贴图上传失败\n");
            delete gFlatNormal;
            gFlatNormal = nullptr;
        }
    }
    return gLight != nullptr;
}

void releaseScene() {
    gMeshes.clear();
    if (gFlatNormal) {
        delete gFlatNormal;
        gFlatNormal = nullptr;
    }
    if (gLight) {
        delete gLight;
        gLight = nullptr;
    }
}

void rebuildLights(int lightMode) {
    gLightMode = (lightMode == 1) ? 1 : 0;
    if (gLight) {
        delete gLight;
        gLight = nullptr;
    }
    gLight = new GN3d::cLIGHT();
    const GNg::sVEC3 ambient = {0.02f, 0.02f, 0.02f};
    gLight->SetAmbient(ambient, 1.0f);

    if (gLightMode == 0) {
        const GNg::sVEC3 sunDir = {SUN_DIRECTION.x, SUN_DIRECTION.y, SUN_DIRECTION.z};
        const GNg::sVEC3 sunColor = {1.0f, 0.98f, 0.95f};
        gLight->AddDirectional(sunDir, sunColor, 3.0f, false);
    } else {
        // 经典 Cornell box 面光的点光近似：放在天花板中心，range 覆盖整个盒子。
        const GNg::sVEC3 pointPos = {ROOM_WIDTH * 0.5f, ROOM_HEIGHT - 0.2f,
                                     ROOM_DEPTH * 0.5f};
        const GNg::sVEC3 pointColor = {1.0f, 0.95f, 0.9f};
        gLight->AddPoint(pointPos, 9.0f, pointColor, 14.0f);
    }
}

void drawScene(const GNg::sMAT44 &viewProj, const GNg::sVEC3 &cameraPos) {
    (void)viewProj;
    (void)cameraPos;
    if (!gLight) return;
    GNg::sMAT44 identity;
    GNg::Mat44Identity(&identity);
    for (size_t i = 0; i < gMeshes.size(); ++i) {
        const sMesh &mesh = gMeshes[i];
        if (mesh.aVertices.empty()) continue;
        const uint32_t *indices = mesh.aIndices.empty() ? nullptr : mesh.aIndices.data();
        GNg::GN_VkDraw3D_GBuffer(mesh.aVertices.data(), static_cast<uint32_t>(mesh.aVertices.size()),
                                 indices, static_cast<uint32_t>(mesh.aIndices.size()),
                                 &identity, mesh.aAlbedo[0], mesh.aAlbedo[1], mesh.aAlbedo[2],
                                 mesh.aRoughness, mesh.aMetallic, mesh.aAo, nullptr, gFlatNormal,
                                 0.0f);
    }
}

} // namespace Cornell
