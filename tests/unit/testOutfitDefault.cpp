// testOutfitDefault.cpp — imoScene 默认服装组合的外部名单解析 + 整数环境变量解析。
//
// 这两个函数原本埋在 imoSceneOutfit.cpp 的匿名命名空间里，各带一个**静默错误**：
//
//   1. `"hair"` 用 `text.find("\"hair\"")` **全文件搜索**：
//      如果 `"hidden"` 名单里恰好有一个叫 `hair` 的槽位（完全可能 ——
//      "Clothes_imo_chan" 这种命名体系里出现 hair 很正常），
//      字段会被误判成存在，然后从那个字符串后面的冒号开始 atoi，读到垃圾。
//
//   2. 环境变量用 `std::atoi`：`IMO_SCENE_HAIR=abc` 得到 0 ——
//      用户想表达"全部显示"（-1），实际变成"选第一套"，而且没有任何提示。
//
// 这里把两处都钉住：正确输入要解析对，错误输入要**明确拒绝**而不是给个看似合理的值。
#include "testFramework.h"

#include <GN.h>
#include "imoSceneOutfit.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

// B7-2 顺手修（环境阻塞）：原硬编码 /work/.sbx/tmp 只在 sbxd 容器存在，宿主上 fopen 必败。
// 临时产物落 build/（make unit 先 mkdir -p build；两种环境 cwd 都是仓库根）。
const char *kTempPath = "build/testOutfitDefault.json";

bool writeTempFile(const char *text) {
    std::FILE *file = std::fopen(kTempPath, "wb");
    if (file == 0) {
        return false;
    }
    const size_t length = std::strlen(text);
    const size_t written = std::fwrite(text, 1, length, file);
    std::fclose(file);
    return written == length;
}

} // namespace

// ═══════════════════════════════════════════════════════════
//  一、JSON 名单解析
// ═══════════════════════════════════════════════════════════

TEST_CASE(outfitDefaultParsesHiddenListAndHair) {
    const char *text =
        "{\n"
        "  \"hidden\": [\"shirt\", \"coat\", \"inner\"],\n"
        "  \"hair\": 5\n"
        "}\n";
    imoScene::sOUTFIT_DEFAULT out;
    CHECK(imoScene::parseOutfitDefaultText(text, out));
    CHECK_EQ(out.aHidden.size(), 3u);
    CHECK_STREQ(out.aHidden[0].c_str(), "shirt");
    CHECK_STREQ(out.aHidden[1].c_str(), "coat");
    CHECK_STREQ(out.aHidden[2].c_str(), "inner");
    CHECK_EQ(out.aHair, 5);
}

TEST_CASE(outfitDefaultHairDefaultsToUnspecified) {
    imoScene::sOUTFIT_DEFAULT out;
    CHECK(imoScene::parseOutfitDefaultText("{\"hidden\": [\"a\"]}", out));
    CHECK_EQ(out.aHidden.size(), 1u);
    CHECK_EQ(out.aHair, -1); // 没写就是"未指定"
}

TEST_CASE(outfitDefaultRequiresHiddenKey) {
    imoScene::sOUTFIT_DEFAULT out;
    CHECK_EQ(imoScene::parseOutfitDefaultText("{\"hair\": 3}", out), false);
    CHECK_EQ(out.aHidden.size(), 0u);
    CHECK_EQ(imoScene::parseOutfitDefaultText("", out), false);
    CHECK_EQ(imoScene::parseOutfitDefaultText("not json at all", out), false);
}

TEST_CASE(outfitDefaultRejectsUnterminatedArray) {
    // 有 "hidden" 但有 '[' 没 ']'：不能瞎猜，返回 false。
    imoScene::sOUTFIT_DEFAULT out;
    CHECK_EQ(imoScene::parseOutfitDefaultText("{\"hidden\": [\"a\", \"b\"", out), false);
    // 有 "hidden" 但根本没有 '['：同样 false
    CHECK_EQ(imoScene::parseOutfitDefaultText("{\"hidden\": \"a\"}", out), false);
}

TEST_CASE(outfitDefaultHairSearchedOnlyAfterHiddenArray) {
    // **回归守卫**：名单里出现 "hair" 这个字符串，不能被当成字段名。
    // 旧实现全文件 find("\"hair\"")，会命中名单里的这一项。
    const char *text =
        "{\n"
        "  \"hidden\": [\"hair\", \"shirt\"],\n"
        "  \"hair\": 7\n"
        "}\n";
    imoScene::sOUTFIT_DEFAULT out;
    CHECK(imoScene::parseOutfitDefaultText(text, out));
    CHECK_EQ(out.aHidden.size(), 2u);
    CHECK_STREQ(out.aHidden[0].c_str(), "hair");
    CHECK_EQ(out.aHair, 7); // 取的是字段后的 7，而不是名单里 "hair" 之后的内容

    // 只有名单里那个 "hair"，没有字段：必须是 -1（未指定），不能读成垃圾数。
    imoScene::sOUTFIT_DEFAULT noField;
    CHECK(imoScene::parseOutfitDefaultText("{\"hidden\": [\"hair\", \"shirt\"]}", noField));
    CHECK_EQ(noField.aHair, -1);
}

TEST_CASE(outfitDefaultIgnoresGarbageHairValue) {
    // "hair": "abc" —— 不是数字，必须保持 -1 而不是读成 0。
    imoScene::sOUTFIT_DEFAULT out;
    CHECK(imoScene::parseOutfitDefaultText("{\"hidden\": [\"a\"], \"hair\": \"abc\"}", out));
    CHECK_EQ(out.aHair, -1);
    // 负数也非法（-1 是内部哨兵，外部不该写）
    imoScene::sOUTFIT_DEFAULT negative;
    CHECK(imoScene::parseOutfitDefaultText("{\"hidden\": [\"a\"], \"hair\": -3}", negative));
    CHECK_EQ(negative.aHair, -1);
}

TEST_CASE(outfitDefaultResetsOutputBeforeParsing) {
    // 复用同一个 out：旧值必须被清掉，不能残留。
    imoScene::sOUTFIT_DEFAULT out;
    CHECK(imoScene::parseOutfitDefaultText("{\"hidden\": [\"a\",\"b\"], \"hair\": 2}", out));
    CHECK_EQ(out.aHidden.size(), 2u);
    CHECK(imoScene::parseOutfitDefaultText("{\"hidden\": [\"c\"]}", out));
    CHECK_EQ(out.aHidden.size(), 1u);
    CHECK_STREQ(out.aHidden[0].c_str(), "c");
    CHECK_EQ(out.aHair, -1);
}

TEST_CASE(outfitDefaultHandlesEmptyHiddenList) {
    imoScene::sOUTFIT_DEFAULT out;
    CHECK(imoScene::parseOutfitDefaultText("{\"hidden\": []}", out));
    CHECK_EQ(out.aHidden.size(), 0u);
    CHECK_EQ(out.aHair, -1);
}

TEST_CASE(outfitDefaultReadsFromFile) {
    CHECK(writeTempFile("{\"hidden\": [\"x\", \"y\"], \"hair\": 1}"));
    imoScene::sOUTFIT_DEFAULT out;
    CHECK(imoScene::parseOutfitDefaultFile(kTempPath, out));
    CHECK_EQ(out.aHidden.size(), 2u);
    CHECK_EQ(out.aHair, 1);
    // 文件不存在：false，且输出是干净的空状态
    imoScene::sOUTFIT_DEFAULT missing;
    missing.aHair = 9;
    missing.aHidden.push_back("stale");
    CHECK_EQ(imoScene::parseOutfitDefaultFile("/nonexistent/nope.json", missing), false);
    CHECK_EQ(missing.aHidden.size(), 0u);
    CHECK_EQ(missing.aHair, -1);
    // 路径为空指针：不能崩
    CHECK_EQ(imoScene::parseOutfitDefaultFile(0, missing), false);
}

// ═══════════════════════════════════════════════════════════
//  二、严格整数解析（环境变量）
// ═══════════════════════════════════════════════════════════

TEST_CASE(parseIntegerStrictAcceptsPlainIntegers) {
    int32_t value = 0;
    CHECK(imoScene::parseIntegerStrict("0", value));
    CHECK_EQ(value, 0);
    CHECK(imoScene::parseIntegerStrict("5", value));
    CHECK_EQ(value, 5);
    CHECK(imoScene::parseIntegerStrict("-1", value)); // -1 = 全部
    CHECK_EQ(value, -1);
    CHECK(imoScene::parseIntegerStrict("  12  ", value)); // 前后空白允许
    CHECK_EQ(value, 12);
}

TEST_CASE(parseIntegerStrictRejectsGarbage) {
    // **回归守卫**：std::atoi("abc") == 0，会把"全部显示"变成"选第一套"。
    int32_t value = 77;
    CHECK_EQ(imoScene::parseIntegerStrict("abc", value), false);
    CHECK_EQ(value, 77); // 失败不写 out
    CHECK_EQ(imoScene::parseIntegerStrict("", value), false);
    CHECK_EQ(imoScene::parseIntegerStrict("   ", value), false);
    CHECK_EQ(imoScene::parseIntegerStrict("12abc", value), false); // 尾部垃圾
    CHECK_EQ(imoScene::parseIntegerStrict("1.5", value), false); // 不是整数
    CHECK_EQ(imoScene::parseIntegerStrict("3 4", value), false); // 两个数
    CHECK_EQ(imoScene::parseIntegerStrict(0, value), false);
    CHECK_EQ(value, 77);
}

TEST_CASE(parseIntegerStrictRejectsOutOfRange) {
    int32_t value = 0;
    CHECK_EQ(imoScene::parseIntegerStrict("-2", value), false); // 比"全部"还小
    CHECK_EQ(imoScene::parseIntegerStrict("1000001", value), false);
    CHECK(imoScene::parseIntegerStrict("1000000", value)); // 上界本身允许
    CHECK_EQ(value, 1000000);
}

TEST_CASE(parseIntegerEnvReadsEnvironment) {
    int32_t value = 0;
    unsetenv("GN_TEST_VIS_ENV");
    CHECK_EQ(imoScene::parseIntegerEnv("GN_TEST_VIS_ENV", value), false); // 未设置
    setenv("GN_TEST_VIS_ENV", "3", 1);
    CHECK(imoScene::parseIntegerEnv("GN_TEST_VIS_ENV", value));
    CHECK_EQ(value, 3);
    setenv("GN_TEST_VIS_ENV", "abc", 1);
    CHECK_EQ(imoScene::parseIntegerEnv("GN_TEST_VIS_ENV", value), false); // 非法
    CHECK_EQ(value, 3); // 保持上次的值，不被垃圾覆盖
    setenv("GN_TEST_VIS_ENV", "", 1);
    CHECK_EQ(imoScene::parseIntegerEnv("GN_TEST_VIS_ENV", value), false); // 空串
    unsetenv("GN_TEST_VIS_ENV");
}
