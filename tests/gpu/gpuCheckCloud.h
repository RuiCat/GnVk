// gpuCheckCloud.h — P11-S2b 中景 billboard 云层验收入口。
//
// RunCloudChecks 覆盖：pass 外/非法参数拒绝、3D pass 内成功提交、云层像素与清屏色不同
// （alpha 混合生效）、Destroy 可重复调用。
#pragma once
#ifndef _GN_GPU_CHECK_CLOUD_H_
#define _GN_GPU_CHECK_CLOUD_H_

namespace GpuCheck {
void RunCloudChecks();
}

#endif // _GN_GPU_CHECK_CLOUD_H_
