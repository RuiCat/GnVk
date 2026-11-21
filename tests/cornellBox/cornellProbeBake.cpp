// cornellProbeBake.cpp — 在 Cornell 内部网格上烘焙 SH9 探针
#include "cornellProbeBake.h"

#include "cornellCpuRt.h"
#include "cornellGeometry.h"
#include "giParams.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace Cornell {

namespace {

const unsigned int kCacheMagic = 0x47505634u; // "GPV4"（depth moments）

struct sCacheHeader {
    unsigned int aMagic;
    unsigned int aCounts[3];
    float aOrigin[3];
    float aSpacing;
    int aFaceSize;
    int aSamples;
    int aLightMode;
    unsigned int aDepthCount;
    unsigned int aDepthRes;
};

std::string cachePath(int lightMode, int faceSize, int samples, uint32_t cx, uint32_t cy,
                      uint32_t cz) {
    char path[256];
    std::snprintf(path, sizeof(path),
                  ".local/cornellbox/probe_cache_%d_%ux%ux%u_%dx%d.bin", lightMode, cx, cy, cz,
                  faceSize, samples);
    return std::string(path);
}

std::string assetCachePath(int lightMode, int faceSize, int samples, uint32_t cx, uint32_t cy,
                           uint32_t cz) {
    char path[256];
    std::snprintf(path, sizeof(path),
                  "assets/probes/cornell_probe_%d_%ux%ux%u_%dx%d.bin", lightMode, cx, cy, cz,
                  faceSize, samples);
    return std::string(path);
}

bool loadCache(const std::string &path, int lightMode, int faceSize, int samples, uint32_t cx,
               uint32_t cy, uint32_t cz, std::vector<float> *out,
               std::vector<float> *depthOut) {
    FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) return false;
    sCacheHeader header = {};
    bool ok = std::fread(&header, sizeof(header), 1, file) == 1;
    const size_t expected = static_cast<size_t>(cx) * cy * cz * 36u;
    if (ok) {
        ok = header.aMagic == kCacheMagic && header.aCounts[0] == cx &&
             header.aCounts[1] == cy && header.aCounts[2] == cz && header.aFaceSize == faceSize &&
             header.aSamples == samples && header.aLightMode == lightMode;
    }
    if (ok) {
        out->resize(expected);
        ok = std::fread(out->data(), sizeof(float), expected, file) == expected;
    }
    const size_t depthExpected = static_cast<size_t>(cx) * cy * cz * 6u *
                                 static_cast<size_t>(faceSize) * faceSize * 2u;
    if (ok && header.aDepthCount == depthExpected && header.aDepthRes ==
              static_cast<unsigned int>(faceSize)) {
        depthOut->resize(depthExpected);
        ok = std::fread(depthOut->data(), sizeof(float), depthExpected, file) == depthExpected;
    } else {
        ok = false;
    }
    std::fclose(file);
    return ok;
}

bool saveCache(const std::string &path, int lightMode, int faceSize, int samples, uint32_t cx,
               uint32_t cy, uint32_t cz, const GNg::sVEC3 &origin, float spacing,
               const std::vector<float> &sh, const std::vector<float> &depth) {
    FILE *file = std::fopen(path.c_str(), "wb");
    if (!file) return false;
    sCacheHeader header = {};
    header.aMagic = kCacheMagic;
    header.aCounts[0] = cx;
    header.aCounts[1] = cy;
    header.aCounts[2] = cz;
    header.aOrigin[0] = origin.x;
    header.aOrigin[1] = origin.y;
    header.aOrigin[2] = origin.z;
    header.aSpacing = spacing;
    header.aFaceSize = faceSize;
    header.aSamples = samples;
    header.aLightMode = lightMode;
    header.aDepthCount = static_cast<unsigned int>(depth.size());
    header.aDepthRes = static_cast<unsigned int>(faceSize);
    bool ok = std::fwrite(&header, sizeof(header), 1, file) == 1;
    ok = ok && std::fwrite(sh.data(), sizeof(float), sh.size(), file) == sh.size();
    ok = ok && std::fwrite(depth.data(), sizeof(float), depth.size(), file) == depth.size();
    std::fclose(file);
    return ok;
}

} // namespace

bool bakeProbeGrid(int lightMode, int faceSize, int samples, sProbeBakeResult *out) {
    if (!out) return false;
    sCornellGeometry geometry;
    buildCornellGeometry(false, &geometry);
    const uint32_t cx = 24, cy = 22, cz = 24;
    out->aOrigin = GNg::sVEC3{0.12f, 0.12f, 0.12f};
    out->aSpacing = 0.228f;
    out->aDepthRes = static_cast<uint32_t>(faceSize);
    out->aCounts[0] = cx;
    out->aCounts[1] = cy;
    out->aCounts[2] = cz;
    const std::string assetPath = assetCachePath(lightMode, faceSize, samples, cx, cy, cz);
    const std::string path = cachePath(lightMode, faceSize, samples, cx, cy, cz);
    const char *cacheEnv = std::getenv("CORNELL_PROBE_CACHE");
    const bool cacheEnabled = !(cacheEnv && cacheEnv[0] == '0');
    if (cacheEnabled &&
        loadCache(assetPath, lightMode, faceSize, samples, cx, cy, cz, &out->aSh9,
                  &out->aDepth)) {
        std::fprintf(stderr, "[probeBake] asset hit: %s\n", assetPath.c_str());
        return true;
    }
    if (cacheEnabled &&
        loadCache(path, lightMode, faceSize, samples, cx, cy, cz, &out->aSh9, &out->aDepth)) {
        std::fprintf(stderr, "[probeBake] cache hit: %s\n", path.c_str());
        return true;
    }
    out->aSh9.assign(static_cast<size_t>(cx) * cy * cz * 36u, 0.0f);
    out->aDepth.assign(
        static_cast<size_t>(cx) * cy * cz * 6u * faceSize * faceSize * 2u, 1.0e30f);
    const int faces = faceSize * faceSize * 3;
    std::vector<float> facesRgb(static_cast<size_t>(faces) * 6u, 0.0f);
    for (uint32_t z = 0; z < cz; ++z) {
        for (uint32_t y = 0; y < cy; ++y) {
            for (uint32_t x = 0; x < cx; ++x) {
                const GNg::sVEC3 probe = {out->aOrigin.x + out->aSpacing * static_cast<float>(x),
                                          out->aOrigin.y + out->aSpacing * static_cast<float>(y),
                                          out->aOrigin.z + out->aSpacing * static_cast<float>(z)};
                const uint32_t index = (z * cy + y) * cx + x;
                const unsigned int seed = 17u + x * 1973u + y * 9277u + z * 1299709u;
                if (!bakeProbeCubemap(geometry, probe, lightMode, faceSize, samples, seed,
                                      facesRgb.data(),
                                      &out->aDepth[static_cast<size_t>(index) * 6u * faceSize *
                                                   faceSize * 2u])) {
                    return false;
                }
                float sh[36] = {0.0f};
                if (!GN3d::shProjectCubemap(facesRgb.data(), faceSize, 3, sh)) return false;
                std::memcpy(&out->aSh9[static_cast<size_t>(index) * 36u], sh,
                            sizeof(float) * 36u);
            }
        }
    }
    // 对 SH9 做 3×3×3 盒式平滑：探针是低频表示，平滑能显著压掉逐探针烘焙噪声，
    // 代价是进一步损失一点空间细节（可接受；直接光仍在运行时逐像素计算）。
    std::vector<float> blurred(out->aSh9.size(), 0.0f);
    for (uint32_t z = 0; z < cz; ++z) {
        for (uint32_t y = 0; y < cy; ++y) {
            for (uint32_t x = 0; x < cx; ++x) {
                const uint32_t dstIndex = (z * cy + y) * cx + x;
                float accum[36] = {0.0f};
                int weightCount = 0;
                for (int dz = -1; dz <= 1; ++dz) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            const int sx = std::max(0, std::min<int>(cx - 1, x + dx));
                            const int sy = std::max(0, std::min<int>(cy - 1, y + dy));
                            const int sz = std::max(0, std::min<int>(cz - 1, z + dz));
                            const uint32_t src = (sz * cy + sy) * cx + sx;
                            for (int k = 0; k < 36; ++k) {
                                accum[k] += out->aSh9[static_cast<size_t>(src) * 36u + k];
                            }
                            ++weightCount;
                        }
                    }
                }
                for (int k = 0; k < 36; ++k) {
                    blurred[static_cast<size_t>(dstIndex) * 36u + k] =
                        accum[k] / static_cast<float>(weightCount);
                }
            }
        }
    }
    out->aSh9.swap(blurred);
    if (cacheEnabled) {
        if (saveCache(path, lightMode, faceSize, samples, cx, cy, cz, out->aOrigin,
                      out->aSpacing, out->aSh9, out->aDepth)) {
            std::fprintf(stderr, "[probeBake] cache saved: %s\n", path.c_str());
        }
    }
    return true;
}

} // namespace Cornell
