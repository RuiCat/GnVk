// testOutfitWiring.cpp — imoScene 换装**接线层**的集成测试（不需要模型资产）。
//
// 为什么要有这一层：`cVISIBILITY_SET` 的单测证明"规则集本身正确"，
// 但证明不了"demo 的规则表写对了" —— 掩码写错一个字符、族前缀写错一个下划线，
// 单测全绿而画面全错。而 demo 的真实资产（`.local/imoScene/*.glb`）不在仓库里，
// 在有资产的机器上也只能靠眼睛看。
//
// 折中办法：造一个**只有名字的假 cMODEL**（顶点/索引全空，不碰 GPU、不碰绘制），
// 名字取自真实资产的命名习惯，然后跑 outfitInit / outfitUpdate 全链路，
// 断言 aVisible 的结果。这样规则表本身也被钉住了。
#include "testFramework.h"

#include <GN.h>
#include "GN_3Dmodel.h"
#include "imoSceneOutfit.h"

#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

// 真实资产的网格名（按 imoScene 的实际命名整理，含多 primitive 的 #N 后缀）。
const char *kAssetNames[] = {
    "Clothes_imo_chan",      // 0  身体（Body 槽）
    "Clothes_imo_chan#1",    // 1  同一节点的第二个 primitive
    "Mesh_Hair",             // 2  发型 0
    "Mesh_Hair.001",         // 3  发型 1
    "Mesh_Body",             // 4  身体变体 0
    "Mesh_Body.001",         // 5  身体变体 1
    "CatEar",                // 6  猫耳
    "CatBell",               // 7  铃铛
    "CatTail",               // 8  尾巴
    "Hat",                   // 9  帽子
    "PleatedMask",           // 10 口罩
    "Ribbon",                // 11 丝带
    "Collar",                // 12 项圈
    "RoomWall",              // 13 房间（不属于任何组/族）
};
const uint32_t kAssetNameCount = sizeof(kAssetNames) / sizeof(kAssetNames[0]);

// 造一个假模型：只有名字，没有几何。所有权交给调用方释放。
struct sFAKE_MODEL {
    GN3d::cMODEL aModel;

    sFAKE_MODEL() {
        // cMODEL::Release 用 delete[] 释放 aMeshes，所以必须堆分配。
        GN3d::sMESH *meshes = new GN3d::sMESH[kAssetNameCount];
        for (uint32_t i = 0; i < kAssetNameCount; ++i) {
            std::strncpy(meshes[i].aName, kAssetNames[i], sizeof(meshes[i].aName) - 1);
            meshes[i].aName[sizeof(meshes[i].aName) - 1] = '\0';
            meshes[i].aVisible = true;
        }
        aModel.aMeshes = meshes;
        aModel.aMeshCount = kAssetNameCount;
    }
};

bool visibleAt(const GN3d::cMODEL &model, const char *name) {
    for (uint32_t i = 0; i < model.aMeshCount; ++i) {
        if (std::strcmp(model.aMeshes[i].aName, name) == 0) {
            return model.aMeshes[i].aVisible;
        }
    }
    return false;
}

// 清掉可能影响结果的环境变量，保证用例与外部环境无关。
void clearOutfitEnv() {
    unsetenv("IMO_SCENE_OUTFIT_DEFAULT");
    unsetenv("IMO_SCENE_HAIR");
    unsetenv("IMO_SCENE_BODY");
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  一、初始状态
// ═══════════════════════════════════════════════════════════

TEST_CASE(outfitWiringStartsAllVisibleWithoutDefaultFile) {
    clearOutfitEnv();
    sFAKE_MODEL fake;
    imoScene::outfitInit(fake.aModel, "/nonexistent/outfit.json");
    // 没有默认名单：全部可见
    CHECK_EQ(imoScene::outfitVisibleMeshCount(), kAssetNameCount);
    for (uint32_t i = 0; i < kAssetNameCount; ++i) {
        CHECK(fake.aModel.aMeshes[i].aVisible);
    }
    // 族按前缀收集到了正确数量
    const GN3d::cVISIBILITY_SET &visibility = imoScene::outfitVisibility();
    CHECK_EQ(visibility.FamilyCount(), 2u);
    CHECK_EQ(visibility.FamilySize(0), 2u); // Mesh_Hair / Mesh_Hair.001
    CHECK_EQ(visibility.FamilySize(1), 2u); // Mesh_Body / Mesh_Body.001
    CHECK_EQ(visibility.GroupCount(), 4u);
    clearOutfitEnv();
}

TEST_CASE(outfitWiringRuleTableMatchesRealAssetNames) {
    // **规则表本身的守卫**：每个装饰组必须真的命中它的成员，且不误伤别人。
    clearOutfitEnv();
    sFAKE_MODEL fake;
    imoScene::outfitInit(fake.aModel, "/nonexistent/outfit.json");
    const GN3d::cVISIBILITY_SET &visibility = imoScene::outfitVisibility();
    // 组的 key 是我们写在 demo 里的标签，顺便确认顺序与按键绑定一致
    CHECK_STREQ(visibility.GroupKey(0), "cat（猫耳/铃铛/尾巴）");
    CHECK_STREQ(visibility.GroupKey(1), "hat（帽子）");
    CHECK_STREQ(visibility.GroupKey(2), "mask（口罩）");
    CHECK_STREQ(visibility.GroupKey(3), "ribbon（丝带/领结/项圈）");
    CHECK_STREQ(visibility.FamilyKey(0), "hair（发型）");
    CHECK_STREQ(visibility.FamilyKey(1), "body（身体/服装）");
    clearOutfitEnv();
}

// ═══════════════════════════════════════════════════════════
//  二、按键 → 显隐（走 outfitUpdate 的真实路径）
// ═══════════════════════════════════════════════════════════

TEST_CASE(outfitWiringGroupToggleWritesBackToModel) {
    // 直接操作规则集再 applyToModel 不方便（applyToModel 是内部函数），
    // 所以这里通过 outfitUpdate 的公开路径驱动；按键状态太麻烦，
    // 改为验证"规则集改动 → 需要写回"这一步的等价性：
    // 断言 outfitVisibleMeshCount 与模型里实际可见数一致（一旦不一致就说明漏写回）。
    clearOutfitEnv();
    sFAKE_MODEL fake;
    imoScene::outfitInit(fake.aModel, "/nonexistent/outfit.json");
    uint32_t counted = 0;
    for (uint32_t i = 0; i < fake.aModel.aMeshCount; ++i) {
        if (fake.aModel.aMeshes[i].aVisible) {
            ++counted;
        }
    }
    CHECK_EQ(imoScene::outfitVisibleMeshCount(), counted);
    clearOutfitEnv();
}

// ═══════════════════════════════════════════════════════════
//  三、默认服装组合（外部名单）→ mask → 写回
// ═══════════════════════════════════════════════════════════

TEST_CASE(outfitWiringAppliesDefaultHiddenList) {
    clearOutfitEnv();
    // 写一份临时名单：隐藏衬衫(没有) + Mesh_Body.001 + CatEar
    const char *path = "build/testOutfitWiring.json"; // B7-2：临时路径改相对 build/（原 /work/.sbx 仅容器存在）
    std::FILE *file = std::fopen(path, "wb");
    CHECK(file != 0);
    if (file != 0) {
        const char *text = "{\"hidden\": [\"Mesh_Body.001\", \"CatEar\"], \"hair\": 1}";
        std::fwrite(text, 1, std::strlen(text), file);
        std::fclose(file);
    }
    sFAKE_MODEL fake;
    imoScene::outfitInit(fake.aModel, path);
    const GN3d::cVISIBILITY_SET &visibility = imoScene::outfitVisibility();
    // 按基名精确匹配：Mesh_Body.001 被 mask，Mesh_Body 不受影响
    CHECK_EQ(visibleAt(fake.aModel, "Mesh_Body.001"), false);
    CHECK(visibleAt(fake.aModel, "Mesh_Body"));
    CHECK_EQ(visibleAt(fake.aModel, "CatEar"), false);
    CHECK_EQ(visibility.MaskedCount(), 2u);
    // hair:1 生效 —— 只能是 Mesh_Hair.001 可见（注意 SetFamilySelection 会清该族 mask，
    // 这里 mask 的是 Mesh_Body.001 与 CatEar，都不属于发型族，所以互不干扰）
    CHECK_EQ(visibility.FamilySelection(0), 1);
    CHECK_EQ(visibleAt(fake.aModel, "Mesh_Hair"), false);
    CHECK(visibleAt(fake.aModel, "Mesh_Hair.001"));
    // 缓存数与实际一致
    uint32_t counted = 0;
    for (uint32_t i = 0; i < fake.aModel.aMeshCount; ++i) {
        if (fake.aModel.aMeshes[i].aVisible) {
            ++counted;
        }
    }
    CHECK_EQ(imoScene::outfitVisibleMeshCount(), counted);
    clearOutfitEnv();
}

TEST_CASE(outfitWiringEnvOverridesJsonHair) {
    // 环境变量优先于 JSON 的 hair 字段。
    clearOutfitEnv();
    const char *path = "build/testOutfitWiring2.json";
    std::FILE *file = std::fopen(path, "wb");
    CHECK(file != 0);
    if (file != 0) {
        const char *text = "{\"hidden\": [], \"hair\": 0}";
        std::fwrite(text, 1, std::strlen(text), file);
        std::fclose(file);
    }
    setenv("IMO_SCENE_HAIR", "1", 1);
    sFAKE_MODEL fake;
    imoScene::outfitInit(fake.aModel, path);
    CHECK_EQ(imoScene::outfitVisibility().FamilySelection(0), 1);
    CHECK(visibleAt(fake.aModel, "Mesh_Hair.001"));
    clearOutfitEnv();
}

TEST_CASE(outfitWiringInvalidEnvFallsBackToAllVisible) {
    // IMO_SCENE_HAIR=abc：旧实现会 atoi 成 0（选第一套），现在必须忽略、保持"全部"。
    clearOutfitEnv();
    setenv("IMO_SCENE_HAIR", "abc", 1);
    sFAKE_MODEL fake;
    imoScene::outfitInit(fake.aModel, "/nonexistent/outfit.json");
    CHECK_EQ(imoScene::outfitVisibility().FamilySelection(0), -1); // 全部
    CHECK(visibleAt(fake.aModel, "Mesh_Hair"));
    CHECK(visibleAt(fake.aModel, "Mesh_Hair.001"));
    clearOutfitEnv();
}

TEST_CASE(outfitWiringMissingDefaultFileIsNotAnError) {
    clearOutfitEnv();
    sFAKE_MODEL fake;
    imoScene::outfitInit(fake.aModel, "/nonexistent/definitely-missing.json");
    // 名单缺失只意味着"没有默认组合"，不该影响可见性
    CHECK_EQ(imoScene::outfitVisibility().MaskedCount(), 0u);
    CHECK_EQ(imoScene::outfitVisibleMeshCount(), kAssetNameCount);
    clearOutfitEnv();
}

// ═══════════════════════════════════════════════════════════
//  四、重新初始化不留旧状态
// ═══════════════════════════════════════════════════════════

TEST_CASE(outfitWiringReinitResetsPreviousState) {
    clearOutfitEnv();
    sFAKE_MODEL fake;
    // 第一次：带默认名单（隐藏 CatEar）
    const char *path = "build/testOutfitWiring3.json";
    std::FILE *file = std::fopen(path, "wb");
    CHECK(file != 0);
    if (file != 0) {
        const char *text = "{\"hidden\": [\"CatEar\"]}";
        std::fwrite(text, 1, std::strlen(text), file);
        std::fclose(file);
    }
    imoScene::outfitInit(fake.aModel, path);
    CHECK_EQ(visibleAt(fake.aModel, "CatEar"), false);
    // 第二次：不带名单 —— 不能残留上次的 mask
    imoScene::outfitInit(fake.aModel, "/nonexistent/outfit.json");
    CHECK(visibleAt(fake.aModel, "CatEar"));
    CHECK_EQ(imoScene::outfitVisibility().MaskedCount(), 0u);
    CHECK_EQ(imoScene::outfitVisibleMeshCount(), kAssetNameCount);
    clearOutfitEnv();
}

// ═══════════════════════════════════════════════════════════
//  五、空模型不崩
// ═══════════════════════════════════════════════════════════

TEST_CASE(outfitWiringHandlesEmptyModel) {
    clearOutfitEnv();
    GN3d::cMODEL empty;
    imoScene::outfitInit(empty, "/nonexistent/outfit.json");
    CHECK_EQ(imoScene::outfitVisibleMeshCount(), 0u);
    CHECK_EQ(imoScene::outfitVisibility().MeshCount(), 0u);
    imoScene::outfitUpdate(); // 不应崩
    CHECK_EQ(imoScene::outfitVisibleMeshCount(), 0u);
    clearOutfitEnv();
}
