/****************************************************************************
 *  GN_mini — Vulkan 后端上下文实例
 *  职责：GN_VkContext 全局实例 GN_Vk 的唯一定义点。
 *  拆出的模块：
 *    vkRenderTarget.cpp  渲染目标（RT pass/framebuffer、sIMAGE 渲染目标入口）
 *    vkOffscreen.cpp    无窗口离屏目标（替代 swapchain 的图/视图/显存）
 *    vkSwapchain.cpp    交换链格式选择、2D 叠层 framebuffer、resize 重建
 *    vkInit.cpp         设备/交换链/管线/命令缓冲初始化（GN_VkInit）
 *    vkDevice.cpp   设备级辅助（SPIR-V 读取/内存/缓冲/视图）
 *    vkImageIo.cpp  纹理 PNG IO 与图集
 *    vkImage.cpp    sIMAGE 生命周期与回读
 *    vkFrame2d.cpp  2D 帧循环与批状态
 *    vkDraw2d.cpp   2D 绘制入口
 *    vk3d*.cpp      3D 网格/pass/前向/初始化/释放
 ****************************************************************************/
#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

namespace GNg {

GN_VkContext GN_Vk;


} // namespace GNg
