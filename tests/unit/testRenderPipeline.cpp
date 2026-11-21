// testRenderPipeline.cpp — 多层渲染 CPU 契约：层掩码/入口解析、程序与渲染项校验、
// 层内稳定排序。全部纯 CPU，不依赖 GPU/Vulkan。
#include <GN.h>
#include "GN_RenderPipeline.h"
#include "testFramework.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace {

using namespace GN3d;

unsigned char gVert[8] = {3, 2, 0x23, 7, 0, 0, 0, 0};
unsigned char gFrag[8] = {3, 2, 0x23, 7, 1, 0, 0, 0};

void initEntry(sSHADER_ENTRY &entry, bool vert, bool frag) {
    std::memset(&entry, 0, sizeof(entry));
    entry.aBlend = -1;
    entry.aDepthTest = -1;
    entry.aDepthWrite = -1;
    entry.aCullBack = -1;
    entry.aTopology = -1;
    if (vert) {
        entry.aVertSpv = gVert;
        entry.aVertSize = sizeof(gVert);
    }
    if (frag) {
        entry.aFragSpv = gFrag;
        entry.aFragSize = sizeof(gFrag);
    }
}

sSHADER_PROGRAM_DESC makeProgramDesc(uint32_t mask) {
    sSHADER_PROGRAM_DESC desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.aName = "gn.test";
    desc.aLayerMask = mask;
    desc.aVertexFormat = eVF_STATIC;
    desc.aSkinData = eSKIN_NONE;
    desc.aLayoutVersion = kBindingLayoutVersion;
    for (uint32_t i = 0; i < (uint32_t)eLAYER_COUNT; ++i) {
        initEntry(desc.aEntries[i], false, false);
    }
    if (mask & (1u << eLAYER_SHADOW)) initEntry(desc.aEntries[eLAYER_SHADOW], true, false);
    if (mask & (1u << eLAYER_FORWARD_OPAQUE)) {
        initEntry(desc.aEntries[eLAYER_FORWARD_OPAQUE], true, true);
    }
    if (mask & (1u << eLAYER_FORWARD_ALPHA)) {
        initEntry(desc.aEntries[eLAYER_FORWARD_ALPHA], true, true);
    }
    return desc;
}

sSHADER_PROGRAM makeProgram(uint32_t id, uint32_t mask) {
    sSHADER_PROGRAM program;
    std::memset(&program, 0, sizeof(program));
    program.aDesc = makeProgramDesc(mask);
    program.aId = id;
    return program;
}

sMATERIAL_INSTANCE makeMaterial(const sSHADER_PROGRAM *program) {
    sMATERIAL_INSTANCE material;
    std::memset(&material, 0, sizeof(material));
    material.aProgram = program;
    return material;
}

sRENDER_ITEM makeItem(const sMATERIAL_INSTANCE *material, float z) {
    sRENDER_ITEM item;
    std::memset(&item, 0, sizeof(item));
    item.aVertices = gVert;
    item.aVertexCount = 2;
    item.aModelMatrix = GNg::sMAT44(true);
    item.aModelMatrix.m[14] = z;
    item.aMaterial = material;
    return item;
}

TEST_CASE(renderPipelineLayerMaskFallback) {
    sSHADER_PROGRAM program = makeProgram(1, (1u << eLAYER_SHADOW) | (1u << eLAYER_FORWARD_OPAQUE));
    sMATERIAL_INSTANCE material = makeMaterial(&program);
    sRENDER_ITEM item = makeItem(&material, 0.0f);

    CHECK_EQ(ResolveLayerMask(item), program.aDesc.aLayerMask);

    material.aLayerMask = 1u << eLAYER_FORWARD_ALPHA;
    CHECK_EQ(ResolveLayerMask(item), 1u << eLAYER_FORWARD_ALPHA);

    item.aLayerMask = 1u << eLAYER_OUTLINE;
    CHECK_EQ(ResolveLayerMask(item), 1u << eLAYER_OUTLINE);

    item.aMaterial = 0;
    CHECK_EQ(ResolveLayerMask(item), 0u);
}

TEST_CASE(renderPipelineResolveEntryAndOverride) {
    sSHADER_PROGRAM base = makeProgram(1, (1u << eLAYER_FORWARD_OPAQUE));
    sSHADER_PROGRAM overrideProgram =
        makeProgram(2, (1u << eLAYER_FORWARD_OPAQUE) | (1u << eLAYER_OUTLINE));
    initEntry(overrideProgram.aDesc.aEntries[eLAYER_OUTLINE], true, true);
    sMATERIAL_INSTANCE material = makeMaterial(&base);
    sRENDER_ITEM item = makeItem(&material, 0.0f);

    const sSHADER_ENTRY *baseEntry = ResolveEntry(item, eLAYER_FORWARD_OPAQUE);
    CHECK(baseEntry != 0);
    CHECK(baseEntry == &base.aDesc.aEntries[eLAYER_FORWARD_OPAQUE]);

    item.aLayerProgram[eLAYER_FORWARD_OPAQUE] = &overrideProgram;
    const sSHADER_ENTRY *overrideEntry = ResolveEntry(item, eLAYER_FORWARD_OPAQUE);
    CHECK(overrideEntry == &overrideProgram.aDesc.aEntries[eLAYER_FORWARD_OPAQUE]);

    CHECK(ResolveEntry(item, eLAYER_GBUFFER) == 0);
}

TEST_CASE(renderPipelineImplicitShadowFallback) {
    sSHADER_PROGRAM mainProgram = makeProgram(1, 1u << eLAYER_FORWARD_OPAQUE);
    mainProgram.aDesc.aFlags = ePROG_IMPLICIT_SHADOW;
    sSHADER_PROGRAM depthProgram = makeProgram(2, 1u << eLAYER_SHADOW);
    sMATERIAL_INSTANCE material = makeMaterial(&mainProgram);
    sRENDER_ITEM item = makeItem(&material, 0.0f);

    CHECK(ResolveEntry(item, eLAYER_SHADOW) == 0);
    const sSHADER_ENTRY *fallback = ResolveEntry(item, eLAYER_SHADOW, &depthProgram);
    CHECK(fallback == &depthProgram.aDesc.aEntries[eLAYER_SHADOW]);

    mainProgram.aDesc.aFlags = 0;
    CHECK(ResolveEntry(item, eLAYER_SHADOW, &depthProgram) == 0);
}

TEST_CASE(renderPipelineProgramValidation) {
    char reason[128];
    sSHADER_PROGRAM_DESC valid = makeProgramDesc((1u << eLAYER_SHADOW) |
                                                 (1u << eLAYER_FORWARD_OPAQUE));
    CHECK(ValidateShaderProgramDesc(valid, reason, sizeof(reason)));

    sSHADER_PROGRAM_DESC bad = valid;
    bad.aLayerMask |= 1u << eLAYER_GBUFFER; // 没有入口
    CHECK(!ValidateShaderProgramDesc(bad, reason, sizeof(reason)));

    bad = valid;
    initEntry(bad.aEntries[eLAYER_FORWARD_OPAQUE], true, false); // 非阴影缺 FS
    CHECK(!ValidateShaderProgramDesc(bad, reason, sizeof(reason)));

    bad = valid;
    bad.aLayoutVersion = kBindingLayoutVersion + 1u;
    CHECK(!ValidateShaderProgramDesc(bad, reason, sizeof(reason)));

    bad = valid;
    bad.aVertexFormat = eVF_SKINNED;
    bad.aSkinData = eSKIN_NONE;
    CHECK(!ValidateShaderProgramDesc(bad, reason, sizeof(reason)));

    bad = valid;
    bad.aFlags = ePROG_IMPLICIT_SHADOW; // 同时有 SHADOW 入口，冲突
    CHECK(!ValidateShaderProgramDesc(bad, reason, sizeof(reason)));

    bad = valid;
    bad.aEntries[eLAYER_FORWARD_OPAQUE].aPushConstantSize = 132u;
    CHECK(!ValidateShaderProgramDesc(bad, reason, sizeof(reason)));
}

TEST_CASE(renderPipelineRenderItemValidation) {
    sSHADER_PROGRAM program = makeProgram(1, 1u << eLAYER_FORWARD_OPAQUE);
    sMATERIAL_INSTANCE material = makeMaterial(&program);
    sRENDER_ITEM valid = makeItem(&material, 0.0f);
    char reason[128];
    CHECK(ValidateRenderItem(valid, reason, sizeof(reason)));

    sRENDER_ITEM bad = valid;
    bad.aMaterial = 0;
    CHECK(!ValidateRenderItem(bad, reason, sizeof(reason)));

    bad = valid;
    bad.aVertices = 0;
    CHECK(!ValidateRenderItem(bad, reason, sizeof(reason)));

    bad = valid;
    bad.aModelMatrix.m[0] = std::nanf("");
    CHECK(!ValidateRenderItem(bad, reason, sizeof(reason)));
}

TEST_CASE(renderPipelineSortOpaqueFrontToBack) {
    sSHADER_PROGRAM program = makeProgram(1, 1u << eLAYER_FORWARD_OPAQUE);
    sMATERIAL_INSTANCE material = makeMaterial(&program);
    sRENDER_ITEM items[3] = {makeItem(&material, 5.0f), makeItem(&material, -2.0f),
                             makeItem(&material, 1.0f)};
    items[0].aSortKey = 9;
    items[1].aSortKey = 8;
    items[2].aSortKey = 7;
    GNg::sMAT44 view(true);
    SortRenderItems(items, 3, eLAYER_FORWARD_OPAQUE, view);
    CHECK_NEAR(items[0].aModelMatrix.m[14], -2.0f, 1e-6);
    CHECK_NEAR(items[1].aModelMatrix.m[14], 1.0f, 1e-6);
    CHECK_NEAR(items[2].aModelMatrix.m[14], 5.0f, 1e-6);
}

TEST_CASE(renderPipelineSortProgramThenAlphaBackToFront) {
    sSHADER_PROGRAM programA = makeProgram(2, 1u << eLAYER_FORWARD_ALPHA);
    sSHADER_PROGRAM programB = makeProgram(1, 1u << eLAYER_FORWARD_ALPHA);
    sMATERIAL_INSTANCE materialA = makeMaterial(&programA);
    sMATERIAL_INSTANCE materialB = makeMaterial(&programB);
    sRENDER_ITEM items[3] = {makeItem(&materialA, 1.0f), makeItem(&materialB, -3.0f),
                             makeItem(&materialB, 4.0f)};
    GNg::sMAT44 view(true);
    SortRenderItems(items, 3, eLAYER_FORWARD_ALPHA, view);
    CHECK_EQ(items[0].aMaterial, &materialB);
    CHECK_NEAR(items[0].aModelMatrix.m[14], 4.0f, 1e-6);
    CHECK_NEAR(items[1].aModelMatrix.m[14], -3.0f, 1e-6);
    CHECK_EQ(items[2].aMaterial, &materialA);
}

TEST_CASE(renderPipelineSortStableOnTie) {
    sSHADER_PROGRAM program = makeProgram(1, 1u << eLAYER_FORWARD_OPAQUE);
    sMATERIAL_INSTANCE material = makeMaterial(&program);
    sRENDER_ITEM items[3] = {makeItem(&material, 1.0f), makeItem(&material, 1.0f),
                             makeItem(&material, 1.0f)};
    items[0].aSortKey = 3;
    items[1].aSortKey = 1;
    items[2].aSortKey = 2;
    GNg::sMAT44 view(true);
    SortRenderItems(items, 3, eLAYER_FORWARD_OPAQUE, view);
    CHECK_EQ(items[0].aSortKey, 1u);
    CHECK_EQ(items[1].aSortKey, 2u);
    CHECK_EQ(items[2].aSortKey, 3u);
}

} // namespace
