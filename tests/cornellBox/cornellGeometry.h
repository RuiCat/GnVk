// tests/cornellBox/cornellGeometry.h — Cornell Box 的纯 CPU 几何契约
//
// 职责：用「四边形 + 球」描述 Cornell box（不依赖引擎/Vulkan），供两个目标共用：
//   * cornellBox（引擎实时）：把四边形/球三角化后提交 G-Buffer；
//   * cornellPt（CPU 路径追踪）：直接做解析求交，避免几何/材质两套定义漂移。
#ifndef CORNELL_GEOMETRY_H
#define CORNELL_GEOMETRY_H

#include <vector>

namespace Cornell {

// 房间尺寸（米）：x ∈ [0, W]、y ∈ [0, H]、z ∈ [0, D]，前面（z=D）开口。
const float ROOM_WIDTH = 5.5f;
const float ROOM_HEIGHT = 5.0f;
const float ROOM_DEPTH = 5.5f;

struct sVec3 {
    float x, y, z;
};

struct sMaterial {
    float aAlbedo[3];
    float aRoughness;
    float aMetallic;
    float aEmission[3]; // 0 = 不发光
    sMaterial();
};

// 四边形：p0 + a*u + b*v，a/b ∈ [0,1]；aNormal = normalize(cross(u,v))。
struct sQuad {
    sVec3 aP0;
    sVec3 aU;
    sVec3 aV;
    sVec3 aNormal;
    int aMaterial;
};

struct sSphere {
    sVec3 aCenter;
    float aRadius;
    int aMaterial;
};

struct sCornellGeometry {
    std::vector<sMaterial> aMaterials;
    std::vector<sQuad> aQuads;
    std::vector<sSphere> aSpheres;
    int aLightQuad; // 面光四边形下标；-1 = 无（sun 变体）
    sCornellGeometry();
};

// 规范相机位姿：位置 + 注视点（世界空间，y-up）。
struct sPose {
    sVec3 aPosition;
    sVec3 aFocus;
};

// 4 个 canonical 位姿（引擎相机与 PT 共用，保证同机位对比）。
extern const sPose CORNELL_POSES[4];
// 方向光的传播方向（与引擎 sLIGHT_UBO 的方向同向：光从光源射向场景）。
extern const sVec3 SUN_DIRECTION;

// 生成 Cornell box。areaLight=true 时加入顶部 1×1 面光（PT 用）；false 时只有 5 面墙 + 2 球。
void buildCornellGeometry(bool areaLight, sCornellGeometry *out);

// 材质下标（与 buildCornellGeometry 的 aMaterials 顺序一致）。
enum eMaterial {
    eMAT_WHITE = 0,
    eMAT_RED = 1,
    eMAT_GREEN = 2,
    eMAT_METAL = 3,
    eMAT_LIGHT = 4
};

} // namespace Cornell
#endif // CORNELL_GEOMETRY_H
