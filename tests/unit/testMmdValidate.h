#pragma once
#ifndef GN_TEST_MMD_VALIDATE_H
#define GN_TEST_MMD_VALIDATE_H
// 单测辅助：MMD 模型的结构不变量校验（索引不越界、材质索引数之和等于索引总数等）。
// 这些判据对任何合法模型都成立，因此既用于合成样本，也用于真实样本。
#include "mmd/pmdModel.h"
#include "mmd/pmxModel.h"

#include <cstdint>
#include <string>

namespace mmdTest {

inline bool indexInRange(int32_t value, size_t count) {
    return value >= 0 && static_cast<size_t>(value) < count;
}

inline bool optionalIndexInRange(int32_t value, size_t count) {
    return value == -1 || indexInRange(value, count);
}

inline bool optionalU16InRange(uint16_t value, size_t count) {
    return value == 0xFFFF || static_cast<size_t>(value) < count;
}

inline void setFail(std::string &why, const char *message) {
    if (why.empty()) {
        why = message;
    }
}

inline bool validatePmd(const mmd::PmdModel &model, std::string &why) {
    size_t materialIndexSum = 0;
    for (size_t i = 0; i < model.materials.size(); ++i) {
        materialIndexSum += model.materials[i].indexCount;
    }
    if (!model.materials.empty() && materialIndexSum != model.indices.size()) {
        setFail(why, "PMD 材质索引数之和 != 索引总数");
        return false;
    }
    for (size_t i = 0; i < model.indices.size(); ++i) {
        if (model.indices[i] >= model.vertices.size()) {
            setFail(why, "PMD 顶点索引越界");
            return false;
        }
    }
    for (size_t i = 0; i < model.bones.size(); ++i) {
        const mmd::PmdBone &bone = model.bones[i];
        if (!optionalU16InRange(bone.parentBoneIndex, model.bones.size()) ||
            !optionalU16InRange(bone.tailPosBoneIndex, model.bones.size())) {
            setFail(why, "PMD 骨骼父/尾索引越界");
            return false;
        }
    }
    for (size_t i = 0; i < model.iks.size(); ++i) {
        const mmd::PmdIk &ik = model.iks[i];
        if (ik.ikBoneIndex >= model.bones.size() || ik.targetBoneIndex >= model.bones.size()) {
            setFail(why, "PMD IK 骨骼索引越界");
            return false;
        }
        for (size_t j = 0; j < ik.ikChildBoneIndex.size(); ++j) {
            if (ik.ikChildBoneIndex[j] >= model.bones.size()) {
                setFail(why, "PMD IK 子骨骼索引越界");
                return false;
            }
        }
    }
    for (size_t i = 0; i < model.faces.size(); ++i) {
        for (size_t j = 0; j < model.faces[i].vertices.size(); ++j) {
            if (!indexInRange(model.faces[i].vertices[j].vertexIndex, model.vertices.size())) {
                setFail(why, "PMD 表情顶点索引越界");
                return false;
            }
        }
    }
    for (size_t i = 0; i < model.boneDisps.size(); ++i) {
        if (model.boneDisps[i].boneIndex >= model.bones.size()) {
            setFail(why, "PMD 骨骼显示索引越界");
            return false;
        }
    }
    for (size_t i = 0; i < model.rigidBodies.size(); ++i) {
        if (!optionalU16InRange(model.rigidBodies[i].relatedBoneIndex, model.bones.size())) {
            setFail(why, "PMD 刚体骨骼索引越界");
            return false;
        }
    }
    for (size_t i = 0; i < model.constraints.size(); ++i) {
        const mmd::PmdConstraint &constraint = model.constraints[i];
        if (constraint.rigidBodyIndexA >= model.rigidBodies.size() ||
            constraint.rigidBodyIndexB >= model.rigidBodies.size()) {
            setFail(why, "PMD 关节刚体索引越界");
            return false;
        }
    }
    return true;
}

inline bool validatePmx(const mmd::PmxModel &model, std::string &why) {
    size_t materialIndexSum = 0;
    for (size_t i = 0; i < model.materials.size(); ++i) {
        materialIndexSum += static_cast<size_t>(model.materials[i].indexCount);
    }
    if (!model.materials.empty() && materialIndexSum != model.indices.size()) {
        setFail(why, "PMX 材质索引数之和 != 索引总数");
        return false;
    }
    for (size_t i = 0; i < model.indices.size(); ++i) {
        if (!indexInRange(model.indices[i], model.vertices.size())) {
            setFail(why, "PMX 顶点索引越界");
            return false;
        }
    }
    for (size_t i = 0; i < model.vertices.size(); ++i) {
        const mmd::PmxVertexSkinning &skinning = model.vertices[i].skinning;
        for (int j = 0; j < 4; ++j) {
            if (!optionalIndexInRange(skinning.boneIndex[j], model.bones.size())) {
                setFail(why, "PMX 顶点蒙皮骨骼索引越界");
                return false;
            }
        }
    }
    for (size_t i = 0; i < model.materials.size(); ++i) {
        const mmd::PmxMaterial &material = model.materials[i];
        if (!optionalIndexInRange(material.diffuseTextureIndex, model.textures.size()) ||
            !optionalIndexInRange(material.sphereTextureIndex, model.textures.size())) {
            setFail(why, "PMX 材质贴图索引越界");
            return false;
        }
        if (material.commonToonFlag == 0 &&
            !optionalIndexInRange(material.toonTextureIndex, model.textures.size())) {
            setFail(why, "PMX 材质卡通贴图索引越界");
            return false;
        }
    }
    for (size_t i = 0; i < model.bones.size(); ++i) {
        const mmd::PmxBone &bone = model.bones[i];
        if (!optionalIndexInRange(bone.parentIndex, model.bones.size()) ||
            !optionalIndexInRange(bone.grantParentIndex, model.bones.size()) ||
            !optionalIndexInRange(bone.ikTargetBoneIndex, model.bones.size())) {
            setFail(why, "PMX 骨骼索引越界");
            return false;
        }
        for (size_t j = 0; j < bone.ikLinks.size(); ++j) {
            if (!optionalIndexInRange(bone.ikLinks[j].linkTarget, model.bones.size())) {
                setFail(why, "PMX IK 链接索引越界");
                return false;
            }
        }
    }
    for (size_t i = 0; i < model.morphs.size(); ++i) {
        const mmd::PmxMorph &morph = model.morphs[i];
        for (size_t j = 0; j < morph.vertexOffsets.size(); ++j) {
            if (!indexInRange(morph.vertexOffsets[j].vertexIndex, model.vertices.size())) {
                setFail(why, "PMX 顶点变形索引越界");
                return false;
            }
        }
        for (size_t j = 0; j < morph.uvOffsets.size(); ++j) {
            if (!indexInRange(morph.uvOffsets[j].vertexIndex, model.vertices.size())) {
                setFail(why, "PMX UV 变形索引越界");
                return false;
            }
        }
        for (size_t j = 0; j < morph.boneOffsets.size(); ++j) {
            if (!indexInRange(morph.boneOffsets[j].boneIndex, model.bones.size())) {
                setFail(why, "PMX 骨骼变形索引越界");
                return false;
            }
        }
        for (size_t j = 0; j < morph.materialOffsets.size(); ++j) {
            if (!indexInRange(morph.materialOffsets[j].materialIndex, model.materials.size())) {
                setFail(why, "PMX 材质变形索引越界");
                return false;
            }
        }
        for (size_t j = 0; j < morph.groupOffsets.size(); ++j) {
            if (!indexInRange(morph.groupOffsets[j].morphIndex, model.morphs.size())) {
                setFail(why, "PMX 组变形索引越界");
                return false;
            }
        }
        for (size_t j = 0; j < morph.flipOffsets.size(); ++j) {
            if (!indexInRange(morph.flipOffsets[j].morphIndex, model.morphs.size())) {
                setFail(why, "PMX Flip 变形索引越界");
                return false;
            }
        }
        for (size_t j = 0; j < morph.impulseOffsets.size(); ++j) {
            if (!indexInRange(morph.impulseOffsets[j].rigidBodyIndex, model.rigidBodies.size())) {
                setFail(why, "PMX Impulse 变形刚体索引越界");
                return false;
            }
        }
    }
    for (size_t i = 0; i < model.frames.size(); ++i) {
        for (size_t j = 0; j < model.frames[i].elements.size(); ++j) {
            const mmd::PmxFrameElement &element = model.frames[i].elements[j];
            const bool valid = element.elementTarget == 0
                                   ? indexInRange(element.index, model.bones.size())
                                   : indexInRange(element.index, model.morphs.size());
            if (!valid) {
                setFail(why, "PMX 显示枠元素索引越界");
                return false;
            }
        }
    }
    for (size_t i = 0; i < model.rigidBodies.size(); ++i) {
        if (!optionalIndexInRange(model.rigidBodies[i].targetBone, model.bones.size())) {
            setFail(why, "PMX 刚体骨骼索引越界");
            return false;
        }
    }
    for (size_t i = 0; i < model.joints.size(); ++i) {
        if (!optionalIndexInRange(model.joints[i].param.rigidBody1, model.rigidBodies.size()) ||
            !optionalIndexInRange(model.joints[i].param.rigidBody2, model.rigidBodies.size())) {
            setFail(why, "PMX 关节刚体索引越界");
            return false;
        }
    }
    for (size_t i = 0; i < model.softBodies.size(); ++i) {
        const mmd::PmxSoftBody &body = model.softBodies[i];
        if (!optionalIndexInRange(body.targetMaterial, model.materials.size())) {
            setFail(why, "PMX 软体材质索引越界");
            return false;
        }
        for (size_t j = 0; j < body.anchors.size(); ++j) {
            if (!indexInRange(body.anchors[j].relatedRigidBody, model.rigidBodies.size()) ||
                !indexInRange(body.anchors[j].relatedVertex, model.vertices.size())) {
                setFail(why, "PMX 软体锚点索引越界");
                return false;
            }
        }
        for (size_t j = 0; j < body.pinVertices.size(); ++j) {
            if (!indexInRange(body.pinVertices[j], model.vertices.size())) {
                setFail(why, "PMX 软体固定顶点索引越界");
                return false;
            }
        }
    }
    return true;
}

} // namespace mmdTest
#endif // GN_TEST_MMD_VALIDATE_H
