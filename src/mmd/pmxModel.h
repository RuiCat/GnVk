#pragma once
#ifndef GN_MMD_PMX_MODEL_H
#define GN_MMD_PMX_MODEL_H
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — PMX (2.0/2.1) 模型数据结构                   ║
// ╚══════════════════════════════════════════════════════════╝
//
// 字段与参考实现 Go3D/lib/mmd/pmx 一一对应；文本统一为 UTF-8，索引统一为 int32
// （宽度由文件头的 Setting 决定，读取时已解析，-1 表示「无」）。
// 与参考实现的差异：
//   * 骨骼/顶点蒙皮用带类型标签的单一结构，避免每个顶点一次堆分配；
//   * 材质变形（MorphMaterialOffset）的材质索引按 PMX 规范使用 materialIndexSize
//     （参考实现误用 morphIndexSize，仅在两者不同时才有区别）；
//   * 补齐参考实现未解析的 Flip(9) / Impulse(10) 变形与 SoftBody。
#include <cstdint>
#include <string>
#include <vector>

namespace mmd {

struct PmxSetting {
    uint8_t encoding = 0; // 0 = UTF-16LE, 1 = UTF-8
    uint8_t additionalUv = 0; // 追加 UV 组数
    uint8_t vertexIndexSize = 2;
    uint8_t textureIndexSize = 2;
    uint8_t materialIndexSize = 2;
    uint8_t boneIndexSize = 2;
    uint8_t morphIndexSize = 2;
    uint8_t rigidBodyIndexSize = 2;
};

// 顶点蒙皮类型（顺序与文件中的类型字节一致）。
enum class PmxSkinningType : uint8_t { BDEF1 = 0, BDEF2 = 1, BDEF4 = 2, SDEF = 3, QDEF = 4 };

struct PmxVertexSkinning {
    PmxSkinningType type = PmxSkinningType::BDEF1;
    int32_t boneIndex[4] = {-1, -1, -1, -1};
    float boneWeight[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float sdefC[3] = {0.0f, 0.0f, 0.0f};
    float sdefR0[3] = {0.0f, 0.0f, 0.0f};
    float sdefR1[3] = {0.0f, 0.0f, 0.0f};
};

struct PmxVertex {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float normal[3] = {0.0f, 0.0f, 0.0f};
    float uv[2] = {0.0f, 0.0f};
    float additionalUv[4][4] = {};
    PmxVertexSkinning skinning;
    float edge = 1.0f;
};

struct PmxMaterial {
    std::string name;
    std::string englishName;
    float diffuse[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float specular[3] = {0.0f, 0.0f, 0.0f};
    float specularity = 0.0f;
    float ambient[3] = {0.0f, 0.0f, 0.0f};
    uint8_t flag = 0;
    float edgeColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float edgeSize = 0.0f;
    int32_t diffuseTextureIndex = -1;
    int32_t sphereTextureIndex = -1;
    uint8_t sphereOpMode = 0;
    uint8_t commonToonFlag = 0;
    int32_t toonTextureIndex = -1;
    std::string memo;
    int32_t indexCount = 0;
};

struct PmxIkLink {
    int32_t linkTarget = -1;
    uint8_t angleLock = 0;
    float maxRadian[3] = {0.0f, 0.0f, 0.0f};
    float minRadian[3] = {0.0f, 0.0f, 0.0f};
};

struct PmxBone {
    std::string name;
    std::string englishName;
    float position[3] = {0.0f, 0.0f, 0.0f};
    int32_t parentIndex = -1;
    int32_t level = 0;
    uint16_t flag = 0;
    float offset[3] = {0.0f, 0.0f, 0.0f};
    int32_t targetIndex = -1;
    int32_t grantParentIndex = -1;
    float grantWeight = 0.0f;
    float lockAxisOrientation[3] = {0.0f, 0.0f, 0.0f};
    float localAxisXOrientation[3] = {0.0f, 0.0f, 0.0f};
    float localAxisYOrientation[3] = {0.0f, 0.0f, 0.0f};
    int32_t key = 0;
    int32_t ikTargetBoneIndex = -1;
    int32_t ikLoop = 0;
    float ikLoopAngleLimit = 0.0f;
    std::vector<PmxIkLink> ikLinks;
};

// PMX 变形类型（顺序与文件中的类型字节一致）。
enum class PmxMorphType : uint8_t {
    GROUP = 0,
    VERTEX = 1,
    BONE = 2,
    UV = 3,
    ADDITIONAL_UV1 = 4,
    ADDITIONAL_UV2 = 5,
    ADDITIONAL_UV3 = 6,
    ADDITIONAL_UV4 = 7,
    MATERIAL = 8,
    FLIP = 9,
    IMPULSE = 10
};

// PMX 变形分类（顺序与文件中的分类字节一致）。
enum class PmxMorphCategory : uint8_t { RESERVED = 0, EYEBROW = 1, EYE = 2, MOUTH = 3, OTHER = 4 };

struct PmxMorphVertexOffset {
    int32_t vertexIndex = -1;
    float positionOffset[3] = {0.0f, 0.0f, 0.0f};
};

struct PmxMorphUvOffset {
    int32_t vertexIndex = -1;
    float uvOffset[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

struct PmxMorphBoneOffset {
    int32_t boneIndex = -1;
    float translation[3] = {0.0f, 0.0f, 0.0f};
    float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
};

struct PmxMorphMaterialOffset {
    int32_t materialIndex = -1;
    uint8_t offsetOperation = 0;
    float diffuse[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float specular[3] = {0.0f, 0.0f, 0.0f};
    float specularity = 0.0f;
    float ambient[3] = {0.0f, 0.0f, 0.0f};
    float edgeColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float edgeSize = 0.0f;
    float textureArgb[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float sphereTextureArgb[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float toonTextureArgb[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

struct PmxMorphGroupOffset {
    int32_t morphIndex = -1;
    float morphWeight = 0.0f;
};

struct PmxMorphFlipOffset {
    int32_t morphIndex = -1;
    float morphValue = 0.0f;
};

struct PmxMorphImpulseOffset {
    int32_t rigidBodyIndex = -1;
    uint8_t isLocal = 0;
    float velocity[3] = {0.0f, 0.0f, 0.0f};
    float angularTorque[3] = {0.0f, 0.0f, 0.0f};
};

struct PmxMorph {
    std::string name;
    std::string englishName;
    PmxMorphCategory category = PmxMorphCategory::RESERVED;
    PmxMorphType type = PmxMorphType::GROUP;
    std::vector<PmxMorphVertexOffset> vertexOffsets;
    std::vector<PmxMorphUvOffset> uvOffsets;
    std::vector<PmxMorphBoneOffset> boneOffsets;
    std::vector<PmxMorphMaterialOffset> materialOffsets;
    std::vector<PmxMorphGroupOffset> groupOffsets;
    std::vector<PmxMorphFlipOffset> flipOffsets;
    std::vector<PmxMorphImpulseOffset> impulseOffsets;
};

struct PmxFrameElement {
    uint8_t elementTarget = 0;
    int32_t index = -1;
};

struct PmxFrame {
    std::string name;
    std::string englishName;
    uint8_t flag = 0;
    std::vector<PmxFrameElement> elements;
};

struct PmxRigidBody {
    std::string name;
    std::string englishName;
    int32_t targetBone = -1;
    uint8_t group = 0;
    uint16_t mask = 0;
    uint8_t shape = 0;
    float size[3] = {0.0f, 0.0f, 0.0f};
    float position[3] = {0.0f, 0.0f, 0.0f};
    float orientation[3] = {0.0f, 0.0f, 0.0f};
    float mass = 0.0f;
    float moveAttenuation = 0.0f;
    float rotationAttenuation = 0.0f;
    float repulsion = 0.0f;
    float friction = 0.0f;
    uint8_t physicsCalcType = 0;
};

// PMX 关节类型（顺序与文件中的类型字节一致）。
enum class PmxJointType : uint8_t {
    GENERIC_6DOF_SPRING = 0,
    GENERIC_6DOF = 1,
    POINT_2_POINT = 2,
    CONE_TWIST = 3,
    SLIDER = 5,
    HINGE = 6
};

struct PmxJointParam {
    int32_t rigidBody1 = -1;
    int32_t rigidBody2 = -1;
    float position[3] = {0.0f, 0.0f, 0.0f};
    float orientation[3] = {0.0f, 0.0f, 0.0f};
    float moveLimitationMin[3] = {0.0f, 0.0f, 0.0f};
    float moveLimitationMax[3] = {0.0f, 0.0f, 0.0f};
    float rotationLimitationMin[3] = {0.0f, 0.0f, 0.0f};
    float rotationLimitationMax[3] = {0.0f, 0.0f, 0.0f};
    float springMoveCoefficient[3] = {0.0f, 0.0f, 0.0f};
    float springRotationCoefficient[3] = {0.0f, 0.0f, 0.0f};
};

struct PmxJoint {
    std::string name;
    std::string englishName;
    PmxJointType type = PmxJointType::GENERIC_6DOF_SPRING;
    PmxJointParam param;
};

struct PmxAnchorRigidBody {
    int32_t relatedRigidBody = -1;
    int32_t relatedVertex = -1;
    bool isNear = false;
};

struct PmxSoftBody {
    std::string name;
    std::string englishName;
    uint8_t shape = 0;
    int32_t targetMaterial = -1;
    uint8_t group = 0;
    uint16_t mask = 0;
    uint8_t flag = 0;
    int32_t blinkDistance = 0;
    int32_t clusterCount = 0;
    float mass = 0.0f;
    float collisionMargin = 0.0f;
    int32_t aeroModel = 0;
    float vcf = 0.0f;
    float dp = 0.0f;
    float dg = 0.0f;
    float lf = 0.0f;
    float pr = 0.0f;
    float vc = 0.0f;
    float df = 0.0f;
    float mt = 0.0f;
    float chr = 0.0f;
    float khr = 0.0f;
    float shr = 0.0f;
    float ahr = 0.0f;
    float srhRcl = 0.0f;
    float skhRcl = 0.0f;
    float sshRcl = 0.0f;
    float srsPltCl = 0.0f;
    float sksPltCl = 0.0f;
    float sssPltCl = 0.0f;
    int32_t vit = 0;
    int32_t pit = 0;
    int32_t dit = 0;
    int32_t cit = 0;
    float lst = 0.0f;
    float ast = 0.0f;
    float vst = 0.0f;
    std::vector<PmxAnchorRigidBody> anchors;
    std::vector<int32_t> pinVertices;
};

struct PmxModel {
    float version = 0.0f;
    PmxSetting setting;
    std::string name;
    std::string englishName;
    std::string comment;
    std::string englishComment;
    std::vector<PmxVertex> vertices;
    std::vector<int32_t> indices;
    std::vector<std::string> textures;
    std::vector<PmxMaterial> materials;
    std::vector<PmxBone> bones;
    std::vector<PmxMorph> morphs;
    std::vector<PmxFrame> frames;
    std::vector<PmxRigidBody> rigidBodies;
    std::vector<PmxJoint> joints;
    std::vector<PmxSoftBody> softBodies;
};

} // namespace mmd
#endif // GN_MMD_PMX_MODEL_H
