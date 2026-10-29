// testTexture.cpp — glTF 纹理图片解码：PNG/JPEG 内存解码、data URI、asset 来源与 sampler 参数。
//
// 固定 fixture：
//   * 2×2 PNG（红/绿/蓝/白四像素，RGBA8，75 字节，base64 内嵌）；
//   * 8×8 纯红 JPEG（(200,40,40)，cjpeg -quality 95）。
// 断言不依赖文件系统，全部走内存字节。
#include "testFramework.h"

#include <GN.h>
#include "GN_3Dtexture.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace GN3d;

namespace {

// 2×2 RGBA PNG：像素顺序 (255,0,0,255) (0,255,0,255) / (0,0,255,255) (255,255,255,255)。
const char *kPngBase64 =
    "iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAEklEQVR4nGP4z8DwHwyBNBgAAEnICff5q7YNAAAAAElFT"
    "kSuQmCC";

// 8×8 纯色 JPEG（RGB=(200,40,40)）。
const char *kJpegBase64 =
    "/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAIBAQEBAQIBAQECAgICAgQDAgICAgUEBAMEBgUGBgYFBgYGBwkIBgcJBwYGCAsI"
    "CQoKCgoKBggLDAsKDAkKCgr/2wBDAQICAgICAgUDAwUKBwYHCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoK"
    "CgoKCgoKCgoKCgoKCgoKCgr/wAARCAAIAAgDASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QA"
    "tRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3"
    "ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLD"
    "xMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QA"
    "tREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2"
    "Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6"
    "wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwD5fooor8TP9QD/2Q==";

std::vector<uint8_t> decodeBase64(const std::string &dataUri) {
    std::vector<uint8_t> bytes;
    decodeDataUriBase64(dataUri.c_str(), bytes);
    return bytes;
}

std::string pngDataUri() {
    return std::string("data:image/png;base64,") + kPngBase64;
}

std::string jpegDataUri() {
    return std::string("data:image/jpeg;base64,") + kJpegBase64;
}

// sRGB 8bit → 线性（与 GL/Vulkan 的 SRGB 采样一致），用于 JPEG 像素的宽容差比较。
float srgbToLinear(float value) {
    return (value <= 0.04045f) ? value / 12.92f
                               : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

} // namespace

// PNG：2×2 四像素逐字节一致，尺寸正确。
TEST_CASE(texturePngDecode) {
    const std::vector<uint8_t> bytes = decodeBase64(pngDataUri());
    CHECK(bytes.size() == 75);
    CHECK_EQ(sniffTextureFormat(bytes.data(), bytes.size()), eTEX_FMT_PNG);

    sTEXTURE_IMAGE image;
    CHECK(decodePngMemory(bytes.data(), bytes.size(), image));
    CHECK_EQ(image.aWidth, 2u);
    CHECK_EQ(image.aHeight, 2u);
    CHECK_EQ(image.aPixels.size(), (size_t)2u * 2u * 4u);
    const uint8_t expected[16] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    for(size_t i = 0; i < 16; ++i) CHECK_EQ(image.aPixels[i], expected[i]);
}

// JPEG：启用 GN_HAS_JPEG 时能解出 8×8 的红色；未启用时明确返回 false。
TEST_CASE(textureJpegDecode) {
    const std::vector<uint8_t> bytes = decodeBase64(jpegDataUri());
    CHECK(!bytes.empty());
    sTEXTURE_IMAGE image;
    const bool decoded = decodeJpegMemory(bytes.data(), bytes.size(), image);
#ifdef GN_HAS_JPEG
    CHECK(decoded);
    CHECK_EQ(image.aWidth, 8u);
    CHECK_EQ(image.aHeight, 8u);
    CHECK_EQ(image.aPixels.size(), (size_t)8u * 8u * 4u);
    const uint8_t *center = image.aPixels.data() + ((size_t)4u * 8u + 4u) * 4u;
    CHECK(center[0] > 180); // 200 附近（JPEG 有损）
    CHECK(center[1] < 80);
    CHECK(center[2] < 80);
    CHECK_EQ(center[3], 255u);
    CHECK(srgbToLinear(center[0] / 255.0f) > 0.5f);
#else
    CHECK(!decoded);
#endif
}

// data URI：只接受 base64 载荷；普通 uri / 非 base64 data URI 被拒。
TEST_CASE(textureDataUri) {
    std::vector<uint8_t> bytes;
    CHECK(decodeDataUriBase64(pngDataUri().c_str(), bytes));
    CHECK_EQ(bytes.size(), 75u);
    CHECK(!decodeDataUriBase64("textures/a.png", bytes));
    CHECK(!decodeDataUriBase64("data:image/png,rawbytes", bytes));
    CHECK(!decodeDataUriBase64("data:image/png;base64,!!!!", bytes));
    CHECK(!decodeDataUriBase64(0, bytes));
}

// sASSET_TEXTURE 来源：aImageBytes 优先；空来源失败；mime 在文件头不可辨时兜底。
TEST_CASE(textureAssetDecode) {
    sASSET_TEXTURE texture;
    texture.aImageBytes = decodeBase64(pngDataUri());
    sTEXTURE_IMAGE image;
    CHECK(decodeAssetTexture(texture, image));
    CHECK_EQ(image.aWidth, 2u);

    sASSET_TEXTURE empty;
    CHECK(!decodeAssetTexture(empty, image));

    sASSET_TEXTURE uriOnly;
    uriOnly.aUri = pngDataUri();
    CHECK(decodeAssetTexture(uriOnly, image));
    CHECK_EQ(image.aWidth, 2u);

    sASSET_TEXTURE jpegTextured;
    jpegTextured.aImageBytes = decodeBase64(jpegDataUri());
    jpegTextured.aMime = "image/jpeg";
    const bool jpegDecoded = decodeAssetTexture(jpegTextured, image);
#ifdef GN_HAS_JPEG
    CHECK(jpegDecoded);
#else
    CHECK(!jpegDecoded);
#endif
}

// sampler 默认值与透传（0 表示 glTF 未指定 → wrap 回落 REPEAT）。
TEST_CASE(textureSamplerDefaults) {
    sASSET_TEXTURE texture;
    texture.aWrapS = 0; // 显式未指定（空 struct 的字段在 C++11 里不保证初始化）
    texture.aWrapT = 0;
    texture.aMagFilter = 0;
    texture.aMinFilter = 0;
    const sTEXTURE_SAMPLER defaults = samplerFromAssetTexture(texture);
    CHECK_EQ(defaults.aWrapS, 10497);
    CHECK_EQ(defaults.aWrapT, 10497);
    CHECK_EQ(defaults.aMagFilter, 0);
    CHECK_EQ(defaults.aMinFilter, 0);

    texture.aWrapS = 33071;
    texture.aWrapT = 33648;
    texture.aMagFilter = 9728;
    texture.aMinFilter = 9987;
    const sTEXTURE_SAMPLER sampler = samplerFromAssetTexture(texture);
    CHECK_EQ(sampler.aWrapS, 33071);
    CHECK_EQ(sampler.aWrapT, 33648);
    CHECK_EQ(sampler.aMagFilter, 9728);
    CHECK_EQ(sampler.aMinFilter, 9987);
}

// glTF 端到端：loadFromMemory 解析 image data URI + texture/sampler，再解码成 RGBA8。
TEST_CASE(textureGltfDataUri) {
    const std::string json =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"images\":[{\"uri\":\"" + pngDataUri() + "\"}],"
        "\"samplers\":[{\"wrapS\":33071,\"wrapT\":33648,\"magFilter\":9728,\"minFilter\":9729}],"
        "\"textures\":[{\"source\":0,\"sampler\":0}]}";
    sASSET asset;
    CHECK(asset.loadFromMemory(json.data(), json.size()));
    CHECK_EQ(asset.aTextures.size(), 1u);
    if(asset.aTextures.size() != 1u) return;

    const sASSET_TEXTURE &texture = asset.aTextures[0];
    CHECK(texture.aImageBytes.empty()); // data URI 不进 aImageBytes
    CHECK_EQ(texture.aUri.compare(0, 5, "data:"), 0);
    CHECK_EQ(texture.aWrapS, 33071);
    CHECK_EQ(texture.aWrapT, 33648);
    CHECK_EQ(texture.aMagFilter, 9728);
    CHECK_EQ(texture.aMinFilter, 9729);

    sTEXTURE_IMAGE image;
    CHECK(decodeAssetTexture(texture, image));
    CHECK_EQ(image.aWidth, 2u);
    CHECK_EQ(image.aHeight, 2u);
}

// 损坏输入：截断 PNG / 随机字节 / 空指针都返回 false，且不修改 out。
TEST_CASE(textureCorruptInput) {
    const std::vector<uint8_t> bytes = decodeBase64(pngDataUri());
    sTEXTURE_IMAGE image;
    image.aWidth = 7;
    image.aHeight = 9;
    image.aPixels.assign(4, 0xAB);

    CHECK(!decodePngMemory(bytes.data(), 16, image)); // 截断
    CHECK_EQ(image.aWidth, 7u); // 失败不改 out
    CHECK_EQ(image.aPixels.size(), 4u);

    const uint8_t garbage[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    CHECK_EQ(sniffTextureFormat(garbage, sizeof(garbage)), eTEX_FMT_UNKNOWN);
    sASSET_TEXTURE texture;
    texture.aImageBytes.assign(garbage, garbage + sizeof(garbage));
    CHECK(!decodeAssetTexture(texture, image));
    CHECK(!decodePngMemory(0, 0, image));
    CHECK(!decodeJpegMemory(0, 0, image));
}
