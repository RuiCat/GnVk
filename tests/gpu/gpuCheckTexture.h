// P1c 纹理验收：glTF 纹理解码 → RGBA8 → VkImage 上传 → 材质采样出图。
#pragma once
#ifndef _GN_GPU_CHECK_TEXTURE_H_
#define _GN_GPU_CHECK_TEXTURE_H_

namespace GpuCheck {
void RunTextureChecks();
} // namespace GpuCheck
#endif // _GN_GPU_CHECK_TEXTURE_H_
