// 管线/着色器缓存库单测：SPIR-V 读取/哈希/魔数、描述校验与内容键（无 GPU，全部纯 CPU）。
#include "GN_Pipeline.h"
#include "GN_Shader.h"
#include "testFramework.h"

#include <cstdint>
#include <vector>

namespace {

using namespace GN3d;

const char *const VERT_PATH = "assets/shaders/gn3d.vert.spv";
const char *const FRAG_PATH = "assets/shaders/gn3d.frag.spv";
const char GARBAGE[] = "this is not a SPIR-V module";

std::vector<char> &vertSpv() {
    static std::vector<char> data;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        LoadSpirvFile(VERT_PATH, data);
    }
    return data;
}

std::vector<char> &fragSpv() {
    static std::vector<char> data;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        LoadSpirvFile(FRAG_PATH, data);
    }
    return data;
}

VkRenderPass fakeRenderPass(std::uintptr_t value) {
    return (VkRenderPass)value;
}

VkPipelineLayout fakeLayout(std::uintptr_t value) {
    return (VkPipelineLayout)value;
}

sPIPELINE_DESC makeGraphicsDesc() {
    sPIPELINE_DESC desc;
    desc.aVertSpv = vertSpv().data();
    desc.aVertSize = vertSpv().size();
    desc.aFragSpv = fragSpv().data();
    desc.aFragSize = fragSpv().size();
    desc.aRenderPass = fakeRenderPass(0x1000);
    desc.aLayout = fakeLayout(0x2000);
    desc.aVertexLayout.Add(0, 0, VK_FORMAT_R32G32B32_SFLOAT);
    desc.aVertexLayout.Add(1, 12, VK_FORMAT_R32G32_SFLOAT);
    return desc;
}

sPIPELINE_DESC makeComputeDesc() {
    sPIPELINE_DESC desc;
    desc.aKind = ePIPE_COMPUTE;
    desc.aCompSpv = fragSpv().data();
    desc.aCompSize = fragSpv().size();
    desc.aLayout = fakeLayout(0x2000);
    return desc;
}

void expectKeyDiffers(const sPIPELINE_DESC &base, const sPIPELINE_DESC &mutated) {
    CHECK(ComputePipelineKey(base) != ComputePipelineKey(mutated));
}

// 以同一用例内的 base 为基准，只改一个字段，要求缓存键必须变化。
#define EXPECT_KEY_DIFFERS(fieldExpr, newValue)                                  \
    do {                                                                         \
        sPIPELINE_DESC mutatedDesc = base;                                       \
        mutatedDesc.fieldExpr = (newValue);                                      \
        expectKeyDiffers(base, mutatedDesc);                                     \
    } while (0)

// 无效描述：必须返回 false 且 reason 非空。
#define EXPECT_INVALID_DESC(descValue)                                          \
    do {                                                                         \
        const char *invalidReason = 0;                                          \
        CHECK(!ValidatePipelineDesc((descValue), &invalidReason));              \
        CHECK(invalidReason != 0 && invalidReason[0] != '\0');                  \
    } while (0)

} // namespace

TEST_CASE(loadSpirvRealFile) {
    std::vector<char> data;
    CHECK(LoadSpirvFile(VERT_PATH, data));
    CHECK(data.size() > 0);
    CHECK(IsSpirv(data.data(), data.size()));

    std::vector<char> frag;
    CHECK(LoadSpirvFile(FRAG_PATH, frag));
    CHECK(frag.size() > 0);
    CHECK(IsSpirv(frag.data(), frag.size()));
}

TEST_CASE(loadSpirvRejectsMissingAndDirectory) {
    std::vector<char> data(4, 'x');
    CHECK(!LoadSpirvFile("assets/shaders/__missing__.spv", data));
    CHECK(data.empty());

    data.assign(4, 'x');
    CHECK(!LoadSpirvFile("assets/shaders", data));
    CHECK(data.empty());

    data.assign(4, 'x');
    CHECK(!LoadSpirvFile(0, data));
    CHECK(data.empty());
}

TEST_CASE(hashSpirvIsContentBased) {
    const std::vector<char> &spv = vertSpv();
    CHECK(spv.size() > 2);

    const uint64_t first = HashSpirv(spv.data(), spv.size());
    CHECK_EQ(HashSpirv(spv.data(), spv.size()), first);

    std::vector<char> changed(spv);
    changed[changed.size() / 2] = static_cast<char>(changed[changed.size() / 2] ^ 0x5A);
    CHECK(first != HashSpirv(changed.data(), changed.size()));

    CHECK_EQ(HashSpirv(0, 0), HashSpirv(0, 0));
    CHECK(HashSpirv(0, 0) != first);
}

TEST_CASE(isSpirvChecksMagicAndSize) {
    const std::vector<char> &spv = vertSpv();
    CHECK(spv.size() >= 4);
    CHECK(IsSpirv(spv.data(), spv.size()));
    CHECK(IsSpirv(spv.data(), 4));

    const char ascii[] = "SPIRV!?";
    CHECK(!IsSpirv(ascii, sizeof(ascii) - 1));
    CHECK(!IsSpirv(spv.data(), 3));
    CHECK(!IsSpirv(0, 0));
    CHECK(!IsSpirv(0, 4));
}

TEST_CASE(validateGraphicsPipelineDesc) {
    const sPIPELINE_DESC valid = makeGraphicsDesc();
    const char *reason = 0;
    CHECK(ValidatePipelineDesc(valid));
    CHECK(ValidatePipelineDesc(valid, &reason));

    sPIPELINE_DESC bad = valid;
    bad.aVertSpv = 0;
    bad.aVertSize = 0;
    reason = GARBAGE;
    CHECK(!ValidatePipelineDesc(bad, &reason));
    CHECK(reason != 0 && reason[0] != '\0');

    bad = valid;
    bad.aFragSpv = 0;
    bad.aFragSize = 0;
    reason = GARBAGE;
    CHECK(!ValidatePipelineDesc(bad, &reason));
    CHECK(reason != 0 && reason[0] != '\0');

    bad = valid;
    bad.aLayout = VK_NULL_HANDLE;
    reason = GARBAGE;
    CHECK(!ValidatePipelineDesc(bad, &reason));
    CHECK(reason != 0 && reason[0] != '\0');

    bad = valid;
    bad.aRenderPass = VK_NULL_HANDLE;
    reason = GARBAGE;
    CHECK(!ValidatePipelineDesc(bad, &reason));
    CHECK(reason != 0 && reason[0] != '\0');

    bad = valid;
    bad.aColorAttachmentCount = 0;
    reason = GARBAGE;
    CHECK(!ValidatePipelineDesc(bad, &reason));
    CHECK(reason != 0 && reason[0] != '\0');

    bad = valid;
    bad.aSubpass = valid.aColorAttachmentCount;
    reason = GARBAGE;
    CHECK(!ValidatePipelineDesc(bad, &reason));
    CHECK(reason != 0 && reason[0] != '\0');

    bad = valid;
    bad.aVertSpv = GARBAGE;
    bad.aVertSize = sizeof(GARBAGE) - 1;
    reason = 0;
    CHECK(!ValidatePipelineDesc(bad, &reason));
    CHECK(reason != 0 && reason[0] != '\0');

    CHECK(ValidatePipelineDesc(valid, 0)); // 合法描述：reason 可省略
}

TEST_CASE(validateComputePipelineDesc) {
    CHECK(fragSpv().size() > 0);
    const sPIPELINE_DESC valid = makeComputeDesc();
    // 计算管线忽略图形字段：默认 renderPass 为空、附件数默认值都不影响合法性
    CHECK(ValidatePipelineDesc(valid));

    const char *reason = 0;
    sPIPELINE_DESC bad = valid;
    bad.aCompSpv = 0;
    bad.aCompSize = 0;
    CHECK(!ValidatePipelineDesc(bad, &reason));
    CHECK(reason != 0 && reason[0] != '\0');

    bad = valid;
    bad.aLayout = VK_NULL_HANDLE;
    reason = 0;
    CHECK(!ValidatePipelineDesc(bad, &reason));
    CHECK(reason != 0 && reason[0] != '\0');

    bad = valid;
    bad.aCompSpv = GARBAGE;
    bad.aCompSize = sizeof(GARBAGE) - 1;
    reason = 0;
    CHECK(!ValidatePipelineDesc(bad, &reason));
    CHECK(reason != 0 && reason[0] != '\0');
}

TEST_CASE(pipelineKeyIgnoresSpvPointers) {
    std::vector<char> vertCopy = vertSpv();
    std::vector<char> fragCopy = fragSpv();

    sPIPELINE_DESC first = makeGraphicsDesc();
    sPIPELINE_DESC second = makeGraphicsDesc();
    second.aVertSpv = vertCopy.data();
    second.aVertSize = vertCopy.size();
    second.aFragSpv = fragCopy.data();
    second.aFragSize = fragCopy.size();

    CHECK(first.aVertSpv != second.aVertSpv);
    CHECK(first.aFragSpv != second.aFragSpv);
    CHECK_EQ(ComputePipelineKey(first), ComputePipelineKey(second));
    CHECK(PipelineDescEqual(first, second));
}

TEST_CASE(pipelineKeySensitiveToGraphicsFields) {
    const sPIPELINE_DESC base = makeGraphicsDesc();
    const uint64_t baseKey = ComputePipelineKey(base);
    CHECK_EQ(ComputePipelineKey(base), baseKey);

    sPIPELINE_DESC changed = base;
    changed.aKind = ePIPE_COMPUTE;
    expectKeyDiffers(base, changed);

    changed = base;
    changed.aLayout = fakeLayout(0x2001);
    expectKeyDiffers(base, changed);

    changed = base;
    changed.aRenderPass = fakeRenderPass(0x1001);
    expectKeyDiffers(base, changed);

    changed = base;
    changed.aSubpass = 1;
    expectKeyDiffers(base, changed);

    changed = base;
    changed.aDepthTest = false;
    expectKeyDiffers(base, changed);

    changed = base;
    changed.aDepthWrite = false;
    expectKeyDiffers(base, changed);

    changed = base;
    changed.aDepthCompare = VK_COMPARE_OP_EQUAL;
    expectKeyDiffers(base, changed);

    changed = base;
    changed.aCullBack = false;
    expectKeyDiffers(base, changed);

    changed = base;
    changed.aFrontFace = VK_FRONT_FACE_CLOCKWISE;
    expectKeyDiffers(base, changed);

    changed = base;
    changed.aBlend = eBLEND_ALPHA;
    expectKeyDiffers(base, changed);

    changed = base;
    changed.aTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    expectKeyDiffers(base, changed);

    changed = base;
    changed.aSamples = VK_SAMPLE_COUNT_4_BIT;
    expectKeyDiffers(base, changed);

    changed = base;
    changed.aColorAttachmentCount = 2;
    expectKeyDiffers(base, changed);

    changed = base;
    changed.aDynamicViewport = false;
    expectKeyDiffers(base, changed);

    changed = base;
    changed.aDynamicScissor = false;
    expectKeyDiffers(base, changed);

    changed = base;
    changed.aDepthStencil = false;
    expectKeyDiffers(base, changed);

    changed = base;
    changed.aVertexLayout.Add(2, 20, VK_FORMAT_R32_SFLOAT);
    expectKeyDiffers(base, changed);

    changed = base;
    changed.aVertexLayout.aAttrs[0].aFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
    expectKeyDiffers(base, changed);

    changed = base;
    changed.aVertexLayout.aAttrs[0].aOffset = 4;
    expectKeyDiffers(base, changed);

    changed = base;
    changed.aVertexLayout.aStride = base.aVertexLayout.aStride + 4;
    expectKeyDiffers(base, changed);

    changed = base;
    changed.aVertexLayout.aAttrCount = 0;
    expectKeyDiffers(base, changed);
}

TEST_CASE(pipelineDescNewFieldDefaults) {
    const sPIPELINE_DESC desc;
    CHECK(desc.aDynamicViewport);
    CHECK(desc.aDynamicScissor);
    CHECK(desc.aDepthStencil);
    CHECK(!desc.aCustomBlend);
    CHECK(desc.aViewport.x == 0.0f && desc.aViewport.y == 0.0f);
    CHECK(desc.aViewport.width == 1.0f && desc.aViewport.height == 1.0f);
    CHECK(desc.aViewport.minDepth == 0.0f && desc.aViewport.maxDepth == 1.0f);
    CHECK(desc.aScissor.offset.x == 0 && desc.aScissor.offset.y == 0);
    CHECK_EQ(desc.aScissor.extent.width, 1u);
    CHECK_EQ(desc.aScissor.extent.height, 1u);
}

TEST_CASE(pipelineKeySensitiveToFixedViewportAndScissor) {
    sPIPELINE_DESC base = makeGraphicsDesc();
    base.aDynamicViewport = false;
    base.aDynamicScissor = false;
    base.aViewport.y = 100.0f;
    base.aViewport.width = 100.0f;
    base.aViewport.height = 100.0f;
    base.aScissor.offset.x = 0;
    base.aScissor.offset.y = 0;
    base.aScissor.extent.width = 100;
    base.aScissor.extent.height = 100;

    EXPECT_KEY_DIFFERS(aViewport.x, 1.0f);
    EXPECT_KEY_DIFFERS(aViewport.y, 99.0f);
    EXPECT_KEY_DIFFERS(aViewport.width, 99.0f);
    EXPECT_KEY_DIFFERS(aViewport.height, -100.0f); // PBR Y 翻转负高度与正高度必须不同键
    EXPECT_KEY_DIFFERS(aViewport.minDepth, 0.5f);
    EXPECT_KEY_DIFFERS(aViewport.maxDepth, 0.5f);
    EXPECT_KEY_DIFFERS(aScissor.offset.x, 1);
    EXPECT_KEY_DIFFERS(aScissor.offset.y, 1);
    EXPECT_KEY_DIFFERS(aScissor.extent.width, 99u);
    EXPECT_KEY_DIFFERS(aScissor.extent.height, 99u);
}

TEST_CASE(pipelineKeySensitiveToCustomBlend) {
    sPIPELINE_DESC base = makeGraphicsDesc();
    base.aCustomBlend = true;
    base.aBlendSrcColor = VK_BLEND_FACTOR_ONE;
    base.aBlendDstColor = VK_BLEND_FACTOR_ZERO;
    base.aBlendSrcAlpha = VK_BLEND_FACTOR_ONE;
    base.aBlendDstAlpha = VK_BLEND_FACTOR_ZERO;

    EXPECT_KEY_DIFFERS(aBlendSrcColor, VK_BLEND_FACTOR_SRC_ALPHA);
    EXPECT_KEY_DIFFERS(aBlendDstColor, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    EXPECT_KEY_DIFFERS(aBlendSrcAlpha, VK_BLEND_FACTOR_ZERO);
    EXPECT_KEY_DIFFERS(aBlendDstAlpha, VK_BLEND_FACTOR_ONE);
    EXPECT_KEY_DIFFERS(aCustomBlend, false);
    EXPECT_KEY_DIFFERS(aBlend, eBLEND_ALPHA); // 预设字段本身仍参与键
}

TEST_CASE(validateViewportScissorAndBlend) {
    const sPIPELINE_DESC valid = makeGraphicsDesc();

    sPIPELINE_DESC fixedViewport = valid;
    fixedViewport.aDynamicViewport = false;
    fixedViewport.aViewport.y = 100.0f;
    fixedViewport.aViewport.width = 100.0f;
    fixedViewport.aViewport.height = -100.0f; // PBR Y 翻转：负高度合法
    CHECK(ValidatePipelineDesc(fixedViewport));

    sPIPELINE_DESC bad = fixedViewport;
    bad.aViewport.width = 0.0f;
    EXPECT_INVALID_DESC(bad);

    bad = fixedViewport;
    bad.aViewport.width = -1.0f;
    EXPECT_INVALID_DESC(bad);

    bad = fixedViewport;
    bad.aViewport.height = 0.0f;
    EXPECT_INVALID_DESC(bad);

    sPIPELINE_DESC fixedScissor = valid;
    fixedScissor.aDynamicScissor = false;
    fixedScissor.aScissor.extent.width = 100;
    fixedScissor.aScissor.extent.height = 100;
    CHECK(ValidatePipelineDesc(fixedScissor));

    bad = fixedScissor;
    bad.aScissor.extent.width = 0;
    EXPECT_INVALID_DESC(bad);

    bad = fixedScissor;
    bad.aScissor.extent.height = 0;
    EXPECT_INVALID_DESC(bad);

    sPIPELINE_DESC customBlend = valid;
    customBlend.aCustomBlend = true;
    customBlend.aBlendSrcColor = VK_BLEND_FACTOR_ONE;
    customBlend.aBlendDstColor = VK_BLEND_FACTOR_ZERO;
    customBlend.aBlendSrcAlpha = VK_BLEND_FACTOR_ONE;
    customBlend.aBlendDstAlpha = VK_BLEND_FACTOR_ZERO;
    CHECK(ValidatePipelineDesc(customBlend)); // 既有 2D alpha 因子 ONE/ZERO 合法

    bad = customBlend;
    bad.aBlendSrcColor = VK_BLEND_FACTOR_MAX_ENUM;
    EXPECT_INVALID_DESC(bad);

    bad = customBlend;
    bad.aBlendDstColor = VK_BLEND_FACTOR_MAX_ENUM;
    EXPECT_INVALID_DESC(bad);

    bad = customBlend;
    bad.aBlendSrcAlpha = VK_BLEND_FACTOR_MAX_ENUM;
    EXPECT_INVALID_DESC(bad);

    bad = customBlend;
    bad.aBlendDstAlpha = VK_BLEND_FACTOR_MAX_ENUM;
    EXPECT_INVALID_DESC(bad);
}

TEST_CASE(pipelineKeySensitiveToSpirvContent) {
    const sPIPELINE_DESC base = makeGraphicsDesc();

    std::vector<char> vertCopy = vertSpv();
    vertCopy[vertCopy.size() / 4] = static_cast<char>(vertCopy[vertCopy.size() / 4] ^ 0x11);
    sPIPELINE_DESC changed = base;
    changed.aVertSpv = vertCopy.data();
    changed.aVertSize = vertCopy.size();
    expectKeyDiffers(base, changed);

    std::vector<char> fragCopy = fragSpv();
    fragCopy[fragCopy.size() / 4] = static_cast<char>(fragCopy[fragCopy.size() / 4] ^ 0x22);
    changed = base;
    changed.aFragSpv = fragCopy.data();
    changed.aFragSize = fragCopy.size();
    expectKeyDiffers(base, changed);

    changed = base;
    changed.aVertSize = 8; // 指针不变、长度变化
    expectKeyDiffers(base, changed);
}

TEST_CASE(pipelineDescEqualMatchesKey) {
    std::vector<std::vector<char>> store;
    for (int i = 0; i < 3; ++i) store.push_back(vertSpv());

    std::vector<sPIPELINE_DESC> variants;
    sPIPELINE_DESC first = makeGraphicsDesc();
    first.aVertSpv = store[0].data();
    first.aVertSize = store[0].size();
    variants.push_back(first);

    sPIPELINE_DESC sameContent = first; // 相同内容、不同指针
    sameContent.aVertSpv = store[1].data();
    sameContent.aVertSize = store[1].size();
    variants.push_back(sameContent);

    sPIPELINE_DESC otherSubpass = first;
    otherSubpass.aSubpass = 1;
    variants.push_back(otherSubpass);

    sPIPELINE_DESC otherBlend = first;
    otherBlend.aBlend = eBLEND_ADDITIVE;
    variants.push_back(otherBlend);

    sPIPELINE_DESC otherDepth = first;
    otherDepth.aDepthTest = false;
    variants.push_back(otherDepth);

    sPIPELINE_DESC otherLayout = first;
    otherLayout.aVertexLayout.Add(3, 20, VK_FORMAT_R32_SFLOAT);
    variants.push_back(otherLayout);

    sPIPELINE_DESC otherViewport = first;
    otherViewport.aViewport.height = -1.0f;
    variants.push_back(otherViewport);

    sPIPELINE_DESC otherDepthStencil = first;
    otherDepthStencil.aDepthStencil = false;
    variants.push_back(otherDepthStencil);

    sPIPELINE_DESC otherCustomBlend = first;
    otherCustomBlend.aCustomBlend = true;
    otherCustomBlend.aBlendSrcColor = VK_BLEND_FACTOR_DST_COLOR;
    variants.push_back(otherCustomBlend);

    sPIPELINE_DESC otherKind = first;
    otherKind.aKind = ePIPE_COMPUTE;
    otherKind.aCompSpv = fragSpv().data();
    otherKind.aCompSize = fragSpv().size();
    variants.push_back(otherKind);

    // 键相同 ⇔ 描述相等（含相同内容的重复项）
    CHECK_EQ(ComputePipelineKey(variants[0]), ComputePipelineKey(variants[1]));
    CHECK(PipelineDescEqual(variants[0], variants[1]));
    CHECK(ComputePipelineKey(variants[0]) != ComputePipelineKey(variants[2]));
    CHECK(!PipelineDescEqual(variants[0], variants[2]));

    for (size_t i = 0; i < variants.size(); ++i) {
        for (size_t j = 0; j < variants.size(); ++j) {
            const bool equal = PipelineDescEqual(variants[i], variants[j]);
            const bool sameKey =
                ComputePipelineKey(variants[i]) == ComputePipelineKey(variants[j]);
            CHECK(equal == sameKey);
        }
    }
}

TEST_CASE(pipelineDescEqualComputeIgnoresGraphicsFields) {
    const sPIPELINE_DESC base = makeComputeDesc();
    sPIPELINE_DESC changed = base;
    changed.aRenderPass = fakeRenderPass(0x1234);
    changed.aSubpass = 2;
    changed.aDepthTest = false;
    changed.aDepthWrite = false;
    changed.aDepthCompare = VK_COMPARE_OP_ALWAYS;
    changed.aCullBack = false;
    changed.aFrontFace = VK_FRONT_FACE_CLOCKWISE;
    changed.aBlend = eBLEND_MULTIPLY;
    changed.aTopology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    changed.aSamples = VK_SAMPLE_COUNT_2_BIT;
    changed.aColorAttachmentCount = 3;
    changed.aDepthStencil = false;
    changed.aDynamicViewport = false;
    changed.aDynamicScissor = false;
    changed.aViewport.x = 5.0f;
    changed.aViewport.height = -5.0f;
    changed.aScissor.extent.width = 7;
    changed.aScissor.extent.height = 9;
    changed.aCustomBlend = true;
    changed.aBlendSrcColor = VK_BLEND_FACTOR_DST_COLOR;
    changed.aBlendDstColor = VK_BLEND_FACTOR_ONE;
    changed.aBlendSrcAlpha = VK_BLEND_FACTOR_ZERO;
    changed.aBlendDstAlpha = VK_BLEND_FACTOR_SRC_ALPHA;
    changed.aVertexLayout.Add(0, 0, VK_FORMAT_R32_SFLOAT);

    CHECK_EQ(ComputePipelineKey(base), ComputePipelineKey(changed));
    CHECK(PipelineDescEqual(base, changed));

    std::vector<char> compCopy = fragSpv();
    compCopy[compCopy.size() / 2] = static_cast<char>(compCopy[compCopy.size() / 2] ^ 0x33);
    changed.aCompSpv = compCopy.data();
    changed.aCompSize = compCopy.size();
    CHECK(ComputePipelineKey(base) != ComputePipelineKey(changed));
    CHECK(!PipelineDescEqual(base, changed));
}

TEST_CASE(cachesRejectWithoutDevice) {
    cSHADER_CACHE shaders;
    const std::vector<char> &spv = vertSpv();
    CHECK(shaders.GetOrCreate(spv.data(), spv.size()) == VK_NULL_HANDLE);
    CHECK_EQ(shaders.GetCount(), 0u);
    CHECK_EQ(shaders.GetHitCount(), 0ull);
    CHECK_EQ(shaders.GetMissCount(), 0ull);
    shaders.Release();
    shaders.Release(); // 可重复调用
    CHECK_EQ(shaders.GetCount(), 0u);

    cPIPELINE_CACHE pipelines;
    bool created = true;
    CHECK(pipelines.GetOrCreate(makeGraphicsDesc(), &created) == VK_NULL_HANDLE);
    CHECK(!created);
    CHECK(pipelines.GetLastError() != 0);
    CHECK(pipelines.GetLastError()[0] != '\0');
    CHECK_EQ(pipelines.GetCount(), 0u);
    pipelines.Release();
    pipelines.Release();
    CHECK_EQ(pipelines.GetCount(), 0u);
}

TEST_CASE(vertexLayoutAddLimitAndClear) {
    sVERTEX_LAYOUT layout;
    CHECK_EQ(layout.aAttrCount, 0u);
    CHECK_EQ(layout.aStride, 0u);

    CHECK(layout.Add(0, 0, VK_FORMAT_R32G32B32_SFLOAT));
    CHECK_EQ(layout.aAttrCount, 1u);
    CHECK_EQ(layout.aAttrs[0].aLocation, 0u);
    CHECK_EQ(layout.aAttrs[0].aFormat, VK_FORMAT_R32G32B32_SFLOAT);
    CHECK(layout.aStride >= 12u);

    for (uint32_t i = 1; i < sVERTEX_LAYOUT::MAX_ATTRS; ++i) {
        CHECK(layout.Add(i, 0, VK_FORMAT_R32_SFLOAT));
    }
    CHECK_EQ(layout.aAttrCount, sVERTEX_LAYOUT::MAX_ATTRS);
    CHECK(!layout.Add(99, 0, VK_FORMAT_R32_SFLOAT));

    layout.Clear();
    CHECK_EQ(layout.aAttrCount, 0u);
    CHECK_EQ(layout.aStride, 0u);
}
