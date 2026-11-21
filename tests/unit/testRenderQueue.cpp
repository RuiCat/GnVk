// testRenderQueue.cpp — P3 CPU 层队列：入队校验、层掩码过滤、层内排序、容量/空参保护。
#include <GN.h>
#include "GN_RenderPipeline.h"
#include "GN_RenderQueue.h"
#include "testFramework.h"

#include <cstring>

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
    if(vert) { entry.aVertSpv = gVert; entry.aVertSize = sizeof(gVert); }
    if(frag) { entry.aFragSpv = gFrag; entry.aFragSize = sizeof(gFrag); }
}

sSHADER_PROGRAM_DESC makeProgramDesc(uint32_t mask) {
    sSHADER_PROGRAM_DESC desc;
    std::memset(&desc, 0, sizeof(desc));
    desc.aName = "gn.test";
    desc.aLayerMask = mask;
    desc.aVertexFormat = eVF_STATIC;
    desc.aSkinData = eSKIN_NONE;
    desc.aLayoutVersion = kBindingLayoutVersion;
    for(uint32_t i = 0; i < (uint32_t)eLAYER_COUNT; ++i) initEntry(desc.aEntries[i], false, false);
    if(mask & (1u << (uint32_t)eLAYER_FORWARD_OPAQUE)) initEntry(desc.aEntries[eLAYER_FORWARD_OPAQUE], true, true);
    if(mask & (1u << (uint32_t)eLAYER_FORWARD_ALPHA)) initEntry(desc.aEntries[eLAYER_FORWARD_ALPHA], true, true);
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

} // namespace

TEST_CASE(renderQueueSubmitValidates) {
    sRENDER_QUEUE queue;
    resetRenderQueue(&queue);
    CHECK_EQ(renderQueueCount(&queue), 0u);
    char reason[128];
    sSHADER_PROGRAM program = makeProgram(1, 1u << (uint32_t)eLAYER_FORWARD_OPAQUE);
    sMATERIAL_INSTANCE material = makeMaterial(&program);
    sRENDER_ITEM bad = makeItem(&material, 0.0f);
    bad.aMaterial = 0;
    CHECK(!submitRenderItem(&queue, bad, reason, sizeof(reason)));
    CHECK(reason[0] != '\0');
    CHECK(!submitRenderItem(0, makeItem(&material, 0.0f), reason, sizeof(reason)));
    CHECK(submitRenderItem(&queue, makeItem(&material, 0.0f), reason, sizeof(reason)));
    CHECK_EQ(renderQueueCount(&queue), 1u);
    resetRenderQueue(&queue);
    CHECK_EQ(renderQueueCount(&queue), 0u);
}

TEST_CASE(renderQueueCollectSortsOpaque) {
    sSHADER_PROGRAM program = makeProgram(1, 1u << (uint32_t)eLAYER_FORWARD_OPAQUE);
    sMATERIAL_INSTANCE material = makeMaterial(&program);
    sRENDER_QUEUE queue;
    resetRenderQueue(&queue);
    char reason[128];
    CHECK(submitRenderItem(&queue, makeItem(&material, 5.0f), reason, sizeof(reason)));
    CHECK(submitRenderItem(&queue, makeItem(&material, -2.0f), reason, sizeof(reason)));
    CHECK(submitRenderItem(&queue, makeItem(&material, 1.0f), reason, sizeof(reason)));
    sRENDER_ITEM out[8];
    uint32_t count = 0;
    GNg::sMAT44 view(true);
    CHECK(collectRenderLayer(&queue, eLAYER_FORWARD_OPAQUE, view, out, 8u, &count, reason, sizeof(reason)));
    CHECK_EQ(count, 3u);
    CHECK_NEAR(out[0].aModelMatrix.m[14], -2.0f, 1e-6);
    CHECK_NEAR(out[1].aModelMatrix.m[14], 1.0f, 1e-6);
    CHECK_NEAR(out[2].aModelMatrix.m[14], 5.0f, 1e-6);
}

TEST_CASE(renderQueueCollectRespectsLayerMask) {
    sSHADER_PROGRAM program = makeProgram(1, (1u << (uint32_t)eLAYER_FORWARD_OPAQUE) |
                                             (1u << (uint32_t)eLAYER_FORWARD_ALPHA));
    sMATERIAL_INSTANCE material = makeMaterial(&program);
    sRENDER_QUEUE queue;
    resetRenderQueue(&queue);
    char reason[128];
    sRENDER_ITEM opaqueOnly = makeItem(&material, 0.0f);
    opaqueOnly.aLayerMask = 1u << (uint32_t)eLAYER_FORWARD_OPAQUE;
    CHECK(submitRenderItem(&queue, opaqueOnly, reason, sizeof(reason)));
    CHECK(submitRenderItem(&queue, makeItem(&material, 0.0f), reason, sizeof(reason)));
    CHECK_EQ(countRenderLayer(&queue, eLAYER_FORWARD_OPAQUE), 2u);
    CHECK_EQ(countRenderLayer(&queue, eLAYER_FORWARD_ALPHA), 1u);
    CHECK_EQ(countRenderLayer(&queue, eLAYER_OUTLINE), 0u);
    sRENDER_ITEM out[8];
    uint32_t count = 0;
    GNg::sMAT44 view(true);
    CHECK(collectRenderLayer(&queue, eLAYER_FORWARD_ALPHA, view, out, 8u, &count, reason, sizeof(reason)));
    CHECK_EQ(count, 1u);
}

TEST_CASE(renderQueueCapacityAndNullGuards) {
    sSHADER_PROGRAM program = makeProgram(1, 1u << (uint32_t)eLAYER_FORWARD_OPAQUE);
    sMATERIAL_INSTANCE material = makeMaterial(&program);
    sRENDER_QUEUE queue;
    resetRenderQueue(&queue);
    char reason[128];
    CHECK(submitRenderItem(&queue, makeItem(&material, 0.0f), reason, sizeof(reason)));
    CHECK(submitRenderItem(&queue, makeItem(&material, 1.0f), reason, sizeof(reason)));
    sRENDER_ITEM out[1];
    uint32_t count = 7;
    GNg::sMAT44 view(true);
    CHECK(!collectRenderLayer(&queue, eLAYER_FORWARD_OPAQUE, view, out, 1u, &count, reason, sizeof(reason)));
    CHECK(reason[0] != '\0');
    CHECK(!collectRenderLayer(0, eLAYER_FORWARD_OPAQUE, view, out, 1u, &count, reason, sizeof(reason)));
    CHECK(!collectRenderLayer(&queue, eLAYER_FORWARD_OPAQUE, view, 0, 1u, &count, reason, sizeof(reason)));
    CHECK(!collectRenderLayer(&queue, eLAYER_FORWARD_OPAQUE, view, out, 1u, 0, reason, sizeof(reason)));
    CHECK_EQ(renderQueueCount(0), 0u);
    CHECK_EQ(countRenderLayer(0, eLAYER_FORWARD_OPAQUE), 0u);
    resetRenderQueue(0);
}
