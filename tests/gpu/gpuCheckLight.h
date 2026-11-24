// gpuCheckLight.h — 光照系统接入验收（光源类型/强度/衰减/环境光 → 像素）。
#pragma once
#ifndef _GN_GPU_CHECK_LIGHT_H_
#define _GN_GPU_CHECK_LIGHT_H_

namespace GpuCheck {
// 用 PBR 路径渲染同一块白quad，只改 sLIGHT_UBO 的字段，验证「改光照 → 像素跟着变」。
// 这条链很长：cLIGHT/sLIGHT_UBO → Begin3DPass 上传 lightBuffer3D → 描述符 binding 2(PBR)
// → gn3d_pbr.frag 的 LightUBO → CalcPBRLight → 合成 → 交换链。任何一环断开都会让
// 下面这些「变亮/变暗」的断言失败。
void RunLightChecks();
}

#endif // _GN_GPU_CHECK_LIGHT_H_
