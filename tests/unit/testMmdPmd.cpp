// testMmdPmd.cpp — PMD 解析：合成样本全字段、截断输入安全、真实样本结构不变量。
#include "testFramework.h"
#include "testMmdValidate.h"
#include "testMmdWriter.h"

#include "mmd/pmdLoader.h"
#include "mmd/mmdMeshBuilder.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

using mmdTest::ByteWriter;

// 合成一个覆盖 PMD 全部段（含英文扩展与物理段）的最小模型。
std::vector<uint8_t> buildSamplePmd(bool withEnglish) {
    ByteWriter writer;
    writer.text("Pmd");
    writer.f32(1.0f);
    writer.fixedText("\x82\xA0\x82\xA2", 20); // Shift-JIS「あい」
    writer.fixedText("comment", 256);

    writer.u32(3);
    for (int i = 0; i < 3; ++i) {
        writer.f3(static_cast<float>(i), 2.0f, 3.0f);
        writer.f3(0.0f, 1.0f, 0.0f);
        writer.f32(0.5f);
        writer.f32(0.25f);
        writer.u16(static_cast<uint32_t>(i));
        writer.u16(2);
        writer.u8(100);
        writer.u8(i == 0 ? 1 : 0);
    }

    writer.u32(3);
    writer.u16(0);
    writer.u16(1);
    writer.u16(2);

    writer.u32(1);
    writer.f4(1.0f, 0.5f, 0.25f, 1.0f);
    writer.f32(5.0f);
    writer.f3(1.0f, 1.0f, 1.0f);
    writer.f3(0.1f, 0.2f, 0.3f);
    writer.u8(1);
    writer.u8(1);
    writer.u32(3);
    writer.fixedText("tex.tga*sph.spa", 20);

    writer.u16(1);
    writer.fixedText("bone", 20);
    writer.u16(0xFFFF);
    writer.u16(0xFFFF);
    writer.u8(1);
    writer.u16(0);
    writer.f3(0.0f, 1.0f, 0.0f);

    writer.u16(1);
    writer.u16(0);
    writer.u16(0);
    writer.u8(2);
    writer.u16(40);
    writer.f32(1.5f);
    writer.u16(0);
    writer.u16(0);

    writer.u16(1);
    writer.fixedText("face", 20);
    writer.i32(2);
    writer.u8(2); // EYE：非 Base 才会写英文名
    writer.i32(0);
    writer.f3(1.0f, 1.0f, 1.0f);
    writer.i32(1);
    writer.f3(2.0f, 2.0f, 2.0f);

    writer.u8(1);
    writer.u16(0);

    writer.u8(1);
    writer.fixedText("disp", 50);

    writer.u32(1);
    writer.u16(0);
    writer.u8(0);

    writer.u8(withEnglish ? 1 : 0);
    if (withEnglish) {
        writer.fixedText("nameEn", 20);
        writer.fixedText("commentEn", 256);
        writer.fixedText("boneEn", 20);
        writer.fixedText("faceEn", 20);
        writer.fixedText("dispEn", 50);
    }

    for (int i = 0; i < 10; ++i) {
        writer.fixedText(i == 0 ? "toon.bmp" : "", 100);
    }

    writer.u32(1);
    writer.fixedText("rigid", 20);
    writer.u16(0);
    writer.u8(0);
    writer.u16(0xFFFF);
    writer.u8(0);
    writer.f3(1.0f, 2.0f, 3.0f);
    writer.f3(1.0f, 1.0f, 1.0f);
    writer.f3(0.0f, 0.0f, 0.0f);
    writer.f32(1.0f);
    writer.f32(0.5f);
    writer.f32(0.5f);
    writer.f32(0.1f);
    writer.f32(0.9f);
    writer.u8(1);

    writer.u32(1);
    writer.fixedText("joint", 20);
    writer.u32(0);
    writer.u32(0);
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

} // namespace

TEST_CASE(mmdPmdSyntheticModelParsesAllSections) {
    const std::vector<uint8_t> bytes = buildSamplePmd(true);
    mmd::PmdModel model;
    std::string error;
    CHECK(mmd::loadPmdStream(bytes.data(), bytes.size(), model, &error));
    CHECK(error.empty());

    CHECK_NEAR(model.version, 1.0f, 1e-6);
    CHECK_STREQ(model.header.name.c_str(), "\xE3\x81\x82\xE3\x81\x84"); // UTF-8「あい」
    CHECK_STREQ(model.header.comment.c_str(), "comment");
    CHECK_STREQ(model.header.nameEnglish.c_str(), "nameEn");
    CHECK_STREQ(model.header.commentEnglish.c_str(), "commentEn");

    CHECK_EQ(model.vertices.size(), 3u);
    CHECK_NEAR(model.vertices[1].position[0], 1.0f, 1e-6);
    CHECK_EQ(model.vertices[2].boneWeight, 100);
    CHECK(model.vertices[0].edgeInvisible);
    CHECK_EQ(model.indices.size(), 3u);
    CHECK_EQ(model.indices[2], 2);

    CHECK_EQ(model.materials.size(), 1u);
    CHECK_NEAR(model.materials[0].diffuse[1], 0.5f, 1e-6);
    CHECK_STREQ(model.materials[0].textureFilename.c_str(), "tex.tga");
    CHECK_STREQ(model.materials[0].sphereFilename.c_str(), "sph.spa");

    CHECK_EQ(model.bones.size(), 1u);
    CHECK_STREQ(model.bones[0].name.c_str(), "bone");
    CHECK_STREQ(model.bones[0].nameEnglish.c_str(), "boneEn");
    CHECK_EQ(model.bones[0].parentBoneIndex, 0xFFFF);
    CHECK(model.bones[0].boneType == mmd::PmdBoneType::ROTATION_AND_MOVE);

    CHECK_EQ(model.iks.size(), 1u);
    CHECK_EQ(model.iks[0].ikChildBoneIndex.size(), 2u);
    CHECK_EQ(model.iks[0].iterations, 40);

    CHECK_EQ(model.faces.size(), 1u);
    CHECK(model.faces[0].type == mmd::PmdFaceCategory::EYE);
    CHECK_STREQ(model.faces[0].nameEnglish.c_str(), "faceEn");
    CHECK_EQ(model.faces[0].vertices.size(), 2u);
    CHECK_EQ(model.faceIndices.size(), 1u);
    CHECK_EQ(model.boneDispNames[0].nameEnglish, std::string("dispEn"));
    CHECK_EQ(model.boneDisps[0].boneIndex, 0);
    CHECK_EQ(model.toonFilenames.size(), 10u);
    CHECK_STREQ(model.toonFilenames[0].c_str(), "toon.bmp");

    CHECK_EQ(model.rigidBodies.size(), 1u);
    CHECK_NEAR(model.rigidBodies[0].size[1], 2.0f, 1e-6);
    CHECK(model.rigidBodies[0].rigidType == mmd::PmdRigidBodyType::PHYSICS);
    CHECK_EQ(model.constraints.size(), 1u);
    CHECK_NEAR(model.constraints[0].angularStiffness[2], 2.0f, 1e-6);

    std::string why;
    CHECK(mmdTest::validatePmd(model, why));
}

TEST_CASE(mmdPmdWithoutEnglishExtensionParses) {
    const std::vector<uint8_t> bytes = buildSamplePmd(false);
    mmd::PmdModel model;
    std::string error;
    CHECK(mmd::loadPmdStream(bytes.data(), bytes.size(), model, &error));
    CHECK(model.bones[0].nameEnglish.empty());
    std::string why;
    CHECK(mmdTest::validatePmd(model, why));
}

TEST_CASE(mmdPmdRejectsInvalidHeader) {
    mmd::PmdModel model;
    std::string error;
    const uint8_t badMagic[8] = {'P', 'm', 'x', 0, 0, 0, 0x80, 0x3F};
    CHECK(!mmd::loadPmdStream(badMagic, sizeof(badMagic), model, &error));
    CHECK(!error.empty());

    const std::vector<uint8_t> bytes = buildSamplePmd(true);
    std::vector<uint8_t> withTrailing = bytes;
    withTrailing.push_back(0);
    CHECK(!mmd::loadPmdStream(withTrailing.data(), withTrailing.size(), model, &error));

    CHECK(!mmd::loadPmdFile("/nonexistent/mmd-sample.pmd", model, &error));
    CHECK(!error.empty());
}

TEST_CASE(mmdPmdTruncatedInputNeverCrashes) {
    const std::vector<uint8_t> bytes = buildSamplePmd(true);
    for (size_t length = 0; length < bytes.size(); ++length) {
        mmd::PmdModel model;
        std::string error;
        if (mmd::loadPmdStream(bytes.data(), length, model, &error)) {
            std::string why;
            CHECK(mmdTest::validatePmd(model, why));
        }
    }
    mmd::PmdModel model;
    CHECK(mmd::loadPmdStream(bytes.data(), bytes.size(), model, nullptr));
}

TEST_CASE(mmdPmdOptionalRealSample) {
    const char *path = std::getenv("GN_MMD_SAMPLE_PMD");
    if (!path || !*path) {
        return;
    }
    mmd::PmdModel model;
    std::string error;
    if (!mmd::loadPmdFile(path, model, &error)) {
        std::fprintf(stderr, "  PMD 样本加载失败: %s\n", error.c_str());
        CHECK(false);
        return;
    }
    std::string why;
    if (!mmdTest::validatePmd(model, why)) {
        std::fprintf(stderr, "  PMD 结构不变量失败: %s\n", why.c_str());
    }
    CHECK(mmdTest::validatePmd(model, why));
    std::printf("  PMD 样本: vertices=%zu indices=%zu materials=%zu bones=%zu rigid=%zu\n",
                model.vertices.size(), model.indices.size(), model.materials.size(), model.bones.size(),
                model.rigidBodies.size());
}

// 镜像 X 会把三角形绕序反转；原始 MMD 数据的绕序与顶点法线是一致的，
// 因此 buildPmdModel 必须再反一次绕序。否则世界 CCW 正面管线下模型会内外翻转
// （症状：只看到内壁、光照像「法线反了」）。真实样本 mode.pmd 的探针统计：
// 镜像前 agree=6189/disagree=40，未修时镜像后 agree=40/disagree=6189。
TEST_CASE(mmdPmdMirrorKeepsWindingConsistentWithNormals) {
    using namespace mmd;

    PmdModel model;
    model.vertices.resize(4);
    const float corners[4][2] = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f}};
    for (int i = 0; i < 4; ++i) {
        PmdVertex &vertex = model.vertices[static_cast<size_t>(i)];
        vertex.position[0] = corners[i][0];
        vertex.position[1] = corners[i][1];
        vertex.position[2] = 0.0f;
        vertex.normal[0] = 0.0f;
        vertex.normal[1] = 0.0f;
        vertex.normal[2] = 1.0f; // 朝向 +Z
        vertex.uv[0] = 0.0f;
        vertex.uv[1] = 0.0f;
    }
    // 面向 +Z 的四边形：两个三角形，绕序与法线一致（与真实 PMD 数据的性质相同）。
    model.indices = {0, 1, 2, 0, 2, 3};
    PmdMaterial material;
    material.diffuse[0] = 1.0f;
    material.diffuse[1] = 1.0f;
    material.diffuse[2] = 1.0f;
    material.diffuse[3] = 1.0f;
    material.power = 10.0f;
    material.indexCount = 6;
    model.materials.push_back(material);

    GN3d::cMODEL built;
    std::vector<MeshTextureBinding> textures;
    std::string error;
    CHECK(mmd::buildPmdModel(model, built, textures, &error));
    CHECK(built.aMeshCount == 1);
    if (built.aMeshCount != 1) {
        return;
    }
    const GN3d::sMESH &mesh = built.aMeshes[0];
    CHECK(mesh.aIndexCount == 6);
    long agree = 0;
    long disagree = 0;
    for (uint32_t t = 0; t + 2 < mesh.aIndexCount; t += 3) {
        const float *p0 = mesh.aVertices + static_cast<size_t>(mesh.aIndices[t + 0]) * 11;
        const float *p1 = mesh.aVertices + static_cast<size_t>(mesh.aIndices[t + 1]) * 11;
        const float *p2 = mesh.aVertices + static_cast<size_t>(mesh.aIndices[t + 2]) * 11;
        const float e1[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
        const float e2[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};
        const float gn[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                             e1[0] * e2[1] - e1[1] * e2[0]};
        const float n[3] = {p0[3] + p1[3] + p2[3], p0[4] + p1[4] + p2[4], p0[5] + p1[5] + p2[5]};
        const float d = gn[0] * n[0] + gn[1] * n[1] + gn[2] * n[2];
        if (d > 0.0f) {
            ++agree;
        } else if (d < 0.0f) {
            ++disagree;
        }
    }
    std::printf("  mirrored winding: agree=%ld disagree=%ld\n", agree, disagree);
    CHECK(agree == 2);
    CHECK(disagree == 0);
}
