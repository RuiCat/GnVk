// tests/imoScene/imoSceneBlobShadow.cpp — 软阴影贴图的加载与贴地绘制。
//
// 形状：一张 128² 的径向渐变 PNG（RGB=黑、A=中心 0.55 到边缘 0），贴地四边形直接走
// 前向 PBR 的 BLEND 管线（albedo 全黑 ⇒ 只把脚下压暗，不引入新光照）。
// 深度：y = 地面 + 0.006，避免与房间木地板 z-fighting。

#include "imoSceneBlobShadow.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace imoScene {

namespace {

// 四边形顶点布局与 VkVertex3D 一致：pos3 / normal3 / tangent3 / uv2。
const int VERTEX_FLOATS = 11;
const float GROUND_LIFT = 0.006f;
const float DEFAULT_RADIUS = 0.46f;

} // namespace

sBlobShadow::sBlobShadow() : aTexture(0), aRadius(DEFAULT_RADIUS) {}

bool blobShadowLoad(sBlobShadow &out, const char *defaultPath) {
    blobShadowRelease(out);
    const char *disabled = std::getenv("IMO_SCENE_BLOB");
    if (disabled != 0 && disabled[0] == '0') {
        return false;
    }
    const char *fromEnv = std::getenv("IMO_SCENE_BLOB_SHADOW");
    const char *path = (fromEnv != 0 && fromEnv[0] != '\0') ? fromEnv : defaultPath;
    if (path == 0 || path[0] == '\0') {
        return false;
    }
    std::FILE *file = std::fopen(path, "rb");
    if (file == 0) {
        std::fprintf(stderr, "[imoScene] 贴地阴影贴图缺失: %s\n", path);
        return false;
    }
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (size <= 0) {
        std::fclose(file);
        return false;
    }
    std::vector<unsigned char> bytes(static_cast<size_t>(size));
    const size_t read = std::fread(bytes.data(), 1, bytes.size(), file);
    std::fclose(file);
    if (read != bytes.size()) {
        return false;
    }
    GNg::sIMAGE *image = new GNg::sIMAGE();
    if (!GNg::ImageLoadFromMem(bytes.data(), static_cast<int>(bytes.size()), image)) {
        std::fprintf(stderr, "[imoScene] 贴地阴影贴图解码失败: %s\n", path);
        delete image;
        return false;
    }
    const char *radiusEnv = std::getenv("IMO_SCENE_BLOB_RADIUS");
    if (radiusEnv != 0 && radiusEnv[0] != '\0') {
        const float radius = static_cast<float>(std::atof(radiusEnv));
        if (radius > 0.05f && radius < 5.0f) {
            out.aRadius = radius;
        }
    }
    out.aTexture = image;
    std::printf("[imoScene] 贴地阴影: %s（半径 %.2f m）\n", path, out.aRadius);
    return true;
}

void blobShadowRelease(sBlobShadow &out) {
    delete out.aTexture;
    out.aTexture = 0;
}

void blobShadowDraw(const sBlobShadow &shadow, float centerX, float centerZ) {
    if (shadow.aTexture == 0) {
        return;
    }
    const float half = shadow.aRadius;
    const float y = GROUND_LIFT;
    const float xs[4] = {centerX - half, centerX + half, centerX + half, centerX - half};
    const float zs[4] = {centerZ - half, centerZ - half, centerZ + half, centerZ + half};
    float vertices[4 * VERTEX_FLOATS];
    std::memset(vertices, 0, sizeof(vertices));
    for (int corner = 0; corner < 4; ++corner) {
        float *vertex = vertices + corner * VERTEX_FLOATS;
        vertex[0] = xs[corner];
        vertex[1] = y;
        vertex[2] = zs[corner];
        vertex[3] = 0.0f;
        vertex[4] = 1.0f;
        vertex[5] = 0.0f;
        vertex[6] = 1.0f; // 切线 +X
        vertex[9] = (xs[corner] - (centerX - half)) / (2.0f * half);
        vertex[10] = (zs[corner] - (centerZ - half)) / (2.0f * half);
    }
    const uint32_t indices[6] = {0, 2, 1, 0, 3, 2};
    GNg::sMAT44 identity;
    GNg::Mat44Identity(&identity);
    // albedo 全黑 + 贴图 alpha：混合管线只压暗脚下，不参与高光/金属。
    GNg::GN_VkDraw3DPBR(vertices, 4, indices, 6, &identity, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
                        shadow.aTexture, 0, 0.0f, true);
}

} // namespace imoScene
