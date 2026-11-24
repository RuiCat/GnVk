// gpuTestHooks.h — 验收程序读引擎内部状态的唯一入口。
//
// 为什么需要它：以前 17 个 gpuCheck 文件共约 300 处直读 `GNg::GN_Vk.xxx`。
// 那样一来，任何对 GN_VkContext 的重排（比如按子系统分组）都会同时打断所有测试，
// 让"架构调整"变成高危操作。改为走这个稳定面之后，内部怎么搬只需改
// `src/graphics/_Vulkan/vkTestHooks.cpp` 一个文件。
//
// 约定：
//   * 这里只做转发与一次性快照，不含任何测试逻辑；
//   * 每次调用 Hooks() 都会重新取快照（结构体 30 余字段，开销可忽略），
//     因此 resize / 重新初始化后的值不会变旧；
//   * 新增需求先在引擎侧 sTEST_HOOKS 里补字段，不要回来直读 GN_Vk。
#ifndef _GN_GPU_TEST_HOOKS_H_
#define _GN_GPU_TEST_HOOKS_H_

#include <GN.h>
#include "GN_Vk.h"

namespace GpuCheck {

// 当前钩子快照（按值填充到内部静态对象后返回引用）。
const GNg::sTEST_HOOKS &Hooks();

// 常用快捷方式：让迁移后的调用点尽量短。
inline VkDevice Dev() { return Hooks().device; }
inline VkPhysicalDevice PhysDev() { return Hooks().physicalDevice; }
inline VkQueue Queue() { return Hooks().graphicsQueue; }
inline VkCommandPool CmdPool() { return Hooks().commandPool; }
inline VkCommandBuffer CmdBuf() { return Hooks().commandBuffer; }
inline VkExtent2D Extent() { return Hooks().extent; }
inline VkFormat SwapFormat() { return Hooks().format; }
inline VkImage SwapImage() { return Hooks().currentSwapImage; }
inline bool EngineReady() { return Hooks().initialized; }
inline ::VkPipeline Pipe(GNg::eTEST_PIPE id) { return GNg::GN_VkTestPipeline((int)id); }

} // namespace GpuCheck
#endif // _GN_GPU_TEST_HOOKS_H_
