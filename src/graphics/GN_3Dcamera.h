#pragma once
#ifndef _GN_3DCAMERA_H_
#define _GN_3DCAMERA_H_
// GN_3Dcamera.h — 3D 摄像机契约：镜头参数、GN 移动原语、自由/轨道/跟随三模式与自动取景。
// 纯 CPU：不读全局输入、不依赖 Vulkan；视图/投影矩阵由自身状态按需构建。

#include "GN_3Dmath.h"
#include "GN_Spatial.h" // sAABB

namespace GN3d {

// 相机工作模式（GN 契约名保留）
enum eCAMERA_MODE { eCAMERA_MODE_FREE, eCAMERA_MODE_ORBIT, eCAMERA_MODE_FOLLOW };

// 观看方位，对齐 GN 的 eCMA_LOOK_MODE 八值
enum eCAMERA_LOOK_MODE {
    eCAMERA_LOOK_TOP,
    eCAMERA_LOOK_FRONT,
    eCAMERA_LOOK_LEFT,
    eCAMERA_LOOK_BOTTOM,
    eCAMERA_LOOK_BACK,
    eCAMERA_LOOK_RIGHT,
    eCAMERA_LOOK_DOWN,
    eCAMERA_LOOK_SWAP
};

// 视角参考方向，对齐 GN 的 eCAMERA_FOV_TYPE
enum eCAMERA_FOV_TYPE { eCAMERA_FOV_X = 1, eCAMERA_FOV_Y, eCAMERA_FOV_XY };

// 共享数值边界（弧度/距离/角度换算）
const float CAMERA_MAX_PITCH = 1.4486233f; // ±83°，与查看器的 ±1.45 rad 对齐
const float CAMERA_MIN_DISTANCE = 0.05f;
const float CAMERA_MAX_DISTANCE = 1.0e5f;
const float CAMERA_DEG_TO_RAD = 0.017453292519943f;
const float CAMERA_RAD_TO_DEG = 57.29577951308232f;

// 目标快照：调用方每帧刷新；相机不持有对象指针
struct sCAMERA_TARGET {
    bool aValid; // false = 无目标
    GNg::sVEC3 aPosition; // 世界位置（跟随/对准中心）
    sAABB aBounds; // 世界 AABB（自动取景用；空盒按点处理）
    sCAMERA_TARGET();
};

// 初始化描述
struct sCAMERA_DESC {
    GNg::sVEC3 aPosition;
    GNg::sQUAT aRotation;
    float aFovYDeg; // 默认 45
    float aNear; // 默认 0.1
    float aFar; // 默认 1000
    float aOrthoH; // 0 = 透视
    eCAMERA_MODE aMode; // 默认 FREE
    float aFollowRatio; // 0..1，默认 0.15
    sCAMERA_DESC();
};

// 控制层输入（与 GN::Key / GN::Mouse 解耦）
struct sCAMERA_INPUT {
    float aMoveFront, aMoveLeft, aMoveUp; // -1..1
    float aLookYaw, aLookPitch; // 弧度增量
    float aZoom; // 滚轮增量（+ 拉近）
    float aPanRight, aPanUp; // 轨道平移
    bool aBoost; // Shift
    sCAMERA_INPUT();
};

class cCAMERA {
public:
    // 状态（沿用 GN 的 a 前缀用户属性风格）
    GNg::sVEC3 aPosition;
    GNg::sQUAT aRotation;
    float aFovY; // 角度
    float aNear, aFar;
    float aOrthoH; // 0 = 透视
    eCAMERA_MODE aMode;

    cCAMERA();
    void Init(const sCAMERA_DESC *desc = 0);
    void Reset(); // 回默认透视 + 自由

    // 镜头（返回 false = 参数非法，状态不变）
    bool SetFov(float deg); // 同时退出正交（GN 语义）
    bool SetOrthoH(float h = 0.0f); // 0 = 回透视
    bool SetNear(float n);
    bool SetFar(float f);
    bool SetNF(float n, float f);
    bool IsOrthoMode() const;

    // GN 移动原语
    void SelfMove(float front, float left, float up);
    void SelfRotLR(float angleRad); // 正 = 右转
    void SelfRotTB(float angleRad); // 正 = 抬头
    void SelfRotCW(float angleRad);
    void SelfRotAxis(float angleRad, const GNg::sVEC3 *axis);
    void ToMove(float x, float y, float z);
    void SetWorldPos(const GNg::sVEC3 *pos);
    void GetWorldPos(GNg::sVEC3 *out) const;
    GNg::sVEC3 GetCoordV() const;
    GNg::sVEC3 Forward() const;
    GNg::sVEC3 Right() const;
    GNg::sVEC3 Up() const;

    // 轨道
    void SetOrbit(const GNg::sVEC3 &focus, float distance, float yawRad, float pitchRad);
    void OrbitRotate(float yawDelta, float pitchDelta);
    void OrbitZoom(float delta);
    void OrbitPan(float right, float up);

    // 目标/跟随
    void SetTarget(const sCAMERA_TARGET &target, float followRatio = -1.0f,
                   bool autoBalanced = true, bool autoCorrectsRot = false);
    void ClearTarget();
    bool HasTarget() const;

    // 控制层推进（不读全局输入）；aLook* 为已积分的弧度增量，lookSpeed 为灵敏度系数
    void ApplyInput(const sCAMERA_INPUT &input, float dt, float moveSpeed, float lookSpeed);
    void Update(float dt); // 轨道解算 / 跟随平滑 / 数值钳制

    // 视图（aspect 由调用方给；不改渲染接口）
    bool BuildViewProj(float aspect,
                       GNg::sMAT44 *viewOut, GNg::sMAT44 *projOut, GNg::sMAT44 *viewProjOut) const;

    // GN 静态换算
    static float FovToOrthoH(float fovDeg, float targetDistance);
    static float FovConvert(eCAMERA_FOV_TYPE dst, eCAMERA_FOV_TYPE src, float fovDeg, float whRatio);

private:
    // 轨道状态（单一事实来源：focus + 角度 + 距离）
    bool aOrbitValid;
    GNg::sVEC3 aOrbitFocus;
    float aOrbitDistance, aOrbitYaw, aOrbitPitch;

    // 跟随状态
    sCAMERA_TARGET aTarget;
    GNg::sVEC3 aFollowOffset;
    GNg::sVEC3 aFollowVelocity;
    GNg::sVEC3 aSmoothedTarget;
    float aFollowRatio;
    bool aFollowBalanced, aFollowCorrectsRot, aSmoothValid;

    void smoothTargetPosition(float dt); // 临界阻尼逐轴平滑 + 收敛清零
    void detachFollowAndOrbit(); // 手动摆位前脱离跟随/轨道（清目标、清轨道标志、回 FREE）
    void applyOrbit(); // 由 focus/角度/距离重解 position/rotation
    void syncOrbitFromPosition(); // 由 position 反算轨道参数（手动摆位后调用）
    static GNg::sQUAT makeLookRotation(const GNg::sVEC3 &dir, const GNg::sVEC3 &upHint);

    // 自动取景需要复用朝向构造并清空跟随/轨道状态
    friend void SetCameraLookDirection(cCAMERA &camera, eCAMERA_LOOK_MODE mode,
                                       const GNg::sVEC3 *focus);
    friend float SetCameraLookRange(cCAMERA &camera, const GNg::sVEC3 &center,
                                    const GNg::sVEC3 &halfExtent, float margin,
                                    eCAMERA_LOOK_MODE mode);
    friend float SetCameraLookObj(cCAMERA &camera, const sCAMERA_TARGET &target, float margin,
                                  eCAMERA_LOOK_MODE mode);
};

// 自动取景（对齐 GN 的自由函数名）
void SetCameraLookDirection(cCAMERA &camera, eCAMERA_LOOK_MODE mode,
                            const GNg::sVEC3 *focus = 0);
float SetCameraLookRange(cCAMERA &camera, const GNg::sVEC3 &center,
                         const GNg::sVEC3 &halfExtent, float margin = 1.0f,
                         eCAMERA_LOOK_MODE mode = eCAMERA_LOOK_FRONT);
float SetCameraLookObj(cCAMERA &camera, const sCAMERA_TARGET &target, float margin = 1.0f,
                       eCAMERA_LOOK_MODE mode = eCAMERA_LOOK_FRONT);

} // namespace GN3d
#endif // _GN_3DCAMERA_H_
