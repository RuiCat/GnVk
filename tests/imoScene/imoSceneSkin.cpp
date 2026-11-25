// tests/imoScene/imoSceneSkin.cpp — GNAC 剪辑读取、逐帧调色板、CPU 重蒙皮。
//
// 职责：把「每关节每帧局部旋转 + 逆绑定矩阵」变成调色板，再把绑定顶点重算成 VkVertex3D。
// 不做：glTF 解析（imoSceneConvert）、GPU 上传与绘制（imoSceneUpload / imoSceneMain）。

#include "imoSceneSkin.h"

#include "GN_3Dskeleton.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace imoScene {

namespace {

// GNAC 文件头：magic(4) + uint32 版本 + uint32 通道数 + uint32 帧数 + float fps。
const uint32_t GNAC_HEADER_BYTES = 20;
const int32_t GNAC_PATH_ROTATION = 1;

bool readU32(const unsigned char *data, size_t size, size_t offset, uint32_t &out) {
    if (offset + 4 > size) {
        return false;
    }
    std::memcpy(&out, data + offset, 4);
    return true;
}

GNg::sQUAT normalizeQuat(GNg::sQUAT q) {
    const float lengthSq = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    if (lengthSq <= 1.0e-20f) {
        GNg::sQUAT identity;
        return identity;
    }
    const float inv = 1.0f / std::sqrt(lengthSq);
    q.x *= inv;
    q.y *= inv;
    q.z *= inv;
    q.w *= inv;
    return q;
}

// 两帧之间归一化线性插值（帧距 1/30 s，角度差很小，nlerp 与 slerp 的差异不可见）。
GNg::sQUAT blendQuat(const GNg::sQUAT &a, const GNg::sQUAT &b, float t) {
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    const float sign = (dot < 0.0f) ? -1.0f : 1.0f;
    GNg::sQUAT out;
    out.x = a.x + (b.x * sign - a.x) * t;
    out.y = a.y + (b.y * sign - a.y) * t;
    out.z = a.z + (b.z * sign - a.z) * t;
    out.w = a.w + (b.w * sign - a.w) * t;
    return normalizeQuat(out);
}

int32_t jointOfNode(const sSkinRig &rig, uint32_t node) {
    for (size_t joint = 0; joint < rig.aJointNodes.size(); ++joint) {
        if (rig.aJointNodes[joint] == node) {
            return static_cast<int32_t>(joint);
        }
    }
    return -1;
}

GNg::sVEC3 transformPoint(const GNg::sMAT44 &m, const GNg::sVEC3 &p) {
    GNg::sVEC3 out;
    out.x = m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12];
    out.y = m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13];
    out.z = m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14];
    return out;
}

GNg::sVEC3 transformDirection(const GNg::sMAT44 &m, const GNg::sVEC3 &v) {
    GNg::sVEC3 out;
    out.x = m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z;
    out.y = m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z;
    out.z = m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z;
    return out;
}

} // namespace

sSkinClip::sSkinClip() : aFrameCount(0), aFps(30.0f) {}

bool loadSkinClip(const char *path, const sSkinRig &rig, sSkinClip &out, std::string &error) {
    out = sSkinClip();
    error.clear();
    if (path == 0 || path[0] == '\0') {
        error = "剪辑路径为空";
        return false;
    }
    std::FILE *file = std::fopen(path, "rb");
    if (file == 0) {
        error = std::string("打不开剪辑: ") + path;
        return false;
    }
    std::fseek(file, 0, SEEK_END);
    const long length = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (length <= static_cast<long>(GNAC_HEADER_BYTES)) {
        std::fclose(file);
        error = "剪辑文件过短";
        return false;
    }
    std::vector<unsigned char> bytes(static_cast<size_t>(length));
    const size_t read = std::fread(bytes.data(), 1, bytes.size(), file);
    std::fclose(file);
    if (read != bytes.size()) {
        error = "剪辑读取不完整";
        return false;
    }
    if (std::memcmp(bytes.data(), "GNAC", 4) != 0) {
        error = "剪辑 magic 不是 GNAC";
        return false;
    }
    uint32_t version = 0;
    uint32_t channelCount = 0;
    uint32_t frameCount = 0;
    uint32_t fpsBits = 0;
    float fps = 30.0f;
    if (!readU32(bytes.data(), bytes.size(), 4, version) ||
        !readU32(bytes.data(), bytes.size(), 8, channelCount) ||
        !readU32(bytes.data(), bytes.size(), 12, frameCount) ||
        !readU32(bytes.data(), bytes.size(), 16, fpsBits)) {
        error = "剪辑头不完整";
        return false;
    }
    std::memcpy(&fps, &fpsBits, 4);
    if (version != 1 || frameCount == 0 || fps <= 0.0f) {
        error = "剪辑版本/帧数/帧率非法";
        return false;
    }
    out.aFrameCount = frameCount;
    out.aFps = fps;
    out.aRot.assign(rig.aJointNodes.size(), std::vector<GNg::sQUAT>());
    const size_t stride = 8 + static_cast<size_t>(frameCount) * 16;
    if (GNAC_HEADER_BYTES + stride * channelCount > bytes.size()) {
        out = sSkinClip();
        error = "剪辑通道数据越界";
        return false;
    }
    const unsigned char *cursor = bytes.data() + GNAC_HEADER_BYTES;
    for (uint32_t channel = 0; channel < channelCount; ++channel) {
        uint32_t node = 0;
        uint32_t pathType = 0;
        std::memcpy(&node, cursor, 4);
        std::memcpy(&pathType, cursor + 4, 4);
        cursor += 8;
        const int32_t joint = jointOfNode(rig, node);
        const bool keep = joint >= 0 && pathType == static_cast<uint32_t>(GNAC_PATH_ROTATION);
        if (keep) {
            out.aRot[static_cast<size_t>(joint)].resize(frameCount);
        }
        for (uint32_t frame = 0; frame < frameCount; ++frame) {
            GNg::sQUAT q;
            std::memcpy(&q.x, cursor, 4);
            std::memcpy(&q.y, cursor + 4, 4);
            std::memcpy(&q.z, cursor + 8, 4);
            std::memcpy(&q.w, cursor + 12, 4);
            if (keep) {
                out.aRot[static_cast<size_t>(joint)][frame] = q;
            }
            cursor += 16;
        }
    }
    return true;
}

bool buildSkinPalette(const sSkinRig &rig, const sSkinClip &clip, float time,
                      std::vector<GNg::sMAT44> &outPalette) {
    outPalette.clear();
    if (rig.aJointNodes.empty() || rig.aParents.size() != rig.aJointNodes.size() ||
        rig.aBindLocal.size() != rig.aJointNodes.size()) {
        return false;
    }
    if (clip.aRot.size() != rig.aJointNodes.size()) {
        return false;
    }
    const float frames = static_cast<float>(clip.aFrameCount);
    float position = time * clip.aFps;
    if (!(position >= 0.0f)) {
        position = 0.0f;
    }
    // 循环播放：首尾相接（最后一帧回到第 0 帧）。
    position = std::fmod(position, frames);
    const uint32_t frame0 = static_cast<uint32_t>(position);
    const uint32_t frame1 = (frame0 + 1u) % clip.aFrameCount;
    const float blend = position - static_cast<float>(frame0);

    std::vector<GN3d::sANIM_TRANSFORM> local(rig.aJointNodes.size());
    for (size_t joint = 0; joint < rig.aJointNodes.size(); ++joint) {
        local[joint] = rig.aBindLocal[joint];
        const std::vector<GNg::sQUAT> &framesOfJoint = clip.aRot[joint];
        if (framesOfJoint.size() == clip.aFrameCount) {
            local[joint].aRotation = blendQuat(framesOfJoint[frame0], framesOfJoint[frame1], blend);
        }
    }

    GN3d::sSKELETON skeleton;
    skeleton.aJoints.resize(rig.aJointNodes.size());
    for (size_t joint = 0; joint < rig.aJointNodes.size(); ++joint) {
        skeleton.aJoints[joint].aParent = rig.aParents[joint];
        skeleton.aJoints[joint].aInverseBind = rig.aInverseBinds[joint];
    }
    return skeleton.computePalette(local, outPalette);
}

void skinBindVertices(const std::vector<sBindVertex> &bind, const std::vector<GNg::sMAT44> &palette,
                      const GNg::sVEC3 &offset, float *outVertices) {
    if (outVertices == 0 || bind.empty() || palette.empty()) {
        return;
    }
    for (size_t index = 0; index < bind.size(); ++index) {
        const sBindVertex &source = bind[index];
        GNg::sVEC3 position;
        position.x = position.y = position.z = 0.0f;
        GNg::sVEC3 normal;
        normal.x = normal.y = normal.z = 0.0f;
        float weightSum = 0.0f;
        for (int slot = 0; slot < 4; ++slot) {
            const float weight = source.aWeights[slot];
            const uint32_t joint = source.aJoints[slot];
            if (weight <= 0.0f || static_cast<size_t>(joint) >= palette.size()) {
                continue;
            }
            const GNg::sMAT44 &matrix = palette[joint];
            const GNg::sVEC3 p = transformPoint(matrix, source.aPosition);
            position.x += p.x * weight;
            position.y += p.y * weight;
            position.z += p.z * weight;
            const GNg::sVEC3 n = transformDirection(matrix, source.aNormal);
            normal.x += n.x * weight;
            normal.y += n.y * weight;
            normal.z += n.z * weight;
            weightSum += weight;
        }
        if (weightSum > 1.0e-6f) {
            position.x /= weightSum;
            position.y /= weightSum;
            position.z /= weightSum;
        } else {
            position = source.aPosition;
        }
        const float lengthSq = normal.x * normal.x + normal.y * normal.y + normal.z * normal.z;
        if (lengthSq > 1.0e-12f) {
            const float inv = 1.0f / std::sqrt(lengthSq);
            normal.x *= inv;
            normal.y *= inv;
            normal.z *= inv;
        }
        float *vertex = outVertices + index * 11u;
        vertex[0] = position.x + offset.x;
        vertex[1] = position.y + offset.y;
        vertex[2] = position.z + offset.z;
        vertex[3] = normal.x;
        vertex[4] = normal.y;
        vertex[5] = normal.z;
        vertex[6] = 0.0f;
        vertex[7] = 0.0f;
        vertex[8] = 0.0f;
        vertex[9] = source.aUv[0];
        vertex[10] = source.aUv[1];
    }
}

} // namespace imoScene