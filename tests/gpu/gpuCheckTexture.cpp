// gpuCheckTexture.cpp — P1c 纹理端到端验收：
//   glTF data URI 解码 → RGBA8 → GN_VkCreateTextureRGBA8（SRGB）→ 材质采样 → 像素回读。
//
// 判据：
//   * 2×2 四色 PNG 解码逐字节一致；最小 glTF（image data URI + sampler）能解出同一张图。
//   * 8×8 纯色 (200,40,40) 上传后宽度/高度/句柄齐全。
//   * 材质 UBO 白色 × 该纹理，中心像素应等于 sRGB→linear 后的颜色（±0.03）。
//   * Destroy 可重复调用，销毁后可再次创建。
#include <GN.h>
#include "GN_Vk.h"
#include "GN_3Dtexture.h"

#include "gpuCheckReport.h"
#include "gpuCheckTexture.h"
#include "gpuPixelRead.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

// 2×2 RGBA PNG（与 testTexture.cpp 同一 fixture）。
const char *kPngBase64 =
    "iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAEklEQVR4nGP4z8DwHwyBNBgAAEnICff5q7YNAAAAAElFT"
    "kSuQmCC";

// Vulkan 视口 Y 翻转后，NDC 逆时针在屏幕上是顺时针；材质管线开背面剔除，
// 索引按“屏幕顺时针”给（与 gpuCheckMaterial 的四边形一致）。
const uint32_t kQuadIndexCCW[6] = {0, 2, 1, 0, 3, 2};
const float kSolidRgba[4] = {200.0f / 255.0f, 40.0f / 255.0f, 40.0f / 255.0f, 1.0f};

float srgbToLinear(float value) {
    return (value <= 0.04045f) ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
}

struct sTEXTURE_MATERIAL_FIXTURE {
    VkBuffer aUboBuffer;
    VkDeviceMemory aUboMemory;
    GNg::sMATERIAL_UBO_RESOURCE aUboRes;
    GNg::sMATERIAL_TEX_RESOURCE aTexRes;
    GN3d::sMATERIAL_RES aMaterial;

    sTEXTURE_MATERIAL_FIXTURE()
        : aUboBuffer(VK_NULL_HANDLE), aUboMemory(VK_NULL_HANDLE) {
        std::memset(&aUboRes, 0, sizeof(aUboRes));
        std::memset(&aTexRes, 0, sizeof(aTexRes));
        std::memset(&aMaterial, 0, sizeof(aMaterial));
    }
};

bool createWhiteUbo(sTEXTURE_MATERIAL_FIXTURE &fixture) {
    VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = 16;
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if(vkCreateBuffer(GNg::GN_Vk.device, &bufferInfo, 0, &fixture.aUboBuffer) != VK_SUCCESS) {
        return false;
    }
    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(GNg::GN_Vk.device, fixture.aUboBuffer, &requirements);
    VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = requirements.size;
    VkPhysicalDeviceMemoryProperties properties;
    vkGetPhysicalDeviceMemoryProperties(GNg::GN_Vk.physicalDevice, &properties);
    bool found = false;
    for(uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
        if((requirements.memoryTypeBits & (1u << i)) &&
           (properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
           (properties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            allocInfo.memoryTypeIndex = i;
            found = true;
            break;
        }
    }
    if(!found) return false;
    if(vkAllocateMemory(GNg::GN_Vk.device, &allocInfo, 0, &fixture.aUboMemory) != VK_SUCCESS) {
        return false;
    }
    if(vkBindBufferMemory(GNg::GN_Vk.device, fixture.aUboBuffer, fixture.aUboMemory, 0) !=
       VK_SUCCESS) {
        return false;
    }
    void *mapped = 0;
    if(vkMapMemory(GNg::GN_Vk.device, fixture.aUboMemory, 0, 16, 0, &mapped) != VK_SUCCESS) {
        return false;
    }
    const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    std::memcpy(mapped, white, sizeof(white));
    vkUnmapMemory(GNg::GN_Vk.device, fixture.aUboMemory);
    fixture.aUboRes.aBuffer = fixture.aUboBuffer;
    fixture.aUboRes.aOffset = 0;
    fixture.aUboRes.aRange = 16;
    return true;
}

void destroyFixture(sTEXTURE_MATERIAL_FIXTURE &fixture) {
    GNg::GN_VkMaterialDestroy(&fixture.aMaterial);
    if(GNg::GN_Vk.device != VK_NULL_HANDLE) {
        if(fixture.aUboBuffer) vkDestroyBuffer(GNg::GN_Vk.device, fixture.aUboBuffer, 0);
        if(fixture.aUboMemory) vkFreeMemory(GNg::GN_Vk.device, fixture.aUboMemory, 0);
    }
    fixture.aUboBuffer = VK_NULL_HANDLE;
    fixture.aUboMemory = VK_NULL_HANDLE;
}

void makeMaterialDesc(const sTEXTURE_MATERIAL_FIXTURE &fixture, GN3d::sMATERIAL_DESC &desc) {
    std::memset(&desc, 0, sizeof(desc));
    desc.aPipelineKey = 0x1ull;
    desc.aBindingCount = 2;
    desc.aBindings[0].aBinding = 0;
    desc.aBindings[0].aKind = GN3d::eMAT_BIND_UNIFORM_BUFFER;
    desc.aBindings[0].aStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    desc.aBindings[0].aResource = &fixture.aUboRes;
    desc.aBindings[1].aBinding = 1;
    desc.aBindings[1].aKind = GN3d::eMAT_BIND_COMBINED_IMAGE_SAMPLER;
    desc.aBindings[1].aStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    desc.aBindings[1].aResource = &fixture.aTexRes;
}

void makeQuad(GNg::VkVertex3D quad[4]) {
    std::memset(quad, 0, 4 * sizeof(GNg::VkVertex3D));
    const float xs[4] = {-1.0f, 1.0f, 1.0f, -1.0f};
    const float ys[4] = {-1.0f, -1.0f, 1.0f, 1.0f};
    for(int i = 0; i < 4; ++i) {
        quad[i].px = xs[i];
        quad[i].py = ys[i];
        quad[i].pz = 0.5f;
        quad[i].nz = 1.0f;
        quad[i].tx = 1.0f;
        quad[i].u = (i == 1 || i == 2) ? 1.0f : 0.0f;
        quad[i].v = (i >= 2) ? 1.0f : 0.0f;
    }
}

bool drawMaterialQuad(const GN3d::sMATERIAL_RES &material) {
    GNg::VkVertex3D quad[4];
    makeQuad(quad);
    GNg::sMAT44 identity;
    GNg::Mat44Identity(&identity);
    GNg::sVEC3 eye = {0.0f, 0.0f, 1.0f};
    if(!GNg::GN_VkBeginFrame()) return false;
    GNg::GN_VkBegin3DPass(&identity, &eye, 0, 0);
    GNg::GN_VkDraw3DMaterial(quad, 4, kQuadIndexCCW, 6, &identity, &material);
    GNg::GN_VkEnd3DPass();
    GNg::GN_VkEndFrame();
    vkQueueWaitIdle(GNg::GN_Vk.graphicsQueue);
    return true;
}

bool readCenterPixel(GpuCheck::sFLOAT_PIXEL &out) {
    std::vector<uint8_t> pixels;
    const uint32_t width = GNg::GN_Vk.swapchainExtent.width;
    const uint32_t height = GNg::GN_Vk.swapchainExtent.height;
    if(!GpuCheck::ReadSwapchain(0, 0, (int)width, (int)height, pixels)) return false;
    out = GpuCheck::SampleSwapchain(pixels, (int)width, (int)(width / 2), (int)(height / 2));
    return true;
}

// 纯色 RGBA8 纹理数据（8×8）。
std::vector<uint8_t> makeSolidPixels(uint32_t width, uint32_t height) {
    std::vector<uint8_t> pixels((size_t)width * height * 4u);
    for(size_t i = 0; i < pixels.size(); i += 4u) {
        pixels[i + 0u] = 200;
        pixels[i + 1u] = 40;
        pixels[i + 2u] = 40;
        pixels[i + 3u] = 255;
    }
    return pixels;
}

void checkDecoding() {
    std::vector<uint8_t> encoded;
    GN3d::decodeDataUriBase64((std::string("data:image/png;base64,") + kPngBase64).c_str(), encoded);
    GN3d::sTEXTURE_IMAGE image;
    const bool decoded = GN3d::decodePngMemory(encoded.data(), encoded.size(), image);
    const bool exact =
        decoded && image.aWidth == 2 && image.aHeight == 2 && image.aPixels.size() == 16 &&
        image.aPixels[0] == 255 && image.aPixels[1] == 0 && image.aPixels[3] == 255 &&
        image.aPixels[4] == 0 && image.aPixels[5] == 255 && image.aPixels[8] == 0 &&
        image.aPixels[9] == 0 && image.aPixels[10] == 255 && image.aPixels[12] == 255;
    char detail[128];
    std::snprintf(detail, sizeof(detail), "decoded=%d size=%ux%u", (int)decoded,
                  image.aWidth, image.aHeight);
    if(exact) GpuCheck::ReportPass("texture.decode", detail);
    else GpuCheck::ReportFail("texture.decode", detail);

    const std::string json =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"images\":[{\"uri\":\"data:image/png;base64," + std::string(kPngBase64) + "\"}],"
        "\"samplers\":[{\"wrapS\":33071,\"wrapT\":33071,\"magFilter\":9728,\"minFilter\":9729}],"
        "\"textures\":[{\"source\":0,\"sampler\":0}]}";
    GN3d::sASSET asset;
    GN3d::sTEXTURE_IMAGE gltfImage;
    const bool loaded = asset.loadFromMemory(json.data(), json.size());
    const bool gltfOk = loaded && asset.aTextures.size() == 1 &&
                        GN3d::decodeAssetTexture(asset.aTextures[0], gltfImage) &&
                        gltfImage.aWidth == 2 && gltfImage.aHeight == 2 &&
                        asset.aTextures[0].aWrapS == 33071 &&
                        asset.aTextures[0].aMagFilter == 9728;
    std::snprintf(detail, sizeof(detail), "loaded=%d textures=%u %ux%u", (int)loaded,
                  (unsigned)asset.aTextures.size(), gltfImage.aWidth, gltfImage.aHeight);
    if(gltfOk) GpuCheck::ReportPass("texture.glTF", detail);
    else GpuCheck::ReportFail("texture.glTF", detail);
}

void checkUploadAndSample() {
    const std::vector<uint8_t> pixels = makeSolidPixels(8, 8);
    GN3d::sTEXTURE_SAMPLER sampler; // 默认 REPEAT + LINEAR
    GNg::sTEXTURE_GPU texture;
    const bool created = GNg::GN_VkCreateTextureRGBA8(pixels.data(), 8, 8, sampler, texture);
    char detail[160];
    std::snprintf(detail, sizeof(detail), "created=%d %ux%u view=%d sampler=%d", (int)created,
                  texture.aWidth, texture.aHeight, (int)(texture.aView != VK_NULL_HANDLE),
                  (int)(texture.aSampler != VK_NULL_HANDLE));
    if(!created || texture.aWidth != 8 || texture.aHeight != 8 || texture.aView == VK_NULL_HANDLE ||
       texture.aSampler == VK_NULL_HANDLE) {
        GpuCheck::ReportFail("texture.upload", detail);
        GNg::GN_VkDestroyTexture(texture);
        return;
    }
    GpuCheck::ReportPass("texture.upload", detail);

    sTEXTURE_MATERIAL_FIXTURE fixture;
    fixture.aTexRes.aImageView = texture.aView;
    fixture.aTexRes.aSampler = texture.aSampler;
    fixture.aTexRes.aLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    GN3d::sMATERIAL_RES material;
    std::memset(&material, 0, sizeof(material));

    bool fixtureOk = createWhiteUbo(fixture);
    if(fixtureOk) {
        makeMaterialDesc(fixture, material.aDesc);
        fixtureOk = GNg::GN_VkMaterialCreate(&material);
    }

    GpuCheck::sFLOAT_PIXEL pixel = {0.0f, 0.0f, 0.0f, 0.0f};
    const bool drawn = fixtureOk && drawMaterialQuad(material) && readCenterPixel(pixel);
    const float expectedR = srgbToLinear(kSolidRgba[0]);
    const float expectedG = srgbToLinear(kSolidRgba[1]);
    const float expectedB = srgbToLinear(kSolidRgba[2]);
    const float tolerance = 0.03f;
    const bool pixelOk = drawn && std::fabs(pixel.r - expectedR) <= tolerance &&
                         std::fabs(pixel.g - expectedG) <= tolerance &&
                         std::fabs(pixel.b - expectedB) <= tolerance;
    std::snprintf(detail, sizeof(detail), "RGBA=%.3f,%.3f,%.3f,%.3f expected=%.3f,%.3f,%.3f",
                  pixel.r, pixel.g, pixel.b, pixel.a, expectedR, expectedG, expectedB);
    if(pixelOk) GpuCheck::ReportPass("texture.sample", detail);
    else GpuCheck::ReportFail("texture.sample", detail);

    destroyFixture(fixture);
    GNg::GN_VkDestroyTexture(texture);
}

void checkDestroyAndRecreate() {
    const std::vector<uint8_t> pixels = makeSolidPixels(4, 4);
    GN3d::sTEXTURE_SAMPLER sampler;
    GNg::sTEXTURE_GPU texture;
    const bool first = GNg::GN_VkCreateTextureRGBA8(pixels.data(), 4, 4, sampler, texture);
    GNg::GN_VkDestroyTexture(texture);
    GNg::GN_VkDestroyTexture(texture); // 重复释放必须安全
    const bool recreated = GNg::GN_VkCreateTextureRGBA8(pixels.data(), 4, 4, sampler, texture);
    const bool ok = first && recreated && texture.aImage != VK_NULL_HANDLE;
    GNg::GN_VkDestroyTexture(texture);

    char detail[128];
    std::snprintf(detail, sizeof(detail), "first=%d recreated=%d doubleDestroySafe=1", (int)first,
                  (int)recreated);
    if(ok) GpuCheck::ReportPass("texture.destroy", detail);
    else GpuCheck::ReportFail("texture.destroy", detail);
}

} // namespace

namespace GpuCheck {

void RunTextureChecks() {
    checkDecoding();
    checkUploadAndSample();
    checkDestroyAndRecreate();
}

} // namespace GpuCheck
