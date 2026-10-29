// testMmdPmx.cpp — PMX 2.0/2.1 解析：UTF-8 与 UTF-16 字符串、全部蒙皮类型、全部变形类型、
// 软体、截断输入安全、真实样本结构不变量。
#include "testFramework.h"
#include "testMmdValidate.h"
#include "testMmdWriter.h"

#include "mmd/pmxLoader.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using mmdTest::ByteWriter;

void writeBone(ByteWriter &writer, const std::string &name, uint16_t flag) {
    writer.prefixedText(name);
    writer.prefixedText(name + "En");
    writer.f3(0.0f, 1.0f, 0.0f);
    writer.u16(0xFFFF); // parentIndex = -1
    writer.i32(0);
    writer.u16(flag);
    if ((flag & 0x0001) == 0) {
        writer.f3(0.0f, 0.0f, 0.0f); // offset：无接续目标时写偏移
    }
}

// 2.0 / UTF-8：覆盖 BDEF1/2/4、SDEF/QDEF、IK、7 种变形、显示枠、刚体与关节。
std::vector<uint8_t> buildSamplePmx20() {
    ByteWriter writer;
    writer.text("PMX ");
    writer.f32(2.0f);
    writer.u8(8); // setting 字节数
    writer.u8(1); // encoding = UTF-8
    writer.u8(1); // additionalUv = 1
    writer.u8(2); // vertexIndexSize
    writer.u8(1); // textureIndexSize
    writer.u8(2); // materialIndexSize
    writer.u8(2); // boneIndexSize
    writer.u8(1); // morphIndexSize
    writer.u8(1); // rigidBodyIndexSize
    writer.prefixedText("model");
    writer.prefixedText("modelEn");
    writer.prefixedText("comment");
    writer.prefixedText("commentEn");

    writer.i32(5);
    for (int i = 0; i < 5; ++i) {
        writer.f3(static_cast<float>(i), 2.0f, 3.0f);
        writer.f3(0.0f, 1.0f, 0.0f);
        writer.f32(0.5f);
        writer.f32(0.25f);
        writer.f4(0.1f, 0.2f, 0.3f, 0.4f); // 追加 UV 1 组
        writer.u8(static_cast<uint32_t>(i));
        if (i == 0) {
            writer.u16(0);
        } else if (i == 1) {
            writer.u16(0);
            writer.u16(1);
            writer.f32(0.25f);
        } else if (i == 2) {
            for (int j = 0; j < 4; ++j) {
                writer.u16(static_cast<uint32_t>(j));
            }
            writer.f4(0.4f, 0.3f, 0.2f, 0.1f);
        } else if (i == 3) {
            writer.u16(0);
            writer.u16(1);
            writer.f32(0.5f);
            writer.f3(1.0f, 2.0f, 3.0f);
            writer.f3(4.0f, 5.0f, 6.0f);
            writer.f3(7.0f, 8.0f, 9.0f);
        } else {
            for (int j = 0; j < 4; ++j) {
                writer.u16(static_cast<uint32_t>(j));
            }
            writer.f4(0.4f, 0.3f, 0.2f, 0.1f);
        }
        writer.f32(1.0f); // edge
    }

    writer.i32(3);
    writer.u16(0);
    writer.u16(1);
    writer.u16(2);

    writer.i32(2);
    writer.prefixedText("tex/a.png");
    writer.prefixedText("sph.spa");

    writer.i32(1);
    writer.prefixedText("mat");
    writer.prefixedText("matEn");
    writer.f4(1.0f, 0.8f, 0.6f, 1.0f);
    writer.f3(1.0f, 1.0f, 1.0f);
    writer.f32(5.0f);
    writer.f3(0.1f, 0.2f, 0.3f);
    writer.u8(15);
    writer.f4(0.0f, 0.0f, 0.0f, 1.0f);
    writer.f32(1.0f);
    writer.u8(0); // diffuseTextureIndex
    writer.u8(1); // sphereTextureIndex
    writer.u8(2); // sphereOpMode
    writer.u8(0); // commonToonFlag
    writer.u8(0); // toonTextureIndex
    writer.prefixedText("memo");
    writer.i32(3);

    writer.i32(5);
    writeBone(writer, "bone0", 0x0001);
    writer.u16(1); // targetIndex
    writeBone(writer, "bone1", 0x0300);
    writer.u16(0); // grantParentIndex
    writer.f32(0.5f); // grantWeight
    writeBone(writer, "bone2", 0x0400);
    writer.f3(1.0f, 0.0f, 0.0f);
    writeBone(writer, "bone3", 0x0800);
    writer.f3(1.0f, 0.0f, 0.0f);
    writer.f3(0.0f, 1.0f, 0.0f);
    writeBone(writer, "bone4", 0x0020);
    writer.u16(0); // ikTargetBoneIndex
    writer.i32(10); // ikLoop
    writer.f32(0.1f); // ikLoopAngleLimit
    writer.i32(1); // ikLinkCount
    writer.u16(0); // linkTarget
    writer.u8(1); // angleLock
    writer.f3(0.5f, 0.5f, 0.5f);
    writer.f3(-0.5f, -0.5f, -0.5f);

    writer.i32(7);
    writer.prefixedText("group");
    writer.prefixedText("groupEn");
    writer.u8(4);
    writer.u8(0);
    writer.i32(1);
    writer.u8(0); // morphIndex
    writer.f32(1.0f);

    writer.prefixedText("vertex");
    writer.prefixedText("vertexEn");
    writer.u8(4);
    writer.u8(1);
    writer.i32(1);
    writer.u16(0); // vertexIndex
    writer.f3(0.1f, 0.2f, 0.3f);

    writer.prefixedText("bone");
    writer.prefixedText("boneEn");
    writer.u8(4);
    writer.u8(2);
    writer.i32(1);
    writer.u16(0);
    writer.f3(1.0f, 2.0f, 3.0f);
    writer.f4(0.0f, 0.0f, 0.0f, 1.0f);

    writer.prefixedText("uv");
    writer.prefixedText("uvEn");
    writer.u8(4);
    writer.u8(3);
    writer.i32(1);
    writer.u16(0);
    writer.f4(0.1f, 0.2f, 0.3f, 0.4f);

    writer.prefixedText("material");
    writer.prefixedText("materialEn");
    writer.u8(4);
    writer.u8(8);
    writer.i32(1);
    writer.u16(0); // materialIndex
    writer.u8(1); // offsetOperation
    writer.f4(0.1f, 0.2f, 0.3f, 0.4f);
    writer.f3(1.0f, 1.0f, 1.0f);
    writer.f32(1.0f);
    writer.f3(0.1f, 0.1f, 0.1f);
    writer.f4(0.0f, 0.0f, 0.0f, 1.0f);
    writer.f32(1.0f);
    writer.f4(1.0f, 1.0f, 1.0f, 1.0f);
    writer.f4(1.0f, 1.0f, 1.0f, 1.0f);
    writer.f4(1.0f, 1.0f, 1.0f, 1.0f);

    writer.prefixedText("flip");
    writer.prefixedText("flipEn");
    writer.u8(4);
    writer.u8(9);
    writer.i32(1);
    writer.u8(0);
    writer.f32(1.0f);

    writer.prefixedText("impulse");
    writer.prefixedText("impulseEn");
    writer.u8(4);
    writer.u8(10);
    writer.i32(1);
    writer.u8(0); // rigidBodyIndex
    writer.u8(1); // isLocal
    writer.f3(1.0f, 2.0f, 3.0f);
    writer.f3(4.0f, 5.0f, 6.0f);

    writer.i32(2);
    writer.prefixedText("frame0");
    writer.prefixedText("frame0En");
    writer.u8(0);
    writer.i32(2);
    writer.u8(0); // bone
    writer.u16(0);
    writer.u8(1); // morph
    writer.u8(0);
    writer.prefixedText("frame1");
    writer.prefixedText("frame1En");
    writer.u8(1);
    writer.i32(0);

    writer.i32(1);
    writer.prefixedText("rigid");
    writer.prefixedText("rigidEn");
    writer.u16(0);
    writer.u8(0);
    writer.u16(0xFFFF);
    writer.u8(0);
    writer.f3(1.0f, 2.0f, 3.0f);
    writer.f3(0.0f, 1.0f, 0.0f);
    writer.f3(0.0f, 0.0f, 0.0f);
    writer.f32(1.0f);
    writer.f32(0.5f);
    writer.f32(0.5f);
    writer.f32(0.1f);
    writer.f32(0.9f);
    writer.u8(1);

    writer.i32(1);
    writer.prefixedText("joint");
    writer.prefixedText("jointEn");
    writer.u8(0);
    writer.u8(0);
    writer.u8(0);
    writer.f3(0.0f, 0.0f, 0.0f);
    writer.f3(0.0f, 0.0f, 0.0f);
    writer.f3(-1.0f, -1.0f, -1.0f);
    writer.f3(1.0f, 1.0f, 1.0f);
    writer.f3(-0.5f, -0.5f, -0.5f);
    writer.f3(0.5f, 0.5f, 0.5f);
    writer.f3(1.0f, 1.0f, 1.0f);
    writer.f3(2.0f, 2.0f, 2.0f);
    return writer.data();
}

// 2.1 / UTF-16LE：最小模型 + 一个软体。
std::vector<uint8_t> buildSamplePmx21() {
    ByteWriter writer;
    writer.text("PMX ");
    writer.f32(2.1f);
    writer.u8(8);
    writer.u8(0); // UTF-16LE
    writer.u8(0);
    writer.u8(2);
    writer.u8(2);
    writer.u8(2);
    writer.u8(2);
    writer.u8(2);
    writer.u8(2);
    writer.utf16Text(u"A\x3042"); // "Aあ"
    writer.utf16Text(u"modelEn");
    writer.utf16Text(u"comment");
    writer.utf16Text(u"commentEn");

    writer.i32(1);
    writer.f3(0.0f, 1.0f, 0.0f);
    writer.f3(0.0f, 1.0f, 0.0f);
    writer.f32(0.5f);
    writer.f32(0.5f);
    writer.u8(0); // BDEF1
    writer.u16(0);
    writer.f32(1.0f);

    writer.i32(3);
    writer.u16(0);
    writer.u16(0);
    writer.u16(0);

    writer.i32(1);
    writer.utf16Text(u"tex.png");

    writer.i32(1);
    writer.utf16Text(u"mat");
    writer.utf16Text(u"matEn");
    writer.f4(1.0f, 1.0f, 1.0f, 1.0f);
    writer.f3(1.0f, 1.0f, 1.0f);
    writer.f32(5.0f);
    writer.f3(0.1f, 0.2f, 0.3f);
    writer.u8(15);
    writer.f4(0.0f, 0.0f, 0.0f, 1.0f);
    writer.f32(1.0f);
    writer.u16(0xFFFF); // diffuseTextureIndex
    writer.u16(0xFFFF); // sphereTextureIndex
    writer.u8(0);
    writer.u8(1); // commonToonFlag
    writer.u8(5); // 共用卡通贴图编号
    writer.utf16Text(u"memo");
    writer.i32(3);

    writer.i32(1);
    writer.utf16Text(u"bone");
    writer.utf16Text(u"boneEn");
    writer.f3(0.0f, 1.0f, 0.0f);
    writer.u16(0xFFFF);
    writer.i32(0);
    writer.u16(0); // 无任何可选段
    writer.f3(0.0f, 0.0f, 0.0f); // offset

    writer.i32(0); // morphs
    writer.i32(0); // frames

    writer.i32(1); // rigid body
    writer.utf16Text(u"rigid");
    writer.utf16Text(u"rigidEn");
    writer.u16(0);
    writer.u8(0);
    writer.u16(0xFFFF);
    writer.u8(0);
    writer.f3(1.0f, 1.0f, 1.0f);
    writer.f3(0.0f, 1.0f, 0.0f);
    writer.f3(0.0f, 0.0f, 0.0f);
    writer.f32(1.0f);
    writer.f32(0.1f);
    writer.f32(0.1f);
    writer.f32(0.1f);
    writer.f32(0.9f);
    writer.u8(1);

    writer.i32(0); // joints

    writer.i32(1); // soft bodies
    writer.utf16Text(u"soft");
    writer.utf16Text(u"softEn");
    writer.u8(0); // shape
    writer.u16(0); // targetMaterial
    writer.u8(1); // group
    writer.u16(0x0003); // mask
    writer.u8(1); // flag
    writer.i32(10); // blinkDistance
    writer.i32(2); // clusterCount
    writer.f32(1.5f); // mass
    writer.f32(0.01f); // collisionMargin
    writer.i32(3); // aeroModel
    for (int i = 0; i < 12; ++i) {
        writer.f32(static_cast<float>(i) + 0.5f);
    }
    for (int i = 0; i < 6; ++i) {
        writer.f32(static_cast<float>(i) + 0.25f);
    }
    writer.i32(1);
    writer.i32(2);
    writer.i32(3);
    writer.i32(4);
    writer.f32(0.1f);
    writer.f32(0.2f);
    writer.f32(0.3f);
    writer.i32(1); // anchorCount
    writer.u16(0); // relatedRigidBody
    writer.u16(0); // relatedVertex
    writer.u8(1); // isNear
    writer.i32(1); // pinVertexCount
    writer.u16(0); // pin vertex
    return writer.data();
}

void checkPmx20(const mmd::PmxModel &model) {
    CHECK_NEAR(model.version, 2.0f, 1e-6);
    CHECK_EQ(model.setting.encoding, 1);
    CHECK_EQ(model.setting.additionalUv, 1);
    CHECK_STREQ(model.name.c_str(), "model");
    CHECK_STREQ(model.englishComment.c_str(), "commentEn");
    CHECK_EQ(model.vertices.size(), 5u);
    CHECK_NEAR(model.vertices[0].additionalUv[0][2], 0.3f, 1e-6);
    CHECK(model.vertices[0].skinning.type == mmd::PmxSkinningType::BDEF1);
    CHECK_NEAR(model.vertices[1].skinning.boneWeight[0], 0.25f, 1e-6);
    CHECK_EQ(model.vertices[2].skinning.boneIndex[3], 3);
    CHECK(model.vertices[3].skinning.type == mmd::PmxSkinningType::SDEF);
    CHECK_NEAR(model.vertices[3].skinning.sdefR1[2], 9.0f, 1e-6);
    CHECK(model.vertices[4].skinning.type == mmd::PmxSkinningType::QDEF);
    CHECK_EQ(model.indices.size(), 3u);
    CHECK_EQ(model.textures.size(), 2u);
    CHECK_EQ(model.materials.size(), 1u);
    CHECK_NEAR(model.materials[0].diffuse[1], 0.8f, 1e-6);
    CHECK_EQ(model.materials[0].diffuseTextureIndex, 0);
    CHECK_EQ(model.materials[0].sphereTextureIndex, 1);
    CHECK_EQ(model.materials[0].indexCount, 3);
    CHECK_EQ(model.bones.size(), 5u);
    CHECK_EQ(model.bones[0].targetIndex, 1);
    CHECK_NEAR(model.bones[1].grantWeight, 0.5f, 1e-6);
    CHECK_NEAR(model.bones[2].lockAxisOrientation[0], 1.0f, 1e-6);
    CHECK_NEAR(model.bones[3].localAxisYOrientation[1], 1.0f, 1e-6);
    CHECK_EQ(model.bones[4].ikLinks.size(), 1u);
    CHECK_EQ(model.bones[4].ikLinks[0].angleLock, 1);
    CHECK_NEAR(model.bones[4].ikLinks[0].maxRadian[0], 0.5f, 1e-6);
    CHECK_EQ(model.morphs.size(), 7u);
    CHECK_EQ(model.morphs[0].groupOffsets.size(), 1u);
    CHECK_EQ(model.morphs[1].vertexOffsets.size(), 1u);
    CHECK_EQ(model.morphs[2].boneOffsets.size(), 1u);
    CHECK_EQ(model.morphs[3].uvOffsets.size(), 1u);
    CHECK_EQ(model.morphs[4].materialOffsets.size(), 1u);
    CHECK_EQ(model.morphs[5].flipOffsets.size(), 1u);
    CHECK_EQ(model.morphs[6].impulseOffsets.size(), 1u);
    CHECK_NEAR(model.morphs[6].impulseOffsets[0].angularTorque[2], 6.0f, 1e-6);
    CHECK_EQ(model.frames.size(), 2u);
    CHECK_EQ(model.frames[0].elements.size(), 2u);
    CHECK_EQ(model.rigidBodies.size(), 1u);
    CHECK_EQ(model.joints.size(), 1u);
    CHECK_EQ(model.softBodies.size(), 0u);
}

} // namespace

TEST_CASE(mmdPmxSyntheticModelParsesAllSections) {
    const std::vector<uint8_t> bytes = buildSamplePmx20();
    mmd::PmxModel model;
    std::string error;
    CHECK(mmd::loadPmxStream(bytes.data(), bytes.size(), model, &error));
    CHECK(error.empty());
    checkPmx20(model);
    std::string why;
    CHECK(mmdTest::validatePmx(model, why));
}

TEST_CASE(mmdPmxUtf16StringsAndSoftBodyParse) {
    const std::vector<uint8_t> bytes = buildSamplePmx21();
    mmd::PmxModel model;
    std::string error;
    CHECK(mmd::loadPmxStream(bytes.data(), bytes.size(), model, &error));
    CHECK_NEAR(model.version, 2.1f, 1e-6);
    CHECK_EQ(model.setting.encoding, 0);
    CHECK_STREQ(model.name.c_str(), "A\xE3\x81\x82");
    CHECK_EQ(model.vertices.size(), 1u);
    CHECK_EQ(model.materials.size(), 1u);
    CHECK_EQ(model.materials[0].toonTextureIndex, 5);
    CHECK_EQ(model.softBodies.size(), 1u);
    CHECK_STREQ(model.softBodies[0].name.c_str(), "soft");
    CHECK_NEAR(model.softBodies[0].mass, 1.5f, 1e-6);
    CHECK_NEAR(model.softBodies[0].ahr, 11.5f, 1e-6);
    CHECK_NEAR(model.softBodies[0].sssPltCl, 5.25f, 1e-6);
    CHECK_EQ(model.softBodies[0].anchors.size(), 1u);
    CHECK(model.softBodies[0].anchors[0].isNear);
    CHECK_EQ(model.softBodies[0].pinVertices.size(), 1u);
    std::string why;
    CHECK(mmdTest::validatePmx(model, why));
}

TEST_CASE(mmdPmxRejectsInvalidHeader) {
    mmd::PmxModel model;
    std::string error;
    const uint8_t badMagic[8] = {'P', 'M', 'Y', ' ', 0, 0, 0, 0x40};
    CHECK(!mmd::loadPmxStream(badMagic, sizeof(badMagic), model, &error));

    const uint8_t badVersion[8] = {'P', 'M', 'X', ' ', 0, 0, 0x40, 0x40};
    CHECK(!mmd::loadPmxStream(badVersion, sizeof(badVersion), model, &error));

    const uint8_t shortSetting[9] = {'P', 'M', 'X', ' ', 0, 0, 0, 0x40, 7};
    CHECK(!mmd::loadPmxStream(shortSetting, sizeof(shortSetting), model, &error));

    CHECK(!mmd::loadPmxFile("/nonexistent/mmd-sample.pmx", model, &error));
}

TEST_CASE(mmdPmxTruncatedInputNeverCrashes) {
    const std::vector<uint8_t> bytes = buildSamplePmx20();
    for (size_t length = 0; length < bytes.size(); ++length) {
        mmd::PmxModel model;
        std::string error;
        if (mmd::loadPmxStream(bytes.data(), length, model, &error)) {
            std::string why;
            CHECK(mmdTest::validatePmx(model, why));
        }
    }
    mmd::PmxModel model;
    CHECK(mmd::loadPmxStream(bytes.data(), bytes.size(), model, nullptr));
}

TEST_CASE(mmdPmxOptionalRealSample) {
    const char *path = std::getenv("GN_MMD_SAMPLE_PMX");
    if (!path || !*path) {
        return;
    }
    mmd::PmxModel model;
    std::string error;
    if (!mmd::loadPmxFile(path, model, &error)) {
        std::fprintf(stderr, "  PMX 样本加载失败: %s\n", error.c_str());
        CHECK(false);
        return;
    }
    std::string why;
    if (!mmdTest::validatePmx(model, why)) {
        std::fprintf(stderr, "  PMX 结构不变量失败: %s\n", why.c_str());
    }
    CHECK(mmdTest::validatePmx(model, why));
    std::printf("  PMX 样本: vertices=%zu indices=%zu materials=%zu bones=%zu morphs=%zu rigid=%zu joints=%zu\n",
                model.vertices.size(), model.indices.size(), model.materials.size(), model.bones.size(),
                model.morphs.size(), model.rigidBodies.size(), model.joints.size());
}
