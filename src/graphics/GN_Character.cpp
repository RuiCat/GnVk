// GN_Character.cpp — 胶囊角色控制器（P5）。
//
// 实现口径（每条都有用例，见 tests/unit/testCharacter.cpp）：
//   1. **不参与刚体求解**：角色自己积分、自己解穿透，只读静态三角网。
//      这样角色与动态刚体不会互相踩（代价：角色不推箱子，v1 边界）。
//   2. 每步顺序：重力积分 → 水平移动 → 解穿透 → 斜坡投影 → 台阶抬升 → 地面检测。
//      顺序不能换：斜坡投影要用解穿透后的接触法线，台阶抬升要用"水平被挡"这个结论。
//   3. 斜坡：法线坡度 <= aMaxSlopeDeg 才算可站地面。走在斜坡上时把**水平位移**投影到
//      坡面上（并补一点向下吸附），否则每步都会离地一点、再掉一点，抖动。
//      超过上限的坡当墙：保留法线方向的推力，切向速度保留 ⇒ 贴墙下滑不卡死。
//   4. 台阶：只有当"水平被挡 + 站着 + 台阶顶不高于 aStepHeight + 抬上去之后确实能前进"
//      才抬。三个条件缺一不可，否则会在真正的墙前面一帧一帧地往上爬。
//   5. 卡住检测：解穿透迭代用尽仍有穿透 ⇒ aStuck。上层可以据此播放"被夹住"或松手。

#include <GN.h> // 先于 GN_Character.h：那一头依赖 GN_API（见它的注释）
#include "GN_Character.h"
#include "GN_Physics.h" // queryCapsuleTriangle

#include <cmath>

namespace GN3d {

namespace {

const float kEps = 1.0e-6f;
const float kDeg = 57.29577951308232f;

GNg::sVEC3 vec(float x, float y, float z) {
    GNg::sVEC3 v = {x, y, z};
    return v;
}
GNg::sVEC3 add(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return vec(a.x + b.x, a.y + b.y, a.z + b.z);
}
GNg::sVEC3 sub(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return vec(a.x - b.x, a.y - b.y, a.z - b.z);
}
GNg::sVEC3 mul(const GNg::sVEC3 &v, float s) {
    return vec(v.x * s, v.y * s, v.z * s);
}
float dot(const GNg::sVEC3 &a, const GNg::sVEC3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
float lengthSq(const GNg::sVEC3 &v) {
    return dot(v, v);
}
float length(const GNg::sVEC3 &v) {
    return std::sqrt(lengthSq(v));
}
bool normalize(GNg::sVEC3 &v) {
    const float sq = lengthSq(v);
    if (!(sq > 1.0e-12f)) return false;
    const float inv = 1.0f / std::sqrt(sq);
    v.x *= inv;
    v.y *= inv;
    v.z *= inv;
    return true;
}
// 去掉沿 normal 的分量（normal 须已归一化）
GNg::sVEC3 projectOnPlane(const GNg::sVEC3 &v, const GNg::sVEC3 &normal) {
    const float along = dot(v, normal);
    return vec(v.x - normal.x * along, v.y - normal.y * along, v.z - normal.z * along);
}
float clampf(float v, float low, float high) {
    if (v < low) return low;
    return v > high ? high : v;
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  描述 / 结果
// ═══════════════════════════════════════════════════════════
sCHARACTER_DESC::sCHARACTER_DESC()
    : aRadius(0.3f), aHeight(1.7f), aMaxSlopeDeg(45.0f), aStepHeight(0.35f), aWalkSpeed(3.5f),
      aSprintSpeed(6.0f), aJumpSpeed(5.0f), aGravityScale(1.0f), aSkinWidth(0.01f),
      aResolveIterations(6) {}

sCHARACTER_STEP::sCHARACTER_STEP()
    : aGrounded(false), aJumped(false), aHitWall(false), aSteppedUp(false), aHitCeiling(false),
      aGroundNormalY(0.0f), aSlopeDeg(0.0f), aHorizontalDistance(0.0f) {}

cCHARACTER::sINTENT::sINTENT()
    : aMoveForward(0.0f), aMoveRight(0.0f), aSprint(false), aJump(false) {}

// ═══════════════════════════════════════════════════════════
//  cCHARACTER
// ═══════════════════════════════════════════════════════════
cCHARACTER::cCHARACTER()
    : aPosition{0.0f, 0.0f, 0.0f}, aFootPosition{0.0f, 0.0f, 0.0f}, aVelocity{0.0f, 0.0f, 0.0f},
      aGravity{0.0f, -9.81f, 0.0f}, aInited(false), aGrounded(false), aStuck(false),
      aGroundNormalY(0.0f), aSlopeDeg(0.0f) {
    aFootPosition.y = -aDesc.aHeight * 0.5f;
}

void cCHARACTER::Init(const sCHARACTER_DESC &desc) {
    aDesc = desc;
    aInited = true;
    // 钳非法参数：半径/总高必须为正，总高至少 2r（否则胶囊退化成球）。
    if (!(aDesc.aRadius > 0.0f)) aDesc.aRadius = 0.3f;
    if (!(aDesc.aHeight >= aDesc.aRadius * 2.0f)) aDesc.aHeight = aDesc.aRadius * 2.0f;
    if (aDesc.aStepHeight < 0.0f) aDesc.aStepHeight = 0.0f;
    if (!(aDesc.aGravityScale > 0.0f)) aDesc.aGravityScale = 1.0f;
    if (aDesc.aMaxSlopeDeg <= 0.0f) aDesc.aMaxSlopeDeg = 45.0f;
    if (aDesc.aMaxSlopeDeg > 89.0f) aDesc.aMaxSlopeDeg = 89.0f;
    if (aDesc.aResolveIterations == 0) aDesc.aResolveIterations = 1;
    if (aDesc.aSkinWidth < 0.0f) aDesc.aSkinWidth = 0.0f;
    resetTransient();
}

const sCHARACTER_DESC &cCHARACTER::Desc() const { return aDesc; }

void cCHARACTER::resetTransient() {
    aGrounded = false;
    aStuck = false;
    aGroundNormalY = 0.0f;
    aSlopeDeg = 0.0f;
    aFootPosition = aPosition;
    aFootPosition.y -= aDesc.aHeight * 0.5f;
}

float cCHARACTER::cylinderHalf() const {
    const float half = (aDesc.aHeight - aDesc.aRadius * 2.0f) * 0.5f;
    return half > 0.0f ? half : 0.0f;
}

void cCHARACTER::segment(const GNg::sVEC3 &center, GNg::sVEC3 &e0, GNg::sVEC3 &e1) const {
    const float half = cylinderHalf();
    e0 = vec(center.x, center.y - half, center.z);
    e1 = vec(center.x, center.y + half, center.z);
}

// ── 几何 ──
void cCHARACTER::ClearGeometry() {
    aTriangles.clear();
    resetTransient();
}

void cCHARACTER::AddTriangle(const GNg::sVEC3 &a, const GNg::sVEC3 &b, const GNg::sVEC3 &c) {
    // 面法线（退化三角形直接丢弃：它既挡不住人也站不上）
    const GNg::sVEC3 ab = sub(b, a);
    const GNg::sVEC3 ac = sub(c, a);
    GNg::sVEC3 normal = vec(ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z,
                            ab.x * ac.y - ab.y * ac.x);
    if (!normalize(normal)) return;
    sTRIANGLE triangle;
    triangle.aA = a;
    triangle.aB = b;
    triangle.aC = c;
    triangle.aNormal = normal;
    aTriangles.push_back(triangle);
}

void cCHARACTER::AddStaticMesh(const sMESH &mesh) {
    if (mesh.aVertices == 0 || mesh.aIndices == 0) return;
    const uint32_t count = mesh.aIndexCount / 3u;
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t i0 = mesh.aIndices[i * 3u];
        const uint32_t i1 = mesh.aIndices[i * 3u + 1u];
        const uint32_t i2 = mesh.aIndices[i * 3u + 2u];
        if (i0 >= mesh.aVertexCount || i1 >= mesh.aVertexCount || i2 >= mesh.aVertexCount) continue;
        const float *p0 = mesh.aVertices + i0 * 11u;
        const float *p1 = mesh.aVertices + i1 * 11u;
        const float *p2 = mesh.aVertices + i2 * 11u;
        AddTriangle(vec(p0[0], p0[1], p0[2]), vec(p1[0], p1[1], p1[2]),
                    vec(p2[0], p2[1], p2[2]));
    }
}

void cCHARACTER::AddFloor(const GNg::sVEC3 &center, float halfX, float halfZ, float height) {
    const GNg::sVEC3 a = vec(center.x - halfX, height, center.z - halfZ);
    const GNg::sVEC3 b = vec(center.x + halfX, height, center.z - halfZ);
    const GNg::sVEC3 c = vec(center.x + halfX, height, center.z + halfZ);
    const GNg::sVEC3 d = vec(center.x - halfX, height, center.z + halfZ);
    // 绕序：从上方看逆时针（引擎的正面口径在世界 CW 下也可见，这里两面都加更稳）
    AddTriangle(a, c, b);
    AddTriangle(a, d, c);
}

void cCHARACTER::AddBox(const GNg::sVEC3 &center, const GNg::sVEC3 &halfSize) {
    const float x0 = center.x - halfSize.x;
    const float x1 = center.x + halfSize.x;
    const float y0 = center.y - halfSize.y;
    const float y1 = center.y + halfSize.y;
    const float z0 = center.z - halfSize.z;
    const float z1 = center.z + halfSize.z;
    const GNg::sVEC3 v[8] = {vec(x0, y0, z0), vec(x1, y0, z0), vec(x1, y1, z0), vec(x0, y1, z0),
                             vec(x0, y0, z1), vec(x1, y0, z1), vec(x1, y1, z1), vec(x0, y1, z1)};
    // 6 面 × 2 三角形（绕序两面都有，避免"看不见但撞得着"的错觉）
    const int faces[6][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}, {0, 1, 5, 4},
                             {3, 2, 6, 7}, {0, 3, 7, 4}, {1, 2, 6, 5}};
    for (int f = 0; f < 6; ++f) {
        const GNg::sVEC3 &p0 = v[faces[f][0]];
        const GNg::sVEC3 &p1 = v[faces[f][1]];
        const GNg::sVEC3 &p2 = v[faces[f][2]];
        const GNg::sVEC3 &p3 = v[faces[f][3]];
        AddTriangle(p0, p1, p2);
        AddTriangle(p0, p2, p3);
    }
}

void cCHARACTER::AddRamp(const GNg::sVEC3 &origin, float run, float rise, float width) {
    if (!(run > 0.0f) || !(width > 0.0f)) return;
    const float halfW = width * 0.5f;
    // 从 origin（低端，y=origin.y）沿 +x 爬升到 origin.x + run（y = origin.y + rise）
    const GNg::sVEC3 lowL = vec(origin.x, origin.y, origin.z - halfW);
    const GNg::sVEC3 lowR = vec(origin.x, origin.y, origin.z + halfW);
    const GNg::sVEC3 highL = vec(origin.x + run, origin.y + rise, origin.z - halfW);
    const GNg::sVEC3 highR = vec(origin.x + run, origin.y + rise, origin.z + halfW);
    // 绕序要让面法线朝上（+y）：反了会让"坡面算不算地面"这类判定事先就被过滤掉。
    AddTriangle(lowL, highR, highL);
    AddTriangle(lowL, lowR, highR);
}

uint32_t cCHARACTER::TriangleCount() const { return (uint32_t)aTriangles.size(); }

// ── 状态 ──
void cCHARACTER::SetPosition(const GNg::sVEC3 &centerPos) {
    aPosition = centerPos;
    aFootPosition = centerPos;
    aFootPosition.y -= aDesc.aHeight * 0.5f;
}

const GNg::sVEC3 &cCHARACTER::Position() const { return aPosition; }

const GNg::sVEC3 &cCHARACTER::FootPosition() const { return aFootPosition; }

void cCHARACTER::SetVelocity(const GNg::sVEC3 &velocity) { aVelocity = velocity; }

const GNg::sVEC3 &cCHARACTER::Velocity() const { return aVelocity; }

void cCHARACTER::SetGravity(const GNg::sVEC3 &gravity) { aGravity = gravity; }

const GNg::sVEC3 &cCHARACTER::Gravity() const { return aGravity; }

void cCHARACTER::Teleport(const GNg::sVEC3 &centerPos) {
    SetPosition(centerPos);
    aVelocity = vec(0.0f, 0.0f, 0.0f); // 残留的落地速度会让瞬移后立刻弹飞/穿地
    resetTransient();
}

bool cCHARACTER::IsInitialized() const { return aInited; }

bool cCHARACTER::IsGrounded() const { return aGrounded; }

float cCHARACTER::GroundNormalY() const { return aGroundNormalY; }

float cCHARACTER::SlopeDeg() const { return aSlopeDeg; }

bool cCHARACTER::IsStuck() const { return aStuck; }

// ── 碰撞 ──
bool cCHARACTER::deepestPenetration(const GNg::sVEC3 &center, GNg::sVEC3 &outNormal,
                                    float &outDepth) const {
    if (aTriangles.empty()) return false;
    GNg::sVEC3 e0;
    GNg::sVEC3 e1;
    segment(center, e0, e1);
    bool hitAny = false;
    float deepest = 0.0f;
    GNg::sVEC3 bestNormal = vec(0.0f, 1.0f, 0.0f);
    for (size_t i = 0; i < aTriangles.size(); ++i) {
        const sTRIANGLE &triangle = aTriangles[i];
        // 粗筛：三角笼外面明显碰不到就跳过（省掉三分法搜索）
        const float reach = aDesc.aRadius + 0.05f;
        const float minY = std::fmin(triangle.aA.y, std::fmin(triangle.aB.y, triangle.aC.y));
        const float maxY = std::fmax(triangle.aA.y, std::fmax(triangle.aB.y, triangle.aC.y));
        if (e1.y + reach < minY || e0.y - reach > maxY) continue;

        sCONTACT_MANIFOLD manifold;
        if (!queryCapsuleTriangle(e0, e1, aDesc.aRadius, triangle.aA, triangle.aB, triangle.aC,
                                  manifold)) {
            continue;
        }
        const float depth = manifold.aPoints[0].aPenetration;
        if (!hitAny || depth > deepest) {
            hitAny = true;
            deepest = depth;
            bestNormal = manifold.aNormal;
        }
    }
    if (!hitAny) return false;
    outNormal = bestNormal;
    outDepth = deepest;
    return true;
}

bool cCHARACTER::resolvePenetration(GNg::sVEC3 &center) const {
    for (uint32_t iteration = 0; iteration < aDesc.aResolveIterations; ++iteration) {
        GNg::sVEC3 normal;
        float depth = 0.0f;
        if (!deepestPenetration(center, normal, depth)) return true; // 干净
        const float push = depth + aDesc.aSkinWidth;
        // 沿法线推出；法线必须归一化（queryCapsuleTriangle 已保证）
        center = add(center, mul(normal, push));
    }
    // 迭代用尽：还穿透就报卡住（上层可以据此松手/换位置）
    GNg::sVEC3 normal;
    float depth = 0.0f;
    return !deepestPenetration(center, normal, depth);
}

bool cCHARACTER::probeGround(const GNg::sVEC3 &center, float &outNormalY,
                             float &outSlopeDeg) const {
    outNormalY = 0.0f;
    outSlopeDeg = 0.0f;
    if (aTriangles.empty()) return false;

    // ① 先看"是否正贴着某个面"：斜坡上胶囊的**侧面**在接触，脚底球未必够得着，
    //    所以先看整体接触。这是斜坡着地能判对的关键（第一版只用脚底探针，
    //    结果 45° 坡上永远 IsGrounded()==false）。
    GNg::sVEC3 contactNormal;
    float contactDepth = 0.0f;
    if (deepestPenetration(center, contactNormal, contactDepth) && contactNormal.y > 0.05f) {
        const float slope = std::acos(clampf(contactNormal.y, -1.0f, 1.0f)) * kDeg;
        if (slope <= aDesc.aMaxSlopeDeg + 0.5f) {
            outNormalY = contactNormal.y;
            outSlopeDeg = slope;
            return true;
        }
    }

    // ② 退化为"脚底球向下探一点"：容忍悬浮 skinWidth 级别的间隙，又不会在高空误判。
    const float probe = aDesc.aRadius * 0.5f + 0.02f;
    const GNg::sVEC3 footCenter = vec(center.x, center.y - cylinderHalf(), center.z);
    bool found = false;
    float bestNormalY = 0.0f;
    float bestSlope = 0.0f;
    for (size_t i = 0; i < aTriangles.size(); ++i) {
        const sTRIANGLE &triangle = aTriangles[i];
        // 只用包围盒 y 范围粗筛。**不要**用三角形的绕序法线过滤：那条判据依赖几何朝向，
        // 一旦有人把三角形绕反（本文件的 AddRamp 第一版就反了），"能不能站"会静默失效。
        const float minY = std::fmin(triangle.aA.y, std::fmin(triangle.aB.y, triangle.aC.y));
        const float maxY = std::fmax(triangle.aA.y, std::fmax(triangle.aB.y, triangle.aC.y));
        if (footCenter.y + aDesc.aRadius + probe < minY ||
            footCenter.y - aDesc.aRadius - probe > maxY) {
            continue;
        }
        sCONTACT_MANIFOLD manifold;
        if (!querySphereTriangle(footCenter, aDesc.aRadius + probe, triangle.aA, triangle.aB,
                                 triangle.aC, manifold)) {
            continue;
        }
        const GNg::sVEC3 &normal = manifold.aNormal;
        if (normal.y <= 0.05f) continue;
        const float normalY = normal.y;
        const float slope = std::acos(clampf(normalY, -1.0f, 1.0f)) * kDeg;
        // 取得最"平"的那块（normalY 最大）：站在台阶边缘时不要被侧壁拉走
        if (!found || normalY > bestNormalY) {
            found = true;
            bestNormalY = normalY;
            bestSlope = slope;
        }
    }
    if (!found) return false;
    outNormalY = bestNormalY;
    outSlopeDeg = bestSlope;
    return true;
}

// ── 每步 ──
sCHARACTER_STEP cCHARACTER::Move(const GNg::sVEC3 &wishDir, bool sprint, bool jump, float dt) {
    sCHARACTER_STEP result;
    if (!(dt > 0.0f) || !std::isfinite(dt)) return result;

    // 0) 本步水平方向（去掉竖直分量后归一化；零向量 = 不主动移动）
    GNg::sVEC3 horizontal = vec(wishDir.x, 0.0f, wishDir.z);
    const bool hasInput = normalize(horizontal);
    const float speed = sprint ? aDesc.aSprintSpeed : aDesc.aWalkSpeed;

    // 1) 重力
    aVelocity = add(aVelocity, mul(aGravity, aDesc.aGravityScale * dt));
    if (!hasInput) {
        // 无输入：水平速度也衰减（否则松手会一直滑）
        const float damp = std::exp(-12.0f * dt);
        aVelocity.x *= damp;
        aVelocity.z *= damp;
    } else {
        aVelocity.x = horizontal.x * speed;
        aVelocity.z = horizontal.z * speed;
    }

    // 2) 起跳（只在着地时；起跳会立刻清掉向下的速度分量）
    if (jump && aGrounded) {
        aVelocity.y = aDesc.aJumpSpeed;
        aGrounded = false; // 本步不再是地面状态（否则下一步会立刻"吸附"回地面）
        result.aJumped = true;
    }

    // 3) 水平移动 + 解穿透
    const GNg::sVEC3 startPos = aPosition;
    GNg::sVEC3 pos = aPosition;
    pos.x += aVelocity.x * dt;
    pos.z += aVelocity.z * dt;

    GNg::sVEC3 blockNormal = vec(0.0f, 1.0f, 0.0f);
    float blockDepth = 0.0f;
    const bool blocked = deepestPenetration(pos, blockNormal, blockDepth);
    const float wanted = std::sqrt((aVelocity.x * dt) * (aVelocity.x * dt) +
                                   (aVelocity.z * dt) * (aVelocity.z * dt));

    // 4) 斜坡：挡住水平移动的是**可走坡**时，把水平位移投影到坡面上继续走。
    //    不做这步的话，45° 坡上一走就被解穿透推回去，表现为"上不去"。
    bool walkedSlope = false;
    if (blocked && blockNormal.y > 0.05f) {
        const float slope = std::acos(clampf(blockNormal.y, -1.0f, 1.0f)) * kDeg;
        if (slope <= aDesc.aMaxSlopeDeg + 0.5f) {
            const GNg::sVEC3 slide =
                projectOnPlane(vec(aVelocity.x * dt, 0.0f, aVelocity.z * dt), blockNormal);
            pos = add(pos, slide);
            walkedSlope = true;
        }
    }
    resolvePenetration(pos);

    // 5) 台阶抬升：只在"被**非可走**的面挡住 + 站着 + 台阶不高 + 抬上去确实能前进"时才做。
    //    四个条件缺一不可 —— 少了"非可走"这一条会在斜坡上每帧抬一次，人就像飘着走。
    float horizontalMoved = std::sqrt((pos.x - startPos.x) * (pos.x - startPos.x) +
                                      (pos.z - startPos.z) * (pos.z - startPos.z));
    if (hasInput && aGrounded && !walkedSlope && aDesc.aStepHeight > 0.0f &&
        horizontalMoved < wanted * 0.5f) {
        GNg::sVEC3 lifted = pos;
        lifted.y += aDesc.aStepHeight;
        if (resolvePenetration(lifted)) {
            GNg::sVEC3 stepped = lifted;
            stepped.x += aVelocity.x * dt;
            stepped.z += aVelocity.z * dt;
            if (resolvePenetration(stepped)) {
                const float moved =
                    std::sqrt((stepped.x - pos.x) * (stepped.x - pos.x) +
                              (stepped.z - pos.z) * (stepped.z - pos.z));
                // 抬上去之后必须真的往前走了，否则这是在墙前面白爬
                if (moved > wanted * 0.5f) {
                    // 落到台阶顶上：从抬升后的位置往下探，直到"再降一步就会穿进去"。
                    // 这样落点正好贴住台阶面，而不是悬在半空等重力（那会在下坡时抖）。
                    const int dropSteps = 16;
                    const float dropStep = aDesc.aStepHeight / (float)dropSteps;
                    GNg::sVEC3 dropped = stepped;
                    for (int i = 0; i < dropSteps; ++i) {
                        GNg::sVEC3 probePos = dropped;
                        probePos.y -= dropStep;
                        GNg::sVEC3 normal;
                        float depth = 0.0f;
                        if (deepestPenetration(probePos, normal, depth)) break;
                        dropped = probePos;
                    }
                    // 落点必须**站得住**（坡面 <= maxSlope）：否则在 60° 坡前面也能
                    // "抬一下 + 蹭一点"，一路蹭上陡坡 —— 那不是台阶，是爬墙。
                    float landNormalY = 0.0f;
                    float landSlope = 0.0f;
                    if (!probeGround(dropped, landNormalY, landSlope) ||
                        landSlope > aDesc.aMaxSlopeDeg + 0.5f) {
                        // 站不住：放弃这次抬升，保持原位置（后面按"被墙挡住"处理）
                        pos = aPosition;
                        resolvePenetration(pos);
                        result.aHitWall = true;
                        aStuck = false;
                    } else {
                        pos = dropped;
                        result.aSteppedUp = true;
                        aVelocity.x = 0.0f; // 这一帧的水平位移已经"用掉"了
                        aVelocity.z = 0.0f;
                    }
                }
            }
        }
    }
    if (blocked && !walkedSlope) result.aHitWall = true;
    aStuck = !resolvePenetration(pos);

    // 6) 竖直移动（重力位移）+ 撞头检测
    const float verticalStep = aVelocity.y * dt;
    if (verticalStep != 0.0f) {
        GNg::sVEC3 verticalPos = pos;
        // 贴地时把探测深度多压一个 skinWidth：否则"悬浮 skin 高度"的胶囊会先自由下落
        // 几毫米、再被解穿透弹回，形成 3 帧锯齿（实测 1.8–10 mm）。多探一点让它在
        // 每一步都被判定为接触并吸附到同一高度，位置才真正稳定。
        // 只对"没有水平意图"的静止站立做贴地吸附：上台阶/斜坡时多探一个 skin
        // 会把台阶侧面当成地面，挡住抬升（characterStepsUpShortStep 回归）。
        const bool idleThisStep = (aVelocity.x == 0.0f && aVelocity.z == 0.0f);
        const float snapProbe =
            (aGrounded && idleThisStep && aVelocity.y <= 0.0f) ? aDesc.aSkinWidth : 0.0f;
        verticalPos.y += verticalStep - snapProbe;
        GNg::sVEC3 normal;
        float depth = 0.0f;
        const bool blocked = deepestPenetration(verticalPos, normal, depth);
        if (blocked) {
            if (verticalStep > 0.0f) {
                result.aHitCeiling = true;
                aVelocity.y = 0.0f; // 撞头：清掉上升速度
            } else {
                aVelocity.y = 0.0f; // 落地：本步的向下位移被接触吸收
            }
            resolvePenetration(verticalPos);
            pos = verticalPos;
        } else {
            pos = verticalPos;
        }
    }

    // 7) 地面检测
    float normalY = 0.0f;
    float slope = 0.0f;
    aGrounded = false;
    if (probeGround(pos, normalY, slope)) {
        // 只有坡度可接受、且竖直速度不是明显向上（起跳瞬间）才算着地
        aGrounded = (slope <= aDesc.aMaxSlopeDeg + 0.5f) && (aVelocity.y <= 0.5f);
    }
    aGroundNormalY = aGrounded ? normalY : 0.0f;
    aSlopeDeg = aGrounded ? slope : 0.0f;

    // 8) 贴墙下滑：水平被墙挡住时不要卡住 —— 保留法线方向的推力，切向速度照常
    if (result.aHitWall) {
        GNg::sVEC3 normal;
        float depth = 0.0f;
        if (deepestPenetration(pos, normal, depth) && normal.y < 0.5f) {
            const float into = dot(aVelocity, normal);
            if (into < 0.0f) {
                aVelocity = sub(aVelocity, mul(normal, into)); // 去掉"往墙里"的分量
            }
        }
    }

    aPosition = pos;
    aFootPosition = pos;
    aFootPosition.y -= aDesc.aHeight * 0.5f;

    result.aGrounded = aGrounded;
    result.aGroundNormalY = aGroundNormalY;
    result.aSlopeDeg = aSlopeDeg;
    result.aHorizontalDistance = std::sqrt((pos.x - startPos.x) * (pos.x - startPos.x) +
                                           (pos.z - startPos.z) * (pos.z - startPos.z));
    return result;
}

sCHARACTER_STEP cCHARACTER::MoveIntent(const GNg::sVEC3 &forwardAxis, const GNg::sVEC3 &rightAxis,
                                       const sINTENT &intent, float dt) {
    GNg::sVEC3 wish = vec(0.0f, 0.0f, 0.0f);
    wish = add(wish, mul(forwardAxis, clampf(intent.aMoveForward, -1.0f, 1.0f)));
    wish = add(wish, mul(rightAxis, clampf(intent.aMoveRight, -1.0f, 1.0f)));
    return Move(wish, intent.aSprint, intent.aJump, dt);
}

} // namespace GN3d
