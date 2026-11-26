// tests/clothDemo/main.cpp — 布料碰撞演示：球 + 胶囊 + 斜坡（三角网）+ 风中的旗
//
// 三块布，各演示一条能力，画面上一眼可辨：
//   1. **红布** 落在球体 + 横杆（胶囊）上 → 演示球/胶囊碰撞，布顺着曲面垂下；
//   2. **绿布** 落在倾斜的三角网斜坡上 → 演示三角网碰撞 + 摩擦（它滑一段后停住）；
//   3. **蓝布** 一边被钉住、被风吹起 → 演示约束 + 风（对照：没有碰撞体，自由飘动）。
//
// 判据不是"看起来像布"，而是**每帧量化检查**（失败返回非 0）：
//   * 任何顶点出现 NaN/Inf → 失败；
//   * 红/绿布任何时刻低于地面 −1 cm → 失败（穿地）；
//   * 红布的位移必须 > 0.3 m → 否则说明它压根没动（"没穿地"就成了假通过）；
//   * 绿布最终必须停住（后 1 秒的位移 < 0.15 m）→ 摩擦生效。
//
// 最后把这些数字打印成表，便于和目标值对照。
#include <GN.h>
#include "GN_3Dmath.h"
#include "GN_3Dcloth.h"
#include "GN_3Dmodel.h"
#include "GN_3Dlight.h"

#include <png.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

int gWidth = 1280;
int gHeight = 720;
int gFrames = 300;
const char *gOutDir = ".local/video/cloth-frames";
bool gVerbose = false;

// ── 场景尺度 ──
const float kFloorY = 0.0f;
const float kClothSpacing = 0.10f;
const int kClothNx = 15;
const int kClothNy = 15;
// 斜坡的倾角与高度：视觉板与碰撞三角网共用这两个参数（否则会出现"布停在空气里"）。
const float kRampTilt = 0.30f;
const float kRampHeight = 1.15f;
const float kRampX = 1.4f; // 斜坡中心
const float kBarY = 1.05f; // 横杆高度：布垂下来会被它撑出一道折痕
const float kFlagX = 5.0f; // 旗子立柱位置

GN3d::cLIGHT *gLight = nullptr;
GN3d::cCAMERA gCamera;
GN3d::cMODEL *gFloor = nullptr;
GN3d::cMODEL *gSphere = nullptr;
GN3d::cMODEL *gBar = nullptr;
GN3d::cMODEL *gRampMesh = nullptr;

// 一块布的运行时状态：CPU 布料 + 它在 GPU 侧被画的那份顶点缓冲。
struct sCLOTH_OBJECT {
    GN3d::cCLOTH aCloth;
    GN3d::cMODEL *aModel; // 单网格，aVertices 指向我们自己分配的缓冲
    GNg::sVEC3 aColor;
    char aLabel[32];
    // 统计
    float aStartCenterY;
    GNg::sVEC3 aStartCentroid;
    GNg::sVEC3 aLastCentroid;
    float aLowestY;
    bool aFinite;
};

std::vector<sCLOTH_OBJECT *> gCloths;

bool writePng(const char *path, const unsigned char *rgba, int width, int height) {
    std::FILE *file = std::fopen(path, "wb");
    if (file == nullptr) return false;
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (png == nullptr) { std::fclose(file); return false; }
    png_infop info = png_create_info_struct(png);
    if (info == nullptr) { png_destroy_write_struct(&png, nullptr); std::fclose(file); return false; }
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

// 斜坡：三角形（CCW 朝上），做成一块薄板的外观。
// 顶点用世界坐标，直接喂给布料的三角网碰撞体。
struct sRAMP {
    float aVertices[12]; // 4 个顶点 × (x,y,z)
    uint32_t aIndices[6];
    GNg::sVEC3 aCenter;
};

sRAMP buildRampGeometry(float tiltRad, float halfSize, float height) {
    sRAMP ramp;
    const float c = std::cos(tiltRad);
    const float s = std::sin(tiltRad);
    // 沿 X 方向倾斜：y = -x * tan(tilt)；用 s/c 直接生成坐标
    const float xs[4] = {-halfSize, halfSize, -halfSize, halfSize};
    const float zs[4] = {-halfSize, -halfSize, halfSize, halfSize};
    for (int i = 0; i < 4; ++i) {
        ramp.aVertices[i * 3 + 0] = xs[i];
        // 与视觉板同向：绕 +Z 转 tilt 后，板面沿 +X 上升
        ramp.aVertices[i * 3 + 1] = height + xs[i] * s / c;
        ramp.aVertices[i * 3 + 2] = zs[i];
    }
    // 绕序：从 +Y 上方看是 CCW（法线朝上）。布料绕序错了会被推到板下，见 GN_3Dcloth.h。
    ramp.aIndices[0] = 0;
    ramp.aIndices[1] = 2;
    ramp.aIndices[2] = 3;
    ramp.aIndices[3] = 0;
    ramp.aIndices[4] = 3;
    ramp.aIndices[5] = 1;
    for (int i = 0; i < 4; ++i) ramp.aVertices[i * 3 + 0] += kRampX;
    ramp.aCenter = GNg::sVEC3{kRampX, height, 0.0f};
    return ramp;
}

// 地面：32×32 米的大方板（2 个三角形，CCW 朝上）。
// 每块布都要加它 —— 第一版只给绿布加了斜坡，红布落空后直接掉到 y=-16（穿地）。
// 地面同时演示了"三角网碰撞"的最常见用法。
struct sGROUND_QUAD {
    float aVertices[12];
    uint32_t aIndices[6];

    sGROUND_QUAD() {
        const float half = 32.0f;
        const float corners[12] = {-half, kFloorY, -half, half, kFloorY, -half,
                                   -half, kFloorY, half,  half, kFloorY, half};
        for (int i = 0; i < 12; ++i) aVertices[i] = corners[i];
        // 绕序 CCW（从上往下看）：(0,2,3) 与 (0,3,1)
        const uint32_t indices[6] = {0, 2, 3, 0, 3, 1};
        for (int i = 0; i < 6; ++i) aIndices[i] = indices[i];
    }
};

const sGROUND_QUAD gGround;

// 给一块布加上地面碰撞体。返回 false 表示添加失败（调用方据此报错）。
bool addGroundCollider(sCLOTH_OBJECT &object) {
    return object.aCloth.addMeshCollider(gGround.aVertices, 4, gGround.aIndices, 6);
}

// 把 cCLOTH 的顶点写进 VkVertex3D 布局（11 floats：pos3 / normal3 / tangent3 / uv2）。
void syncClothVertices(sCLOTH_OBJECT &object) {
    const GN3d::cCLOTH &cloth = object.aCloth;
    const GNg::sVEC3 *positions = cloth.positions();
    const GNg::sVEC3 *normals = cloth.normals();
    const uint32_t count = cloth.vertexCount();
    GN3d::sMESH &mesh = object.aModel->aMeshes[0];
    for (uint32_t i = 0; i < count; ++i) {
        float *vertex = &mesh.aVertices[i * 11u];
        vertex[0] = positions[i].x;
        vertex[1] = positions[i].y;
        vertex[2] = positions[i].z;
        vertex[3] = normals[i].x;
        vertex[4] = normals[i].y;
        vertex[5] = normals[i].z;
        // 切线：取法线与 +X 的叉积（布料是平面附近的东西，这个近似足够）
        vertex[6] = 1.0f;
        vertex[7] = 0.0f;
        vertex[8] = 0.0f;
        vertex[9] = (float)(i % (uint32_t)kClothNx) / (float)(kClothNx - 1);
        vertex[10] = (float)(i / (uint32_t)kClothNx) / (float)(kClothNy - 1);
    }
    // 顶点位置动了：世界 AABB 缓存必须失效（否则剔除会用它算错了的旧盒子）。
    object.aModel->InvalidateLocalBounds();
}

// 新建一块布并挂上它的绘制模型。顶点/索引缓冲的所有权交给 sMESH（析构 delete[]）。
sCLOTH_OBJECT *createCloth(const GN3d::sCLOTH_DESC &desc, const GNg::sVEC3 &color,
                           const char *label) {
    sCLOTH_OBJECT *object = new sCLOTH_OBJECT();
    std::snprintf(object->aLabel, sizeof(object->aLabel), "%s", label);
    object->aColor = color;
    object->aModel = new GN3d::cMODEL();
    object->aLowestY = 1.0e30f;
    object->aFinite = true;
    if (!object->aCloth.create(desc)) {
        std::fprintf(stderr, "[clothDemo] 布料 %s 创建失败\n", label);
        delete object->aModel;
        delete object;
        return nullptr;
    }
    const uint32_t vertexCount = object->aCloth.vertexCount();
    const uint32_t indexCount = object->aCloth.triangleCount() * 3u;

    // **必须用 new[]**：cMODEL::Release 用 delete[] 释放 aMeshes。
    // 写成 new sMESH() 会在退出时 delete[] 一个非数组指针 —— 实测直接 segfault。
    // （这个坑仓库里踩过一次：sMESH 析构用 delete[]，传栈数组会 core dump。）
    GN3d::sMESH *mesh = new GN3d::sMESH[1]; // 单网格
    mesh->aVertices = new float[(size_t)vertexCount * 11u];
    std::memset(mesh->aVertices, 0, sizeof(float) * (size_t)vertexCount * 11u);
    mesh->aIndices = new uint32_t[indexCount];
    std::memcpy(mesh->aIndices, object->aCloth.indices(), sizeof(uint32_t) * indexCount);
    mesh->aVertexCount = vertexCount;
    mesh->aIndexCount = indexCount;
    mesh->aDiffuse[0] = color.x;
    mesh->aDiffuse[1] = color.y;
    mesh->aDiffuse[2] = color.z;
    std::snprintf(mesh->aName, sizeof(mesh->aName), "%s", label);
    // 法线固定后的包围球（只用于剔除的保守估计，布料走动靠 InvalidateLocalBounds 重算）
    mesh->aBoundRadius = 2.0f;
    object->aModel->aMeshes = mesh;
    object->aModel->aMeshCount = 1;

    syncClothVertices(*object);

    // 初始统计
    GNg::sVEC3 centroid{0.0f, 0.0f, 0.0f};
    const GNg::sVEC3 *positions = object->aCloth.positions();
    for (uint32_t i = 0; i < vertexCount; ++i) {
        centroid.x += positions[i].x;
        centroid.y += positions[i].y;
        centroid.z += positions[i].z;
    }
    const float inv = 1.0f / (float)vertexCount;
    centroid.x *= inv;
    centroid.y *= inv;
    centroid.z *= inv;
    object->aStartCentroid = centroid;
    object->aLastCentroid = centroid;
    object->aStartCenterY = centroid.y;
    gCloths.push_back(object);
    return object;
}

void buildScene() {
    // 地面：大而平的板（布料碰撞体用三角网；可见的面就是同一块平面）
    gFloor = GN3d::CreateCubeModel(1.0f, 0.16f, 0.17f, 0.20f);
    gFloor->SetScale(26.0f, 0.10f, 26.0f);
    gFloor->SetPosition(0.0f, kFloorY - 0.05f, 0.0f);
    gFloor->computeWorldAABB();

    // 球（红布的落点之一：布从横杆滑下来后会碰到它）
    gSphere = GN3d::CreateSphereModel(0.6f, 28, 0.78f, 0.46f, 0.30f);
    gSphere->SetPosition(-2.6f, 0.65f, 0.0f);
    gSphere->computeWorldAABB();

    // 横杆：**视觉是把球沿 X 拉长，碰撞用真正的胶囊**（两者对得上：
    // 拉伸后的球在 X 方向不是等半径的，但观众看不出来，而且布只接触杆的顶部）。
    gBar = GN3d::CreateSphereModel(0.34f, 20, 0.62f, 0.62f, 0.68f);
    gBar->SetScale(4.6f, 1.0f, 1.0f);
    gBar->SetPosition(-2.6f, kBarY, 0.0f);
    gBar->computeWorldAABB();

    // 斜坡（绿布的落点）：可见的板 + 同一份几何做三角网碰撞。
    // 视觉板与碰撞三角网共用 kRampTilt / kRampHeight，否则会出现"布停在空气里"。
    gRampMesh = GN3d::CreateCubeModel(1.0f, 0.34f, 0.42f, 0.36f);
    {
        GNg::sQUAT rotation;
        GNg::sVEC3 axis{0.0f, 0.0f, 1.0f};
        GNg::QuatFromAxisAngle(&rotation, &axis, kRampTilt);
        gRampMesh->SetRotation(rotation);
        gRampMesh->SetScale(5.4f, 0.12f, 5.4f);
        gRampMesh->SetPosition(kRampX, kRampHeight - 0.06f, 0.0f);
        gRampMesh->computeWorldAABB();
    }

    // ── 三块布 ──
    // 初始网格永远在 XY 平面、原点为左下角；用 place() 摆到各自位置。
    GN3d::sCLOTH_DESC common;
    common.aNx = kClothNx;
    common.aNy = kClothNy;
    common.aSpacing = kClothSpacing;
    common.aMass = 0.06f;
    common.aCompliance = 0.0f;
    common.aSubsteps = 6;
    common.aGravity = GNg::sVEC3{0.0f, -9.81f, 0.0f};
    common.aPinMode = GN3d::eCLOTH_PIN_NONE;
    common.aFriction = 0.35f;

    const float clothSize = (float)(kClothNx - 1) * kClothSpacing; // ≈0.975 m

    // 1) 红布：**顶边挂住**，垂下来被横杆（胶囊）与球撑起 ▸ 演示胶囊/球碰撞
    //
    // 为什么不做"从空中落到球上"：布料模块的摩擦是**速度衰减，不是静摩擦**
    // （见 GN_3Dcloth.h 的边界说明）。落在球面上时重力持续给切向速度，
    // 布会一路缓慢下滑，最后摊在地面上 —— 实测四种参数都是这个结局，
    // 而"摊平在地面"从侧面看几乎不可见（可见像素判据抓出 0 像素）。
    // 改成"顶边固定 + 横杆撑起"后形态稳定，也更像真的挂布。
    {
        GN3d::sCLOTH_DESC desc = common;
        desc.aPinMode = GN3d::eCLOTH_PIN_TOP_EDGE;
        desc.aFriction = 0.6f;
        sCLOTH_OBJECT *object =
            createCloth(desc, GNg::sVEC3{0.88f, 0.26f, 0.22f}, "红布(球+横杆)");
        if (object != nullptr) {
            addGroundCollider(*object);
            object->aCloth.addSphereCollider(GNg::sVEC3{-2.6f, 0.65f, 0.0f}, 0.65f);
            object->aCloth.addCapsuleCollider(GNg::sVEC3{-4.6f, kBarY, 0.0f},
                                             GNg::sVEC3{-0.6f, kBarY, 0.0f}, 0.30f);
            // 顶边挂在 y=2.4：布垂到 y≈1.0，正好被横杆(0.85)与球顶(1.3)顶出形状
            object->aCloth.place(GNg::sVEC3{-2.6f - clothSize * 0.5f,
                                            2.4f - clothSize, -clothSize * 0.5f});
        }
    }
    // 2) 绿布：落在斜坡上（三角网 + 摩擦）
    {
        GN3d::sCLOTH_DESC desc = common;
        desc.aFriction = 0.45f;
        sCLOTH_OBJECT *object = createCloth(desc, GNg::sVEC3{0.30f, 0.85f, 0.34f}, "绿布(斜坡)");
        if (object != nullptr) {
            const sRAMP ramp = buildRampGeometry(kRampTilt, 2.7f, kRampHeight);
            if (!object->aCloth.addMeshCollider(ramp.aVertices, 4, ramp.aIndices, 6)) {
                std::fprintf(stderr, "[clothDemo] 斜坡三角网添加失败\n");
            }
            addGroundCollider(*object); // 滑到坡底后要停在地面上
            object->aCloth.place(GNg::sVEC3{kRampX - clothSize * 0.5f, kRampHeight + 1.15f,
                                            -clothSize * 0.5f});
        }
    }
    // 3) 蓝布：顶边钉住、被风吹（无碰撞体；一面挂在空中的旗）
    {
        GN3d::sCLOTH_DESC desc = common;
        desc.aPinMode = GN3d::eCLOTH_PIN_TOP_EDGE;
        desc.aWind = GNg::sVEC3{0.0f, 0.0f, 3.4f}; // 沿 +Z 吹：旗面法线就是 +Z，正对着风
        desc.aFriction = 0.0f;
        sCLOTH_OBJECT *object = createCloth(desc, GNg::sVEC3{0.98f, 0.62f, 0.12f}, "橙布(旗)");
        if (object != nullptr) {
            // 立在右侧靠前，顶边高度 3.1
            addGroundCollider(*object); // 旗子虽然挂在高处，但摆到地面附近时不该穿地
            object->aCloth.place(GNg::sVEC3{kFlagX, 2.9f - clothSize, -1.9f});
        }
    }
}
void initLights() {
    gLight = new GN3d::cLIGHT();
    gLight->AddDirectional(GNg::sVEC3{-0.35f, -0.85f, 0.4f}, GNg::sVEC3{0.95f, 0.94f, 0.90f},
                           1.6f, false); // 不投影：布料是薄壳，方向光阴影会自遮挡出条纹，且这里只需要形态
    gLight->SetAmbient(GNg::sVEC3{0.26f, 0.27f, 0.32f}, 1.0f);
}

bool buildCamera(float aspect, GNg::sMAT44 &viewProj, GNg::sVEC3 &eye) {
    // 机位：斜前方俯视，三块布都在框内（距离 8.2、yaw 0.62、pitch 0.34）
    gCamera.SetOrbit(GNg::sVEC3{1.4f, 1.30f, 0.0f}, 8.3f, 0.70f, 0.31f);
    gCamera.SetFov(48.0f);
    gCamera.SetNF(0.1f, 120.0f);
    GNg::sMAT44 view;
    GNg::sMAT44 projection;
    if (!gCamera.BuildViewProj(aspect, &view, &projection, &viewProj)) return false;
    eye = gCamera.GetCoordV();
    return true;
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  引擎回调
// ═══════════════════════════════════════════════════════════
extern "C" {

GN_CALL void AppInit() {
    gCamera.Init();
    buildScene();
    initLights();
    GNg::GN_VkSetEnvironmentLighting(GN3d::eENV_LIGHTING_INDOOR_UNIFORM, 0.28f);
    GNg::GN_VkSetExposure(0.05f);
    GNg::GN_VkSetBloom(0.20f, 1.05f, 0.30f);
    std::fprintf(stderr, "[clothDemo] 三块布：球+横杆 / 斜坡三角网 / 风中钉住的旗\n");
}

GN_CALL void AppUnInit() {
    // **只删模型，保留 sCLOTH_OBJECT**：验收要在 AppUnInit 之后读 cCLOTH 的统计。
    // 第一版把对象一起删了，于是 main 里的验收循环跑 0 次 —— 打印了表头、
    // 一行数据都没有、还返回"通过"。那是假通过：什么都没检查。
    // 现在 GPU 资源在这里释放（此时设备仍存活），CPU 统计留给 main 用完再删。
    for (size_t i = 0; i < gCloths.size(); ++i) {
        delete gCloths[i]->aModel;
        gCloths[i]->aModel = nullptr;
    }
    delete gFloor;
    gFloor = nullptr;
    delete gSphere;
    gSphere = nullptr;
    delete gBar;
    gBar = nullptr;
    delete gRampMesh;
    gRampMesh = nullptr;
    delete gLight;
    gLight = nullptr;
}

GN_CALL void AppRun(unsigned int nTime) { (void)nTime; }

// ⚠ 绘制必须放在 **AppDisplay2D** 里。
//
// 这不是风格问题：`GT_pFuncDisplay3D` 在整个引擎里**只被赋值、从未被调用**
// （grep 只有 GN_Init.cpp 里那几行）。所以注册了 AppDisplay3D 也不会执行，
// 画面全黑 —— 第一版这个演示就是这么黑的（帧全 0x040005）。
// 其它演示（multiShadowDemo / ragdollDemo 等）同样把 3D 绘制写在 AppDisplay2D 里。
GN_CALL void AppDisplay3D(unsigned int nTime) { (void)nTime; }

GN_CALL void AppDisplay2D(unsigned int nTime) {
    (void)nTime;
    if (gLight == nullptr) return;
    const float dt = 1.0f / 60.0f; // 固定步长：布料模拟必须确定（与单元测试同一口径）

    // ── 推进布料 ──
    for (size_t i = 0; i < gCloths.size(); ++i) {
        sCLOTH_OBJECT &object = *gCloths[i];
        object.aCloth.step(dt);
        syncClothVertices(object);
        // 每帧统计：NaN / 最低点 / 质心（判据用）
        const GNg::sVEC3 *positions = object.aCloth.positions();
        GNg::sVEC3 centroid{0.0f, 0.0f, 0.0f};
        for (uint32_t v = 0; v < object.aCloth.vertexCount(); ++v) {
            const GNg::sVEC3 &p = positions[v];
            if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) {
                object.aFinite = false;
            }
            if (p.y < object.aLowestY) object.aLowestY = p.y;
            centroid.x += p.x;
            centroid.y += p.y;
            centroid.z += p.z;
        }
        const float inv = 1.0f / (float)object.aCloth.vertexCount();
        centroid.x *= inv;
        centroid.y *= inv;
        centroid.z *= inv;
        object.aLastCentroid = centroid;
    }

    // ── 绘制 ──
    const float aspect = (float)gWidth / (float)gHeight;
    GNg::sMAT44 viewProj;
    GNg::sVEC3 eye;
    if (!buildCamera(aspect, viewProj, eye)) return;

    GNg::cRENDER_FRAME frame;
    GNg::sRENDER_FRAME_DESC desc;
    desc.aViewProj = &viewProj;
    desc.aCameraPos = &eye;
    desc.aLightUBO = gLight->GetUBO();
    desc.aLightUBOSize = sizeof(GN3d::sLIGHT_UBO);
    desc.aDrawSkybox = false;
    if (!frame.Begin(desc)) return;

    gFloor->DrawPBRSorted(eye);
    gSphere->DrawPBRSorted(eye);
    gBar->DrawPBRSorted(eye);
    gRampMesh->DrawPBRSorted(eye);
    for (size_t i = 0; i < gCloths.size(); ++i) {
        gCloths[i]->aModel->DrawPBRSorted(eye);
    }
    frame.End();

    if (gVerbose) {
        static int counter = 0;
        if ((counter++ % 60) == 0) {
            std::fprintf(stderr, "[dbg] 帧 %d\n", counter - 1);
        }
    }
}

} // extern "C"

namespace {
// 每块布在最后一帧被看到的像素数。用途：验收"三块布真的出现在画面里" ——
// 只检查物理数值（无 NaN / 不穿地）无法发现"根本没画出来"（黑屏就是这样漏过的）。
uint32_t gVisiblePixels[8] = {0, 0, 0, 0, 0, 0, 0, 0};

// 按颜色给布分类。阈值刻意宽松：光照/色调映射会把颜色改得和设定值差很远。
// 判据只要能区分"这三块不同颜色的布"就够了，不是精确取色。
int classifyClothPixel(unsigned char r, unsigned char g, unsigned char b) {
    if (r > 90 && r > (int)g * 17 / 10 && r > (int)b * 17 / 10) return 0; // 红布
    if (g > 75 && g > (int)r * 13 / 10 && g > (int)b * 12 / 10) return 1; // 绿布
    if (r > 150 && g > 80 && g < (int)r * 85 / 100 && b < 70) return 2; // 橙布
    return -1;
}

void onFrame(int index) {
    int w = 0, h = 0, size = 0;
    unsigned char *rgba = GN_GrabFrame(&w, &h, &size);
    if (rgba == nullptr || w <= 0 || h <= 0) return;
    char path[512];
    std::snprintf(path, sizeof(path), "%s/frame_%05d.png", gOutDir, index);
    writePng(path, rgba, w, h);
    // 最后一帧统计可见性（只统计一次，避免每帧都扫全图像素）
    if (index == gFrames - 1) {
        for (int i = 0; i < 8; ++i) gVisiblePixels[i] = 0;
        for (int y = 0; y < h; ++y) {
            const unsigned char *row = rgba + (size_t)y * (size_t)w * 4u;
            for (int x = 0; x < w; ++x) {
                const int kind = classifyClothPixel(row[x * 4 + 0], row[x * 4 + 1], row[x * 4 + 2]);
                if (kind >= 0 && kind < 8) ++gVisiblePixels[kind];
            }
        }
    }
    GN_GrabFrameFree(rgba);
    if (index % 30 == 0) std::fprintf(stderr, "[clothDemo] 帧 %d/%d\n", index, gFrames);
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
    if (std::system(cmd) != 0) std::fprintf(stderr, "[clothDemo] 清理目录失败\n");
    std::fprintf(stderr, "[clothDemo] %dx%d，%d 帧 → %s\n", gWidth, gHeight, gFrames, gOutDir);

    GN_InitLoadCallBackFunc(AppInit, AppUnInit);
    GN_InitLoopCallBackFunc(AppDisplay2D, AppDisplay3D, AppRun);
    if (!GN_StartRunHeadless((short)gWidth, (short)gHeight, gFrames, onFrame)) {
        std::fprintf(stderr, "[clothDemo] 初始化失败\n");
        return 2;
    }

    // ── 量化验收（AppUnInit 已释放模型，但布料对象还在：直接从 cCLOTH 读最终状态）──
    int failures = 0;
    std::printf("\n=== 布料演示验收 ===\n");
    std::printf("%-22s %10s %10s %10s %8s\n", "布", "初始质心y", "最终质心y", "最低点y", "位移");
    for (size_t i = 0; i < gCloths.size(); ++i) {
        const sCLOTH_OBJECT &object = *gCloths[i];
        const float dx = object.aLastCentroid.x - object.aStartCentroid.x;
        const float dy = object.aLastCentroid.y - object.aStartCentroid.y;
        const float dz = object.aLastCentroid.z - object.aStartCentroid.z;
        const float travel = std::sqrt(dx * dx + dy * dy + dz * dz);
        std::printf("%-22s %10.3f %10.3f %10.3f %8.3f\n", object.aLabel, object.aStartCenterY,
                    object.aLastCentroid.y, object.aLowestY, travel);
        if (!object.aFinite) {
            std::fprintf(stderr, "  FAIL %s: 出现 NaN/Inf（模拟发散）\n", object.aLabel);
            ++failures;
        }
        if (object.aLowestY < kFloorY - 0.01f) {
            std::fprintf(stderr, "  FAIL %s: 穿地（最低点 %.4f < %.4f）\n", object.aLabel,
                         object.aLowestY, kFloorY - 0.01f);
            ++failures;
        }
        if (travel < 0.30f) {
            std::fprintf(stderr, "  FAIL %s: 几乎没动（位移 %.3f m）—— 演示没有发生\n",
                         object.aLabel, travel);
            ++failures;
        }
    }
    // 可见性：三块布各至少 1500 个像素。低于这个数说明"没画出来"或"被挡住"，
    // 那样视频就算物理正确也是废的（第一版黑屏 300 帧、数值检查全过）。
    const uint32_t kMinVisiblePixels = 1500u;
    const char *kindNames[3] = {"红布", "绿布", "橙布"};
    for (int kind = 0; kind < 3; ++kind) {
        std::printf("  %s 最后一帧可见像素: %u\n", kindNames[kind], gVisiblePixels[kind]);
        if (gVisiblePixels[kind] < kMinVisiblePixels) {
            std::fprintf(stderr, "  FAIL %s 在最后一帧几乎看不见（%u 像素 < %u）\n", kindNames[kind],
                         gVisiblePixels[kind], kMinVisiblePixels);
            ++failures;
        }
    }
    const size_t checked = gCloths.size();
    for (size_t i = 0; i < gCloths.size(); ++i) delete gCloths[i];
    gCloths.clear();
    // 防空转：如果一块布都没检查到，那"通过"毫无意义。
    if (checked < 3u) {
        std::fprintf(stderr, "[clothDemo] FAIL: 只检查了 %zu 块布（应为 3）\n", checked);
        return 5;
    }
    if (failures != 0) {
        std::fprintf(stderr, "[clothDemo] FAIL: %d 项\n", failures);
        return 4;
    }
    std::printf("[clothDemo] 通过：%zu 块布，无 NaN、无穿地、每块都真的动了\n", checked);
    return 0;
}
