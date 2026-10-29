#pragma once
#ifndef GN_MMD_PMD_MODEL_H
#define GN_MMD_PMD_MODEL_H
// ╔══════════════════════════════════════════════════════════╗
// ║  GN_mini — PMD 模型数据结构                             ║
// ╚══════════════════════════════════════════════════════════╝
//
// 字段与参考实现 Go3D/lib/mmd/pmd 一一对应；差异只在编码：文本解析后统一为 UTF-8
// （参考实现保留原始 Shift-JIS 字节）。索引类字段保留文件中的原始宽度与哨兵值
// （0xFFFF 表示「无」），由使用方按需解释。
#include <cstdint>
#include <string>
#include <vector>

namespace mmd {

struct PmdHeader {
    std::string name;
    std::string nameEnglish;
    std::string comment;
    std::string commentEnglish;
};

struct PmdVertex {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float normal[3] = {0.0f, 0.0f, 0.0f};
    float uv[2] = {0.0f, 0.0f};
    uint16_t boneIndex[2] = {0, 0};
    uint8_t boneWeight = 0;
    bool edgeInvisible = false;
};

struct PmdMaterial {
    float diffuse[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float power = 0.0f;
    float specular[3] = {0.0f, 0.0f, 0.0f};
    float ambient[3] = {0.0f, 0.0f, 0.0f};
    uint8_t toonIndex = 0;
    uint8_t edgeFlag = 0;
    uint32_t indexCount = 0;
    std::string textureFilename;
    std::string sphereFilename;
};

// PMD 骨骼类型（顺序与文件中的类型字节一致）。
enum class PmdBoneType : uint8_t {
    ROTATION = 0,
    ROTATION_AND_MOVE = 1,
    IK_EFFECTOR = 2,
    UNKNOWN = 3,
    IK_EFFECTABLE = 4,
    ROTATION_EFFECTABLE = 5,
    IK_TARGET = 6,
    INVISIBLE = 7,
    TWIST = 8,
    ROTATION_MOVEMENT = 9
};

struct PmdBone {
    std::string name;
    std::string nameEnglish;
    uint16_t parentBoneIndex = 0xFFFF;
    uint16_t tailPosBoneIndex = 0xFFFF;
    PmdBoneType boneType = PmdBoneType::ROTATION;
    uint16_t ikParentBoneIndex = 0;
    float boneHeadPos[3] = {0.0f, 0.0f, 0.0f};
};

struct PmdIk {
    uint16_t ikBoneIndex = 0;
    uint16_t targetBoneIndex = 0;
    uint16_t iterations = 0;
    float angleLimit = 0.0f;
    std::vector<uint16_t> ikChildBoneIndex;
};

struct PmdFaceVertex {
    int32_t vertexIndex = 0;
    float position[3] = {0.0f, 0.0f, 0.0f};
};

// PMD 表情分类（顺序与文件中的分类字节一致）。
enum class PmdFaceCategory : uint8_t { BASE = 0, EYEBROW = 1, EYE = 2, MOUTH = 3, OTHER = 4 };

struct PmdFace {
    std::string name;
    PmdFaceCategory type = PmdFaceCategory::BASE;
    std::vector<PmdFaceVertex> vertices;
    std::string nameEnglish;
};

struct PmdBoneDispName {
    std::string name;
    std::string nameEnglish;
};

struct PmdBoneDisp {
    uint16_t boneIndex = 0;
    uint8_t boneDispIndex = 0;
};

// PMD 刚体形状（顺序与文件中的形状字节一致）。
enum class PmdRigidBodyShape : uint8_t { SPHERE = 0, BOX = 1, CAPSULE = 2 };

// PMD 刚体类型（顺序与文件中的类型字节一致）。
enum class PmdRigidBodyType : uint8_t { BONE_CONNECTED = 0, PHYSICS = 1, CONNECTED_PHYSICS = 2 };

struct PmdRigidBody {
    std::string name;
    uint16_t relatedBoneIndex = 0;
    uint8_t groupIndex = 0;
    uint16_t mask = 0;
    PmdRigidBodyShape shape = PmdRigidBodyShape::SPHERE;
    float size[3] = {0.0f, 0.0f, 0.0f};
    float position[3] = {0.0f, 0.0f, 0.0f};
    float orientation[3] = {0.0f, 0.0f, 0.0f};
    float weight = 0.0f;
    float linearDamping = 0.0f;
    float angularDamping = 0.0f;
    float restitution = 0.0f;
    float friction = 0.0f;
    PmdRigidBodyType rigidType = PmdRigidBodyType::BONE_CONNECTED;
};

struct PmdConstraint {
    std::string name;
    uint32_t rigidBodyIndexA = 0;
    uint32_t rigidBodyIndexB = 0;
    float position[3] = {0.0f, 0.0f, 0.0f};
    float orientation[3] = {0.0f, 0.0f, 0.0f};
    float linearLowerLimit[3] = {0.0f, 0.0f, 0.0f};
    float linearUpperLimit[3] = {0.0f, 0.0f, 0.0f};
    float angularLowerLimit[3] = {0.0f, 0.0f, 0.0f};
    float angularUpperLimit[3] = {0.0f, 0.0f, 0.0f};
    float linearStiffness[3] = {0.0f, 0.0f, 0.0f};
    float angularStiffness[3] = {0.0f, 0.0f, 0.0f};
};

struct PmdModel {
    float version = 0.0f;
    PmdHeader header;
    std::vector<PmdVertex> vertices;
    std::vector<uint16_t> indices;
    std::vector<PmdMaterial> materials;
    std::vector<PmdBone> bones;
    std::vector<PmdIk> iks;
    std::vector<PmdFace> faces;
    std::vector<uint16_t> faceIndices;
    std::vector<PmdBoneDispName> boneDispNames;
    std::vector<PmdBoneDisp> boneDisps;
    std::vector<std::string> toonFilenames;
    std::vector<PmdRigidBody> rigidBodies;
    std::vector<PmdConstraint> constraints;
};

} // namespace mmd
#endif // GN_MMD_PMD_MODEL_H
