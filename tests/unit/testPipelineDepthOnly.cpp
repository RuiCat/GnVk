// 深度专用管线单测：0 颜色附件 + 无片元阶段 + 深度偏移（纯 CPU，无 GPU）。
#include "GN_Pipeline.h"
#include "GN_Shader.h"
#include "testFramework.h"

#include <cstdint>
#include <vector>

namespace {

using namespace GN3d;

const char *const VERT_PATH = "assets/shaders/gn3d.vert.spv";
const char *const FRAG_PATH = "assets/shaders/gn3d.frag.spv";

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

// 阴影 pass 的典型形态：只有 VS、无颜色附件、subpass 0、深度状态存在。
sPIPELINE_DESC makeDepthOnlyDesc() {
    sPIPELINE_DESC desc;
    desc.aVertSpv = vertSpv().data();
    desc.aVertSize = vertSpv().size();
    desc.aRenderPass = fakeRenderPass(0x1000);
    desc.aLayout = fakeLayout(0x2000);
    desc.aVertexLayout.Add(0, 0, VK_FORMAT_R32G32B32_SFLOAT);
    desc.aColorAttachmentCount = 0;
    desc.aDepthStencil = true;
    desc.aSubpass = 0;
    return desc;
}

void expectKeyDiffers(const sPIPELINE_DESC &base, const sPIPELINE_DESC &mutated) {
    CHECK(ComputePipelineKey(base) != ComputePipelineKey(mutated));
}

#define EXPECT_KEY_DIFFERS(fieldExpr, newValue)                                  \
    do {                                                                         \
        sPIPELINE_DESC mutatedDesc = base;                                       \
        mutatedDesc.fieldExpr = (newValue);                                      \
        expectKeyDiffers(base, mutatedDesc);                                     \
    } while (0)

} // namespace

TEST_CASE(depthOnlyDescIsValid) {
    CHECK(vertSpv().size() > 0);
    const sPIPELINE_DESC desc = makeDepthOnlyDesc();
    CHECK(ValidatePipelineDesc(desc));
    CHECK_EQ(desc.aColorAttachmentCount, 0u);
    CHECK(desc.aFragSpv == 0 && desc.aFragSize == 0);
    CHECK(desc.aDepthStencil);
    CHECK_EQ(desc.aSubpass, 0u);
}

TEST_CASE(depthOnlyRejectsFragmentShaderAndSubpass) {
    const sPIPELINE_DESC valid = makeDepthOnlyDesc();
    const char *reason = 0;

    sPIPELINE_DESC bad = valid;
    bad.aFragSpv = fragSpv().data();
    bad.aFragSize = fragSpv().size();
    CHECK(!ValidatePipelineDesc(bad, &reason));
    CHECK(reason != 0 && reason[0] != '\0');

    bad = valid;
    bad.aFragSpv = fragSpv().data(); // 只给指针、size=0 也算"提供了片元着色器"
    bad.aFragSize = 0;
    reason = 0;
    CHECK(!ValidatePipelineDesc(bad, &reason));
    CHECK(reason != 0 && reason[0] != '\0');

    bad = valid;
    bad.aSubpass = 1;
    reason = 0;
    CHECK(!ValidatePipelineDesc(bad, &reason));
    CHECK(reason != 0 && reason[0] != '\0');
}

TEST_CASE(depthBiasDefaultsAndKeySensitivity) {
    const sPIPELINE_DESC defaults;
    CHECK(!defaults.aDepthBiasEnable);
    CHECK(defaults.aDepthBiasConstant == 0.0f);
    CHECK(defaults.aDepthBiasSlope == 0.0f);
    CHECK_EQ(defaults.aColorAttachmentCount, 1u);

    sPIPELINE_DESC base = makeDepthOnlyDesc();
    base.aDepthBiasEnable = true;
    base.aDepthBiasConstant = 1.25f; // 阴影 pass 的既有值
    base.aDepthBiasSlope = 1.75f;
    CHECK(ValidatePipelineDesc(base));

    EXPECT_KEY_DIFFERS(aDepthBiasEnable, false);
    EXPECT_KEY_DIFFERS(aDepthBiasConstant, 1.0f);
    EXPECT_KEY_DIFFERS(aDepthBiasSlope, 2.0f);

    sPIPELINE_DESC same = base; // 字段相同 → 键相同、描述相等
    CHECK_EQ(ComputePipelineKey(base), ComputePipelineKey(same));
    CHECK(PipelineDescEqual(base, same));

    sPIPELINE_DESC otherBias = base;
    otherBias.aDepthBiasConstant = 0.5f;
    CHECK(!PipelineDescEqual(base, otherBias));
}

TEST_CASE(depthOnlyKeyDiffersFromColorPipeline) {
    sPIPELINE_DESC color = makeDepthOnlyDesc();
    color.aColorAttachmentCount = 1;
    color.aFragSpv = fragSpv().data();
    color.aFragSize = fragSpv().size();
    const sPIPELINE_DESC depthOnly = makeDepthOnlyDesc();

    CHECK(ValidatePipelineDesc(color));
    CHECK(ValidatePipelineDesc(depthOnly));
    CHECK(ComputePipelineKey(color) != ComputePipelineKey(depthOnly));
    CHECK(!PipelineDescEqual(color, depthOnly));
}

TEST_CASE(colorPipelineStillRequiresFragmentShader) {
    // 反向规则：有颜色附件但没片元着色器仍判无效（原有行为保留）。
    sPIPELINE_DESC desc = makeDepthOnlyDesc();
    desc.aColorAttachmentCount = 1;
    const char *reason = 0;
    CHECK(!ValidatePipelineDesc(desc, &reason));
    CHECK(reason != 0 && reason[0] != '\0');
}
