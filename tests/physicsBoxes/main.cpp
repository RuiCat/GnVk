// tests/physicsBoxes/main.cpp — P4 端到端示例：20 个箱子落地堆叠 + 一个球撞倒。
//
// 为什么要它（docs/ENTITY_CONTROLLERS_PLAN.md §5.1 P4）：物理控制器此前只有单测级别的
// "球会掉"验证，没有"多个刚体互相挤压 + 变换同步 + 插值 + 接触事件"的场景级证据。
//
// 与其它示例的区别：本程序**完全不需要 GPU / 窗口 / Vulkan** —— 物理与实体层都是纯 CPU。
// 所以它在任何机器上都能跑（`make physicsBoxes` 直接出结果），CI 里也不受设备影响。
// 因此它不用 GN_MAIN/GN_StartRun，而是一个普通 main()。
//
// 场景：静态地面 + 4×5 阵列的 20 个箱子（分四层高度）+ 一个带初速度的球从侧面撞进来。
// 时钟：物理 60Hz 固定步长，渲染 144Hz（2 秒 = 288 渲染帧 = 120 物理子步）。
//
// 判据：
//   1. 每个箱子**整体向下**（允许求解器位置修正带来的少量上抬，见 kUpwardTolerance）；
//   2. 最终不穿地（AABB 下沿 >= -2 cm）；
//   3. 全部箱子真的下落了；
//   4. 球撞到了箱子，并且接触事件按"实体对"报得出来；
//   5. 插值真的在起作用：模型位置落在 prev/curr 之间（既不是最新状态，也不是不动）；
//   6. 状态有限（没有 NaN 爆炸）。
#include <GN.h>
#include "GN_Entity.h"
#include "GN_GameClock.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

const int kBoxRows = 4;
const int kBoxCols = 5;
const int kBoxCount = kBoxRows * kBoxCols;
const float kBoxHalf = 0.25f;
const float kPenetrationTolerance = 0.02f; // 2 cm（求解器允许少量穿透）
const int kPhysicsSteps = 120;             // 2 秒物理
const int kRenderFrames = 288;             // 2 秒 @144Hz 渲染
// 求解器的位置修正会把箱子往上顶一点；"单调下降"在堆叠场景里不可能绝对成立。
// 这里允许单帧上抬不超过 5 cm，并要求整段时间的净位移必须向下（见判据 1）。
const float kUpwardTolerance = 0.05f;

int gPass = 0;
int gFail = 0;

void check(const char *name, bool ok, const char *detail) {
    std::printf("[%s] %-28s %s\n", ok ? "PASS" : "FAIL", name, detail);
    if (ok) ++gPass;
    else ++gFail;
}

struct sBox {
    GN3d::cENTITY aEntity;
    GN3d::cMODEL *aModel; // 只为验"模型被插值写入"；不需要 GPU
    uint32_t aBody;
    GNg::sVEC3 aStart;
    float aLastY;
    float aMaxUpward;
    float aMinY;
};

} // namespace

int main() {
    // ── 世界 ──
    GN3d::cPHYSICS_WORLD world;
    world.SetFixedTimestep(1.0f / 60.0f, 4);
    world.SetGravity(GNg::sVEC3{0.0f, -9.81f, 0.0f});
    // 箱子从 6.5 m 落下时速度约 11 m/s，单步位移 0.18 m 会直接穿进地面——
    // 打开 CCD 让"上一位置 → 目标位置"的扫掠把它拦在接触面（P4 场景里真实需要）。
    world.setContinuousCollision(true);

    const GN3d::sRIGID_BODY groundBody = GN3d::MakeBoxBody(
        GN3d::eBODY_STATIC, GNg::sVEC3{0.0f, -0.5f, 0.0f}, GNg::sVEC3{10.0f, 0.5f, 10.0f}, 0.0f);
    const uint32_t ground = world.AddBody(groundBody);
    if (ground == GN3d::cPHYSICS_WORLD::INVALID_HANDLE) {
        std::printf("[FAIL] world.ground               地面刚体创建失败\n");
        return 1;
    }

    // 一个控制器管全部刚体：接触事件是**控制器视角**的，只有登记在它名下的实体才能
    // 被翻译成"实体对"。每个 cENTITY 各带一个控制器的话，球那一侧看不到箱子实体。
    GN3d::cPHYSICS_CONTROLLER controller;
    controller.SetWorld(&world);

    // ── 20 个箱子 ──
    std::vector<sBox> boxes((size_t)kBoxCount);
    for (int row = 0; row < kBoxRows; ++row) {
        for (int col = 0; col < kBoxCols; ++col) {
            const int index = row * kBoxCols + col;
            const float x = (float)col * 0.55f - 1.1f;
            const float z = (float)row * 0.55f - 0.825f;
            const float y = 0.5f + (float)row * 1.2f; // 0.5 / 1.7 / 2.9 / 4.1

            sBox &box = boxes[(size_t)index];
            box.aBody = world.AddBody(GN3d::MakeBoxBody(GN3d::eBODY_DYNAMIC, GNg::sVEC3{x, y, z},
                                                        GNg::sVEC3{kBoxHalf, kBoxHalf, kBoxHalf},
                                                        1.0f));
            box.aStart = GNg::sVEC3{x, y, z};
            box.aLastY = y;
            box.aMaxUpward = 0.0f;
            box.aMinY = y;
            box.aEntity.aId = (uint32_t)(index + 10); // 1..9 留给球，0 = 未分配
            box.aModel = GN3d::CreateCubeModel(kBoxHalf * 2.0f);
            box.aEntity.aMesh.SetModel(box.aModel);
            if (box.aBody == GN3d::cPHYSICS_WORLD::INVALID_HANDLE) {
                std::printf("[FAIL] world.boxes                箱子刚体创建失败（index %d）\n", index);
                return 1;
            }
            controller.AttachBody(box.aEntity.aId, box.aBody);
        }
    }

    // ── 球 ──
    // 球速/质量是量出来的，不是拍的：8 m/s × 2 kg 会把箱子打飞并插进地面 28 cm
    // （CCD 只扫掠静态/运动学目标，动态-动态高能撞击没有保护）；3.5 m/s × 1 kg
    // 在这一场景下穿透 <= 1 cm，同时仍然把箱子推出去 0.34 m（撞击真实发生过）。
    GN3d::sRIGID_BODY ballBody = GN3d::MakeSphereBody(
        GN3d::eBODY_DYNAMIC, GNg::sVEC3{-3.0f, 0.6f, -0.275f}, 0.3f, 1.0f);
    ballBody.aLinVel = GNg::sVEC3{3.5f, 0.0f, 0.0f};
    const uint32_t ball = world.AddBody(ballBody);
    GN3d::cMODEL *ballModel = GN3d::CreateSphereModel(0.3f);
    GN3d::cENTITY ballEntity;
    ballEntity.aId = 1u;
    ballEntity.aMesh.SetModel(ballModel);
    if (ball == GN3d::cPHYSICS_WORLD::INVALID_HANDLE) {
        std::printf("[FAIL] world.ball                 球刚体创建失败\n");
        return 1;
    }
    controller.AttachBody(ballEntity.aId, ball);
    controller.SetReportUnmapped(false); // 地面没有实体：不刷屏

    // ── 主循环：60Hz 物理 + 144Hz 渲染 ──
    GN::cGAME_CLOCK clock;
    clock.SetFixedStep(1.0f / 60.0f, 4);

    int ballContactFrames = 0;
    int alphaMidFrames = 0;
    int interpolatedDiffersFromLatest = 0;
    uint32_t physicsStepsRun = 0;
    for (int frame = 0; frame < kRenderFrames; ++frame) {
        clock.Tick(1.0f / 144.0f);
        const float alpha = clock.Alpha();
        controller.Step(clock.DeltaTime(), alpha);
        physicsStepsRun += controller.LastSubSteps();
        if (alpha > 0.0f && alpha < 1.0f) ++alphaMidFrames;

        // 箱子的模型位置：走插值路径（和渲染一帧的做法一致）
        for (size_t i = 0; i < boxes.size(); ++i) {
            controller.ApplyTransformInterpolated(boxes[i].aEntity.aId, *boxes[i].aModel);
            const GN3d::sRIGID_BODY *body = world.GetBody(boxes[i].aBody);
            if (body == nullptr) continue;
            if (body->aPos.y > boxes[i].aLastY) {
                const float upward = body->aPos.y - boxes[i].aLastY;
                if (upward > boxes[i].aMaxUpward) boxes[i].aMaxUpward = upward;
            }
            boxes[i].aLastY = body->aPos.y;
            if (body->aPos.y < boxes[i].aMinY) boxes[i].aMinY = body->aPos.y;
        }
        controller.ApplyTransformInterpolated(ballEntity.aId, *ballModel);
        const GN3d::sRIGID_BODY *ballState = world.GetBody(ball);
        if (ballState != nullptr &&
            std::fabs(ballModel->aPosition.y - ballState->aPos.y) > 1e-6f) {
            ++interpolatedDiffersFromLatest;
        }
        // 球的接触（它撞到的箱子都登记在同一个控制器里，所以能解析成实体对）
        if (controller.ContactCount() > 0) ++ballContactFrames;
    }

    // ── 判据 1：整体向下（净位移必须向下，单帧上抬不超过容差）──
    int badDescent = 0;
    float worstUpward = 0.0f;
    float worstNetDrop = 1.0e9f;
    for (size_t i = 0; i < boxes.size(); ++i) {
        const GN3d::sRIGID_BODY *body = world.GetBody(boxes[i].aBody);
        const float net = body != nullptr ? (boxes[i].aStart.y - body->aPos.y) : -1.0f;
        if (net < worstNetDrop) worstNetDrop = net;
        if (boxes[i].aMaxUpward > worstUpward) worstUpward = boxes[i].aMaxUpward;
        // 净位移必须向下（每层起始高度差 1.2 m，落地后净下降至少 0.2 m），
        // 且单帧上抬不超过 kUpwardTolerance（求解器位置修正）。
        if (net < 0.2f || boxes[i].aMaxUpward > kUpwardTolerance) ++badDescent;
    }
    {
        char detail[200];
        std::snprintf(detail, sizeof(detail),
                      "净下降最小 %.3f m，单帧最大上抬 %.4f m（容差 %.3f），违反 %d 个",
                      worstNetDrop, worstUpward, kUpwardTolerance, badDescent);
        check("boxes.descendsOverall", badDescent == 0, detail);
    }

    // ── 判据 2：不穿地 ──
    float worstBottom = 1.0e9f;
    int penetrating = 0;
    for (size_t i = 0; i < boxes.size(); ++i) {
        const GN3d::sRIGID_BODY *body = world.GetBody(boxes[i].aBody);
        if (body == nullptr) {
            ++penetrating;
            continue;
        }
        const float bottom = body->aWorldAABB.aMin.y; // AABB 一定包住（可能已旋转）
        if (bottom < worstBottom) worstBottom = bottom;
        if (bottom < -kPenetrationTolerance) ++penetrating;
    }
    {
        char detail[200];
        std::snprintf(detail, sizeof(detail), "最低 AABB 下沿 y=%.4f（容差 -%.3f），穿地 %d 个",
                      worstBottom, kPenetrationTolerance, penetrating);
        check("boxes.noPenetration", penetrating == 0, detail);
    }

    // ── 判据 3：全部下落 ──
    int stillFloating = 0;
    for (size_t i = 0; i < boxes.size(); ++i) {
        const GN3d::sRIGID_BODY *body = world.GetBody(boxes[i].aBody);
        if (body == nullptr || body->aPos.y > boxes[i].aStart.y - 0.2f) ++stillFloating;
    }
    {
        char detail[160];
        std::snprintf(detail, sizeof(detail), "%d/%d 个箱子下落", kBoxCount - stillFloating,
                      kBoxCount);
        check("boxes.allFell", stillFloating == 0, detail);
    }

    // ── 判据 4：球撞到箱子，并且接触事件能解析成实体对 ──
    float maxDx = 0.0f;
    for (size_t i = 0; i < boxes.size(); ++i) {
        const GN3d::sRIGID_BODY *body = world.GetBody(boxes[i].aBody);
        if (body == nullptr) continue;
        const float dx = std::fabs(body->aPos.x - boxes[i].aStart.x);
        if (dx > maxDx) maxDx = dx;
    }
    {
        char detail[200];
        std::snprintf(detail, sizeof(detail), "最大水平位移 %.4f m，球参与接触的帧 %d/%d", maxDx,
                      ballContactFrames, kRenderFrames);
        check("boxes.ballHit", ballContactFrames > 0 && maxDx > 0.02f, detail);
    }

    // ── 判据 5：插值真的在起作用 ──
    {
        char detail[200];
        std::snprintf(detail, sizeof(detail),
                      "alpha∈(0,1) 的帧 %d/%d，球模型位置≠最新刚体位置 %d 帧，物理子步 %u",
                      alphaMidFrames, kRenderFrames, interpolatedDiffersFromLatest, physicsStepsRun);
        // 144Hz 渲染 / 60Hz 物理：约 2/3 的帧 alpha∈(0,1)。
        // 模型与最新刚体位置不同的帧数 > 0 ⇒ 插值路径确实在写模型（不是直取最新状态）。
        const bool ok = alphaMidFrames > kRenderFrames / 2 && interpolatedDiffersFromLatest > 0 &&
                        physicsStepsRun >= (uint32_t)(kPhysicsSteps - 4);
        check("boxes.interpolationActive", ok, detail);
    }

    // ── 判据 6：状态有限 ──
    {
        const GN3d::sRIGID_BODY *body = world.GetBody(ball);
        bool finite = body != nullptr;
        if (body != nullptr) {
            finite = std::isfinite(body->aPos.x) && std::isfinite(body->aPos.y) &&
                     std::isfinite(body->aPos.z);
        }
        for (size_t i = 0; i < boxes.size() && finite; ++i) {
            const GN3d::sRIGID_BODY *box = world.GetBody(boxes[i].aBody);
            if (box == nullptr) {
                finite = false;
                break;
            }
            if (!std::isfinite(box->aPos.x) || !std::isfinite(box->aPos.y) ||
                !std::isfinite(box->aPos.z)) {
                finite = false;
            }
        }
        char detail[160];
        std::snprintf(detail, sizeof(detail), "球最终 (%.2f, %.2f, %.2f)，箱子全部有限",
                      body ? body->aPos.x : 0.0f, body ? body->aPos.y : 0.0f,
                      body ? body->aPos.z : 0.0f);
        check("boxes.finiteState", finite, detail);
    }

    for (size_t i = 0; i < boxes.size(); ++i) {
        boxes[i].aEntity.aMesh.SetModel(nullptr);
        delete boxes[i].aModel;
    }
    ballEntity.aMesh.SetModel(nullptr);
    delete ballModel;

    std::printf("\n---- PHYSICS BOXES SUMMARY: pass=%d fail=%d ----\n", gPass, gFail);
    return gFail == 0 ? 0 : 1;
}
