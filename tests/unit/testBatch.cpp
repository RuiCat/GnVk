// testBatch.cpp — P9-c 批处理：稳定排序键、稳定性、序号保留与状态切换统计。
#include "testFramework.h"

#include <GN.h>
#include "GN_3Dbatch.h"

#include <cstring>
#include <vector>

using namespace GN3d;

namespace {

sBATCH_DRAW_ITEM makeItem(uint64_t pipelineKey, uint64_t materialKey, float depth,
                          uint32_t order) {
    sBATCH_DRAW_ITEM item;
    std::memset(&item, 0, sizeof(item));
    item.aModel = (const void *)(size_t)(order + 1u);
    item.aPipelineKey = pipelineKey;
    item.aMaterialKey = materialKey;
    item.aDepth = depth;
    item.aOrder = order;
    return item;
}

} // namespace

// 空指针 / 0 / 1 个元素：安全且不改数据。
TEST_CASE(batchSortGuards) {
    sortBatchItems(0, 0);
    sortBatchItems(0, 5); // 空指针安全

    sBATCH_DRAW_ITEM single = makeItem(3, 4, 1.0f, 7);
    const sBATCH_DRAW_ITEM copy = single;
    sortBatchItems(&single, 1);
    CHECK(std::memcmp(&single, &copy, sizeof(single)) == 0);
    CHECK_EQ(countBatchSwitches(&single, 1), 0u);
    CHECK_EQ(countBatchSwitches(0, 3), 0u);
}

// 排序键：pipelineKey → materialKey → depth → order。
TEST_CASE(batchSortKeys) {
    sBATCH_DRAW_ITEM items[4] = {
        makeItem(2, 1, 0.5f, 0),
        makeItem(1, 9, 2.0f, 1),
        makeItem(1, 3, 3.0f, 2),
        makeItem(1, 3, 1.0f, 3),
    };
    sortBatchItems(items, 4);
    CHECK_EQ(items[0].aPipelineKey, 1u);
    CHECK_EQ(items[0].aMaterialKey, 3u);
    CHECK_NEAR(items[0].aDepth, 1.0f, 1e-6);
    CHECK_EQ(items[1].aOrder, 2u); // 同管线同材质，深度 1.0 在前
    CHECK_EQ(items[2].aMaterialKey, 9u);
    CHECK_EQ(items[3].aPipelineKey, 2u);
}

// 稳定：同键同深度按 aOrder 升序（不依赖输入顺序）。
TEST_CASE(batchSortStable) {
    sBATCH_DRAW_ITEM items[3] = {
        makeItem(1, 1, 1.0f, 9),
        makeItem(1, 1, 1.0f, 4),
        makeItem(1, 1, 1.0f, 6),
    };
    sortBatchItems(items, 3);
    CHECK_EQ(items[0].aOrder, 4u);
    CHECK_EQ(items[1].aOrder, 6u);
    CHECK_EQ(items[2].aOrder, 9u);
    CHECK_EQ(countBatchSwitches(items, 3), 0u); // 全同键
}

// 排序不改模型句柄与序号集合（只重排）。
TEST_CASE(batchSortPreservesPayload) {
    sBATCH_DRAW_ITEM items[4] = {
        makeItem(2, 0, 0.0f, 0), makeItem(1, 0, 0.0f, 1),
        makeItem(2, 0, 0.0f, 2), makeItem(1, 0, 0.0f, 3),
    };
    const void *models[4] = {items[0].aModel, items[1].aModel, items[2].aModel, items[3].aModel};
    sortBatchItems(items, 4);
    bool allFound = true;
    for(int i = 0; i < 4; ++i) {
        bool found = false;
        for(int j = 0; j < 4; ++j) {
            if(items[j].aModel == models[i]) found = true;
        }
        if(!found) allFound = false;
    }
    CHECK(allFound);
    CHECK_EQ(items[0].aPipelineKey, 1u);
    CHECK_EQ(items[3].aPipelineKey, 2u);
}

// 切换计数：交替 = 每步都切；同键连续 = 0 次；排序后切换数下降。
TEST_CASE(batchSwitchCounting) {
    sBATCH_DRAW_ITEM items[4] = {
        makeItem(1, 1, 0.0f, 0), makeItem(1, 2, 0.0f, 1),
        makeItem(1, 1, 0.0f, 2), makeItem(1, 2, 0.0f, 3),
    };
    CHECK_EQ(countBatchSwitches(items, 4), 3u); // 1→2→1→2
    sortBatchItems(items, 4);
    CHECK_EQ(countBatchSwitches(items, 4), 1u); // 1,1,2,2

    sBATCH_DRAW_ITEM same[2] = {makeItem(5, 7, 0.0f, 0), makeItem(5, 7, 0.0f, 1)};
    CHECK_EQ(countBatchSwitches(same, 2), 0u);

    sBATCH_DRAW_ITEM pipelines[2] = {makeItem(1, 7, 0.0f, 0), makeItem(2, 7, 0.0f, 1)};
    CHECK_EQ(countBatchSwitches(pipelines, 2), 1u); // 只有管线键变化也算切换
}
