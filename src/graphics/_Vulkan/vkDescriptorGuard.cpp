// vkDescriptorGuard.cpp — 描述符集改写的「提交期绑定」体检。
//
// 规则（VUID-vkUpdateDescriptorSets-None-03047）：已经录制进命令缓冲并绑定的描述符集
// 不得再被 vkUpdateDescriptorSets 改写；否则该命令缓冲进入 invalid state，部分驱动会
// 丢弃其后的 bind/draw。这个问题在引擎里出现过两次，表现为「画到一半后面的东西全没了」
// 且不报错，很难从画面反推，所以做成可断言的计数器而不是靠人看代码。
//
// 判定口径：
//   * 渲染通道活跃（s_inRenderPass）时改写**规范集**（引擎对外公开、会在通道内被绑定的
//     那几个）→ 计为违规，记录首个站点并打一次 stderr；
//   * 环集（按纹理组合分配的私有 set）不计：它们在首次使用前写入，且从未被绑定录制过。
//
// 回归：tests/gpu 的 desc.noUpdateWhileBound 断言违规次数 == 0（且总次数 > 0）。
#define GN_VK_NO_DESC_GUARD // 关掉 vkInternal.h 里的同名宏，本文件用真实入口

#include <GN.h>
#include "GN_Vk.h"
#include "vkInternal.h"

#include <cstdio>

namespace GNg {
namespace {

uint32_t s_updatesTotal = 0;
uint32_t s_updatesWhileBound = 0;
const char *s_firstSite = nullptr;
const char *s_firstSetName = nullptr;
// 规范集内容代际号：任一规范集被写入就 +1。纹理环集靠它跳过无关帧的全量重同步。
uint32_t s_canonicalGen = 1;

// 引擎的规范描述符集：这些会在渲染通道内被绑定，因此不能再改写。
const char *canonicalSetName(VkDescriptorSet set) {
    if(set == VK_NULL_HANDLE) return nullptr;
    if(set == GN_Vk.descriptorSet3D) return "descriptorSet3D";
    if(set == GN_Vk.descriptorSetPBR) return "descriptorSetPBR";
    if(set == GN_Vk.gBufferDescriptorSet) return "gBufferDescriptorSet";
    if(set == GN_Vk.deferredLightDescriptorSet) return "deferredLightDescriptorSet";
    if(set == GN_Vk.compositeDescriptorSet) return "compositeDescriptorSet";
    if(set == GN_Vk.skyboxDescriptorSet) return "skyboxDescriptorSet";
    if(set == GN_Vk.ssaoDescriptorSet) return "ssaoDescriptorSet";
    if(set == GN_Vk.ssgiDescriptorSet) return "ssgiDescriptorSet";
    return nullptr;
}

} // namespace

void GuardedUpdateDescriptorSets(const char *file, int line, VkDevice device,
                                 uint32_t writeCount, const VkWriteDescriptorSet *writes,
                                 uint32_t copyCount, const VkCopyDescriptorSet *copies) {
    ++s_updatesTotal;
    {
        bool touchedCanonical = false;
        for(uint32_t i = 0; i < writeCount && !touchedCanonical; ++i) {
            touchedCanonical = canonicalSetName(writes[i].dstSet) != nullptr;
        }
        for(uint32_t i = 0; i < copyCount && !touchedCanonical; ++i) {
            touchedCanonical = canonicalSetName(copies[i].dstSet) != nullptr;
        }
        if(touchedCanonical) ++s_canonicalGen;
    }
    if(s_inRenderPass) {
        const char *hit = nullptr;
        for(uint32_t i = 0; i < writeCount && hit == nullptr; ++i) {
            hit = canonicalSetName(writes[i].dstSet);
        }
        for(uint32_t i = 0; i < copyCount && hit == nullptr; ++i) {
            hit = canonicalSetName(copies[i].dstSet);
        }
        if(hit != nullptr) {
            ++s_updatesWhileBound;
            if(s_firstSite == nullptr) {
                s_firstSite = file;
                s_firstSetName = hit;
                fprintf(stderr,
                        "[GN_Vk] 违规: 渲染通道内改写已绑定的描述符集 %s（%s:%d）—— "
                        "会触发 VUID-vkUpdateDescriptorSets-None-03047\n",
                        hit, file, line);
            }
        }
    }
    vkUpdateDescriptorSets(device, writeCount, writes, copyCount, copies);
}

uint32_t GN_VkCanonicalDescGeneration() { return s_canonicalGen; }

uint32_t GN_VkDescriptorUpdatesTotal() { return s_updatesTotal; }

uint32_t GN_VkDescriptorUpdatesWhileBound() { return s_updatesWhileBound; }

const char *GN_VkDescriptorUpdateSite() { return s_firstSite; }

const char *GN_VkDescriptorUpdateSetName() { return s_firstSetName; }

} // namespace GNg
