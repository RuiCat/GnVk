// tests/cornellBox/cornellCpuRt.h — CPU 软件光追预览（自由相机；镜面反射 + 直接光）
#ifndef CORNELL_CPU_RT_H
#define CORNELL_CPU_RT_H

#include <vector>

namespace GNg {
struct sVEC3;
}

namespace Cornell {

struct sCornellGeometry;

// 从探针位置渲染 6 面 cubemap（RGB 线性 radiance），用于 SH9 烘焙。
// facesRgb 长度 = 6 * faceSize² * 3，面序与 GN3d::cubemapFaceDirection 一致。
bool bakeProbeCubemap(const sCornellGeometry &geometry, const GNg::sVEC3 &probe, int lightMode,
                      int faceSize, int samples, unsigned int seed, float *facesRgb,
                      float *depthMoments = nullptr);

// 渲染当前相机视角到 RGBA8（width*height*4）。纯 CPU，不依赖 Vulkan。
// eye/fwd/right/up 为世界空间基；fovYDeg 为垂直 FOV；lightMode: 0=sun, 1=point。
bool renderCpuRt(int width, int height, const float eye[3], const float fwd[3],
                 const float right[3], const float up[3], float fovYDeg, int lightMode,
                 std::vector<unsigned char> *outRgba);

} // namespace Cornell
#endif // CORNELL_CPU_RT_H
