// Test 10: 3D Scene — Blinn-Phong + SetScale 绕序修正
#include "common/test_utils.h"
#include <cstdio>
#include <cmath>

static GN3d::cMODEL *groundPlane  = nullptr;
static GN3d::cMODEL *cubeRed      = nullptr;
static GN3d::cMODEL *cubeBlue     = nullptr;
static GN3d::cMODEL *cubeCyan     = nullptr;
static GN3d::cMODEL *sphereBlue   = nullptr;
static GN3d::cMODEL *sphereGold   = nullptr;
static GN3d::cMODEL *centralCube  = nullptr;
static GN3d::cLIGHT *lightMgr     = nullptr;

static float   camYaw      = 0.0f;
static float   camPitch    = 0.25f;
static float   camDistance = 8.0f;
static GNg::sVEC3 camTarget = {0.0f, 1.0f, 0.0f};
static bool    autoRotate  = true;
static float   rotSpeed    = 0.5f;
static float   g_dt        = 0.016f;
static float   centralRot  = 0.0f;

static inline float Clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// 翻转三角形绕序: CCW → CW (修正 Vulkan Y-flip 背面剔除)
static void FixWinding(GN3d::cMODEL *m) {
    if (!m || !m->aMeshes) return;
    for (uint32_t mi = 0; mi < m->aMeshCount; mi++) {
        GN3d::sMESH &mesh = m->aMeshes[mi];
        if (!mesh.aIndices || mesh.aIndexCount < 3) continue;
        uint32_t n = mesh.aIndexCount / 3;
        for (uint32_t t = 0; t < n; t++) {
            uint32_t tmp = mesh.aIndices[t*3+1];
            mesh.aIndices[t*3+1] = mesh.aIndices[t*3+2];
            mesh.aIndices[t*3+2] = tmp;
        }
    }
}

extern "C" {

GN_CALL void AppInit() {
    // 几何体生成器已产出 CW 绕序, 无需 FixWinding
    groundPlane = GN3d::CreatePlaneModel(20.0f, 20.0f, 0.25f, 0.38f, 0.25f);
    groundPlane->SetPosition(0.0f, -0.02f, 0.0f);
    // 地面绕序: XZ平面CCW→cross指向-Y, 需要翻转
    {   // 交换每个三角形的index[1]↔[2] 翻转绕序
        GN3d::sMESH &m = groundPlane->aMeshes[0];
        uint32_t t = m.aIndices[1]; m.aIndices[1] = m.aIndices[2]; m.aIndices[2] = t;
        t = m.aIndices[4]; m.aIndices[4] = m.aIndices[5]; m.aIndices[5] = t;
    }

    cubeRed = GN3d::CreateCubeModel(1.0f, 0.85f, 0.18f, 0.18f);
    cubeRed->SetPosition(-3.5f, 0.5f, 0.0f);

    cubeBlue = GN3d::CreateCubeModel(1.0f, 0.18f, 0.22f, 0.82f);
    cubeBlue->SetPosition(3.5f, 0.5f, 0.0f);

    cubeCyan = GN3d::CreateCubeModel(0.6f, 0.18f, 0.75f, 0.78f);
    cubeCyan->SetPosition(0.0f, 0.3f, 3.2f);

    sphereBlue = GN3d::CreateSphereModel(0.8f, 16, 0.16f, 0.20f, 0.88f);
    sphereBlue->SetPosition(-2.5f, 0.8f, -2.5f);

    sphereGold = GN3d::CreateSphereModel(0.6f, 16, 0.92f, 0.52f, 0.08f);
    sphereGold->SetPosition(2.5f, 0.6f, -2.0f);

    centralCube = GN3d::CreateCubeModel(1.5f, 0.55f, 0.28f, 0.92f);
    centralCube->SetPosition(0.0f, 0.75f, 0.0f);

    lightMgr = new GN3d::cLIGHT();
    // 高环境光保证所有面可见, 排除光照干扰
    GNg::sVEC3 amb = {0.5f, 0.5f, 0.55f};
    lightMgr->SetAmbient(amb, 1.0f);
    // 方向光从前方上方照过来
    GNg::sVEC3 sunDir = {-0.3f, -0.8f, 0.5f};
    GNg::sVEC3 sunCol = {1.0f, 0.95f, 0.90f};
    lightMgr->AddDirectional(sunDir, sunCol, 1.2f, false);
    // 前方点光源
    GNg::sVEC3 fPos = {0.0f, 3.0f, 8.0f};
    GNg::sVEC3 fCol = {0.50f, 0.50f, 0.60f};
    lightMgr->AddPoint(fPos, 25.0f, fCol, 0.7f);
    // 后方补光
    GNg::sVEC3 bPos = {0.0f, 2.0f, -5.0f};
    GNg::sVEC3 bCol = {0.40f, 0.40f, 0.55f};
    lightMgr->AddPoint(bPos, 20.0f, bCol, 0.5f);
}

GN_CALL void AppUnInit() {
    delete groundPlane;  delete cubeRed;   delete cubeBlue;
    delete cubeCyan;     delete sphereBlue; delete sphereGold;
    delete centralCube;  delete lightMgr;
}

GN_CALL void AppDisplay2D(unsigned int nTime) {
    (void)nTime;

    if (autoRotate) camYaw += rotSpeed * g_dt;

    float cp = cosf(camPitch), sp = sinf(camPitch);
    float sy = sinf(camYaw),   cy = cosf(camYaw);
    GNg::sVEC3 pos;
    pos.x = camTarget.x + camDistance * cp * sy;
    pos.y = camTarget.y + camDistance * sp;
    pos.z = camTarget.z + camDistance * cp * cy;
    GNg::sVEC3 up = {0.0f, 1.0f, 0.0f};

    GNg::sMAT44 vm, pm;
    GNg::Mat44LookAtRH(&vm, &pos, &camTarget, &up);
    GNg::Mat44PerspectiveVK(&pm, 0.785398f, 800.0f/600.0f, 0.1f, 100.0f);
    GNg::sMAT44 vp;
    GNg::Mat44Mul(&vp, &pm, &vm);

    // Blinn-Phong Pass (含天空盒)
    GNg::GN_VkBegin3DPass(&vp, &pos,
                           lightMgr->GetUBO(),
                           sizeof(GN3d::sLIGHT_UBO));

    // 天空盒：在 3D pass 内、模型之前绘制（深度写在远平面，不遮挡模型）
    GNg::GN_VkDrawSkybox(&vp);

    // 球体先画(最大缓冲)
    sphereBlue ->Draw();
    sphereGold ->Draw();
    cubeRed    ->Draw();
    cubeBlue   ->Draw();
    cubeCyan   ->Draw();

    centralRot += 1.0f * g_dt;
    GNg::sVEC3 ax = {0.0f, 1.0f, 0.0f};
    GNg::sQUAT q;
    GNg::QuatFromAxisAngle(&q, &ax, centralRot);
    centralCube->SetRotation(q);
    centralCube->Draw();

    groundPlane->Draw();

    GNg::GN_VkEnd3DPass();
}

GN_CALL void AppDisplay3D(unsigned int nT) { (void)nT; }

GN_CALL void AppRun(unsigned int nTime) {
    static unsigned int last = 0;
    g_dt = (float)(nTime - last) / 1000.0f;
    last = nTime;
    if (g_dt <= 0.0f || g_dt > 0.5f) g_dt = 0.016f;

    float orb = 1.5f * g_dt;
    if (GN::Key[GK_W]) camPitch += orb;
    if (GN::Key[GK_S]) camPitch -= orb;
    if (GN::Key[GK_A]) camYaw   -= orb;
    if (GN::Key[GK_D]) camYaw   += orb;
    float pan = 2.0f * g_dt;
    if (GN::Key[GK_UP])   camTarget.y += pan;
    if (GN::Key[GK_DOWN]) camTarget.y -= pan;
    if (GN::Mouse[GM_LEFT] && GN::MouseM) {
        camTarget.x -= GNg::ViewCur->mouseMSX * 0.02f;
        camTarget.z -= GNg::ViewCur->mouseMSY * 0.02f;
    }
    if (GN::Key[GK_ADD])      camDistance -= 4.0f*g_dt;
    if (GN::Key[GK_SUBTRACT]) camDistance += 4.0f*g_dt;
    if (GN::Mouse[GM_TOP])    camDistance -= 0.5f;
    if (GN::Mouse[GM_BOTTOM]) camDistance += 0.5f;

    camPitch    = Clamp(camPitch,  -1.45f, 1.45f);
    camDistance = Clamp(camDistance, 2.0f, 30.0f);

    if (GN::Key1[GK_SPACE]) autoRotate = !autoRotate;
    if (GN::Key1[GK_R]) {
        camYaw=0; camPitch=0.25f; camDistance=8.0f;
        camTarget.x=0; camTarget.y=1; camTarget.z=0;
    }
    if (GN::Key1[GK_1]) rotSpeed=0.2f;
    if (GN::Key1[GK_2]) rotSpeed=0.5f;
    if (GN::Key1[GK_3]) rotSpeed=1.0f;
    if (GN::Key1[GK_4]) rotSpeed=2.0f;
}

} // extern "C"
