// PMX 2.1 软体（SoftBody）解析：参考实现只 panic 未实现，此处按 PMX 规范补全。
#include "pmxParse.h"

namespace mmd {
namespace detail {

namespace {

// 软体参数里大量 0..1 的系数（VCF/DP/DG/... ），字段名沿用规范缩写，读取顺序即规范顺序。
bool readSoftBodyTuning(ByteReader &reader, PmxSoftBody &body) {
    reader.readF32(body.mass);
    reader.readF32(body.collisionMargin);
    reader.readI32(body.aeroModel);
    reader.readF32(body.vcf);
    reader.readF32(body.dp);
    reader.readF32(body.dg);
    reader.readF32(body.lf);
    reader.readF32(body.pr);
    reader.readF32(body.vc);
    reader.readF32(body.df);
    reader.readF32(body.mt);
    reader.readF32(body.chr);
    reader.readF32(body.khr);
    reader.readF32(body.shr);
    reader.readF32(body.ahr);
    reader.readF32(body.srhRcl);
    reader.readF32(body.skhRcl);
    reader.readF32(body.sshRcl);
    reader.readF32(body.srsPltCl);
    reader.readF32(body.sksPltCl);
    reader.readF32(body.sssPltCl);
    reader.readI32(body.vit);
    reader.readI32(body.pit);
    reader.readI32(body.dit);
    reader.readI32(body.cit);
    reader.readF32(body.lst);
    reader.readF32(body.ast);
    reader.readF32(body.vst);
    return reader.ok();
}

bool readSoftBodyAnchors(ByteReader &reader, const PmxSetting &setting, PmxSoftBody &body) {
    int32_t count = 0;
    if (!reader.readI32(count) || count < 0 || static_cast<size_t>(count) > reader.remaining()) {
        return false;
    }
    body.anchors.resize(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        PmxAnchorRigidBody &anchor = body.anchors[static_cast<size_t>(i)];
        if (!readPmxIndex(reader, setting.rigidBodyIndexSize, anchor.relatedRigidBody) ||
            !readPmxIndex(reader, setting.vertexIndexSize, anchor.relatedVertex) ||
            !reader.readBool(anchor.isNear)) {
            return false;
        }
    }
    return true;
}

bool readSoftBodyPinVertices(ByteReader &reader, const PmxSetting &setting, PmxSoftBody &body) {
    int32_t count = 0;
    if (!reader.readI32(count) || count < 0 || static_cast<size_t>(count) > reader.remaining()) {
        return false;
    }
    body.pinVertices.resize(static_cast<size_t>(count));
    for (int32_t i = 0; i < count; ++i) {
        if (!readPmxIndex(reader, setting.vertexIndexSize, body.pinVertices[static_cast<size_t>(i)])) {
            return false;
        }
    }
    return true;
}

} // namespace

bool parsePmxSoftBody(ByteReader &reader, const PmxSetting &setting, PmxSoftBody &body) {
    if (!readPmxString(reader, setting.encoding, body.name) ||
        !readPmxString(reader, setting.encoding, body.englishName) || !reader.readU8(body.shape) ||
        !readPmxIndex(reader, setting.materialIndexSize, body.targetMaterial) ||
        !reader.readU8(body.group) || !reader.readU16(body.mask) || !reader.readU8(body.flag) ||
        !reader.readI32(body.blinkDistance) || !reader.readI32(body.clusterCount)) {
        return false;
    }
    if (!readSoftBodyTuning(reader, body)) {
        return false;
    }
    return readSoftBodyAnchors(reader, setting, body) && readSoftBodyPinVertices(reader, setting, body);
}

} // namespace detail
} // namespace mmd
