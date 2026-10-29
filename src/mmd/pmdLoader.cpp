// PMD 加载器实现：严格按参考实现 Go3D/lib/mmd/pmd 的段顺序读取，所有读取带边界检查。
#include "pmdLoader.h"

#include "mmdByteReader.h"
#include "mmdEncoding.h"
#include "mmdFile.h"

#include <vector>

namespace mmd {

namespace {

const uint32_t PMD_HEADER_NAME_BYTES = 20;
const uint32_t PMD_HEADER_COMMENT_BYTES = 256;
const uint32_t PMD_TOON_NAME_BYTES = 100;
const uint32_t PMD_TOON_COUNT = 10;
const size_t PMD_VERTEX_BYTES = 38;
const size_t PMD_MATERIAL_BYTES = 70;
const size_t PMD_BONE_BYTES = 39;
const size_t PMD_IK_BYTES = 11;
const size_t PMD_FACE_HEADER_BYTES = 25;
const size_t PMD_FACE_VERTEX_BYTES = 16;
const size_t PMD_RIGID_BODY_BYTES = 83;
const size_t PMD_CONSTRAINT_BYTES = 100;

bool fail(std::string *error, const char *message) {
    if (error) {
        *error = message;
    }
    return false;
}

// 把定长字段截到首个 NUL 并解码 Shift-JIS。
std::string decodeFixedText(const uint8_t *data, size_t size) {
    size_t length = 0;
    while (length < size && data[length] != 0) {
        ++length;
    }
    return shiftJisToUtf8(data, length);
}

bool readFixedText(ByteReader &reader, size_t size, std::string &out) {
    std::vector<uint8_t> raw;
    if (!reader.readBytes(size, raw)) {
        return false;
    }
    out = decodeFixedText(raw.data(), raw.size());
    return true;
}

bool readCount32(ByteReader &reader, size_t minBytes, uint32_t &count, std::string *error) {
    int32_t value = 0;
    if (!reader.readI32(value) || value < 0) {
        return fail(error, "PMD 计数无效或数据截断");
    }
    count = static_cast<uint32_t>(value);
    if (minBytes > 0 && static_cast<size_t>(count) > reader.remaining() / minBytes) {
        return fail(error, "PMD 数据截断");
    }
    return true;
}

bool readCount16(ByteReader &reader, size_t minBytes, uint32_t &count, std::string *error) {
    uint16_t value = 0;
    if (!reader.readU16(value)) {
        return fail(error, "PMD 数据截断");
    }
    count = value;
    if (minBytes > 0 && static_cast<size_t>(count) > reader.remaining() / minBytes) {
        return fail(error, "PMD 数据截断");
    }
    return true;
}

bool readCount8(ByteReader &reader, size_t minBytes, uint32_t &count, std::string *error) {
    uint8_t value = 0;
    if (!reader.readU8(value)) {
        return fail(error, "PMD 数据截断");
    }
    count = value;
    if (minBytes > 0 && static_cast<size_t>(count) > reader.remaining() / minBytes) {
        return fail(error, "PMD 数据截断");
    }
    return true;
}

void readFloat3(ByteReader &reader, float out[3]) {
    reader.readF32(out[0]);
    reader.readF32(out[1]);
    reader.readF32(out[2]);
}

void readFloat4(ByteReader &reader, float out[4]) {
    reader.readF32(out[0]);
    reader.readF32(out[1]);
    reader.readF32(out[2]);
    reader.readF32(out[3]);
}

bool readVertex(ByteReader &reader, PmdVertex &vertex) {
    readFloat3(reader, vertex.position);
    readFloat3(reader, vertex.normal);
    reader.readF32(vertex.uv[0]);
    reader.readF32(vertex.uv[1]);
    reader.readU16(vertex.boneIndex[0]);
    reader.readU16(vertex.boneIndex[1]);
    reader.readU8(vertex.boneWeight);
    reader.readBool(vertex.edgeInvisible);
    return reader.ok();
}

bool readMaterial(ByteReader &reader, PmdMaterial &material) {
    readFloat4(reader, material.diffuse);
    reader.readF32(material.power);
    readFloat3(reader, material.specular);
    readFloat3(reader, material.ambient);
    reader.readU8(material.toonIndex);
    reader.readU8(material.edgeFlag);
    reader.readU32(material.indexCount);
    std::vector<uint8_t> raw;
    if (!reader.readBytes(PMD_HEADER_NAME_BYTES, raw)) {
        return false;
    }
    // 纹理与球面贴图共用一个 20 字节字段，以 '*' 分隔（参考实现把 '*' 留在球面名里，此处按规范去掉）。
    size_t star = raw.size();
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '*') {
            star = i;
            break;
        }
    }
    if (star == raw.size() || star == 0) {
        material.textureFilename = decodeFixedText(raw.data(), raw.size());
        material.sphereFilename.clear();
    } else {
        material.textureFilename = decodeFixedText(raw.data(), star);
        material.sphereFilename = decodeFixedText(raw.data() + star + 1, raw.size() - star - 1);
    }
    return reader.ok();
}

bool readBone(ByteReader &reader, PmdBone &bone) {
    if (!readFixedText(reader, PMD_HEADER_NAME_BYTES, bone.name)) {
        return false;
    }
    uint8_t boneType = 0;
    reader.readU16(bone.parentBoneIndex);
    reader.readU16(bone.tailPosBoneIndex);
    reader.readU8(boneType);
    reader.readU16(bone.ikParentBoneIndex);
    readFloat3(reader, bone.boneHeadPos);
    bone.boneType = static_cast<PmdBoneType>(boneType);
    return reader.ok();
}

bool readIk(ByteReader &reader, PmdIk &ik) {
    uint8_t childCount = 0;
    reader.readU16(ik.ikBoneIndex);
    reader.readU16(ik.targetBoneIndex);
    reader.readU8(childCount);
    reader.readU16(ik.iterations);
    reader.readF32(ik.angleLimit);
    ik.ikChildBoneIndex.assign(childCount, 0);
    for (uint32_t i = 0; i < childCount; ++i) {
        reader.readU16(ik.ikChildBoneIndex[i]);
    }
    return reader.ok();
}

bool readFace(ByteReader &reader, PmdFace &face) {
    uint32_t count = 0;
    uint8_t category = 0;
    if (!readFixedText(reader, PMD_HEADER_NAME_BYTES, face.name)) {
        return false;
    }
    if (!readCount32(reader, PMD_FACE_VERTEX_BYTES, count, nullptr)) {
        return false;
    }
    reader.readU8(category);
    face.type = static_cast<PmdFaceCategory>(category);
    face.vertices.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        reader.readI32(face.vertices[i].vertexIndex);
        readFloat3(reader, face.vertices[i].position);
    }
    return reader.ok();
}

bool readRigidBody(ByteReader &reader, PmdRigidBody &body) {
    uint8_t shape = 0;
    uint8_t rigidType = 0;
    if (!readFixedText(reader, PMD_HEADER_NAME_BYTES, body.name)) {
        return false;
    }
    reader.readU16(body.relatedBoneIndex);
    reader.readU8(body.groupIndex);
    reader.readU16(body.mask);
    reader.readU8(shape);
    readFloat3(reader, body.size);
    readFloat3(reader, body.position);
    readFloat3(reader, body.orientation);
    reader.readF32(body.weight);
    reader.readF32(body.linearDamping);
    reader.readF32(body.angularDamping);
    reader.readF32(body.restitution);
    reader.readF32(body.friction);
    reader.readU8(rigidType);
    body.shape = static_cast<PmdRigidBodyShape>(shape);
    body.rigidType = static_cast<PmdRigidBodyType>(rigidType);
    return reader.ok();
}

bool readConstraint(ByteReader &reader, PmdConstraint &constraint) {
    if (!readFixedText(reader, PMD_HEADER_NAME_BYTES, constraint.name)) {
        return false;
    }
    reader.readU32(constraint.rigidBodyIndexA);
    reader.readU32(constraint.rigidBodyIndexB);
    readFloat3(reader, constraint.position);
    readFloat3(reader, constraint.orientation);
    readFloat3(reader, constraint.linearLowerLimit);
    readFloat3(reader, constraint.linearUpperLimit);
    readFloat3(reader, constraint.angularLowerLimit);
    readFloat3(reader, constraint.angularUpperLimit);
    readFloat3(reader, constraint.linearStiffness);
    readFloat3(reader, constraint.angularStiffness);
    return reader.ok();
}

bool readToonNames(ByteReader &reader, PmdModel &model) {
    model.toonFilenames.resize(PMD_TOON_COUNT);
    for (uint32_t i = 0; i < PMD_TOON_COUNT; ++i) {
        if (!readFixedText(reader, PMD_TOON_NAME_BYTES, model.toonFilenames[i])) {
            return false;
        }
    }
    return true;
}

bool readEnglishExtension(ByteReader &reader, PmdModel &model) {
    if (!readFixedText(reader, PMD_HEADER_NAME_BYTES, model.header.nameEnglish) ||
        !readFixedText(reader, PMD_HEADER_COMMENT_BYTES, model.header.commentEnglish)) {
        return false;
    }
    for (size_t i = 0; i < model.bones.size(); ++i) {
        if (!readFixedText(reader, PMD_HEADER_NAME_BYTES, model.bones[i].nameEnglish)) {
            return false;
        }
    }
    for (size_t i = 0; i < model.faces.size(); ++i) {
        // 参考实现跳过 Base 类型表情的英文名。
        if (model.faces[i].type == PmdFaceCategory::BASE) {
            continue;
        }
        if (!readFixedText(reader, PMD_HEADER_NAME_BYTES, model.faces[i].nameEnglish)) {
            return false;
        }
    }
    for (size_t i = 0; i < model.boneDispNames.size(); ++i) {
        if (!readFixedText(reader, 50, model.boneDispNames[i].nameEnglish)) {
            return false;
        }
    }
    return true;
}

} // namespace

bool loadPmdStream(const uint8_t *data, size_t size, PmdModel &model, std::string *error) {
    model = PmdModel();
    if (!data || size == 0) {
        return fail(error, "PMD 流为空");
    }
    ByteReader reader(data, size);

    uint8_t magic[3] = {0, 0, 0};
    reader.readU8(magic[0]);
    reader.readU8(magic[1]);
    reader.readU8(magic[2]);
    if (!reader.ok() || magic[0] != 'P' || magic[1] != 'm' || magic[2] != 'd') {
        return fail(error, "不是 PMD 文件（magic 不匹配）");
    }
    reader.readF32(model.version);
    if (model.version != 1.0f) {
        return fail(error, "不支持的 PMD 版本");
    }
    if (!readFixedText(reader, PMD_HEADER_NAME_BYTES, model.header.name) ||
        !readFixedText(reader, PMD_HEADER_COMMENT_BYTES, model.header.comment)) {
        return fail(error, "PMD 头部截断");
    }

    uint32_t count = 0;
    if (!readCount32(reader, PMD_VERTEX_BYTES, count, error)) {
        return false;
    }
    model.vertices.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (!readVertex(reader, model.vertices[i])) {
            return fail(error, "PMD 顶点段截断");
        }
    }

    if (!readCount32(reader, 2, count, error)) {
        return false;
    }
    model.indices.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        reader.readU16(model.indices[i]);
    }
    if (!reader.ok()) {
        return fail(error, "PMD 索引段截断");
    }

    if (!readCount32(reader, PMD_MATERIAL_BYTES, count, error)) {
        return false;
    }
    model.materials.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (!readMaterial(reader, model.materials[i])) {
            return fail(error, "PMD 材质段截断");
        }
    }

    if (!readCount16(reader, PMD_BONE_BYTES, count, error)) {
        return false;
    }
    model.bones.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (!readBone(reader, model.bones[i])) {
            return fail(error, "PMD 骨骼段截断");
        }
    }

    if (!readCount16(reader, PMD_IK_BYTES, count, error)) {
        return false;
    }
    model.iks.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (!readIk(reader, model.iks[i])) {
            return fail(error, "PMD IK 段截断");
        }
    }

    if (!readCount16(reader, PMD_FACE_HEADER_BYTES, count, error)) {
        return false;
    }
    model.faces.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (!readFace(reader, model.faces[i])) {
            return fail(error, "PMD 表情段截断");
        }
    }

    if (!readCount8(reader, 2, count, error)) {
        return false;
    }
    model.faceIndices.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        reader.readU16(model.faceIndices[i]);
    }
    if (!reader.ok()) {
        return fail(error, "PMD 表情索引段截断");
    }

    if (!readCount8(reader, 50, count, error)) {
        return false;
    }
    model.boneDispNames.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (!readFixedText(reader, 50, model.boneDispNames[i].name)) {
            return fail(error, "PMD 骨骼显示名段截断");
        }
    }

    if (!readCount32(reader, 3, count, error)) {
        return false;
    }
    model.boneDisps.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        reader.readU16(model.boneDisps[i].boneIndex);
        reader.readU8(model.boneDisps[i].boneDispIndex);
    }
    if (!reader.ok()) {
        return fail(error, "PMD 骨骼显示段截断");
    }

    bool hasEnglish = false;
    if (!reader.readBool(hasEnglish)) {
        return fail(error, "PMD 英文名标志截断");
    }
    if (hasEnglish && !readEnglishExtension(reader, model)) {
        return fail(error, "PMD 英文名段截断");
    }

    // 参考实现的行为：没有后续段就正常结束；有则必须足够长。
    if (reader.remaining() == 0) {
        return true;
    }
    if (!readToonNames(reader, model)) {
        return fail(error, "PMD 卡通贴图段截断");
    }
    if (reader.remaining() == 0) {
        return true;
    }

    if (!readCount32(reader, PMD_RIGID_BODY_BYTES, count, error)) {
        return false;
    }
    model.rigidBodies.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (!readRigidBody(reader, model.rigidBodies[i])) {
            return fail(error, "PMD 刚体段截断");
        }
    }

    if (!readCount32(reader, PMD_CONSTRAINT_BYTES, count, error)) {
        return false;
    }
    model.constraints.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (!readConstraint(reader, model.constraints[i])) {
            return fail(error, "PMD 关节段截断");
        }
    }

    if (reader.remaining() != 0) {
        return fail(error, "PMD 文件尾部有未知数据");
    }
    return true;
}

bool loadPmdFile(const std::string &path, PmdModel &model, std::string *error) {
    std::vector<uint8_t> bytes;
    if (!readFileBytes(path, bytes, error)) {
        return false;
    }
    return loadPmdStream(bytes.empty() ? nullptr : &bytes[0], bytes.size(), model, error);
}

} // namespace mmd
