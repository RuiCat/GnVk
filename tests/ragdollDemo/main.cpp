// tests/ragdollDemo/main.cpp — C3b 演示：布娃娃
//
// 三列对比（同一个骨架、同一个击飞冲量）：
//   左：**无物理**（保持站立姿势）—— 参考：一个僵硬的人
//   中：布娃娃（distance 关节）—— 自然倒地
//   右：布娃娃 + 更强冲击 —— 被击飞后翻滚
//
// 用程序化"火柴人"几何（胶囊/盒），因为仓库没有模型资产。
// 每根骨画一个胶囊，颜色按骨类型区分。
#include <GN.h>
#include "GN_3Dmath.h"
#include "GN_3Dmodel.h"
#include "GN_3Dskeleton.h"
#include "GN_Physics.h"
#include "GN_Ragdoll.h"
#include "GN_Gizmo.h"

#include <png.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

int gWidth = 1280;
int gHeight = 720;
int gFrames = 360;
const char *gOutDir = ".local/video/ragdoll-frames";
bool gVerbose = false;
bool gPoseCheckFailed = false;
float gTime = 0.0f;
float gRecordedMinY = 1e30f;
float gRecordedMaxY = -1e30f;

GN3d::cLIGHT *gLight = nullptr;
GN3d::cCAMERA gCamera;

const float kDt = 1.0f / 60.0f;

// ── 骨架：火柴人（root → 颈 → 头；root → 左右上臂 → 前臂；root → 左右大腿 → 小腿）
GN3d::sSKELETON makeSkeleton() {
    GN3d::sSKELETON sk;
    sk.aJoints.resize(10);
    struct J {
        int parent;
        float x, y, z;
    };
    // y 是相对父的局部平移（向下为负）
    const J joints[10] = {
        {-1, 0.0f, 1.70f, 0.0f},   // 0 root（髋）
        {0, 0.0f, 0.30f, 0.0f},    // 1 颈（相对髋 +0.30）
        {1, 0.0f, 0.24f, 0.0f},    // 2 头
        {1, -0.19f, 0.08f, 0.0f},  // 3 左上臂
        {3, 0.0f, -0.30f, 0.0f},   // 4 左前臂
        {1, 0.19f, 0.08f, 0.0f},   // 5 右上臂
        {5, 0.0f, -0.30f, 0.0f},   // 6 右前臂
        {0, -0.11f, -0.06f, 0.0f}, // 7 左大腿
        {7, 0.0f, -0.42f, 0.0f},   // 8 左小腿
        {0, 0.11f, -0.06f, 0.0f},  // 9 右大腿
    };
    for (int i = 0; i < 10; ++i) {
        sk.aJoints[i].aParent = joints[i].parent;
        sk.aJoints[i].aBindTranslation = GNg::sVEC3{joints[i].x, joints[i].y, joints[i].z};
        GNg::QuatIdentity(&sk.aJoints[i].aBindRotation);
        sk.aJoints[i].aBindScale = GNg::sVEC3{1.0f, 1.0f, 1.0f};
    }
    return sk;
}

std::vector<GN3d::sANIM_TRANSFORM> bindPose(const GN3d::sSKELETON &sk) {
    std::vector<GN3d::sANIM_TRANSFORM> local(sk.aJoints.size());
    for (size_t i = 0; i < sk.aJoints.size(); ++i) {
        local[i].aTranslation = sk.aJoints[i].aBindTranslation;
        local[i].aRotation = sk.aJoints[i].aBindRotation;
        local[i].aScale = sk.aJoints[i].aBindScale;
    }
    return local;
}

bool writePng(const char *path, const unsigned char *rgba, int width, int height) {
    std::FILE *file = std::fopen(path, "wb");
    if (file == nullptr) return false;
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (png == nullptr) {
        std::fclose(file);
        return false;
    }
    png_infop info = png_create_info_struct(png);
    if (info == nullptr) {
        png_destroy_write_struct(&png, nullptr);
        std::fclose(file);
        return false;
    }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        std::fclose(file);
        return false;
    }
    png_init_io(png, file);
    png_set_IHDR(png, info, (png_uint_32)width, (png_uint_32)height, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_compression_level(png, 3);
    png_write_info(png, info);
    std::vector<png_byte> row((size_t)width * 3u);
    for (int y = 0; y < height; ++y) {
        const unsigned char *src = rgba + (size_t)y * (size_t)width * 4u;
        for (int x = 0; x < width; ++x) {
            row[(size_t)x * 3u + 0u] = src[(size_t)x * 4u + 0u];
            row[(size_t)x * 3u + 1u] = src[(size_t)x * 4u + 1u];
            row[(size_t)x * 3u + 2u] = src[(size_t)x * 4u + 2u];
        }
        png_write_row(png, row.data());
    }
    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    std::fclose(file);
    return true;
}

// 往物理世界加一块平地（两个三角形）。sMESH 的顶点是 VkVertex3D 布局
// （11 floats：px,py,pz, nx,ny,nz, tx,ty,tz, u,v），这里只需要位置，其余填零。
//
// 绕序与 cCHARACTER::AddFloor 一致（从上方看逆时针），法线才会朝上 ——
// 绕序反了法线就朝下，"推出去"变成往下推，布娃娃会陷进地面。
void addGroundMesh(GN3d::cPHYSICS_WORLD *world) {
    if (world == nullptr) return;
    // **必须堆分配**：sMESH 的析构函数会 free(aVertices)/free(aIndices)，
    // 传栈数组会在析构时 free 栈内存（实测 free(): invalid pointer 直接 core dump）。
    // 这里交给 sMESH 接管所有权（作用域结束时由它的析构释放）。
    std::vector<float> verts(4 * 11, 0.0f);
    const float xs[4] = {-14.0f, 14.0f, 14.0f, -14.0f};
    const float zs[4] = {-14.0f, -14.0f, 14.0f, 14.0f};
    for (int i = 0; i < 4; ++i) {
        verts[i * 11 + 0] = xs[i];
        verts[i * 11 + 1] = 0.0f;
        verts[i * 11 + 2] = zs[i];
        verts[i * 11 + 4] = 1.0f; // 法线 +Y
    }
    std::vector<uint32_t> idx = {0, 1, 2, 0, 2, 3};
    GN3d::sMESH mesh;
    // 拷到堆上交给 sMESH。**必须用 new[]**：sMESH::Release() 用的是 `delete[]`，
    // malloc 的内存配 delete[] 是未定义行为（虽然常常"看起来没事"）。
    mesh.aVertices = new float[verts.size()];
    mesh.aIndices = new uint32_t[idx.size()];
    if (mesh.aVertices == nullptr || mesh.aIndices == nullptr) return;
    std::memcpy(mesh.aVertices, verts.data(), verts.size() * sizeof(float));
    std::memcpy(mesh.aIndices, idx.data(), idx.size() * sizeof(uint32_t));
    mesh.aVertexCount = 4;
    mesh.aIndexCount = 6;
    world->AddStaticMesh(mesh);
}

// 一个人偶：布娃娃 + 每根骨的显示模型
struct sFigure {
    GN3d::cRAGDOLL aRagdoll;
    GN3d::cPHYSICS_WORLD aWorld;
    GN3d::sSKELETON aSkeleton;
    std::vector<GN3d::sANIM_TRANSFORM> aPose;
    std::vector<GN3d::cMODEL *> aBoneMeshes; // 与 aRagdoll.BoneAt(i) 对应
    GN3d::cMODEL *aGroundMarker;
    float aX;
    bool aUsePhysics;
    bool aStrongImpulse;
    bool aImpulseApplied;

    sFigure()
        : aGroundMarker(nullptr), aX(0.0f), aUsePhysics(true), aStrongImpulse(false),
          aImpulseApplied(false) {}
};

std::vector<sFigure *> gFigures;
std::vector<GN3d::cMODEL *> gGroundTiles;
std::vector<GN3d::sEDIT_LINE> gBoneLines; // 把球串成骨架的连线

void initLights() {
    gLight = new GN3d::cLIGHT();
    gLight->AddDirectional(GNg::sVEC3{0.40f, -0.78f, -0.48f}, GNg::sVEC3{1.0f, 0.97f, 0.93f}, 1.9f,
                           false);
    gLight->AddDirectional(GNg::sVEC3{-0.55f, -0.35f, 0.70f}, GNg::sVEC3{0.42f, 0.52f, 0.80f},
                           0.40f, false);
    gLight->SetAmbient(GNg::sVEC3{0.24f, 0.26f, 0.32f}, 1.0f);
}

void buildScene() {
    // 地面
    GN3d::cMODEL *floor = GN3d::CreateCubeModel(1.0f, 0.30f, 0.32f, 0.35f);
    floor->SetScale(20.0f, 0.08f, 20.0f);
    floor->SetPosition(0.0f, -0.04f, 0.0f);
    floor->computeWorldAABB();
    gGroundTiles.push_back(floor);

    const float xs[3] = {-1.7f, 0.0f, 1.7f};
    for (int i = 0; i < 3; ++i) {
        sFigure *f = new sFigure();
        f->aX = xs[i];
        f->aUsePhysics = (i > 0);
        f->aStrongImpulse = (i == 2);
        f->aSkeleton = makeSkeleton();

        // 骨架每个关节平移到该列的位置
        std::vector<GN3d::sANIM_TRANSFORM> pose = bindPose(f->aSkeleton);
        // 根的平移是绝对位置（根的父为 -1）
        pose[0].aTranslation.x += xs[i];
        f->aPose = pose;

        if (f->aUsePhysics) {
            f->aWorld.SetGravity(GNg::sVEC3{0.0f, -9.81f, 0.0f});
            f->aWorld.SetFixedTimestep(1.0f / 120.0f, 8); // 1/120 x8：关节误差从 0.15 降到 0.000
            // 地面：**静态三角网**。
            //
            // 一开始我用静态**盒**刚体，结果布娃娃穿过地面自由落体（动能涨到 6000+）。
            // 原因：物理引擎 v1 的"球/胶囊对盒"碰撞**没有实现**（GN_Physics.h 里
            // 明写了"盒未实现，直接跳过"），所以胶囊根本感觉不到那块盒。
            // 三角网是唯一被支持的静态形状。
            addGroundMesh(&f->aWorld);

            GN3d::sRAGDOLL_DESC desc;
            desc.aBoneRadius = 0.09f; // 不能用更小：会穿地（见 sRAGDOLL_DESC 注释）
            desc.aMassScale = 8.0f;
            desc.aNoCollideGroup = 0; // 自动分配
            if (!f->aRagdoll.Build(&f->aWorld, f->aSkeleton, f->aPose, desc)) {
                std::fprintf(stderr, "[ragdollDemo] 布娃娃建立失败 #%d\n", i);
            }

            // 为每根骨建一个**球**显示模型。
            //
            // 不能用方块：布娃娃的骨刚体是球体，它的 aRot 是随机值（球各向同性，
            // 没有任何物理量约束朝向），拿来当方块朝向会得到杂乱无章的旋转 ——
            // 第一版就是这样，画面上像"散洒的一堆方块"而不是一个人。
            // 球没有朝向，直接摆位置即可。
            for (uint32_t b = 0; b < f->aRagdoll.BoneCount(); ++b) {
                const GN3d::sRIGID_BODY *body = f->aWorld.GetBody(f->aRagdoll.BoneAt(b).aBody);
                const float radius = (body != nullptr) ? body->aRadius : 0.09f;
                const float tint = 0.70f + 0.12f * (float)(b % 3);
                GN3d::cMODEL *m = GN3d::CreateSphereModel(1.0f, 16, tint, tint * 0.90f, tint * 0.80f);
                m->SetScale(radius, radius, radius);
                f->aBoneMeshes.push_back(m);
            }
        } else {
            // 无物理：直接用绑定姿势画
            std::vector<GNg::sMAT44> worldMat;
            f->aSkeleton.computeWorld(f->aPose, worldMat);
            for (size_t j = 0; j < f->aSkeleton.jointCount(); ++j) {
                const int32_t p = f->aSkeleton.aJoints[j].aParent;
                if (p < 0) continue;
                GNg::sVEC3 a, b;
                GNg::Mat44GetTranslation(&a, &worldMat[(size_t)p]);
                GNg::Mat44GetTranslation(&b, &worldMat[j]);
                const float len = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y) +
                                            (b.z - a.z) * (b.z - a.z));
                GN3d::cMODEL *m = GN3d::CreateCubeModel(1.0f, 0.72f, 0.65f, 0.58f);
                m->SetScale(0.105f, len, 0.105f);
                f->aBoneMeshes.push_back(m);
            }
        }
        gFigures.push_back(f);
    }
}

// 用布娃娃的骨位姿摆显示模型
void syncBoneMeshes(sFigure *f) {
    if (!f->aRagdoll.IsBuilt()) return;
    for (uint32_t b = 0; b < f->aRagdoll.BoneCount() && b < f->aBoneMeshes.size(); ++b) {
        const GN3d::sRIGID_BODY *body =
            f->aWorld.GetBody(f->aRagdoll.BoneAt(b).aBody);
        if (body == nullptr) continue;
        GN3d::cMODEL *m = f->aBoneMeshes[b];
        m->SetPosition(body->aPos.x, body->aPos.y, body->aPos.z);
        // 球体：不设旋转（朝向无意义）
        m->computeWorldAABB();
    }
}

// 无物理：按骨架姿势摆
void syncStaticMeshes(sFigure *f) {
    if (f->aUsePhysics) return;
    std::vector<GNg::sMAT44> worldMat;
    if (!f->aSkeleton.computeWorld(f->aPose, worldMat)) return;
    size_t mi = 0;
    for (size_t j = 0; j < f->aSkeleton.jointCount(); ++j) {
        const int32_t p = f->aSkeleton.aJoints[j].aParent;
        if (p < 0) continue;
        if (mi >= f->aBoneMeshes.size()) break;
        GNg::sVEC3 a, b;
        GNg::Mat44GetTranslation(&a, &worldMat[(size_t)p]);
        GNg::Mat44GetTranslation(&b, &worldMat[j]);
        const GNg::sVEC3 dir{b.x - a.x, b.y - a.y, b.z - a.z};
        const float len =
            std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        if (len <= 1e-5f) continue;
        GN3d::cMODEL *m = f->aBoneMeshes[mi];
        m->SetPosition((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.z + b.z) * 0.5f);
        m->SetScale(0.105f, len, 0.105f);
        // 让局部 -Y 指向骨方向（模型是长方体，+/-Y 对称，符号不影响外观）
        GNg::sVEC3 down{0.0f, -1.0f, 0.0f};
        const GNg::sQUAT q = GN3d::quatFromTo(down, dir);
        m->SetRotation(q);
        m->computeWorldAABB();
        ++mi;
    }
}

bool buildCamera(float aspect, GNg::sMAT44 &viewProj, GNg::sMAT44 &view, GNg::sVEC3 &eye) {
    gCamera.SetOrbit(GNg::sVEC3{0.0f, 0.75f, 0.0f}, 6.2f, 0.62f, 0.26f);
    gCamera.SetFov(46.0f);
    gCamera.SetNF(0.1f, 120.0f);
    GNg::sMAT44 projection;
    if (!gCamera.BuildViewProj(aspect, &view, &projection, &viewProj)) return false;
    eye = gCamera.GetCoordV();
    return true;
}

} // namespace

extern "C" {

GN_CALL void AppInit() {
    gCamera.Init();
    buildScene();
    initLights();
    GNg::GN_VkSetEnvironmentLighting(GN3d::eENV_LIGHTING_INDOOR_UNIFORM, 0.22f);
    GNg::GN_VkSetExposure(-0.05f);
    GNg::GN_VkSetBloom(0.22f, 1.10f, 0.30f);
    std::fprintf(stderr, "[ragdollDemo] 左=僵硬站立 中=布娃娃倒地 右=布娃娃+强冲击\n");
}

GN_CALL void AppUnInit() {
    for (size_t i = 0; i < gFigures.size(); ++i) {
        sFigure *f = gFigures[i];
        f->aRagdoll.Destroy();
        for (size_t m = 0; m < f->aBoneMeshes.size(); ++m) delete f->aBoneMeshes[m];
        delete f;
    }
    gFigures.clear();
    for (size_t i = 0; i < gGroundTiles.size(); ++i) delete gGroundTiles[i];
    gGroundTiles.clear();
    delete gLight;
    gLight = nullptr;
}

GN_CALL void AppRun(unsigned int nTime) { (void)nTime; }
GN_CALL void AppDisplay3D(unsigned int nTime) { (void)nTime; }

GN_CALL void AppDisplay2D(unsigned int nTime) {
    (void)nTime;
    if (gLight == nullptr) return;

    // 物理推进（固定步长，与真实帧率解耦）
    for (size_t i = 0; i < gFigures.size(); ++i) {
        sFigure *f = gFigures[i];
        if (!f->aUsePhysics) continue;

        // 第 0.5 秒施加冲击（"被打中"）
        if (!f->aImpulseApplied && gTime > 0.5f) {
            f->aImpulseApplied = true;
            const float strength = f->aStrongImpulse ? 7.0f : 2.4f;
            f->aRagdoll.ApplyImpulse(GNg::sVEC3{strength * 0.35f, strength * 0.55f, -strength});
        }
        f->aWorld.Step(kDt);
        // **必须**钳角速度：接触瞬间的尖峰可以到 35+ rad/s，
        // 后果是动能飙到 5000（视觉上是"炸开"）。见 ClampAngularSpeeds 的说明。
        f->aRagdoll.ClampAngularSpeeds();
        syncBoneMeshes(f);
    }
    for (size_t i = 0; i < gFigures.size(); ++i) syncStaticMeshes(gFigures[i]);

    // 记录布娃娃当前的高度范围（用于退出时的量化验收）。
    //
    // **每帧覆盖，不累积**：累积最大值会把"第 0 帧还站着时的 2.2 m"也算进去，
    // 于是"是否倒地"的判据永远失败（第一版就是这么错的）。
    // 我们要的是**最终状态**，所以只保留最新一帧。
    {
        float mnY = 1e30f;
        float mxY = -1e30f;
        bool any = false;
        for (size_t i = 1; i < gFigures.size(); ++i) {
            if (!gFigures[i]->aRagdoll.IsBuilt()) continue;
            GNg::sVEC3 mn, mx;
            gFigures[i]->aRagdoll.WorldBounds(mn, mx);
            if (mn.y < mnY) mnY = mn.y;
            if (mx.y > mxY) mxY = mx.y;
            any = true;
        }
        if (any) {
            gRecordedMinY = mnY;
            gRecordedMaxY = mxY;
        }
    }

    const float aspect = (float)gWidth / (float)gHeight;
    GNg::sMAT44 view;
    GNg::sMAT44 viewProj;
    GNg::sVEC3 eye;
    if (!buildCamera(aspect, viewProj, view, eye)) return;

    GNg::cRENDER_FRAME frame;
    GNg::sRENDER_FRAME_DESC desc;
    desc.aViewProj = &viewProj;
    desc.aCameraPos = &eye;
    desc.aLightUBO = gLight->GetUBO();
    desc.aLightUBOSize = sizeof(GN3d::sLIGHT_UBO);
    desc.aDrawSkybox = false;
    if (!frame.Begin(desc)) return;

    for (size_t i = 0; i < gGroundTiles.size(); ++i) gGroundTiles[i]->DrawPBRSorted(eye);
    for (size_t i = 0; i < gFigures.size(); ++i) {
        for (size_t m = 0; m < gFigures[i]->aBoneMeshes.size(); ++m) {
            gFigures[i]->aBoneMeshes[m]->DrawPBRSorted(eye);
        }
    }
    // 骨连接线：把球串成骨架。只用球的话画面上是一簇小球，看不出人形。
    // 关节位置口径与 MaxJointError 一致（球体模式下不能用 aRot，它是随机的）。
    gBoneLines.clear();
    for (size_t i = 0; i < gFigures.size(); ++i) {
        sFigure *f = gFigures[i];
        if (!f->aRagdoll.IsBuilt()) continue;
        const uint32_t nb = f->aRagdoll.BoneCount();
        for (uint32_t b = 0; b < nb; ++b) {
            const GN3d::sRIGID_BODY *body = f->aWorld.GetBody(f->aRagdoll.BoneAt(b).aBody);
            if (body == nullptr) continue;
            const uint32_t myJoint = f->aRagdoll.BoneAt(b).aJoint;
            const int32_t parentJoint = f->aSkeleton.aJoints[myJoint].aParent;
            if (parentJoint < 0) continue;
            int32_t pb = -1;
            for (uint32_t k = 0; k < nb; ++k) {
                if (f->aRagdoll.BoneAt(k).aJoint == (uint32_t)parentJoint) {
                    pb = (int32_t)k;
                    break;
                }
            }
            if (pb < 0) continue;
            const GN3d::sRIGID_BODY *pbody = f->aWorld.GetBody(f->aRagdoll.BoneAt(pb).aBody);
            if (pbody == nullptr) continue;
            const uint32_t color = (i == 0) ? 0xFFB0B0B0u : (i == 1 ? 0xFFFFD080u : 0xFF80C8FFu);
            gBoneLines.push_back(GN3d::sEDIT_LINE(pbody->aPos, body->aPos, color));
        }
    }
    if (!gBoneLines.empty())
        GNg::GN_VkDrawEditLines(&gBoneLines[0], (uint32_t)gBoneLines.size());

    frame.End();

    if (gVerbose && ((int)(gTime * 30.0f) % 30 == 0)) {
        for (size_t i = 0; i < gFigures.size(); ++i) {
            sFigure *f = gFigures[i];
            if (!f->aUsePhysics) continue;
            std::fprintf(stderr, "[dbg] t=%.1f 图%zu 动能=%.2f 关节误差=%.4f\n", gTime, i,
                         f->aRagdoll.KineticEnergy(), f->aRagdoll.MaxJointError());
        }
    }
    gTime += kDt;
}

} // extern "C"

namespace {
void onFrame(int index) {
    int w = 0, h = 0, size = 0;
    unsigned char *rgba = GN_GrabFrame(&w, &h, &size);
    if (rgba == nullptr || w <= 0 || h <= 0) return;
    char path[512];
    std::snprintf(path, sizeof(path), "%s/frame_%05d.png", gOutDir, index);
    writePng(path, rgba, w, h);
    GN_GrabFrameFree(rgba);
    if (index % 30 == 0) std::fprintf(stderr, "[ragdollDemo] 帧 %d/%d\n", index, gFrames);
}
} // namespace

int main(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) gOutDir = argv[++i];
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) gFrames = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--size") == 0 && i + 2 < argc) {
            gWidth = std::atoi(argv[++i]);
            gHeight = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--verbose") == 0) gVerbose = true;
    }
    if (gFrames < 1) gFrames = 1;
    char cmd[512];
    std::snprintf(cmd, sizeof(cmd), "mkdir -p '%s' && rm -f '%s'/*.png", gOutDir, gOutDir);
    if (std::system(cmd) != 0) std::fprintf(stderr, "[ragdollDemo] 清理目录失败\n");
    std::fprintf(stderr, "[ragdollDemo] %dx%d，%d 帧 → %s\n", gWidth, gHeight, gFrames, gOutDir);
    GN_InitLoadCallBackFunc(AppInit, AppUnInit);
    GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);
    if (!GN_StartRunHeadless((short)gWidth, (short)gHeight, gFrames, onFrame)) {
        std::fprintf(stderr, "[ragdollDemo] 初始化失败\n");
        return 2;
    }

    // **量化验收**
    // 注意：**不能用 gFigures 里的 cRAGDOLL 现算** —— GN_StartRunHeadless 返回时
    // AppUnInit 已经跑过，布娃娃已被 Destroy，WorldBounds 会返回 ±1e30（实测）。
    // 所以高度范围在渲染过程中累积（见 gRecordedMinY/MaxY）。
    const float minY = gRecordedMinY;
    const float maxY = gRecordedMaxY;
    std::fprintf(stderr, "[ragdollDemo] 布娃娃最终高度范围 y ∈ [%.2f, %.2f]\n", minY, maxY);
    // 布娃娃必须倒地（整个人不应还站在 1.7 m 高）
    if (!(maxY < 1.6f)) {
        std::fprintf(stderr, "[ragdollDemo] FAIL: 布娃娃没有倒地（最高点 %.2f m）\n", maxY);
        return 4;
    }
    // 也不能穿到地面以下太多
    if (minY < -0.35f) {
        std::fprintf(stderr, "[ragdollDemo] FAIL: 布娃娃穿进地面（最低点 %.2f m）\n", minY);
        return 4;
    }
    if (gPoseCheckFailed) return 3;
    std::printf("[ragdollDemo] 完成：%d 帧\n", gFrames);
    return 0;
}
