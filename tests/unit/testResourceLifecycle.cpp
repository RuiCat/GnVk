// testResourceLifecycle.cpp — GPU 资源释放契约（A4）。
//
// 为什么需要这组测试：这套代码有 56 处调用 `sIMAGE::Release()`，其中不少发生在
// 析构里（静态/全局对象、AppUnInit 之后才释放的模型）。若设备已经销毁，拿旧句柄
// 去 vkDestroy* 就是未定义行为——而这种错不会让功能测试失败，只会在某些退出路径上
// 偶发崩溃（本项目确实出现过 SIGSEGV 139 退出的偶发问题）。
//
// 这里守住三条契约：
//   1. 设备不在时 Release/ReleaseGPU 只清句柄，绝不调用 Vulkan。
//   2. 重复释放安全（第二次是 no-op）。
//   3. sIMAGE::ReleaseGPU 与 sMESH::ReleaseGPU 是同一语义（都释放 GPU），
//      而 sMESH::Release 释放 CPU —— 命名不再靠猜。
#include "testFramework.h"

#include <GN.h>

using GNg::sIMAGE; // sIMAGE 在 GNg 命名空间

namespace {

// 造一个“看起来有句柄、但设备已不存在”的 sIMAGE。
// 用非空假句柄是为了确保旧实现真的会走进 vkDestroy* 分支（而不是被 if 挡掉）。
sIMAGE makeImageWithFakeHandles() {
    sIMAGE img;
    img.vkImage = (VkImage)(uintptr_t)0x1000;
    img.vkImageMemory = (VkDeviceMemory)(uintptr_t)0x2000;
    img.vkImageView = (VkImageView)(uintptr_t)0x3000;
    img.vkSampler = (VkSampler)(uintptr_t)0x4000;
    img.texnum = 1;
    return img;
}

} // namespace

// 设备不在时（单测环境不初始化 Vulkan），ReleaseGPU 必须只清句柄、不崩。
TEST_CASE(resImageReleaseWithoutDevice) {
    sIMAGE img = makeImageWithFakeHandles();

    // 前提：单测进程里设备确实没起来。
    CHECK(!GNg::GN_VkDeviceAlive());

    img.ReleaseGPU(); // 旧实现会在这里对 0x3000 调 vkDestroyImageView(无效 device)

    CHECK(img.vkImage == VK_NULL_HANDLE);
    CHECK(img.vkImageView == VK_NULL_HANDLE);
    CHECK(img.vkImageMemory == VK_NULL_HANDLE);
    CHECK(img.vkSampler == VK_NULL_HANDLE);
    CHECK(img.texnum == 0);
}

// 重复释放安全：第二次调用是 no-op（句柄已空）。
TEST_CASE(resImageReleaseTwice) {
    sIMAGE img = makeImageWithFakeHandles();
    img.ReleaseGPU();
    img.ReleaseGPU(); // 不应崩溃
    CHECK(img.vkImageView == VK_NULL_HANDLE);
    CHECK(img.vkImage == VK_NULL_HANDLE);
}

// Release() 是 ReleaseGPU() 的兼容别名：两者做同一件事。
TEST_CASE(resImageReleaseAliasEquivalent) {
    sIMAGE a = makeImageWithFakeHandles();
    sIMAGE b = makeImageWithFakeHandles();

    a.ReleaseGPU();
    b.Release();

    CHECK(a.vkImage == b.vkImage);
    CHECK(a.vkImageView == b.vkImageView);
    CHECK(a.vkImageMemory == b.vkImageMemory);
    CHECK(a.vkSampler == b.vkSampler);
    CHECK(a.texnum == b.texnum);
}

// 析构路径同样安全（这是最常见的调用点：P<sIMAGE> 出作用域）。
TEST_CASE(resImageDestructorWithoutDevice) {
    {
        sIMAGE img = makeImageWithFakeHandles();
        // 故意不手动释放，交给析构
    }
    CHECK(true); // 走到这里即说明没崩
}

// 拷贝赋值时旧资源要被释放（operator= 里第一件事就是 Release）。
TEST_CASE(resImageAssignReleasesOld) {
    sIMAGE dst = makeImageWithFakeHandles();
    sIMAGE src; // 干净的空图
    src.width = 64;
    src.height = 32;

    dst = src;
    CHECK(dst.vkImage == VK_NULL_HANDLE);   // 旧句柄已释放
    CHECK(dst.width == 64);
    CHECK(dst.height == 32);
}

// sMESH 的两条释放路径要能分开调用、且都能重复调用。
TEST_CASE(resMeshReleaseCpuAndGpuSeparate) {
    GN3d::sMESH mesh;
    // 走一遍两次释放：设备不在时 ReleaseGPU 只能清句柄（内部 GN_VkRelease3DMesh 判了 initialized）
    mesh.Release();
    mesh.ReleaseGPU();
    mesh.Release();
    mesh.ReleaseGPU();
    CHECK(mesh.aVertices == 0);
    CHECK(mesh.aIndices == 0);
    CHECK(mesh.aVertexCount == 0);
    CHECK(mesh.aIndexCount == 0);
}
