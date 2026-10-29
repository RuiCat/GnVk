// PMX 变形（Morph）解析：包含参考实现未处理的 Flip(9) / Impulse(10) 类型。
#include "pmxParse.h"

namespace mmd {
namespace detail {

namespace {

// 读取偏移量条数并做基本截断校验（每个偏移量至少占 1 字节）。
bool readOffsetCount(ByteReader &reader, int32_t &count) {
    if (!reader.readI32(count) || count < 0) {
        return false;
    }
    return static_cast<size_t>(count) <= reader.remaining();
}

bool readMorphHeader(ByteReader &reader, const PmxSetting &setting, PmxMorph &morph) {
    uint8_t category = 0;
    uint8_t type = 0;
    if (!readPmxString(reader, setting.encoding, morph.name) ||
        !readPmxString(reader, setting.encoding, morph.englishName) || !reader.readU8(category) ||
        !reader.readU8(type)) {
        return false;
    }
    morph.category = static_cast<PmxMorphCategory>(category);
    morph.type = static_cast<PmxMorphType>(type);
    return true;
}

bool readGroupOffsets(ByteReader &reader, const PmxSetting &setting, int32_t count, PmxMorph &morph) {
    morph.groupOffsets.resize(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        PmxMorphGroupOffset &offset = morph.groupOffsets[static_cast<size_t>(i)];
        if (!readPmxIndex(reader, setting.morphIndexSize, offset.morphIndex)) {
            return false;
        }
        reader.readF32(offset.morphWeight);
    }
    return reader.ok();
}

bool readVertexOffsets(ByteReader &reader, const PmxSetting &setting, int32_t count, PmxMorph &morph) {
    morph.vertexOffsets.resize(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        PmxMorphVertexOffset &offset = morph.vertexOffsets[static_cast<size_t>(i)];
        if (!readPmxIndex(reader, setting.vertexIndexSize, offset.vertexIndex)) {
            return false;
        }
        readFloat3(reader, offset.positionOffset);
    }
    return reader.ok();
}

bool readBoneOffsets(ByteReader &reader, const PmxSetting &setting, int32_t count, PmxMorph &morph) {
    morph.boneOffsets.resize(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        PmxMorphBoneOffset &offset = morph.boneOffsets[static_cast<size_t>(i)];
        if (!readPmxIndex(reader, setting.boneIndexSize, offset.boneIndex)) {
            return false;
        }
        readFloat3(reader, offset.translation);
        readFloat4(reader, offset.rotation);
    }
    return reader.ok();
}

bool readMaterialOffsets(ByteReader &reader, const PmxSetting &setting, int32_t count, PmxMorph &morph) {
    morph.materialOffsets.resize(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        PmxMorphMaterialOffset &offset = morph.materialOffsets[static_cast<size_t>(i)];
        // PMX 规范：材质索引使用 materialIndexSize（参考实现误用了 morphIndexSize）。
        if (!readPmxIndex(reader, setting.materialIndexSize, offset.materialIndex) ||
            !reader.readU8(offset.offsetOperation)) {
            return false;
        }
        readFloat4(reader, offset.diffuse);
        readFloat3(reader, offset.specular);
        reader.readF32(offset.specularity);
        readFloat3(reader, offset.ambient);
        readFloat4(reader, offset.edgeColor);
        reader.readF32(offset.edgeSize);
        readFloat4(reader, offset.textureArgb);
        readFloat4(reader, offset.sphereTextureArgb);
        readFloat4(reader, offset.toonTextureArgb);
    }
    return reader.ok();
}

bool readUvOffsets(ByteReader &reader, const PmxSetting &setting, int32_t count, PmxMorph &morph) {
    morph.uvOffsets.resize(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        PmxMorphUvOffset &offset = morph.uvOffsets[static_cast<size_t>(i)];
        if (!readPmxIndex(reader, setting.vertexIndexSize, offset.vertexIndex)) {
            return false;
        }
        readFloat4(reader, offset.uvOffset);
    }
    return reader.ok();
}

bool readFlipOffsets(ByteReader &reader, const PmxSetting &setting, int32_t count, PmxMorph &morph) {
    morph.flipOffsets.resize(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        PmxMorphFlipOffset &offset = morph.flipOffsets[static_cast<size_t>(i)];
        if (!readPmxIndex(reader, setting.morphIndexSize, offset.morphIndex)) {
            return false;
        }
        reader.readF32(offset.morphValue);
    }
    return reader.ok();
}

bool readImpulseOffsets(ByteReader &reader, const PmxSetting &setting, int32_t count, PmxMorph &morph) {
    morph.impulseOffsets.resize(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        PmxMorphImpulseOffset &offset = morph.impulseOffsets[static_cast<size_t>(i)];
        // PMX 规范：刚体索引使用 rigidBodyIndexSize（参考实现误用了 morphIndexSize）。
        if (!readPmxIndex(reader, setting.rigidBodyIndexSize, offset.rigidBodyIndex) ||
            !reader.readU8(offset.isLocal)) {
            return false;
        }
        readFloat3(reader, offset.velocity);
        readFloat3(reader, offset.angularTorque);
    }
    return reader.ok();
}

} // namespace

bool parsePmxMorph(ByteReader &reader, const PmxSetting &setting, PmxMorph &morph) {
    if (!readMorphHeader(reader, setting, morph)) {
        return false;
    }
    int32_t count = 0;
    if (!readOffsetCount(reader, count)) {
        return false;
    }
    switch (morph.type) {
    case PmxMorphType::GROUP:
        return readGroupOffsets(reader, setting, count, morph);
    case PmxMorphType::VERTEX:
        return readVertexOffsets(reader, setting, count, morph);
    case PmxMorphType::BONE:
        return readBoneOffsets(reader, setting, count, morph);
    case PmxMorphType::UV:
    case PmxMorphType::ADDITIONAL_UV1:
    case PmxMorphType::ADDITIONAL_UV2:
    case PmxMorphType::ADDITIONAL_UV3:
    case PmxMorphType::ADDITIONAL_UV4:
        return readUvOffsets(reader, setting, count, morph);
    case PmxMorphType::MATERIAL:
        return readMaterialOffsets(reader, setting, count, morph);
    case PmxMorphType::FLIP:
        return readFlipOffsets(reader, setting, count, morph);
    case PmxMorphType::IMPULSE:
        return readImpulseOffsets(reader, setting, count, morph);
    default:
        return false;
    }
}

} // namespace detail
} // namespace mmd
