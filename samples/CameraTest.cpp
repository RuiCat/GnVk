// ╔══════════════════════════════════════════════════════════╗
// ║  CameraTest — FPS 摄像机操作测试                        ║
// ║  验证键盘/鼠标/滚轮输入的完整实现                        ║
// ╚══════════════════════════════════════════════════════════╝

#include <GN.h>
#ifdef GN_USING_3D
#include "GN_3Dlight.h"
#include "GN_3Dmodel.h"
using namespace GNg;
using namespace GN3d;

// ═══════════════════════════════════════════════════════════
//  场景对象
//  ★ 引擎前向渲染使用共用顶点/索引缓冲, 所有 vkCmdDraw*
//  执行时读到同一位移的数据 → 最后一个 draw 的顶点被所有 draw 共用。
//  解决: 全部物体用同一个立方体网格, 通过矩阵变换改变形状。
// ═══════════════════════════════════════════════════════════
static cMODEL *gCube  = nullptr;  // 立方体网格 (唯一几何体)
static cLIGHT *gLight = nullptr;  // 光源

// ═══════════════════════════════════════════════════════════
//  摄像机状态 (四元数旋转 — 无万向锁)
// ═══════════════════════════════════════════════════════════
static float gCamX   = 0.0f;
static float gCamY   = 5.0f;
static float gCamZ   = 15.0f;
static sQUAT gCamRot;             // 摄像机朝向四元数 (初始化为单位四元数)
static float gFov    = 0.785f;    // 视场角 (45°)
static bool  gCursorLocked = true;

// ── 灵敏度 / 速度 ──
static const float MOUSE_SENS = 0.008f;   // 约 0.46°/像素 (提升响应)
static const float MOVE_SPEED = 0.10f;     // 移动速度
static const float BOOST_MULT = 3.0f;
static const float SCROLL_SENS = 0.05f;

// ── 场景配置 ──
static const int   GRID_SIZE     = 7;      // 7x7 立方体阵列
static const float GRID_SPACING  = 3.0f;

// 散布的标记物位置 (立方体)
static const float gMarkers[][3] = {
    { -6.0f, 0.5f, -6.0f }, {  6.0f, 0.5f, -5.0f },
    { -4.0f, 0.5f,  7.0f }, {  7.0f, 0.5f,  3.0f },
    { -8.0f, 0.5f,  2.0f }, {  3.0f, 0.5f, -7.0f },
    {  0.0f, 0.5f,  0.0f }, { -5.0f, 0.5f, -3.0f },
    {  5.0f, 0.5f,  6.0f }, { -7.0f, 0.5f, -5.0f },
    {  8.0f, 0.5f, -2.0f }, { -2.0f, 0.5f,  8.0f },
};
static const int MARKER_COUNT = sizeof(gMarkers) / sizeof(gMarkers[0]);

// ═══════════════════════════════════════════════════════════
//  辅助: 组装 TRS 矩阵 (翻译 + 单位旋转 + 缩放)
// ═══════════════════════════════════════════════════════════
static void MakeTransform(sMAT44 *out, float x, float y, float z, float scale) {
    Mat44Identity(out);
    out->m[0]  = scale;
    out->m[5]  = scale;
    out->m[10] = scale;
    out->m[12] = x;
    out->m[13] = y;
    out->m[14] = z;
}

extern "C" {

// ═══════════════════════════════════════════════════════════
//  AppInit — 创建场景资源
// ═══════════════════════════════════════════════════════════
GN_CALL void AppInit() {
    // ── 启用 FPS 鼠标模式: GLFW CURSOR_DISABLED ──
    glfwSetInputMode(GT_MainWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // ── 初始化摄像机朝向: 从 (0,5,15) 看向原点 ──
    sVEC3 initAxis = {0.0f, 1.0f, 0.0f};
    QuatFromAxisAngle(&gCamRot, &initAxis, 3.14159f);  // Y轴旋转180° → 看向 -Z

    // ── 创建光源: 方向光 + 环境光 ──
    gLight = new cLIGHT();
    gLight->AddDirectional(
        sVEC3{0.4f, -1.0f, 0.3f},    // 光照方向 (指向光源的相反方向)
        sVEC3{1.0f, 0.95f, 0.85f},    // 暖色调
        1.2f);                          // 强度
    gLight->SetAmbient(sVEC3{0.2f, 0.22f, 0.28f}, 1.0f);

    // ── 唯一的立方体网格 (所有物体共用) ──
    gCube = CreateCubeModel(1.0f, 0.8f, 0.8f, 0.8f);
    gCube->UploadAllToGPU();
}

// ═══════════════════════════════════════════════════════════
//  AppUnInit — 释放资源
// ═══════════════════════════════════════════════════════════
GN_CALL void AppUnInit() {
    if (gCube)  { gCube->ReleaseGPU();  delete gCube;  gCube  = nullptr; }
    if (gLight) { delete gLight; gLight = nullptr; }
}

// ═══════════════════════════════════════════════════════════
//  AppRun — 每帧逻辑更新 (摄像机控制)
// ═══════════════════════════════════════════════════════════
GN_CALL void AppRun(unsigned int nTime) {
    (void)nTime;

    if (!gLight) return;

    // ── 1. 鼠标: 切换光标捕获模式 (右键单击) ──
    if (GN::Mouse1[GM_RIGHT]) {
        gCursorLocked = !gCursorLocked;
        if (gCursorLocked) {
            glfwSetInputMode(GT_MainWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else {
            glfwSetInputMode(GT_MainWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }

    // ── 2. 鼠标: 四元数视角旋转 — 世界空间轴, 左侧乘 ──
    //  偏航: 绕世界 Y 轴      pitchQ * yawQ * gCamRot
    //  俯仰: 绕摄像机当前右向量 (从 quat 提取到世界空间)
    {
        static int  gPrevMX = 0, gPrevMY = 0;
        static bool gMouseInit = false;
        if (!gMouseInit) {
            gPrevMX = GN::MouseMX; gPrevMY = GN::MouseMY;
            gMouseInit = true;
        }
        float rawDX = (float)(GN::MouseMX - gPrevMX);
        float rawDY = (float)(GN::MouseMY - gPrevMY);
        gPrevMX = GN::MouseMX;
        gPrevMY = GN::MouseMY;

        if (gCursorLocked && (rawDX != 0.0f || rawDY != 0.0f)) {
            float dx = rawDX * MOUSE_SENS;
            float dy = rawDY * MOUSE_SENS;

            // 偏航: 绕世界 Y (0,1,0)
            sQUAT yawQ;
            sVEC3 worldY = {0.0f, 1.0f, 0.0f};
            QuatFromAxisAngle(&yawQ, &worldY, -dx);

            // 俯仰: 从当前四元数提取摄像机的世界空间右向量
            sVEC3 worldRight;
            QuatRotateVec3(&worldRight, &sVEC3{1.0f, 0.0f, 0.0f}, &gCamRot);
            sQUAT pitchQ;
            QuatFromAxisAngle(&pitchQ, &worldRight, -dy);

            // 两者均为世界空间旋转 → 均左侧乘 (先俯仰后偏航)
            gCamRot = yawQ * pitchQ * gCamRot;
            QuatNormalize(&gCamRot, &gCamRot);
        }
    }

    // ── 3. 键盘: 移动速度 (Shift 加速) ──
    float speed = MOVE_SPEED;
    if (GN::Key[GK_LSHIFT] || GN::Key[GK_RSHIFT]) speed *= BOOST_MULT;

    // ── 4. 键盘: WASD 移动 (基于四元数提取的前向/右向) ──
    sVEC3 camFwd, camRight;
    QuatRotateVec3(&camFwd,   &sVEC3{0.0f, 0.0f, 1.0f}, &gCamRot);
    QuatRotateVec3(&camRight, &sVEC3{1.0f, 0.0f, 0.0f}, &gCamRot);

    if (GN::Key[GK_W]) { gCamX += camFwd.x * speed; gCamZ += camFwd.z * speed; }
    if (GN::Key[GK_S]) { gCamX -= camFwd.x * speed; gCamZ -= camFwd.z * speed; }
    if (GN::Key[GK_A]) { gCamX -= camRight.x * speed; gCamZ -= camRight.z * speed; }
    if (GN::Key[GK_D]) { gCamX += camRight.x * speed; gCamZ += camRight.z * speed; }

    // ── 5. 键盘: Q/E 垂直升降 ──
    if (GN::Key[GK_Q]) gCamY -= speed;
    if (GN::Key[GK_E]) gCamY += speed;

    // ── 6. 键盘: 方向键替代 WASD ──
    if (GN::Key[GK_UP])    { gCamX += camFwd.x * speed; gCamZ += camFwd.z * speed; }
    if (GN::Key[GK_DOWN])  { gCamX -= camFwd.x * speed; gCamZ -= camFwd.z * speed; }
    if (GN::Key[GK_LEFT])  { gCamX -= camRight.x * speed; gCamZ -= camRight.z * speed; }
    if (GN::Key[GK_RIGHT]) { gCamX += camRight.x * speed; gCamZ += camRight.z * speed; }

    // ── 7. 鼠标滚轮: 调整 FOV ──
    if (GN::Mouse1[GM_TOP]) {    // 滚轮上滚 → 缩小 FOV (放大)
        gFov -= SCROLL_SENS;
        if (gFov < 0.2f) gFov = 0.2f;
    }
    if (GN::Mouse1[GM_BOTTOM]) { // 滚轮下滚 → 扩大 FOV (缩小)
        gFov += SCROLL_SENS;
        if (gFov > 2.0f) gFov = 2.0f;
    }

    // ── 8. 键盘: R 重置摄像机 ──
    if (GN::Key1[GK_R]) {
        gCamX = 0.0f;  gCamY = 5.0f;  gCamZ = 15.0f;
        sVEC3 axis = {0.0f, 1.0f, 0.0f};
        QuatFromAxisAngle(&gCamRot, &axis, 3.14159f);  // 看向原点
        gFov = 0.785f;
    }

    // ── 9. 键盘: ESC 退出 ──
    if (GN::Key1[GK_ESCAPE]) {
        GN_GameBreakLoop();
    }
}

// ═══════════════════════════════════════════════════════════
//  AppDisplay2D — 3D 渲染 + 2D HUD 叠加
// ═══════════════════════════════════════════════════════════
GN_CALL void AppDisplay2D(unsigned int nTime) {
    (void)nTime;
    if (!gLight || !gCube) return;

    // ── 视口尺寸 (跟随窗口 resize) ──
    float vw = (float)(GN_Vk.viewWidth  > 0 ? GN_Vk.viewWidth  : 800);
    float vh = (float)(GN_Vk.viewHeight > 0 ? GN_Vk.viewHeight : 600);
    float aspect = vw / vh;

    // ═══════════════════════════════════════
    //  构建 ViewProj 矩阵
    //  从四元数提取前向和上向量 (代替固定 (0,1,0) 消除 ±90° 奇异性)
    // ═══════════════════════════════════════
    sVEC3 camFwd, camUp;
    QuatRotateVec3(&camFwd, &sVEC3{0.0f, 0.0f, 1.0f}, &gCamRot);
    QuatRotateVec3(&camUp,  &sVEC3{0.0f, 1.0f, 0.0f}, &gCamRot);

    sVEC3 eye    = { gCamX, gCamY, gCamZ };
    sVEC3 center = { eye.x + camFwd.x, eye.y + camFwd.y, eye.z + camFwd.z };

    sMAT44 view, proj, vp;
    Mat44LookAtRH(&view, &eye, &center, &camUp);
    Mat44PerspectiveVK(&proj, gFov, aspect, 0.1f, 200.0f);
    Mat44Mul(&vp, &proj, &view);

    // ═══════════════════════════════════════
    //  ★ 2D HUD 必须先于 3D 渲染 ★
    //  原因: GN_VkBegin3DPass 会结束 2D 渲染通道并开始 3D 通道,
    //        GN_VkEnd3DPass 结束后 s_inRenderPass=false,
    //        此后若再绘制 2D 内容会导致 vkCmdDraw 在无渲染通道时执行 → 崩溃
    // ═══════════════════════════════════════
    char buf[256];
    int  ly = 24;
    const int LH = 18;
    const unsigned int COL_TITLE = 0xffFFD700;
    const unsigned int COL_INFO  = 0xffCCCCCC;
    const unsigned int COL_WARN  = 0xffFF6B6B;

    GN_RunTextOut("=== FPS Camera Test ===", 0, 16, ly, 0, 0, COL_TITLE, eBLEND_NORMAL);
    ly += LH + 4;

    sprintf(buf, "Position:  (%.1f,  %.1f,  %.1f)", gCamX, gCamY, gCamZ);
    GN_RunTextOut(buf, 0, 16, ly, 0, 0, COL_INFO, eBLEND_NORMAL);  ly += LH;

    // 从前向向量反算偏航/俯仰 (仅用于显示, 复用上面 camFwd)
    float dispYaw   = atan2f(camFwd.x, camFwd.z) * 57.29578f;
    float dispPitch = asinf(camFwd.y) * 57.29578f;
    sprintf(buf, "Rotation:  Yaw=%.1f°  Pitch=%.1f°  [quaternion]",
            dispYaw, dispPitch);
    GN_RunTextOut(buf, 0, 16, ly, 0, 0, COL_INFO, eBLEND_NORMAL);  ly += LH;

    sprintf(buf, "FOV:       %.0f°", gFov * 57.29578f);
    GN_RunTextOut(buf, 0, 16, ly, 0, 0, COL_INFO, eBLEND_NORMAL);  ly += LH;

    sprintf(buf, "Cursor:    %s  |  Mouse: (%d, %d)",
            gCursorLocked ? "LOCKED" : "FREE",
            GN::MouseMX, GN::MouseMY);
    GN_RunTextOut(buf, 0, 16, ly, 0, 0,
                  gCursorLocked ? 0xff44FF44 : COL_WARN, eBLEND_NORMAL);
    ly += LH;

    {
        static int prevHudMX = 0, prevHudMY = 0;
        static bool hudInit = false;
        if (!hudInit) { prevHudMX = GN::MouseMX; prevHudMY = GN::MouseMY; hudInit = true; }
        float hudDX = (float)(GN::MouseMX - prevHudMX);
        float hudDY = (float)(GN::MouseMY - prevHudMY);
        prevHudMX = GN::MouseMX; prevHudMY = GN::MouseMY;
        sprintf(buf, "Mouse Δ:   (%.0f, %.0f)  raw=(%d,%d)",
                hudDX, hudDY, GN::MouseMX, GN::MouseMY);
        GN_RunTextOut(buf, 0, 16, ly, 0, 0, COL_INFO, eBLEND_NORMAL);
    }
    ly += LH + 8;

    GN_RunTextOut("─── Controls ───", 0, 16, ly, 0, 0, COL_TITLE, eBLEND_NORMAL);
    ly += LH;

    const char *controls[] = {
        "W/A/S/D or Arrows: Move",
        "Q/E: Down / Up",
        "Mouse Move: Look around",
        "Mouse Wheel: Zoom (FOV)",
        "Shift+Move: Boost speed",
        "Right Click: Toggle cursor lock",
        "R: Reset camera",
        "Esc: Exit",
    };
    for (int i = 0; i < 8; ++i) {
        GN_RunTextOut(controls[i], 0, 16, ly, 0, 0, 0xffAAAAAA, eBLEND_NORMAL);
        ly += LH;
    }

    if (GN::Mouse1[GM_TOP] || GN::Mouse1[GM_BOTTOM]) {
        sprintf(buf, ">>> Scroll: %s <<<",
                GN::Mouse1[GM_TOP] ? "UP" : "DOWN");
        GN_RunTextOut(buf, 0,
            (short)(vw * 0.35f), (short)(vh - 40),
            0, 0, COL_TITLE, eBLEND_NORMAL);
    }

    // ═══════════════════════════════════════
    //  3D 渲染 Pass (前向渲染) — 必须在 2D 之后
    // ═══════════════════════════════════════
    GN_VkBegin3DPass(&vp, &eye,
                     gLight->GetUBO(), sizeof(sLIGHT_UBO));

    sMESH &mesh = gCube->aMeshes[0];  // 唯一网格, 36顶点/36索引
    sMAT44 mdl;

    // ★ 所有物体共用 cube mesh → 缓冲始终是立方体数据, 无覆盖问题

    // ── 地板: 扁平宽立方体 ──
    MakeTransform(&mdl, 0.0f, -0.05f, 0.0f, 1.0f);
    mdl.m[5]  = 0.05f;  // Y 压缩到 5%
    mdl.m[0]  = 20.0f;  // X 扩到 20
    mdl.m[10] = 20.0f;  // Z 扩到 20
    GN_VkDraw3D(mesh.aVertices, mesh.aVertexCount,
                mesh.aIndices,  mesh.aIndexCount,
                &mdl, 0.25f, 0.55f, 0.20f,   // 绿褐色
                0.1f, 0.1f, 0.1f, 8.0f);

    // ── 散布的标记物 (小立方体, 多种颜色) ──
    for (int i = 0; i < MARKER_COUNT; ++i) {
        MakeTransform(&mdl, gMarkers[i][0], gMarkers[i][1], gMarkers[i][2], 0.4f);
        float hue = (float)i / (float)MARKER_COUNT;
        GN_VkDraw3D(mesh.aVertices, mesh.aVertexCount,
                    mesh.aIndices,  mesh.aIndexCount,
                    &mdl,
                    0.7f + 0.3f * hue,
                    0.2f + 0.5f * (1.0f - hue),
                    0.15f,
                    0.8f, 0.8f, 0.8f, 64.0f);
    }

    // ── 立方体阵列 (多彩网格 7x7) ──
    int half = GRID_SIZE / 2;
    for (int ix = -half; ix <= half; ++ix) {
        for (int iz = -half; iz <= half; ++iz) {
            float px = (float)ix * GRID_SPACING;
            float pz = (float)iz * GRID_SPACING;
            float cr = (float)(ix + half) / (float)(GRID_SIZE - 1);
            float cg = (float)(iz + half) / (float)(GRID_SIZE - 1);
            float cb = 1.0f - (cr + cg) * 0.5f;

            if (ix == 0 && iz == 0) {
                // ★ 中心红塔: 5层堆叠立方体 (最后画, 数据留在缓冲中)
                for (int stack = 0; stack < 5; ++stack) {
                    float sc = (stack == 4) ? 1.5f : 1.0f;
                    MakeTransform(&mdl, px, 0.5f + (float)stack, pz, sc);
                    GN_VkDraw3D(mesh.aVertices, mesh.aVertexCount,
                                mesh.aIndices,  mesh.aIndexCount,
                                &mdl,
                                stack == 4 ? 1.0f : 0.9f,
                                stack == 4 ? 0.1f : 0.2f,
                                stack == 4 ? 0.1f : 0.2f,
                                0.6f, 0.6f, 0.6f, 64.0f);
                }
            } else {
                MakeTransform(&mdl, px, 0.5f, pz, 1.0f);
                GN_VkDraw3D(mesh.aVertices, mesh.aVertexCount,
                            mesh.aIndices,  mesh.aIndexCount,
                            &mdl, cr, cg, cb,
                            0.3f, 0.3f, 0.3f, 16.0f);
            }
        }
    }

    GN_VkEnd3DPass();
}

// ═══════════════════════════════════════════════════════════
//  AppDisplay3D — 未使用 (3D 渲染在 AppDisplay2D 中完成)
// ═══════════════════════════════════════════════════════════
GN_CALL void AppDisplay3D(unsigned int nTime) {
    (void)nTime;
}

} // extern "C"

#else // !GN_USING_3D
// ── 3D 未启用时的降级桩 ──
extern "C" {
GN_CALL void AppInit()   {}
GN_CALL void AppUnInit() {}
GN_CALL void AppDisplay2D(unsigned int nTime) { (void)nTime; }
GN_CALL void AppDisplay3D(unsigned int nTime) { (void)nTime; }
GN_CALL void AppRun(unsigned int nTime)      { (void)nTime; }
}
#endif
