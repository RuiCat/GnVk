// testSpirv.cpp — P5：SPIR-V 最小校验 + 程序层入口绑定/封口（原子性）。
#include <GN.h>
#include "GN_RenderPipeline.h"
#include "GN_Spirv.h"
#include "vkShaderProgram.h"
#include "testFramework.h"

#include <cstdint>
#include <cstring>

namespace {

const uint32_t kValidModule[5] = {0x07230203u, 0x00010000u, 0u, 2u, 0u};
const uint32_t kValidFrag[5] = {0x07230203u, 0x00010000u, 0u, 2u, 0u};
uint32_t kBadMagic[5] = {0x03022307u, 0x00010000u, 0u, 2u, 0u};
uint32_t kBadVersion[5] = {0x07230203u, 0x00020000u, 0u, 2u, 0u};
uint32_t kBadBound[5] = {0x07230203u, 0x00010000u, 0u, 0u, 0u};

uint32_t layerBit(GN3d::eRENDER_LAYER layer) { return 1u << (uint32_t)layer; }

} // namespace

TEST_CASE(spirvValidationRejectsBadModules) {
    char reason[128];
    CHECK(GN3d::validateSpirvModule(kValidModule, sizeof(kValidModule), reason, sizeof(reason)));
    CHECK(reason[0] == '\0');
    CHECK(!GN3d::validateSpirvModule(0, 0u, reason, sizeof(reason)));
    CHECK(!GN3d::validateSpirvModule(kValidModule, 0u, reason, sizeof(reason)));
    CHECK(!GN3d::validateSpirvModule(kValidModule, 19u, reason, sizeof(reason)));
    CHECK(!GN3d::validateSpirvModule(kValidModule, 6u, reason, sizeof(reason)));
    CHECK(!GN3d::validateSpirvModule(kBadMagic, sizeof(kBadMagic), reason, sizeof(reason)));
    CHECK(!GN3d::validateSpirvModule(kBadVersion, sizeof(kBadVersion), reason, sizeof(reason)));
    CHECK(!GN3d::validateSpirvModule(kBadBound, sizeof(kBadBound), reason, sizeof(reason)));
}

TEST_CASE(spirvBindOverwritesOnlyOnSuccess) {
    GN3d::ResetShaderPrograms();
    char reason[160];
    CHECK(GN3d::RegisterShaderProgramMeta("ext.test", layerBit(GN3d::eLAYER_FORWARD_OPAQUE),
                                          GN3d::eVF_STATIC, GN3d::eSKIN_NONE, 0u,
                                          (uint32_t)GN3d::ePROG_STATE_DECLARED, reason, sizeof(reason)));
    CHECK_EQ(GN3d::ShaderProgramState("ext.test"), (uint32_t)GN3d::ePROG_STATE_DECLARED);
    CHECK(GN3d::BindProgramEntry("ext.test", GN3d::eLAYER_FORWARD_OPAQUE, kValidModule, sizeof(kValidModule),
                                 kValidFrag, sizeof(kValidFrag), 64u, reason, sizeof(reason)));
    const GN3d::sSHADER_PROGRAM *program = GN3d::FindShaderProgram("ext.test");
    CHECK(program != 0);
    CHECK(program->aDesc.aEntries[GN3d::eLAYER_FORWARD_OPAQUE].aVertSpv == kValidModule);
    CHECK_EQ(program->aDesc.aEntries[GN3d::eLAYER_FORWARD_OPAQUE].aFragSize, (uint32_t)sizeof(kValidFrag));
    CHECK(!GN3d::BindProgramEntry("ext.test", GN3d::eLAYER_FORWARD_OPAQUE, 0, 0u, 0, 0u, 64u, reason, sizeof(reason)));
    CHECK(program->aDesc.aEntries[GN3d::eLAYER_FORWARD_OPAQUE].aVertSpv == kValidModule);
    CHECK(!GN3d::BindProgramEntry("ext.test", GN3d::eLAYER_FORWARD_OPAQUE, kBadMagic, sizeof(kBadMagic), 0, 0u, 64u, reason, sizeof(reason)));
    CHECK(program->aDesc.aEntries[GN3d::eLAYER_FORWARD_OPAQUE].aVertSpv == kValidModule);
    CHECK(!GN3d::BindProgramEntry("ext.test", GN3d::eLAYER_SHADOW, kValidModule, sizeof(kValidModule), 0, 0u, 64u, reason, sizeof(reason)));
    CHECK(!GN3d::BindProgramEntry("missing.test", GN3d::eLAYER_FORWARD_OPAQUE, kValidModule, sizeof(kValidModule), 0, 0u, 64u, reason, sizeof(reason)));
}

TEST_CASE(spirvSealRequiresCompleteEntries) {
    GN3d::ResetShaderPrograms();
    char reason[160];
    CHECK(GN3d::RegisterShaderProgramMeta("seal.test", layerBit(GN3d::eLAYER_FORWARD_OPAQUE),
                                          GN3d::eVF_STATIC, GN3d::eSKIN_NONE, 0u,
                                          (uint32_t)GN3d::ePROG_STATE_DECLARED, reason, sizeof(reason)));
    CHECK(!GN3d::SealShaderProgram("seal.test", reason, sizeof(reason)));
    CHECK_EQ(GN3d::ShaderProgramState("seal.test"), (uint32_t)GN3d::ePROG_STATE_DECLARED);
    CHECK(GN3d::BindProgramEntry("seal.test", GN3d::eLAYER_FORWARD_OPAQUE, kValidModule, sizeof(kValidModule), 0, 0u, 64u, reason, sizeof(reason)));
    CHECK(!GN3d::SealShaderProgram("seal.test", reason, sizeof(reason)));
    CHECK_EQ(GN3d::ShaderProgramState("seal.test"), (uint32_t)GN3d::ePROG_STATE_DECLARED);
    CHECK(GN3d::BindProgramEntry("seal.test", GN3d::eLAYER_FORWARD_OPAQUE, kValidModule, sizeof(kValidModule),
                                 kValidFrag, sizeof(kValidFrag), 64u, reason, sizeof(reason)));
    CHECK(GN3d::SealShaderProgram("seal.test", reason, sizeof(reason)));
    CHECK_EQ(GN3d::ShaderProgramState("seal.test"), (uint32_t)GN3d::ePROG_STATE_READY);
    CHECK(!GN3d::SealShaderProgram("missing.test", reason, sizeof(reason)));
}

TEST_CASE(spirvStateAndReset) {
    GN3d::ResetShaderPrograms();
    char reason[160];
    CHECK_EQ(GN3d::ShaderProgramState("none"), 0u);
    CHECK(GN3d::RegisterBuiltinShaderPrograms(reason, sizeof(reason)));
    CHECK_EQ(GN3d::ShaderProgramState("gn.pbr"), (uint32_t)GN3d::ePROG_STATE_DECLARED);
    CHECK_EQ(GN3d::ShaderProgramState("gn.depth"), (uint32_t)GN3d::ePROG_STATE_DECLARED);
    GN3d::ResetShaderPrograms();
    CHECK_EQ(GN3d::ShaderProgramState("gn.pbr"), 0u);
    CHECK_EQ(GN3d::ShaderProgramCount(), 0u);
}
