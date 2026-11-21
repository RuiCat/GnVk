// testBindingLayout.cpp — P2a canonical 绑定表与程序注册表骨架的 CPU 测试（不依赖 GPU/Vulkan）。
#include <GN.h>
#include "GN_RenderPipeline.h"
#include "vkBindingLayout.h"
#include "vkShaderProgram.h"
#include "testFramework.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

using namespace GN3d;

uint32_t layerBit(eRENDER_LAYER layer) { return 1u << (uint32_t)layer; }

} // namespace

TEST_CASE(bindingLayoutCanonicalTables) {
    uint32_t count = 0;
    const sVK_BINDING *scene = CanonicalSceneBindings(&count);
    CHECK(scene != 0);
    CHECK_EQ(count, 13u);
    CHECK_EQ(scene[0].aDescriptorType, (uint32_t)eVK_DESC_UNIFORM_BUFFER);
    CHECK_EQ(scene[4].aArrayCount, 4u);
    CHECK_EQ(scene[kVkShadowBinding].aDescriptorType, (uint32_t)eVK_DESC_UNIFORM_BUFFER);
    CHECK_EQ(CanonicalMaterialBindings(&count)[0].aDescriptorType, (uint32_t)eVK_DESC_UNIFORM_BUFFER);
    CHECK_EQ(count, 8u);
    CHECK_EQ(CanonicalSetBindingCount(eVK_SET_SCENE, 0u), 13u);
    CHECK_EQ(CanonicalSetBindingCount(eVK_SET_MATERIAL, 0u), 8u);
    CHECK_EQ(CanonicalSetBindingCount(eVK_SET_SKIN, (uint32_t)eSKIN_NONE), 0u);
    CHECK_EQ(CanonicalSetBindingCount(eVK_SET_SKIN, (uint32_t)eSKIN_UBO), 1u);
    CHECK_EQ(std::strcmp(BindingSetName(eVK_SET_SCENE), "scene"), 0);
    CHECK_EQ(std::strcmp(BindingSetName(eVK_SET_MATERIAL), "material"), 0);
    CHECK_EQ(std::strcmp(BindingSetName(eVK_SET_SKIN), "skin"), 0);
}

TEST_CASE(bindingLayoutShadowException) {
    uint32_t count = 0;
    const sVK_BINDING *shadow = ShadowBindings(&count);
    CHECK(shadow != 0);
    CHECK_EQ(count, 1u);
    CHECK_EQ(shadow[0].aSet, (uint32_t)eVK_SET_SCENE);
    CHECK_EQ(shadow[0].aBinding, kVkShadowBinding);
    const sVK_BINDING *scene = CanonicalSceneBindings(0);
    CHECK_EQ(shadow[0].aDescriptorType, scene[kVkShadowBinding].aDescriptorType);
    CHECK_EQ(shadow[0].aArrayCount, scene[kVkShadowBinding].aArrayCount);
    CHECK_EQ(shadow[0].aStageFlags, scene[kVkShadowBinding].aStageFlags);
}

TEST_CASE(bindingLayoutSkinVariants) {
    CHECK(CanonicalSkinBinding((uint32_t)eSKIN_NONE) == 0);
    const sVK_BINDING *ubo = CanonicalSkinBinding((uint32_t)eSKIN_UBO);
    const sVK_BINDING *ssbo = CanonicalSkinBinding((uint32_t)eSKIN_SSBO);
    CHECK(ubo != 0);
    CHECK(ssbo != 0);
    CHECK_EQ(ubo[0].aSet, (uint32_t)eVK_SET_SKIN);
    CHECK_EQ(ubo[0].aBinding, 0u);
    CHECK_EQ(ubo[0].aDescriptorType, (uint32_t)eVK_DESC_UNIFORM_BUFFER);
    CHECK_EQ(ssbo[0].aDescriptorType, (uint32_t)eVK_DESC_STORAGE_BUFFER);
}

TEST_CASE(bindingLayoutValidate) {
    char reason[160];
    CHECK(ValidateCanonicalBindingTable(reason, sizeof(reason)));
    CHECK_EQ(reason[0], '\0');
    CHECK(ValidateCanonicalBindingTable(0, 0));
}

TEST_CASE(shaderProgramBuiltins) {
    ResetShaderPrograms();
    char reason[160];
    CHECK(RegisterBuiltinShaderPrograms(reason, sizeof(reason)));
    CHECK_EQ(ShaderProgramCount(), 2u);
    const sSHADER_PROGRAM *pbr = FindShaderProgram("gn.pbr");
    const sSHADER_PROGRAM *depth = FindShaderProgram("gn.depth");
    CHECK(pbr != 0);
    CHECK(depth != 0);
    CHECK((pbr->aDesc.aLayerMask & layerBit(eLAYER_GBUFFER)) != 0u);
    CHECK((pbr->aDesc.aLayerMask & layerBit(eLAYER_FORWARD_OPAQUE)) != 0u);
    CHECK_EQ(pbr->aDesc.aLayoutVersion, kVkBindingLayoutVersion);
    CHECK_EQ(depth->aDesc.aLayerMask, layerBit(eLAYER_SHADOW));
    CHECK(RegisterBuiltinShaderPrograms(reason, sizeof(reason)));
    CHECK_EQ(ShaderProgramCount(), 2u);
    ResetShaderPrograms();
    CHECK_EQ(ShaderProgramCount(), 0u);
    CHECK(FindShaderProgram("gn.pbr") == 0);
}

TEST_CASE(shaderProgramRejectsBadInput) {
    ResetShaderPrograms();
    char reason[160];
    const uint32_t declared = (uint32_t)ePROG_STATE_DECLARED;
    CHECK(!RegisterShaderProgramMeta("", layerBit(eLAYER_GBUFFER), eVF_STATIC, eSKIN_NONE, 0u, declared, reason, sizeof(reason)));
    std::string longName(64, 'x');
    CHECK(!RegisterShaderProgramMeta(longName.c_str(), layerBit(eLAYER_GBUFFER), eVF_STATIC, eSKIN_NONE, 0u, declared, reason, sizeof(reason)));
    CHECK(!RegisterShaderProgramMeta("bad.mask", 0u, eVF_STATIC, eSKIN_NONE, 0u, declared, reason, sizeof(reason)));
    CHECK(!RegisterShaderProgramMeta("bad.mask2", 1u << 31, eVF_STATIC, eSKIN_NONE, 0u, declared, reason, sizeof(reason)));
    CHECK(!RegisterShaderProgramMeta("bad.vf", layerBit(eLAYER_GBUFFER), (eVERTEX_FORMAT)99, eSKIN_NONE, 0u, declared, reason, sizeof(reason)));
    CHECK(!RegisterShaderProgramMeta("bad.skin", layerBit(eLAYER_GBUFFER), eVF_STATIC, eSKIN_UBO, 0u, declared, reason, sizeof(reason)));
    CHECK(!RegisterShaderProgramMeta("bad.skin2", layerBit(eLAYER_GBUFFER), eVF_SKINNED, eSKIN_NONE, 0u, declared, reason, sizeof(reason)));
    CHECK(!RegisterShaderProgramMeta("bad.flags", layerBit(eLAYER_SHADOW), eVF_STATIC, eSKIN_NONE, (uint32_t)ePROG_IMPLICIT_SHADOW, declared, reason, sizeof(reason)));
    CHECK(!RegisterShaderProgramMeta("bad.state", layerBit(eLAYER_GBUFFER), eVF_STATIC, eSKIN_NONE, 0u, (uint32_t)ePROG_STATE_EMPTY, reason, sizeof(reason)));
    CHECK(RegisterShaderProgramMeta("dup.pbr", layerBit(eLAYER_GBUFFER), eVF_STATIC, eSKIN_NONE, 0u, declared, reason, sizeof(reason)));
    CHECK(!RegisterShaderProgramMeta("dup.pbr", layerBit(eLAYER_GBUFFER), eVF_STATIC, eSKIN_NONE, 0u, declared, reason, sizeof(reason)));
    CHECK(FindShaderProgram("dup.pbr") != 0);
    ResetShaderPrograms();
}

TEST_CASE(shaderProgramCapacity) {
    ResetShaderPrograms();
    char reason[160];
    char name[24];
    uint32_t registered = 0;
    const uint32_t declared = (uint32_t)ePROG_STATE_DECLARED;
    for(uint32_t i = 0; i <= kMaxShaderPrograms; ++i) {
        std::snprintf(name, sizeof(name), "cap.%u", i);
        if(RegisterShaderProgramMeta(name, layerBit(eLAYER_GBUFFER), eVF_STATIC, eSKIN_NONE, 0u, declared, reason, sizeof(reason))) ++registered;
    }
    CHECK_EQ(registered, kMaxShaderPrograms);
    CHECK_EQ(ShaderProgramCount(), kMaxShaderPrograms);
    ResetShaderPrograms();
}

TEST_CASE(shaderProgramStateName) {
    CHECK_EQ(std::strcmp(ProgramStateName((uint32_t)ePROG_STATE_DECLARED), "declared"), 0);
    CHECK_EQ(std::strcmp(ProgramStateName((uint32_t)ePROG_STATE_READY), "ready"), 0);
    CHECK_EQ(std::strcmp(ProgramStateName((uint32_t)ePROG_STATE_EMPTY), "empty"), 0);
    CHECK_EQ(std::strcmp(ProgramStateName(99u), "invalid"), 0);
}
