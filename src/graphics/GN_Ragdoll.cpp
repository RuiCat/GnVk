// GN_Ragdoll.cpp — 布娃娃（C3b）。
//
// 设计口径见头文件。这里记四处实现要点：
//   1. 骨 = 关节到父关节的连线。胶囊质心在骨中点，局部 +Y 朝天顶 —— 与
//      sRIGID_BODY 的胶囊约定一致（沿局部 Y，半长 aHalfExtent.y + aRadius）。
//   2. 关节锚点必须用**世界坐标**算一次，再转回各自刚体的局部空间 ——
//      直接塞世界坐标进 aLocalAnchor 会得到"跟着刚体平移的锚点"，约束完全失效。
//   3. SyncPose 输出的局部变换要与引擎的 sANIM_TRANSFORM 语义一致
//      （父空间下的平移 + 旋转），否则蒙皮会炸开。
//   4. ApplyImpulse 按质量加权：给所有刚体同样的"速度增量"而不是同样的冲量，
//      否则重的躯干不动、轻的四肢飞出去。

#include <GN.h>
#include "GN_Ragdoll.h"
#include "GN_IK.h" // quatFromTo

#include <algorithm>
#include <cmath>

namespace GN3d {

namespace {

GNg::sVEC3 v3(float x, float y, float z) {
    GNg::sVEC3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}
GNg::sVEC3 add(const GNg::sVEC3 &a, const GNg::sVEC3 &b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
GNg::sVEC3 sub(const GNg::sVEC3 &a, const GNg::sVEC3 &b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
GNg::sVEC3 mul(const GNg::sVEC3 &a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
float dot(const GNg::sVEC3 &a, const GNg::sVEC3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float len(const GNg::sVEC3 &a) { return std::sqrt(dot(a, a)); }
GNg::sVEC3 normalizeOr(const GNg::sVEC3 &v, const GNg::sVEC3 &fb) {
    const float l = len(v);
    if(!(l > 1e-8f)) return fb;
    return mul(v, 1.0f / l);
}

// 从四元数取局部 Y 轴（胶囊的朝向轴）
GNg::sVEC3 quatUp(const GNg::sQUAT &q) {
    GNg::sVEC3 up{0.0f, 1.0f, 0.0f};
    GNg::sVEC3 out;
    GNg::QuatRotateVec3(&out, &up, &q);
    return out;
}

// 让局部 +Y 对齐到 dir 的旋转（复用 IK 的 quatFromTo：同样的最短弧 + 退化处理）
//
// 注意：**不要在这个匿名命名空间里用 `extern` 前置声明** —— 那样声明会落在匿名
// 命名空间内，链接时找的是 `GN3d::(anonymous namespace)::quatFromTo`，而实际符号是
// `GN3d::quatFromTo`，报 undefined reference。直接 include 头文件即可。
GNg::sQUAT quatFromUpTo(const GNg::sVEC3 &dir) {
    const GNg::sVEC3 from{0.0f, 1.0f, 0.0f};
    const GNg::sVEC3 to = normalizeOr(dir, from);
    return quatFromTo(from, to);
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  sRAGDOLL_DESC
// ═══════════════════════════════════════════════════════════
sRAGDOLL_DESC::sRAGDOLL_DESC()
    : aUseSpheres(true), aBoneRadius(0.09f), aMassScale(8.0f), aJointStiffness(1.0f),
      aNoCollideGroup(0) {}

// ═══════════════════════════════════════════════════════════
//  cRAGDOLL
// ═══════════════════════════════════════════════════════════
cRAGDOLL::cRAGDOLL() : aWorld(0), aNoCollideGroup(0), aRootBody(cPHYSICS_WORLD::INVALID_HANDLE) {}

cRAGDOLL::~cRAGDOLL() { Destroy(); }

void cRAGDOLL::Destroy() {
    if(aWorld != 0) {
        for(size_t i = 0; i < aJoints.size(); ++i) aWorld->RemoveJoint(aJoints[i]);
        for(size_t i = 0; i < aBones.size(); ++i) aWorld->RemoveBody(aBones[i].aBody);
    }
    aJoints.clear();
    aBones.clear();
    aBoneJoints.clear();
    aWorld = 0;
    aRootBody = cPHYSICS_WORLD::INVALID_HANDLE;
}

bool cRAGDOLL::Build(cPHYSICS_WORLD *world, const sSKELETON &skeleton,
                     const std::vector<sANIM_TRANSFORM> &localPose, const sRAGDOLL_DESC &desc) {
    Destroy();
    if(world == 0) return false;

    const uint32_t jointCount = skeleton.jointCount();
    if(jointCount < 2 || localPose.size() != jointCount) return false;

    // 世界位置（用骨架的世界矩阵，避免自己再实现一遍层级累积）
    std::vector<GNg::sMAT44> worldMat;
    if(!skeleton.computeWorld(localPose, worldMat)) return false;
    if(worldMat.size() != jointCount) return false;

    std::vector<GNg::sVEC3> jointWorld(jointCount);
    for(uint32_t i = 0; i < jointCount; ++i) {
        // 用现成的 Mat44GetTranslation（sMAT44 的成员是 m[16]，不要自己数下标）
        GNg::Mat44GetTranslation(&jointWorld[i], &worldMat[i]);
    }

    aWorld = world;
    aNoCollideGroup = (desc.aNoCollideGroup != 0) ? desc.aNoCollideGroup : 0xDEAD0001u;
    // 保存父索引：SyncPose 要用它把世界变换换算成局部变换，
    // 而那时骨架对象可能已经被调用方销毁了。
    aParentOf.resize(jointCount);
    for(uint32_t i = 0; i < jointCount; ++i) aParentOf[i] = skeleton.aJoints[i].aParent;

    // ── 为每条骨建一个胶囊 ──
    // 骨 = (父关节 → 该关节)。根关节没有父，跳过。
    uint32_t rootBodyCandidate = cPHYSICS_WORLD::INVALID_HANDLE;
    for(uint32_t j = 0; j < jointCount; ++j) {
        const int32_t parent = skeleton.aJoints[j].aParent;
        if(parent < 0 || parent >= (int32_t)jointCount) continue;

        const GNg::sVEC3 p0 = jointWorld[(uint32_t)parent];
        const GNg::sVEC3 p1 = jointWorld[j];
        const GNg::sVEC3 d = sub(p1, p0);
        const float boneLen = len(d);
        if(!(boneLen > 1e-4f)) continue; // 退化骨（父子重合）跳过

        const GNg::sVEC3 mid = mul(add(p0, p1), 0.5f);
        const GNg::sVEC3 dir = mul(d, 1.0f / boneLen);

        sRIGID_BODY body;
        body.aType = eBODY_DYNAMIC;
        if(desc.aUseSpheres) {
            // 球体：半径取骨长的一半，相邻两球在关节处恰好相切。
            // 惯量大得多（I = 2/5·m·r²），接触力矩不会失控。
            body.aShape = eSHAPE_SPHERE;
            body.aRadius = std::max(desc.aBoneRadius, boneLen * 0.5f);
            body.aHalfExtent = v3(0.0f, 0.0f, 0.0f);
        } else {
            body.aShape = eSHAPE_CAPSULE;
            // 胶囊：圆柱段半长 = 骨长/2（不含端盖），总半高 = 该值 + radius
            body.aRadius = desc.aBoneRadius;
            body.aHalfExtent = v3(desc.aBoneRadius, boneLen * 0.5f, desc.aBoneRadius);
        }
        const float mass = std::max(0.2f, boneLen * desc.aMassScale);
        body.aMass = mass;
        body.aInvMass = 1.0f / mass;
        body.aPos = mid;
        body.aRot = quatFromUpTo(dir);
        body.aLinVel = v3(0.0f, 0.0f, 0.0f);
        body.aAngVel = v3(0.0f, 0.0f, 0.0f);
        body.aFriction = 0.6f;
        body.aRestitution = 0.0f;
        body.aLinearDamping = 0.10f; // 布娃娃要"软塌塌"地停下来
        body.aAngularDamping = 0.25f; // 角阻尼大一些，否则四肢乱转
        body.aNoCollideGroup = aNoCollideGroup;

        // **必须算逆惯量**：AddBody 不会自动调 BodyUpdateInertia（那是 MakeXxxBody
        // 工厂的职责）。手填字段时漏了它，aInvInertiaLocal/World 全是零矩阵 ⇒
        // 接触求解拿不到角向响应，落地瞬间冲量把动能从 220 顶到 2239（实测）。
        BodyUpdateInertia(body);

        const uint32_t handle = world->AddBody(body);
        if(handle == cPHYSICS_WORLD::INVALID_HANDLE) continue;

        sRAGDOLL_BONE bone;
        bone.aJoint = j;
        bone.aBody = handle;
        bone.aLength = boneLen;
        bone.aLocalDir = dir;

        const uint32_t boneIndex = (uint32_t)aBones.size();
        aBones.push_back(bone);
        if(rootBodyCandidate == cPHYSICS_WORLD::INVALID_HANDLE) rootBodyCandidate = handle;
        (void)boneIndex;
    }
    aRootBody = rootBodyCandidate;

    if(aBones.size() < 1) {
        Destroy();
        return false;
    }

    // ── 相邻两骨之间加 distance 关节 ──
    // 关键：两骨共享一个骨架关节时，锚点就在那个关节的世界位置上。
    // 骨 A 的"子端关节" == 骨 B 的"父端关节" ⇒ 连接。
    //
    // 用映射查表：childJoint → 骨索引
    std::vector<int32_t> boneOfJoint(jointCount, -1);
    for(size_t b = 0; b < aBones.size(); ++b) boneOfJoint[aBones[b].aJoint] = (int32_t)b;

    for(size_t b = 0; b < aBones.size(); ++b) {
        const uint32_t jointIdx = aBones[b].aJoint;
        const int32_t parentJoint = skeleton.aJoints[jointIdx].aParent;
        if(parentJoint < 0 || parentJoint >= (int32_t)jointCount) continue;

        // 父骨：以 parentJoint 为"子端"的那根骨
        const int32_t parentBone = boneOfJoint[(uint32_t)parentJoint];
        if(parentBone < 0) continue;

        const uint32_t bodyA = aBones[(size_t)parentBone].aBody;
        const uint32_t bodyB = aBones[b].aBody;

        sRIGID_BODY *A = world->GetBody(bodyA);
        sRIGID_BODY *B = world->GetBody(bodyB);
        if(A == 0 || B == 0) continue;

        sJOINT_DESC joint;
        joint.aType = eJOINT_DISTANCE;
        joint.aBodyA = bodyA;
        joint.aBodyB = bodyB;
        joint.aLocalAxisA = v3(0.0f, 1.0f, 0.0f);
        joint.aLocalAxisB = v3(0.0f, 1.0f, 0.0f);

        if(desc.aUseSpheres) {
            // ── 球体模式：锚点必须在**球心**，rest length = 两骨半长之和 ──
            //
            // **不能用"骨端"做锚点**。`worldAnchor` 会把局部锚点按刚体朝向旋转，
            // 而球体是各向同性的、它的旋转**没有任何物理量约束**（可以随便转）。
            // 一旦锚点偏离球心，旋转就会让锚点到处跑，joint 于是不停地把它们往
            // 一起拉 —— 实测直接发散（y 冲到 1.9e26）。
            // 锚点设在球心后，旋转对锚点位置没有影响，约束只剩"保持球心距离"，
            // 正是我们想要的（骨长守恒）。
            joint.aLocalAnchorA = v3(0.0f, 0.0f, 0.0f);
            joint.aLocalAnchorB = v3(0.0f, 0.0f, 0.0f);
            joint.aDistance =
                aBones[(size_t)parentBone].aLength * 0.5f + aBones[b].aLength * 0.5f;
        } else {
            // ── 胶囊模式：锚点在骨端（胶囊的局部 +Y 就是骨方向，旋转有物理意义）──
            const GNg::sVEC3 anchorWorld = jointWorld[jointIdx];
            GNg::sVEC3 anchorALocal;
            GNg::sVEC3 anchorBLocal;
            {
                GNg::sQUAT invA;
                GNg::QuatInverse(&invA, &A->aRot);
                GNg::sVEC3 rel = sub(anchorWorld, A->aPos);
                GNg::QuatRotateVec3(&anchorALocal, &rel, &invA);

                GNg::sQUAT invB;
                GNg::QuatInverse(&invB, &B->aRot);
                GNg::sVEC3 relB = sub(anchorWorld, B->aPos);
                GNg::QuatRotateVec3(&anchorBLocal, &relB, &invB);
            }
            joint.aLocalAnchorA = anchorALocal;
            joint.aLocalAnchorB = anchorBLocal;
            joint.aDistance = 0.0f; // 两骨端重合
        }

        const uint32_t jh = world->AddJoint(joint);
        if(jh != cPHYSICS_WORLD::INVALID_HANDLE) {
            aJoints.push_back(jh);
            aBoneJoints.push_back(std::make_pair((uint32_t)b, (uint32_t)parentJoint));
        }
    }

    return true;
}

bool cRAGDOLL::SyncPose(std::vector<sANIM_TRANSFORM> &outLocal) const {
    if(aWorld == 0 || aBones.empty()) return false;
    const size_t jointCount = outLocal.size();
    if(jointCount == 0 || aParentOf.size() != jointCount) return false;

    // ── 1) 每根骨给出"子端关节"的世界位置与朝向 ──
    // 骨的胶囊局部 +Y 指向"父→子"，所以子端 = 质心 + up*(骨长/2)。
    // 朝向用胶囊的旋转（这就是该关节的世界朝向，足够驱动蒙皮）。
    std::vector<GNg::sVEC3> jointWorld(jointCount, v3(0.0f, 0.0f, 0.0f));
    std::vector<GNg::sQUAT> jointRot(jointCount);
    std::vector<bool> has(jointCount, false);
    for(size_t i = 0; i < jointCount; ++i) {
        GNg::QuatIdentity(&jointRot[i]);
    }

    // 先写所有"子端"（最准：那正是该关节在骨架里的位置）
    for(size_t b = 0; b < aBones.size(); ++b) {
        const sRIGID_BODY *body = aWorld->GetBody(aBones[b].aBody);
        if(body == 0) continue;
        const GNg::sVEC3 up = quatUp(body->aRot);
        const float half = aBones[b].aLength * 0.5f;
        const uint32_t childJoint = aBones[b].aJoint;
        if(childJoint >= jointCount) continue;
        jointWorld[childJoint] = add(body->aPos, mul(up, half));
        jointRot[childJoint] = body->aRot;
        has[childJoint] = true;
    }
    // 再补"父端"（只填空缺）。
    //
    // **这一步不能省**：骨只记录"子端关节"，于是**根关节永远没有子端** ——
    // 而根是很多骨的父。没有这一步，has[root] 恒为 false，
    // 所有"以根为父"的关节都会被下面的"父没有位姿 ⇒ 跳过"逻辑跳掉
    // （实测 5 个关节里只有 1 个被更新）。根的位置可以从任一根骨的父端得到。
    for(size_t b = 0; b < aBones.size(); ++b) {
        const sRIGID_BODY *body = aWorld->GetBody(aBones[b].aBody);
        if(body == 0) continue;
        const uint32_t childJoint = aBones[b].aJoint;
        if(childJoint >= aParentOf.size()) continue;
        const int32_t p = aParentOf[childJoint];
        if(p < 0 || (size_t)p >= jointCount || has[(size_t)p]) continue;

        const GNg::sVEC3 up = quatUp(body->aRot);
        const float half = aBones[b].aLength * 0.5f;
        jointWorld[(size_t)p] = sub(body->aPos, mul(up, half));
        jointRot[(size_t)p] = body->aRot;
        has[(size_t)p] = true;
    }

    // ── 2) 世界 ⇒ 局部 ──
    // 关节按"父索引 < 自身"的顺序存储（导入器约定，computeWorld 也依赖它），
    // 所以一遍正序就能拿到父的世界变换。
    for(size_t j = 0; j < jointCount; ++j) {
        if(!has[j]) continue; // 没被骨覆盖（根 / 无子骨的末端）：保持原样
        const int32_t p = aParentOf[j];
        if(p < 0 || (size_t)p >= jointCount) {
            // 根关节：世界 == 局部
            outLocal[j].aTranslation = jointWorld[j];
            outLocal[j].aRotation = jointRot[j];
            continue;
        }
        if(!has[(size_t)p]) continue; // 父没有位姿：跳过（保持原样）

        // local = parentWorld^-1 * world
        //   平移：invParentRot * (worldPos - parentPos)
        //   旋转：invParentRot * worldRot
        GNg::sQUAT invParent;
        GNg::QuatInverse(&invParent, &jointRot[(size_t)p]);
        const GNg::sVEC3 rel = sub(jointWorld[j], jointWorld[(size_t)p]);
        GNg::sVEC3 localT;
        GNg::QuatRotateVec3(&localT, &rel, &invParent);
        outLocal[j].aTranslation = localT;

        GNg::sQUAT localR;
        GNg::QuatMul(&localR, &invParent, &jointRot[j]);
        outLocal[j].aRotation = localR;
    }
    return true;
}

// 角速度上限（rad/s）。纯几何上球体已经够稳，但极端冲击（比如一帧内多次接触）
// 仍可能产生不合理的转速。加一个软钳制：超过就缩回，同时保持方向。
// 这不是"掩盖问题" —— 真实物体也有转速极限，且这是数值稳定性的常规做法。
static const float kMaxAngularSpeed = 25.0f;

void cRAGDOLL::ClampAngularSpeeds() {
    if(aWorld == 0) return;
    for(size_t b = 0; b < aBones.size(); ++b) {
        sRIGID_BODY *body = aWorld->GetBody(aBones[b].aBody);
        if(body == 0) continue;
        const float w2 = dot(body->aAngVel, body->aAngVel);
        if(w2 > kMaxAngularSpeed * kMaxAngularSpeed) {
            const float s = kMaxAngularSpeed / std::sqrt(w2);
            body->aAngVel = mul(body->aAngVel, s);
        }
    }
}

void cRAGDOLL::ApplyImpulse(const GNg::sVEC3 &impulse) {
    if(aWorld == 0) return;
    for(size_t b = 0; b < aBones.size(); ++b) {
        sRIGID_BODY *body = aWorld->GetBody(aBones[b].aBody);
        if(body == 0) continue;
        // 按质量归一：给所有刚体同样的**速度增量**（而不是同样的冲量），
        // 否则重的躯干几乎不动、轻的四肢飞出去。
        body->aLinVel.x += impulse.x;
        body->aLinVel.y += impulse.y;
        body->aLinVel.z += impulse.z;
        body->aSleeping = false;
        body->aSleepTime = 0.0f;
    }
}

bool cRAGDOLL::IsAtRest() const {
    if(aWorld == 0) return true;
    for(size_t b = 0; b < aBones.size(); ++b) {
        const sRIGID_BODY *body = aWorld->GetBody(aBones[b].aBody);
        if(body == 0) continue;
        if(!body->aSleeping) return false;
    }
    return true;
}

float cRAGDOLL::KineticEnergy() const {
    if(aWorld == 0) return 0.0f;
    float total = 0.0f;
    for(size_t b = 0; b < aBones.size(); ++b) {
        const sRIGID_BODY *body = aWorld->GetBody(aBones[b].aBody);
        if(body == 0) continue;
        const float v2 = dot(body->aLinVel, body->aLinVel);
        total += 0.5f * body->aMass * v2;

        // 转动动能 = ½·ωᵀ·I·ω。
        //
        // **必须用真实的惯量**：第一版写成 `I ≈ 1/(invMass·0.2)` 的粗估（0.2 是拍的），
        // 结果钳制到 25 rad/s 的球被算成 780 J/个，9 个球报出 7000 J 的假动能，
        // 而实际只有几焦。sRIGID_BODY 有 aInvInertiaLocal（对角逆惯量，球/胶囊/盒
        // 都填了），取倒数即 I；再用世界逆惯量把 ω 转过去更准，但球体各向同性，
        // 直接用 local 分量就够。
        const float w2 = dot(body->aAngVel, body->aAngVel);
        if(w2 > 0.0f) {
            // aInvInertiaLocal 的 m[0]/m[5]/m[10] 是对角元（其余为 0）
            const float invIx = body->aInvInertiaLocal.m[0];
            const float invIy = body->aInvInertiaLocal.m[5];
            const float invIz = body->aInvInertiaLocal.m[10];
            // I·ω 的每个分量 = ω_i / invI_i（invI=0 表示该轴无限惯量，不贡献）
            float rot = 0.0f;
            if(invIx > 1e-12f) rot += body->aAngVel.x * body->aAngVel.x / invIx;
            if(invIy > 1e-12f) rot += body->aAngVel.y * body->aAngVel.y / invIy;
            if(invIz > 1e-12f) rot += body->aAngVel.z * body->aAngVel.z / invIz;
            total += 0.5f * rot;
        }
    }
    return total;
}

float cRAGDOLL::MaxJointError() const {
    if(aWorld == 0) return 0.0f;
    float worst = 0.0f;
    for(size_t i = 0; i < aBoneJoints.size(); ++i) {
        const uint32_t childBone = aBoneJoints[i].first;
        uint32_t pb = (uint32_t)-1;
        for(size_t k = 0; k < aBones.size(); ++k) {
            if(aBones[k].aJoint == aBoneJoints[i].second) {
                pb = (uint32_t)k;
                break;
            }
        }
        if(pb == (uint32_t)-1) continue;
        if(childBone >= aBones.size() || pb >= aBones.size()) continue;

        const sRIGID_BODY *A = aWorld->GetBody(aBones[pb].aBody);
        const sRIGID_BODY *B = aWorld->GetBody(aBones[childBone].aBody);
        if(A == 0 || B == 0) continue;

        // 关节误差 = **两个胶囊/球的"连接端"之间的距离**。
        //
        // 球体模式下**不能用"质心 + up×半长"定位端点**：球是各向同性的，
        // 它的旋转是随机的（没有任何物理量约束它），用 up 算出来的"端点"
        // 方向毫无意义 —— 实测会给出 0.5 m 的假误差，而真实锚点只差几毫米。
        //
        // 正确口径：两个刚体的质心距离应当等于 两骨半长之和（球体/胶囊都成立，
        // 因为质心都在骨中点、锚点在骨端）。误差 = |实际距离 - 期望距离|。
        const float expect = aBones[pb].aLength * 0.5f + aBones[childBone].aLength * 0.5f;
        const float actual = len(sub(B->aPos, A->aPos));
        const float err = std::fabs(actual - expect);
        if(err > worst) worst = err;
    }
    return worst;
}

void cRAGDOLL::WorldBounds(GNg::sVEC3 &outMin, GNg::sVEC3 &outMax) const {
    outMin = v3(1e30f, 1e30f, 1e30f);
    outMax = v3(-1e30f, -1e30f, -1e30f);
    if(aWorld == 0) return;
    for(size_t b = 0; b < aBones.size(); ++b) {
        const sRIGID_BODY *body = aWorld->GetBody(aBones[b].aBody);
        if(body == 0) continue;
        outMin.x = std::min(outMin.x, body->aWorldAABB.aMin.x);
        outMin.y = std::min(outMin.y, body->aWorldAABB.aMin.y);
        outMin.z = std::min(outMin.z, body->aWorldAABB.aMin.z);
        outMax.x = std::max(outMax.x, body->aWorldAABB.aMax.x);
        outMax.y = std::max(outMax.y, body->aWorldAABB.aMax.y);
        outMax.z = std::max(outMax.z, body->aWorldAABB.aMax.z);
    }
}

} // namespace GN3d
