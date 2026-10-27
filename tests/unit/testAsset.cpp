// testAsset.cpp — glTF 2.0 导入：节点/网格/皮肤(IBM)/动画，内存 fixture 与非法输入。
#include "testFramework.h"

#include <GN.h>
#include "GN_3Dasset.h"

#include <cstring>
#include <string>
#include <vector>

using namespace GN3d;

namespace {

void writeFloat(std::vector<unsigned char> &buffer, size_t offset, float value) {
    std::memcpy(&buffer[offset], &value, sizeof(float));
}

void writeU16(std::vector<unsigned char> &buffer, size_t offset, unsigned short value) {
    std::memcpy(&buffer[offset], &value, sizeof(unsigned short));
}

std::string base64Encode(const std::vector<unsigned char> &data) {
    static const char *table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    while(i + 3 <= data.size()) {
        const unsigned int value = ((unsigned int)data[i] << 16) | ((unsigned int)data[i + 1] << 8) |
                                   (unsigned int)data[i + 2];
        out.push_back(table[(value >> 18) & 63]);
        out.push_back(table[(value >> 12) & 63]);
        out.push_back(table[(value >> 6) & 63]);
        out.push_back(table[value & 63]);
        i += 3;
    }
    if(i < data.size()) {
        unsigned int value = (unsigned int)data[i] << 16;
        if(i + 1 < data.size()) value |= (unsigned int)data[i + 1] << 8;
        out.push_back(table[(value >> 18) & 63]);
        out.push_back(table[(value >> 12) & 63]);
        out.push_back((i + 1 < data.size()) ? table[(value >> 6) & 63] : '=');
        out.push_back('=');
    }
    return out;
}

// 最小 glTF：1 个三角形网格 + 2 个节点 + 1 关节 skin + 1 条平移动画。
// buffer 布局：位置 0..35、索引 36..41、IBM 44..107、时间 108..115、平移 116..139。
std::string makeGltfFixture() {
    std::vector<unsigned char> buffer(140, 0);
    const float positions[9] = {-1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    for(int i = 0; i < 9; ++i) writeFloat(buffer, (size_t)i * 4u, positions[i]);
    writeU16(buffer, 36, 0);
    writeU16(buffer, 38, 1);
    writeU16(buffer, 40, 2);
    for(int k = 0; k < 16; ++k) writeFloat(buffer, 44u + (size_t)k * 4u, (k % 5 == 0) ? 1.0f : 0.0f);
    writeFloat(buffer, 108, 0.0f);
    writeFloat(buffer, 112, 1.0f);
    writeFloat(buffer, 116, 0.0f);
    writeFloat(buffer, 120, 0.0f);
    writeFloat(buffer, 124, 0.0f);
    writeFloat(buffer, 128, 1.0f);
    writeFloat(buffer, 132, 0.0f);
    writeFloat(buffer, 136, 0.0f);

    const std::string encoded = base64Encode(buffer);
    std::string json = "{";
    json += "\"asset\":{\"version\":\"2.0\"},";
    json += "\"scene\":0,\"scenes\":[{\"nodes\":[0,1]}],";
    json += "\"nodes\":[{\"name\":\"meshNode\",\"mesh\":0},{\"name\":\"root\"}],";
    json += "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}],";
    json += "\"skins\":[{\"joints\":[1],\"inverseBindMatrices\":2}],";
    json += "\"animations\":[{\"name\":\"move\",\"channels\":[{\"sampler\":0,\"target\":{\"node\":1,"
            "\"path\":\"translation\"}}],\"samplers\":[{\"input\":3,\"output\":4,\"interpolation\":"
            "\"LINEAR\"}]}],";
    json += "\"accessors\":[";
    json += "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
            "\"min\":[-1,0,0],\"max\":[1,1,0]},";
    json += "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"},";
    json += "{\"bufferView\":2,\"componentType\":5126,\"count\":1,\"type\":\"MAT4\"},";
    json += "{\"bufferView\":3,\"componentType\":5126,\"count\":2,\"type\":\"SCALAR\"},";
    json += "{\"bufferView\":4,\"componentType\":5126,\"count\":2,\"type\":\"VEC3\"}],";
    json += "\"bufferViews\":[";
    json += "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},";
    json += "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6},";
    json += "{\"buffer\":0,\"byteOffset\":44,\"byteLength\":64},";
    json += "{\"buffer\":0,\"byteOffset\":108,\"byteLength\":8},";
    json += "{\"buffer\":0,\"byteOffset\":116,\"byteLength\":24}],";
    json += "\"buffers\":[{\"byteLength\":140,\"uri\":\"data:application/octet-stream;base64,";
    json += encoded;
    json += "\"}]}";
    return json;
}

} // namespace

// 内存导入：节点/网格/皮肤/动画的字段与采样。
TEST_CASE(assetLoadFromMemory) {
    const std::string json = makeGltfFixture();
    sASSET asset;
    CHECK(asset.loadFromMemory(json.data(), json.size()));
    CHECK(asset.isLoaded());
    CHECK_EQ(asset.aNodes.size(), 2u);
    CHECK_EQ(asset.aMeshes.size(), 1u);
    CHECK_EQ(asset.aSkins.size(), 1u);
    CHECK_EQ(asset.aAnimations.size(), 1u);

    CHECK_EQ(asset.aNodes[0].aMesh, 0);
    CHECK_EQ(asset.aNodes[0].aParent, -1);
    CHECK_EQ(asset.aNodes[1].aParent, -1);

    const sASSET_MESH &mesh = asset.aMeshes[0];
    CHECK_EQ(mesh.aPrimitives.size(), 1u);
    const sASSET_PRIMITIVE &primitive = mesh.aPrimitives[0];
    CHECK_EQ(primitive.aVertices.size(), 3u);
    CHECK_EQ(primitive.aIndices.size(), 3u);
    CHECK_EQ(primitive.aIndices[0], 0u);
    CHECK_EQ(primitive.aIndices[2], 2u);
    CHECK_NEAR(primitive.aVertices[0].aPosition.x, -1.0f, 1e-6f);
    CHECK_NEAR(primitive.aVertices[2].aPosition.y, 1.0f, 1e-6f);
    CHECK_NEAR(primitive.aVertices[0].aWeights[0], 0.0f, 1e-6f);
    CHECK_NEAR(primitive.aVertices[0].aNormal.y, 1.0f, 1e-6f); // 缺省法线

    const sASSET_SKIN &skin = asset.aSkins[0];
    CHECK_EQ(skin.aJoints.size(), 1u);
    CHECK_EQ(skin.aJoints[0], 1u);
    CHECK_EQ(skin.aInverseBinds.size(), 1u);
    CHECK_NEAR(skin.aInverseBinds[0].Elem(0, 0), 1.0f, 1e-6f);
    CHECK_NEAR(skin.aInverseBinds[0].Elem(1, 1), 1.0f, 1e-6f);
    CHECK_NEAR(skin.aInverseBinds[0].Elem(3, 0), 0.0f, 1e-6f);

    const sASSET_ANIMATION &animation = asset.aAnimations[0];
    CHECK_STREQ(animation.aName.c_str(), "move");
    CHECK_EQ(animation.aClip.aChannels.size(), 1u);
    CHECK_NEAR(animation.aClip.aDuration, 1.0f, 1e-6f);
    sANIM_TRANSFORM transform;
    CHECK(animation.aClip.sample(1u, 0.5f, transform));
    CHECK_NEAR(transform.aTranslation.x, 0.5f, 1e-4f);
    CHECK_NEAR(transform.aTranslation.y, 0.0f, 1e-6f);
}

// 非法输入：非 JSON、空指针、不存在的文件都返回 false 且不残留数据。
TEST_CASE(assetRejectsInvalidInput) {
    sASSET asset;
    CHECK(!asset.loadFromMemory("not json", 8));
    CHECK(!asset.isLoaded());
    CHECK(!asset.loadFromMemory(0, 10));
    CHECK(!asset.loadFromFile("/nonexistent/asset.gltf"));
    CHECK(!asset.isLoaded());
}
