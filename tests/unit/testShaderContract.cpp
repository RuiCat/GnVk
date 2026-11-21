// testShaderContract.cpp — S-25 抽取契约：两条光照 frag 共用 brdfLighting/sceneLighting，
// 且不再各自定义被抽走的函数（防止后续改动把重复代码又复制回去）。
#include "testFramework.h"

#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string readText(const char *path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::string();
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

unsigned int countOf(const std::string &text, const std::string &needle) {
    if (needle.empty()) return 0u;
    unsigned int n = 0u;
    std::size_t pos = 0u;
    while ((pos = text.find(needle, pos)) != std::string::npos) {
        ++n;
        pos += needle.size();
    }
    return n;
}

} // namespace


TEST_CASE(shaderContractCornellSharedRayLib) {
    const std::string rt = readText("assets/shaders/gn_cornell_rt.comp");
    const std::string upd = readText("assets/shaders/gn_cornell_probe_update.comp");
    const std::string rel = readText("assets/shaders/gn_cornell_probe_relocation.comp");
    const std::string lib = readText("assets/shaders/cornellRayLib.glsl");
    CHECK(!rt.empty() && !upd.empty() && !rel.empty() && !lib.empty());
    // 三个 compute 都走同一份场景/着色/探针采样库。
    CHECK_EQ(countOf(rt, "#include \"cornellRayLib.glsl\""), 1u);
    CHECK_EQ(countOf(upd, "#include \"cornellRayLib.glsl\""), 1u);
    CHECK_EQ(countOf(rel, "#include \"cornellRayLib.glsl\""), 1u);
    // 关键实现只允许存在一份，防止复制回去后发散。
    CHECK_EQ(countOf(lib, "Hit traceCornell("), 1u);
    CHECK_EQ(countOf(lib, "vec3 evalProbeSh("), 1u);
    CHECK_EQ(countOf(lib, "vec3 radiance("), 1u);
    CHECK_EQ(countOf(rt, "Hit traceCornell("), 0u);
    CHECK_EQ(countOf(upd, "Hit traceCornell("), 0u);
    // probe update 的共享 SSBO 布局契约：binding 15 由更新 shader 自己声明，其余来自公共库。
    CHECK_EQ(countOf(lib, "layout(std430, binding = 12) buffer ProbeSh"), 1u);
    CHECK_EQ(countOf(lib, "layout(std430, binding = 13) buffer ProbeDepth"), 1u);
    CHECK_EQ(countOf(upd, "layout(std430, binding = 15) buffer ProbeState"), 1u);
    CHECK_EQ(countOf(rt, "layout(std430, binding = 12)"), 0u);
}

TEST_CASE(shaderContractFilesExist) {
    const char *files[4] = {
        "assets/shaders/gn3d_pbr.frag",
        "assets/shaders/gn3d_deferred_light.frag",
        "assets/shaders/brdfLighting.glsl",
        "assets/shaders/sceneLighting.glsl",
    };
    for (int i = 0; i < 4; ++i) {
        const std::string text = readText(files[i]);
        if (text.empty()) std::fprintf(stderr, "  缺少文件（cwd 必须是仓库根）: %s\n", files[i]);
        CHECK(!text.empty());
    }
}

TEST_CASE(shaderContractIncludes) {
    const std::string pbr = readText("assets/shaders/gn3d_pbr.frag");
    const std::string def = readText("assets/shaders/gn3d_deferred_light.frag");
    CHECK(countOf(pbr, "#include \"brdfLighting.glsl\"") == 1u);
    CHECK(countOf(def, "#include \"brdfLighting.glsl\"") == 1u);
    CHECK(countOf(pbr, "#include \"sceneLighting.glsl\"") == 1u);
    CHECK(countOf(def, "#include \"sceneLighting.glsl\"") == 1u);
    CHECK(countOf(pbr, "#define PI 3.14159265359") == 1u);
    CHECK(countOf(def, "#define PI 3.14159265359") == 1u);
}

TEST_CASE(shaderContractNoDuplicateDefinitions) {
    const std::string pbr = readText("assets/shaders/gn3d_pbr.frag");
    const std::string def = readText("assets/shaders/gn3d_deferred_light.frag");
    const char *sigs[6] = {
        "vec3 evalSH9(vec3 n)",
        "vec3 applyAerialPerspective(vec3 litColor, vec3 worldPos)",
        "float fogAmountAt(vec3 worldPosition)",
        "vec3 sampleSunTransmittance(vec3 p",
        "float shadowCompare(vec2 uv",
        "int selectCascade(vec3 worldPos",
    };
    for (int i = 0; i < 6; ++i) {
        if (countOf(pbr, sigs[i]) != 0u) std::fprintf(stderr, "  pbr 仍有重复定义: %s\n", sigs[i]);
        if (countOf(def, sigs[i]) != 0u) std::fprintf(stderr, "  deferred 仍有重复定义: %s\n", sigs[i]);
        CHECK_EQ(countOf(pbr, sigs[i]), 0u);
        CHECK_EQ(countOf(def, sigs[i]), 0u);
    }
}

TEST_CASE(shaderContractLibsAreSelfContained) {
    const std::string brdf = readText("assets/shaders/brdfLighting.glsl");
    const std::string scene = readText("assets/shaders/sceneLighting.glsl");
    // 纯数学库不得引用 UBO/采样器/绑定（否则无法在 #version 后立即 include）。
    CHECK_EQ(countOf(brdf, "layout("), 0u);
    CHECK_EQ(countOf(brdf, "uniform "), 0u);
    CHECK_EQ(countOf(brdf, "sampler"), 0u);
    // UBO 依赖库不得自己声明绑定（调用方负责声明，绑定号两条路径不同）。
    CHECK_EQ(countOf(scene, "layout(binding"), 0u);
    // 两个库都要有 include guard。
    CHECK_EQ(countOf(brdf, "#ifndef BRDF_LIGHTING_GLSL"), 1u);
    CHECK_EQ(countOf(scene, "#ifndef SCENE_LIGHTING_GLSL"), 1u);
    // 被抽走的函数必须仍然只存在于公共库里。
    CHECK_EQ(countOf(brdf, "float DistributionGGX("), 1u);
    CHECK_EQ(countOf(scene, "vec3 evalSH9(vec3 n)"), 1u);
}