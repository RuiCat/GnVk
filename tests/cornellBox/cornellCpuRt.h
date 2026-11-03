// tests/cornellBox/cornellCpuRt.h — CPU 软件光追预览（自由相机；镜面反射 + 直接光）
#ifndef CORNELL_CPU_RT_H
#define CORNELL_CPU_RT_H

#include <vector>

namespace Cornell {

// 渲染当前相机视角到 RGBA8（width*height*4）。纯 CPU，不依赖 Vulkan。
// eye/fwd/right/up 为世界空间基；fovYDeg 为垂直 FOV；lightMode: 0=sun, 1=point。
bool renderCpuRt(int width, int height, const float eye[3], const float fwd[3],
                 const float right[3], const float up[3], float fovYDeg, int lightMode,
                 std::vector<unsigned char> *outRgba);

} // namespace Cornell
#endif // CORNELL_CPU_RT_H
