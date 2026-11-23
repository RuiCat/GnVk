// vkValidation.h — Khronos 校验层（VK_LAYER_KHRONOS_validation）接入与 debug messenger。
// 只在设置环境变量 GN_VK_VALIDATION=1 时生效；未安装校验层时静默跳过（不影响正常渲染）。
#ifndef _GN_VK_VALIDATION_H_
#define _GN_VK_VALIDATION_H_

#include <vulkan/vulkan.h>
#include <vector>

namespace GNg {

// 是否请求了校验层（GN_VK_VALIDATION=1）且本机确实安装了 Khronos 校验层。
bool vkValidationAvailable();

// 把校验层名与 VK_EXT_debug_utils 追加到实例创建参数里（无可用层时不动）。
void vkValidationAppendInstanceParams(std::vector<const char *> &layers,
                                      std::vector<const char *> &extensions);

// 实例创建成功后建立 messenger；失败静默（只影响诊断输出）。
void vkValidationCreateMessenger(VkInstance instance);

// 销毁 messenger（实例销毁前调用）。
void vkValidationDestroyMessenger();

// 帧内命令缓冲分段标签（仅校验模式下生效；用于定位「命令缓冲在帧内哪一段变 invalid」）。
void vkValidationBeginLabel(VkCommandBuffer cmd, const char *name);
void vkValidationEndLabel(VkCommandBuffer cmd);

} // namespace GNg

#endif // _GN_VK_VALIDATION_H_
