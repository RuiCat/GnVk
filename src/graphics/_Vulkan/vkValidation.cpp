// vkValidation.cpp — 校验层接入：按 GN_VK_VALIDATION=1 启用 Khronos 校验层并把消息打到 stderr。
#include <GN.h>
#include "vkValidation.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace GNg {
namespace {

const char *const kValidationLayer = "VK_LAYER_KHRONOS_validation";
const char *const kDebugUtilsExt = "VK_EXT_debug_utils";

VkDebugUtilsMessengerEXT s_messenger = VK_NULL_HANDLE;
VkInstance s_instance = VK_NULL_HANDLE;

bool envEnabled() {
    const char *v = std::getenv("GN_VK_VALIDATION");
    return v != 0 && v[0] != '\0' && std::strcmp(v, "0") != 0;
}

bool layerInstalled() {
    uint32_t count = 0;
    if(vkEnumerateInstanceLayerProperties(&count, 0) != VK_SUCCESS || count == 0) return false;
    std::vector<VkLayerProperties> props(count);
    if(vkEnumerateInstanceLayerProperties(&count, &props[0]) != VK_SUCCESS) return false;
    for(uint32_t i = 0; i < count; ++i) {
        if(std::strcmp(props[i].layerName, kValidationLayer) == 0) return true;
    }
    return false;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                            VkDebugUtilsMessageTypeFlagsEXT types,
                                            const VkDebugUtilsMessengerCallbackDataEXT *data,
                                            void *user) {
    (void)types;
    (void)user;
    if(data == 0 || data->pMessage == 0) return VK_FALSE;
    const char *tag = "INFO";
    if(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) tag = "ERROR";
    else if(severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) tag = "WARN";
    std::fprintf(stderr, "[VK-VALIDATION %s] %s\n", tag, data->pMessage);
    return VK_FALSE;
}

} // namespace

bool vkValidationAvailable() { return envEnabled() && layerInstalled(); }

void vkValidationAppendInstanceParams(std::vector<const char *> &layers,
                                      std::vector<const char *> &extensions) {
    if(!vkValidationAvailable()) return;
    layers.push_back(kValidationLayer);
    extensions.push_back(kDebugUtilsExt);
}

void vkValidationCreateMessenger(VkInstance instance) {
    if(!vkValidationAvailable() || instance == VK_NULL_HANDLE) return;
    s_instance = instance;
    PFN_vkCreateDebugUtilsMessengerEXT createFn =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,
                                                                 "vkCreateDebugUtilsMessengerEXT");
    if(createFn == 0) {
        std::fprintf(stderr, "[VK-VALIDATION] 取不到 vkCreateDebugUtilsMessengerEXT，校验输出不可用\n");
        return;
    }
    VkDebugUtilsMessengerCreateInfoEXT info = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = debugCallback;
    if(createFn(instance, &info, 0, &s_messenger) != VK_SUCCESS) {
        std::fprintf(stderr, "[VK-VALIDATION] messenger 创建失败\n");
        s_messenger = VK_NULL_HANDLE;
    }
}

namespace {
PFN_vkCmdBeginDebugUtilsLabelEXT s_beginLabelFn = 0;
PFN_vkCmdEndDebugUtilsLabelEXT s_endLabelFn = 0;
bool s_labelFnsLoaded = false;
} // namespace

void vkValidationBeginLabel(VkCommandBuffer cmd, const char *name) {
    if(!vkValidationAvailable() || cmd == VK_NULL_HANDLE || name == 0) return;
    if(!s_labelFnsLoaded) {
        s_beginLabelFn = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(
            s_instance, "vkCmdBeginDebugUtilsLabelEXT");
        s_endLabelFn = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(
            s_instance, "vkCmdEndDebugUtilsLabelEXT");
        s_labelFnsLoaded = true;
    }
    if(s_beginLabelFn == 0) return;
    VkDebugUtilsLabelEXT label = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
    label.pLabelName = name;
    label.color[0] = 0.2f;
    label.color[1] = 0.8f;
    label.color[2] = 0.4f;
    label.color[3] = 1.0f;
    s_beginLabelFn(cmd, &label);
}

void vkValidationEndLabel(VkCommandBuffer cmd) {
    if(!vkValidationAvailable() || cmd == VK_NULL_HANDLE || s_endLabelFn == 0) return;
    s_endLabelFn(cmd);
}

void vkValidationDestroyMessenger() {
    if(s_messenger == VK_NULL_HANDLE || s_instance == VK_NULL_HANDLE) return;
    PFN_vkDestroyDebugUtilsMessengerEXT destroyFn =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(s_instance,
                                                                  "vkDestroyDebugUtilsMessengerEXT");
    if(destroyFn != 0) destroyFn(s_instance, s_messenger, 0);
    s_messenger = VK_NULL_HANDLE;
    s_instance = VK_NULL_HANDLE;
}

} // namespace GNg
