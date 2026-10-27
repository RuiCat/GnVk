// testMaterial.cpp — GPU 材质实例 CPU 侧：描述校验、内容键/相等、去重表。
#include "testFramework.h"

#include <GN.h>
#include "GN_Material.h"

#include <cstdint>

using namespace GN3d;

namespace {

sMATERIAL_DESC makeDesc() {
    sMATERIAL_DESC desc;
    desc.aPipelineKey = 0x1234567890ABCDEFull;
    desc.aBindingCount = 2;
    desc.aBindings[0].aBinding = 0;
    desc.aBindings[0].aKind = eMAT_BIND_UNIFORM_BUFFER;
    desc.aBindings[0].aStageFlags = 0x10; // 任意非零阶段位掩码
    desc.aBindings[0].aResource = (const void *)0x1000;
    desc.aBindings[1].aBinding = 1;
    desc.aBindings[1].aKind = eMAT_BIND_COMBINED_IMAGE_SAMPLER;
    desc.aBindings[1].aStageFlags = 0x20;
    desc.aBindings[1].aResource = (const void *)0x2000;
    return desc;
}

} // namespace

// 校验：管线键、绑定数、递增/去重、资源非空、种类与阶段位。
TEST_CASE(materialValidation) {
    CHECK(ValidateMaterialDesc(makeDesc()));

    sMATERIAL_DESC desc = makeDesc();
    desc.aPipelineKey = 0;
    CHECK(!ValidateMaterialDesc(desc));

    desc = makeDesc();
    desc.aBindingCount = 9;
    CHECK(!ValidateMaterialDesc(desc));

    desc = makeDesc();
    desc.aBindings[1].aBinding = 0; // 与第 0 个重复
    CHECK(!ValidateMaterialDesc(desc));

    desc = makeDesc();
    desc.aBindings[0].aBinding = 2; // 非递增
    CHECK(!ValidateMaterialDesc(desc));

    desc = makeDesc();
    desc.aBindings[0].aResource = 0;
    CHECK(!ValidateMaterialDesc(desc));

    desc = makeDesc();
    desc.aBindings[0].aKind = (eMATERIAL_BINDING_KIND)99;
    CHECK(!ValidateMaterialDesc(desc));

    desc = makeDesc();
    desc.aBindings[0].aStageFlags = 0;
    CHECK(!ValidateMaterialDesc(desc));
}

// 键与相等：内容相同键相同；资源/管线键/种类变化则键变化且不相等；非法描述不相等。
TEST_CASE(materialKeyAndEquality) {
    const sMATERIAL_DESC base = makeDesc();
    const sMATERIAL_DESC same = makeDesc();
    CHECK_EQ(ComputeMaterialKey(base), ComputeMaterialKey(same));
    CHECK(MaterialDescEqual(base, same));

    sMATERIAL_DESC changedResource = makeDesc();
    changedResource.aBindings[1].aResource = (const void *)0x3000;
    CHECK(ComputeMaterialKey(changedResource) != ComputeMaterialKey(base));
    CHECK(!MaterialDescEqual(base, changedResource));

    sMATERIAL_DESC changedPipeline = makeDesc();
    changedPipeline.aPipelineKey = base.aPipelineKey + 1ull;
    CHECK(ComputeMaterialKey(changedPipeline) != ComputeMaterialKey(base));

    sMATERIAL_DESC changedKind = makeDesc();
    changedKind.aBindings[1].aKind = eMAT_BIND_UNIFORM_BUFFER;
    CHECK(ComputeMaterialKey(changedKind) != ComputeMaterialKey(base));

    sMATERIAL_DESC invalid = makeDesc();
    invalid.aPipelineKey = 0;
    CHECK(!MaterialDescEqual(base, invalid));
}

// 去重表：同描述返回同句柄；资源不同新建；非法拒绝；Release 清空。
TEST_CASE(materialCacheDedup) {
    cMATERIAL_CACHE cache;
    const sMATERIAL_DESC desc = makeDesc();
    const uint32_t handleA = cache.GetOrCreate(desc);
    CHECK(handleA != cMATERIAL_CACHE::INVALID_HANDLE);
    const uint32_t handleB = cache.GetOrCreate(desc);
    CHECK_EQ(handleA, handleB);
    CHECK_EQ(cache.GetCount(), 1u);

    const sMATERIAL_RES *result = cache.Get(handleA);
    CHECK(result != 0);
    if(result) {
        CHECK_EQ(result->aKey, ComputeMaterialKey(desc));
        CHECK(result->aDescriptorSet == 0); // GPU 句柄留 P8
    }

    sMATERIAL_DESC changed = desc;
    changed.aBindings[0].aResource = (const void *)0x4000;
    const uint32_t handleC = cache.GetOrCreate(changed);
    CHECK(handleC != cMATERIAL_CACHE::INVALID_HANDLE);
    CHECK(handleC != handleA);
    CHECK_EQ(cache.GetCount(), 2u);

    sMATERIAL_DESC invalid = desc;
    invalid.aPipelineKey = 0;
    CHECK(cache.GetOrCreate(invalid) == cMATERIAL_CACHE::INVALID_HANDLE);
    CHECK_EQ(cache.GetCount(), 2u);
    CHECK(cache.Get(999u) == 0);

    cache.Release();
    CHECK_EQ(cache.GetCount(), 0u);
    CHECK(cache.Get(handleA) == 0);
}
