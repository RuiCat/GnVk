// PMX 加载器实现：按参考实现 Go3D/lib/mmd/pmx 的段顺序读取，所有读取带边界检查。
#include "pmxLoader.h"

#include "mmdByteReader.h"
#include "mmdEncoding.h"
#include "mmdFile.h"
#include "pmxParse.h"

#include <vector>

namespace mmd {
namespace detail {

bool readPmxIndex(ByteReader &reader, uint8_t size, int32_t &value) {
    if (size == 1) {
        uint8_t raw = 0;
        if (!reader.readU8(raw)) {
            return false;
        }
        value = (raw == 255) ? -1 : static_cast<int32_t>(raw);
        return true;
    }
    if (size == 2) {
        uint16_t raw = 0;
        if (!reader.readU16(raw)) {
            return false;
        }
        value = (raw == 65535) ? -1 : static_cast<int32_t>(raw);
        return true;
    }
    if (size == 4) {
        return reader.readI32(value);
    }
    return false;
}

bool readPmxString(ByteReader &reader, uint8_t encoding, std::string &value) {
    int32_t size = 0;
    if (!reader.readI32(size) || size < 0) {
        return false;
    }
    if (size == 0) {
        value.clear();
        return true;
    }
    if (static_cast<size_t>(size) > reader.remaining()) {
        return false;
    }
    std::string raw;
    if (!reader.readBytes(static_cast<size_t>(size), raw)) {
        return false;
    }
    if (encoding == 0) {
        value = utf16LeToUtf8(reinterpret_cast<const uint8_t *>(raw.data()), raw.size());
    } else {
        value.swap(raw);
    }
    return true;
}

bool readFloat3(ByteReader &reader, float out[3]) {
    reader.readF32(out[0]);
    reader.readF32(out[1]);
    reader.readF32(out[2]);
    return reader.ok();
}

bool readFloat4(ByteReader &reader, float out[4]) {
    reader.readF32(out[0]);
    reader.readF32(out[1]);
    reader.readF32(out[2]);
    reader.readF32(out[3]);
    return reader.ok();
}

} // namespace detail

namespace {

using detail::readFloat3;
using detail::readFloat4;
using detail::readPmxIndex;

const size_t PMX_VERTEX_MIN_BYTES = 32;

bool fail(std::string *error, const char *message) {
    if (error) {
        *error = message;
    }
    return false;
}

bool readCount(ByteReader &reader, size_t minBytes, int32_t &count, std::string *error) {
    if (!reader.readI32(count) || count < 0) {
        return fail(error, "PMX 计数无效或数据截断");
    }
    if (minBytes > 0 && static_cast<size_t>(count) > reader.remaining() / minBytes) {
        return fail(error, "PMX 数据截断");
    }
    return true;
}

bool isValidIndexSize(uint8_t size) { return size == 1 || size == 2 || size == 4; }

bool readSetting(ByteReader &reader, PmxSetting &setting, std::string *error) {
    uint8_t count = 0;
    if (!reader.readU8(count) || count < 8) {
        return fail(error, "PMX 头部设置段无效");
    }
    reader.readU8(setting.encoding);
    reader.readU8(setting.additionalUv);
    reader.readU8(setting.vertexIndexSize);
    reader.readU8(setting.textureIndexSize);
    reader.readU8(setting.materialIndexSize);
    reader.readU8(setting.boneIndexSize);
    reader.readU8(setting.morphIndexSize);
    reader.readU8(setting.rigidBodyIndexSize);
    if (!reader.ok()) {
        return fail(error, "PMX 头部设置段截断");
    }
    if (count > 8 && !reader.skip(static_cast<size_t>(count) - 8)) {
        return fail(error, "PMX 头部设置段截断");
    }
    // 追加 UV 最多 4 组，索引宽度只能是 1/2/4；不在解析中途失败，避免把坏文件读成错位结构。
    if (setting.additionalUv > 4 || !isValidIndexSize(setting.vertexIndexSize) ||
        !isValidIndexSize(setting.textureIndexSize) || !isValidIndexSize(setting.materialIndexSize) ||
        !isValidIndexSize(setting.boneIndexSize) || !isValidIndexSize(setting.morphIndexSize) ||
        !isValidIndexSize(setting.rigidBodyIndexSize)) {
        return fail(error, "PMX 头部设置段取值非法");
    }
    return true;
}

bool readVertexSkinning(ByteReader &reader, const PmxSetting &setting, PmxVertexSkinning &skinning) {
    uint8_t type = 0;
    if (!reader.readU8(type)) {
        return false;
    }
    skinning.type = static_cast<PmxSkinningType>(type);
    switch (skinning.type) {
    case PmxSkinningType::BDEF1:
        return readPmxIndex(reader, setting.boneIndexSize, skinning.boneIndex[0]);
    case PmxSkinningType::BDEF2:
        if (!readPmxIndex(reader, setting.boneIndexSize, skinning.boneIndex[0]) ||
            !readPmxIndex(reader, setting.boneIndexSize, skinning.boneIndex[1])) {
            return false;
        }
        reader.readF32(skinning.boneWeight[0]);
        return reader.ok();
    case PmxSkinningType::BDEF4:
    case PmxSkinningType::QDEF:
        for (int i = 0; i < 4; ++i) {
            if (!readPmxIndex(reader, setting.boneIndexSize, skinning.boneIndex[i])) {
                return false;
            }
        }
        for (int i = 0; i < 4; ++i) {
            reader.readF32(skinning.boneWeight[i]);
        }
        return reader.ok();
    case PmxSkinningType::SDEF:
        if (!readPmxIndex(reader, setting.boneIndexSize, skinning.boneIndex[0]) ||
            !readPmxIndex(reader, setting.boneIndexSize, skinning.boneIndex[1])) {
            return false;
        }
        reader.readF32(skinning.boneWeight[0]);
        readFloat3(reader, skinning.sdefC);
        readFloat3(reader, skinning.sdefR0);
        readFloat3(reader, skinning.sdefR1);
        return reader.ok();
    default:
        return false;
    }
}

bool readVertex(ByteReader &reader, const PmxSetting &setting, PmxVertex &vertex) {
    readFloat3(reader, vertex.position);
    readFloat3(reader, vertex.normal);
    reader.readF32(vertex.uv[0]);
    reader.readF32(vertex.uv[1]);
    for (uint32_t i = 0; i < setting.additionalUv && i < 4; ++i) {
        readFloat4(reader, vertex.additionalUv[i]);
    }
    if (!readVertexSkinning(reader, setting, vertex.skinning)) {
        return false;
    }
    reader.readF32(vertex.edge);
    return reader.ok();
}

bool readMaterial(ByteReader &reader, const PmxSetting &setting, PmxMaterial &material) {
    if (!detail::readPmxString(reader, setting.encoding, material.name) ||
        !detail::readPmxString(reader, setting.encoding, material.englishName)) {
        return false;
    }
    readFloat4(reader, material.diffuse);
    readFloat3(reader, material.specular);
    reader.readF32(material.specularity);
    readFloat3(reader, material.ambient);
    reader.readU8(material.flag);
    readFloat4(reader, material.edgeColor);
    reader.readF32(material.edgeSize);
    if (!readPmxIndex(reader, setting.textureIndexSize, material.diffuseTextureIndex) ||
        !readPmxIndex(reader, setting.textureIndexSize, material.sphereTextureIndex) ||
        !reader.readU8(material.sphereOpMode) || !reader.readU8(material.commonToonFlag)) {
        return false;
    }
    if (material.commonToonFlag != 0) {
        uint8_t toonIndex = 0;
        if (!reader.readU8(toonIndex)) {
            return false;
        }
        material.toonTextureIndex = toonIndex;
    } else if (!readPmxIndex(reader, setting.textureIndexSize, material.toonTextureIndex)) {
        return false;
    }
    if (!detail::readPmxString(reader, setting.encoding, material.memo)) {
        return false;
    }
    reader.readI32(material.indexCount);
    return reader.ok();
}

bool readIkLink(ByteReader &reader, const PmxSetting &setting, PmxIkLink &link) {
    if (!readPmxIndex(reader, setting.boneIndexSize, link.linkTarget) ||
        !reader.readU8(link.angleLock)) {
        return false;
    }
    if (link.angleLock == 1) {
        readFloat3(reader, link.maxRadian);
        readFloat3(reader, link.minRadian);
    }
    return reader.ok();
}

bool readBone(ByteReader &reader, const PmxSetting &setting, PmxBone &bone) {
    if (!detail::readPmxString(reader, setting.encoding, bone.name) ||
        !detail::readPmxString(reader, setting.encoding, bone.englishName)) {
        return false;
    }
    readFloat3(reader, bone.position);
    if (!readPmxIndex(reader, setting.boneIndexSize, bone.parentIndex)) {
        return false;
    }
    reader.readI32(bone.level);
    reader.readU16(bone.flag);
    if (bone.flag & 0x0001) {
        if (!readPmxIndex(reader, setting.boneIndexSize, bone.targetIndex)) {
            return false;
        }
    } else {
        readFloat3(reader, bone.offset);
    }
    if (bone.flag & (0x0100 | 0x0200)) {
        if (!readPmxIndex(reader, setting.boneIndexSize, bone.grantParentIndex)) {
            return false;
        }
        reader.readF32(bone.grantWeight);
    }
    if (bone.flag & 0x0400) {
        readFloat3(reader, bone.lockAxisOrientation);
    }
    if (bone.flag & 0x0800) {
        readFloat3(reader, bone.localAxisXOrientation);
        readFloat3(reader, bone.localAxisYOrientation);
    }
    if (bone.flag & 0x2000) {
        reader.readI32(bone.key);
    }
    if (bone.flag & 0x0020) {
        int32_t linkCount = 0;
        if (!readPmxIndex(reader, setting.boneIndexSize, bone.ikTargetBoneIndex) ||
            !reader.readI32(bone.ikLoop) || !reader.readF32(bone.ikLoopAngleLimit) ||
            !readCount(reader, 1, linkCount, nullptr)) {
            return false;
        }
        bone.ikLinks.resize(static_cast<size_t>(linkCount));
        for (int32_t i = 0; i < linkCount; ++i) {
            if (!readIkLink(reader, setting, bone.ikLinks[static_cast<size_t>(i)])) {
                return false;
            }
        }
    }
    return reader.ok();
}

bool readFrame(ByteReader &reader, const PmxSetting &setting, PmxFrame &frame) {
    if (!detail::readPmxString(reader, setting.encoding, frame.name) ||
        !detail::readPmxString(reader, setting.encoding, frame.englishName) ||
        !reader.readU8(frame.flag)) {
        return false;
    }
    int32_t count = 0;
    if (!readCount(reader, 2, count, nullptr)) {
        return false;
    }
    frame.elements.resize(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        PmxFrameElement &element = frame.elements[static_cast<size_t>(i)];
        if (!reader.readU8(element.elementTarget)) {
            return false;
        }
        const uint8_t indexSize = element.elementTarget == 0 ? setting.boneIndexSize : setting.morphIndexSize;
        if (!readPmxIndex(reader, indexSize, element.index)) {
            return false;
        }
    }
    return reader.ok();
}

bool readRigidBody(ByteReader &reader, const PmxSetting &setting, PmxRigidBody &body) {
    if (!detail::readPmxString(reader, setting.encoding, body.name) ||
        !detail::readPmxString(reader, setting.encoding, body.englishName) ||
        !readPmxIndex(reader, setting.boneIndexSize, body.targetBone) || !reader.readU8(body.group) ||
        !reader.readU16(body.mask) || !reader.readU8(body.shape)) {
        return false;
    }
    readFloat3(reader, body.size);
    readFloat3(reader, body.position);
    readFloat3(reader, body.orientation);
    reader.readF32(body.mass);
    reader.readF32(body.moveAttenuation);
    reader.readF32(body.rotationAttenuation);
    reader.readF32(body.repulsion);
    reader.readF32(body.friction);
    reader.readU8(body.physicsCalcType);
    return reader.ok();
}

bool readJoint(ByteReader &reader, const PmxSetting &setting, PmxJoint &joint) {
    uint8_t type = 0;
    if (!detail::readPmxString(reader, setting.encoding, joint.name) ||
        !detail::readPmxString(reader, setting.encoding, joint.englishName) || !reader.readU8(type)) {
        return false;
    }
    joint.type = static_cast<PmxJointType>(type);
    PmxJointParam &param = joint.param;
    if (!readPmxIndex(reader, setting.rigidBodyIndexSize, param.rigidBody1) ||
        !readPmxIndex(reader, setting.rigidBodyIndexSize, param.rigidBody2)) {
        return false;
    }
    readFloat3(reader, param.position);
    readFloat3(reader, param.orientation);
    readFloat3(reader, param.moveLimitationMin);
    readFloat3(reader, param.moveLimitationMax);
    readFloat3(reader, param.rotationLimitationMin);
    readFloat3(reader, param.rotationLimitationMax);
    readFloat3(reader, param.springMoveCoefficient);
    readFloat3(reader, param.springRotationCoefficient);
    return reader.ok();
}

} // namespace

bool loadPmxStream(const uint8_t *data, size_t size, PmxModel &model, std::string *error) {
    model = PmxModel();
    if (!data || size == 0) {
        return fail(error, "PMX 流为空");
    }
    ByteReader reader(data, size);

    uint8_t magic[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; ++i) {
        reader.readU8(magic[i]);
    }
    if (!reader.ok() || magic[0] != 0x50 || magic[1] != 0x4D || magic[2] != 0x58 || magic[3] != 0x20) {
        return fail(error, "不是 PMX 文件（magic 不匹配）");
    }
    reader.readF32(model.version);
    if (model.version != 2.0f && model.version != 2.1f) {
        return fail(error, "不支持的 PMX 版本（仅 2.0 / 2.1）");
    }
    if (!readSetting(reader, model.setting, error)) {
        return false;
    }
    if (!detail::readPmxString(reader, model.setting.encoding, model.name) ||
        !detail::readPmxString(reader, model.setting.encoding, model.englishName) ||
        !detail::readPmxString(reader, model.setting.encoding, model.comment) ||
        !detail::readPmxString(reader, model.setting.encoding, model.englishComment)) {
        return fail(error, "PMX 模型信息段截断");
    }

    int32_t count = 0;
    if (!readCount(reader, PMX_VERTEX_MIN_BYTES, count, error)) {
        return false;
    }
    model.vertices.resize(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        if (!readVertex(reader, model.setting, model.vertices[static_cast<size_t>(i)])) {
            return fail(error, "PMX 顶点段截断或蒙皮类型未知");
        }
    }

    if (!readCount(reader, 1, count, error)) {
        return false;
    }
    model.indices.resize(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        if (!readPmxIndex(reader, model.setting.vertexIndexSize, model.indices[static_cast<size_t>(i)])) {
            return fail(error, "PMX 索引段截断");
        }
    }

    if (!readCount(reader, 4, count, error)) {
        return false;
    }
    model.textures.resize(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        if (!detail::readPmxString(reader, model.setting.encoding, model.textures[static_cast<size_t>(i)])) {
            return fail(error, "PMX 贴图段截断");
        }
    }

    if (!readCount(reader, 40, count, error)) {
        return false;
    }
    model.materials.resize(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        if (!readMaterial(reader, model.setting, model.materials[static_cast<size_t>(i)])) {
            return fail(error, "PMX 材质段截断");
        }
    }

    if (!readCount(reader, 28, count, error)) {
        return false;
    }
    model.bones.resize(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        if (!readBone(reader, model.setting, model.bones[static_cast<size_t>(i)])) {
            return fail(error, "PMX 骨骼段截断");
        }
    }

    if (!readCount(reader, 14, count, error)) {
        return false;
    }
    model.morphs.resize(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        if (!detail::parsePmxMorph(reader, model.setting, model.morphs[static_cast<size_t>(i)])) {
            return fail(error, "PMX 变形段截断或类型未知");
        }
    }

    if (!readCount(reader, 13, count, error)) {
        return false;
    }
    model.frames.resize(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        if (!readFrame(reader, model.setting, model.frames[static_cast<size_t>(i)])) {
            return fail(error, "PMX 显示枠段截断");
        }
    }

    if (!readCount(reader, 40, count, error)) {
        return false;
    }
    model.rigidBodies.resize(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        if (!readRigidBody(reader, model.setting, model.rigidBodies[static_cast<size_t>(i)])) {
            return fail(error, "PMX 刚体段截断");
        }
    }

    if (!readCount(reader, 40, count, error)) {
        return false;
    }
    model.joints.resize(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        if (!readJoint(reader, model.setting, model.joints[static_cast<size_t>(i)])) {
            return fail(error, "PMX 关节段截断");
        }
    }

    if (model.version == 2.1f) {
        if (!readCount(reader, 30, count, error)) {
            return false;
        }
        model.softBodies.resize(static_cast<size_t>(count));
        for (int32_t i = 0; i < count; ++i) {
            if (!detail::parsePmxSoftBody(reader, model.setting, model.softBodies[static_cast<size_t>(i)])) {
                return fail(error, "PMX 软体段截断");
            }
        }
    }
    return true;
}

bool loadPmxFile(const std::string &path, PmxModel &model, std::string *error) {
    std::vector<uint8_t> bytes;
    if (!readFileBytes(path, bytes, error)) {
        return false;
    }
    return loadPmxStream(bytes.empty() ? nullptr : &bytes[0], bytes.size(), model, error);
}

} // namespace mmd
