// gpuTestHooks.cpp — Hooks() 的唯一实现（快照缓存）。
#include "gpuTestHooks.h"

namespace GpuCheck {

const GNg::sTEST_HOOKS &Hooks() {
    static GNg::sTEST_HOOKS sHooks;
    GNg::GN_VkGetTestHooks(&sHooks); // 每次刷新，避免 resize/重初始化后变旧
    return sHooks;
}

} // namespace GpuCheck
