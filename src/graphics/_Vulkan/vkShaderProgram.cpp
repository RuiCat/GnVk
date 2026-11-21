// vkShaderProgram.cpp — 程序注册表实现（P2a 元数据 + P5 层入口 SPIR-V 绑定/封口）。
#include <GN.h>

#include "GN_RenderPipeline.h"
#include "GN_Spirv.h"
#include "vkBindingLayout.h"
#include "vkShaderProgram.h"

#include <cstdio>
#include <cstring>

namespace GN3d {
namespace {

const uint32_t kMaxPrograms = kMaxShaderPrograms;
char sNameStorage[kMaxPrograms][64];
sSHADER_PROGRAM sPrograms[kMaxPrograms];
uint8_t sProgramStates[kMaxPrograms];
uint32_t sProgramCount = 0;

void setReason(char *reason, size_t reasonSize, const char *message) {
    if(reason && reasonSize) snprintf(reason, reasonSize, "%s", message);
}

int32_t indexOf(const char *name) {
    if(!name) return -1;
    for(uint32_t i = 0; i < sProgramCount; ++i) {
        if(sPrograms[i].aDesc.aName && std::strcmp(sPrograms[i].aDesc.aName, name) == 0) return (int32_t)i;
    }
    return -1;
}

bool validLayerMask(uint32_t mask) {
    const uint32_t all = (1u << (uint32_t)eLAYER_COUNT) - 1u;
    return mask != 0u && (mask & ~all) == 0u;
}

bool validVertexFormat(eVERTEX_FORMAT format) {
    return format == eVF_STATIC || format == eVF_SKINNED;
}

} // namespace

bool RegisterShaderProgramMeta(const char *name, uint32_t layerMask, eVERTEX_FORMAT vertexFormat,
                               eSKIN_DATA skinData, uint32_t flags, uint32_t state,
                               char *reason, size_t reasonSize) {
    if(reason && reasonSize) reason[0] = 0;
    if(!name || name[0] == 0) { setReason(reason, reasonSize, "program name is empty"); return false; }
    const size_t nameLen = std::strlen(name);
    if(nameLen > 63u) { setReason(reason, reasonSize, "program name exceeds 63 chars"); return false; }
    if(!validLayerMask(layerMask)) { setReason(reason, reasonSize, "layer mask is empty or has unknown bits"); return false; }
    if(!validVertexFormat(vertexFormat)) { setReason(reason, reasonSize, "unknown vertex format"); return false; }
    if(vertexFormat == eVF_SKINNED) {
        if(skinData != eSKIN_UBO && skinData != eSKIN_SSBO) {
            setReason(reason, reasonSize, "skinned program requires UBO or SSBO skin data");
            return false;
        }
    } else if(skinData != eSKIN_NONE) {
        setReason(reason, reasonSize, "static program must not carry skin data");
        return false;
    }
    if((flags & ~(uint32_t)ePROG_IMPLICIT_SHADOW) != 0u) { setReason(reason, reasonSize, "unknown program flags"); return false; }
    if((flags & (uint32_t)ePROG_IMPLICIT_SHADOW) != 0u && (layerMask & (1u << (uint32_t)eLAYER_SHADOW)) != 0u) {
        setReason(reason, reasonSize, "implicit shadow flag conflicts with shadow entry");
        return false;
    }
    if(state != (uint32_t)ePROG_STATE_DECLARED && state != (uint32_t)ePROG_STATE_READY) {
        setReason(reason, reasonSize, "invalid program state");
        return false;
    }
    if(indexOf(name) >= 0) { setReason(reason, reasonSize, "program name already registered"); return false; }
    if(sProgramCount >= kMaxPrograms) { setReason(reason, reasonSize, "program registry is full"); return false; }
    sSHADER_PROGRAM &program = sPrograms[sProgramCount];
    std::memset(&program, 0, sizeof(program));
    std::memcpy(sNameStorage[sProgramCount], name, nameLen + 1u);
    sProgramStates[sProgramCount] = (uint8_t)state;
    program.aId = sProgramCount + 1u;
    program.aDesc.aName = sNameStorage[sProgramCount];
    program.aDesc.aLayerMask = layerMask;
    program.aDesc.aLayoutVersion = kVkBindingLayoutVersion;
    program.aDesc.aVertexFormat = vertexFormat;
    program.aDesc.aSkinData = skinData;
    program.aDesc.aFlags = flags;
    ++sProgramCount;
    return true;
}

const sSHADER_PROGRAM *FindShaderProgram(const char *name) {
    const int32_t idx = indexOf(name);
    return idx < 0 ? 0 : &sPrograms[idx];
}

uint32_t ShaderProgramCount() { return sProgramCount; }

bool BindProgramEntry(const char *name, eRENDER_LAYER layer, const void *vertSpv, uint32_t vertSize,
                      const void *fragSpv, uint32_t fragSize, uint32_t pushConstantBytes,
                      char *reason, size_t reasonSize) {
    if(reason && reasonSize) reason[0] = 0;
    const int32_t idx = indexOf(name);
    if(idx < 0) { setReason(reason, reasonSize, "program is not registered"); return false; }
    if((uint32_t)layer >= (uint32_t)eLAYER_COUNT) { setReason(reason, reasonSize, "unknown render layer"); return false; }
    sSHADER_PROGRAM &program = sPrograms[idx];
    if((program.aDesc.aLayerMask & (1u << (uint32_t)layer)) == 0u) {
        setReason(reason, reasonSize, "layer is not in the program mask");
        return false;
    }
    if(pushConstantBytes > kMaxPushConstantBytes) { setReason(reason, reasonSize, "push constant exceeds 128 bytes"); return false; }
    if(!vertSpv || vertSize == 0u) { setReason(reason, reasonSize, "entry needs a vertex spir-v module"); return false; }
    char detail[160];
    if(!validateSpirvModule(vertSpv, vertSize, detail, sizeof(detail))) { setReason(reason, reasonSize, detail); return false; }
    if(fragSize > 0u) {
        if(!fragSpv) { setReason(reason, reasonSize, "fragment size without a fragment module"); return false; }
        if(!validateSpirvModule(fragSpv, fragSize, detail, sizeof(detail))) { setReason(reason, reasonSize, detail); return false; }
    }
    sSHADER_ENTRY &entry = program.aDesc.aEntries[(uint32_t)layer];
    std::memset(&entry, 0, sizeof(entry));
    entry.aBlend = -1;
    entry.aDepthTest = -1;
    entry.aDepthWrite = -1;
    entry.aCullBack = -1;
    entry.aTopology = -1;
    entry.aVertSpv = vertSpv;
    entry.aVertSize = vertSize;
    entry.aFragSpv = fragSize > 0u ? fragSpv : 0;
    entry.aFragSize = fragSize;
    entry.aPushConstantSize = pushConstantBytes;
    return true;
}

bool SealShaderProgram(const char *name, char *reason, size_t reasonSize) {
    if(reason && reasonSize) reason[0] = 0;
    const int32_t idx = indexOf(name);
    if(idx < 0) { setReason(reason, reasonSize, "program is not registered"); return false; }
    char detail[160];
    if(!ValidateShaderProgramDesc(sPrograms[idx].aDesc, detail, sizeof(detail))) { setReason(reason, reasonSize, detail); return false; }
    sProgramStates[idx] = (uint8_t)ePROG_STATE_READY;
    return true;
}

uint32_t ShaderProgramState(const char *name) {
    const int32_t idx = indexOf(name);
    return idx < 0 ? 0u : (uint32_t)sProgramStates[idx];
}

bool RegisterBuiltinShaderPrograms(char *reason, size_t reasonSize) {
    if(FindShaderProgram("gn.pbr") && FindShaderProgram("gn.depth")) return true;
    const uint32_t pbrMask = (1u << (uint32_t)eLAYER_GBUFFER) | (1u << (uint32_t)eLAYER_FORWARD_OPAQUE) |
                             (1u << (uint32_t)eLAYER_FORWARD_ALPHA);
    const uint32_t before = sProgramCount;
    if(FindShaderProgram("gn.pbr") == 0) {
        if(!RegisterShaderProgramMeta("gn.pbr", pbrMask, eVF_STATIC, eSKIN_NONE, 0u,
                                      (uint32_t)ePROG_STATE_DECLARED, reason, reasonSize)) {
            sProgramCount = before;
            return false;
        }
    }
    if(FindShaderProgram("gn.depth") == 0) {
        if(!RegisterShaderProgramMeta("gn.depth", 1u << (uint32_t)eLAYER_SHADOW, eVF_STATIC, eSKIN_NONE, 0u,
                                      (uint32_t)ePROG_STATE_DECLARED, reason, reasonSize)) {
            sProgramCount = before;
            return false;
        }
    }
    return true;
}

void ResetShaderPrograms() {
    std::memset(sPrograms, 0, sizeof(sPrograms));
    std::memset(sNameStorage, 0, sizeof(sNameStorage));
    std::memset(sProgramStates, 0, sizeof(sProgramStates));
    sProgramCount = 0;
}

const char *ProgramStateName(uint32_t state) {
    switch(state) {
        case (uint32_t)ePROG_STATE_EMPTY: return "empty";
        case (uint32_t)ePROG_STATE_DECLARED: return "declared";
        case (uint32_t)ePROG_STATE_READY: return "ready";
        default: return "invalid";
    }
}

} // namespace GN3d
