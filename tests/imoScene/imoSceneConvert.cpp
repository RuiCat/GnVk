// tests/imoScene/imoSceneConvert.cpp — glTF/GLB → 烘焙网格（CPU，无 Vulkan）。
//
// 职责：读入 GN3d::sASSET，把蒙皮顶点按导入姿势烘焙进世界坐标，
//       按贴图 alpha 分布重判材质 alpha 模式，并统一重定心（脚底 y=0、xz 居中）。
// 不做：GPU 上传、贴图解码成 sIMAGE、任何绘制（分别见 imoSceneUpload / imoSceneMain）。

#include "imoSceneConvert.h"

#include "GN_3Dasset.h"
#include "GN_3Dscene.h"
#include "GN_3Dtexture.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <tuple>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace imoScene {

namespace {

// 顶点浮点数 = VkVertex3D 的 11 个 float（pos3 / normal3 / tangent3 / uv2）。
const size_t VERTEX_FLOATS = 11;
// alpha test 阈值：与 glTF 默认一致，头发/镂空衣物按硬边裁剪。
const float ALPHA_CUTOFF = 0.5f;
// 半透明像素占比超过该值才走 BLEND；其余 alpha 视为二值镂空 → MASK。
const float PARTIAL_ALPHA_BLEND_THRESHOLD = 0.30f;
// 权重和下限：低于该值的顶点视为未绑定，保留原始位置。
const float MIN_WEIGHT_SUM = 1.0e-6f;

// 列主序 4x4 变换点（含平移）。
GNg::sVEC3 transformPoint(const GNg::sMAT44 &mat, const GNg::sVEC3 &p) {
    GNg::sVEC3 out;
    out.x = mat.m[0] * p.x + mat.m[4] * p.y + mat.m[8] * p.z + mat.m[12];
    out.y = mat.m[1] * p.x + mat.m[5] * p.y + mat.m[9] * p.z + mat.m[13];
    out.z = mat.m[2] * p.x + mat.m[6] * p.y + mat.m[10] * p.z + mat.m[14];
    return out;
}

// 列主序 4x4 变换方向（不含平移）。
GNg::sVEC3 transformDirection(const GNg::sMAT44 &mat, const GNg::sVEC3 &v) {
    GNg::sVEC3 out;
    out.x = mat.m[0] * v.x + mat.m[4] * v.y + mat.m[8] * v.z;
    out.y = mat.m[1] * v.x + mat.m[5] * v.y + mat.m[9] * v.z;
    out.z = mat.m[2] * v.x + mat.m[6] * v.y + mat.m[10] * v.z;
    return out;
}

bool isFiniteVec(const GNg::sVEC3 &v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// 逗号分隔的子串匹配（大小写不敏感）：空列表返回 true。
bool matchesNameFilter(const std::string &name, const std::string &filter) {
    if (filter.empty()) {
        return true;
    }
    std::string lowerName = name;
    for (size_t i = 0; i < lowerName.size(); ++i) {
        lowerName[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowerName[i])));
    }
    size_t start = 0;
    while (start <= filter.size()) {
        const size_t comma = filter.find(',', start);
        const size_t end = (comma == std::string::npos) ? filter.size() : comma;
        std::string token = filter.substr(start, end - start);
        for (size_t i = 0; i < token.size(); ++i) {
            token[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(token[i])));
        }
        if (!token.empty() && lowerName.find(token) != std::string::npos) {
            return true;
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return false;
}

// 贴图 alpha 分布 → 引擎 alpha 模式：
//   * 无 alpha 像素 → 源模式（本资产为 OPAQUE）；
//   * 半透明像素占比 > 30%（Toumei 这类玻璃/薄纱）→ BLEND；
//   * 其余是二值镂空（衣料外侧 alpha=0、本体 1）→ MASK（Blender CLIP 渲染与 BLEND 结果一致，
//     比 BLEND 少一次透明排序、且能写深度）。
GN3d::eALPHA_MODE resolveAlphaMode(const sTextureData &texture, GN3d::eALPHA_MODE source) {
    if (source == GN3d::eALPHA_OPAQUE) {
        return source;
    }
    // 贴图没有 alpha 通道/全不透明：内容本来就是实心的，必须按 OPAQUE 画。
    // 否则 glTF 的 BLEND 会让这些网格走“深度写关”的混合管线——远侧表面会盖住近侧
    // （帽子压住头发、衣料互相穿透），表现就是“远处遮挡近处”。
    if (!texture.aHasAlpha) {
        return GN3d::eALPHA_OPAQUE;
    }
    if (texture.aPartialAlphaRatio > PARTIAL_ALPHA_BLEND_THRESHOLD) {
        return GN3d::eALPHA_BLEND;
    }
    return GN3d::eALPHA_MASK;
}

// 解出贴图的 alpha 统计；无法解码时按无 alpha 处理（仍然把原始字节留给上传层）。
void measureTextureAlpha(const GN3d::sASSET_TEXTURE &source, sTextureData &out) {
    out.aHasAlpha = false;
    out.aPartialAlphaRatio = 0.0f;
    GN3d::sTEXTURE_IMAGE image;
    if (!GN3d::decodeAssetTexture(source, image) || image.aWidth == 0 || image.aHeight == 0) {
        return;
    }
    const size_t total = static_cast<size_t>(image.aWidth) * static_cast<size_t>(image.aHeight);
    if (total == 0 || image.aPixels.size() < total * 4) {
        return;
    }
    size_t zeroPixels = 0;
    size_t partialPixels = 0;
    for (size_t pixel = 0; pixel < total; ++pixel) {
        const unsigned alpha = image.aPixels[pixel * 4 + 3];
        if (alpha == 0) {
            ++zeroPixels;
        } else if (alpha < 250) {
            ++partialPixels;
        }
    }
    out.aHasAlpha = (zeroPixels + partialPixels) > 0;
    out.aPartialAlphaRatio = static_cast<float>(partialPixels) / static_cast<float>(total);
}

// 非蒙皮网格若挂在骨骼下，找它最近的 skin 关节（-1 = 没有）：动画时用该关节的调色板刚性跟随。
// 典型物件：项圈、发饰、镜片 —— 在 glTF 里没有 skin，但父节点是骨头。
int32_t nearestSkinJoint(const GN3d::sASSET &asset, uint32_t skinIndex, int32_t nodeIndex) {
    if (skinIndex >= asset.aSkins.size()) {
        return -1;
    }
    const GN3d::sASSET_SKIN &skin = asset.aSkins[skinIndex];
    int32_t current = nodeIndex;
    while (current >= 0 && static_cast<size_t>(current) < asset.aNodes.size()) {
        for (size_t joint = 0; joint < skin.aJoints.size(); ++joint) {
            if (static_cast<int32_t>(skin.aJoints[joint]) == current) {
                return static_cast<int32_t>(joint);
            }
        }
        current = asset.aNodes[static_cast<size_t>(current)].aParent;
    }
    return -1;
}

// 逆绑定矩阵 → 关节的绑定世界位置（刚性矩阵：p = -R^T t）。
GNg::sVEC3 inverseBindPosition(const GNg::sMAT44 &inverseBind) {
    const float tx = inverseBind.m[12];
    const float ty = inverseBind.m[13];
    const float tz = inverseBind.m[14];
    GNg::sVEC3 out;
    out.x = -(inverseBind.m[0] * tx + inverseBind.m[1] * ty + inverseBind.m[2] * tz);
    out.y = -(inverseBind.m[4] * tx + inverseBind.m[5] * ty + inverseBind.m[6] * tz);
    out.z = -(inverseBind.m[8] * tx + inverseBind.m[9] * ty + inverseBind.m[10] * tz);
    return out;
}

// 没有骨骼父级的物件（项圈、发饰等挂在 Armature 下、顶点已烘进世界坐标）：
// 按顶点质心选最近的关节，让它们至少跟着身体动，而不是留在原地。
int32_t nearestJointToCentroid(const GN3d::sASSET &asset, uint32_t skinIndex,
                               const std::vector<sBindVertex> &vertices) {
    if (skinIndex >= asset.aSkins.size() || vertices.empty()) {
        return -1;
    }
    const GN3d::sASSET_SKIN &skin = asset.aSkins[skinIndex];
    GNg::sVEC3 centroid = {0.0f, 0.0f, 0.0f};
    for (size_t index = 0; index < vertices.size(); ++index) {
        centroid.x += vertices[index].aPosition.x;
        centroid.y += vertices[index].aPosition.y;
        centroid.z += vertices[index].aPosition.z;
    }
    const float inverseCount = 1.0f / static_cast<float>(vertices.size());
    centroid.x *= inverseCount;
    centroid.y *= inverseCount;
    centroid.z *= inverseCount;
    int32_t best = -1;
    float bestDistance = 1.0e30f;
    for (size_t joint = 0; joint < skin.aJoints.size(); ++joint) {
        const GNg::sVEC3 position = inverseBindPosition(skin.aInverseBinds[joint]);
        const float dx = position.x - centroid.x;
        const float dy = position.y - centroid.y;
        const float dz = position.z - centroid.z;
        const float distance = dx * dx + dy * dy + dz * dz;
        if (distance < bestDistance) {
            bestDistance = distance;
            best = static_cast<int32_t>(joint);
        }
    }
    return best;
}

// 法线平滑：FBX 导出的身体网格在同位置上有多份顶点、每份带不同面法线（实测 1024 个重复顶点、
// 951 处法线不同），画出来就是硬边的多边形手指。这里按「同位置」合并，夹角不超过
// 阈值（默认 60°）就取面积加权平均法线；超过阈值（真硬边，如领口/鞋底）保持原样。
// 统计：跨网格累加，便于确认平滑真的生效（IMO_SCENE_SMOOTH_DEBUG=1 时打印）。
size_t gSmoothedGroups = 0;

bool smoothMeshNormals(sMeshData &mesh, float thresholdCos) {
    const size_t vertexCount = mesh.aVertices.size() / 11u;
    if (vertexCount == 0 || mesh.aIndices.size() < 3) {
        return false;
    }
    const size_t triangleCount = mesh.aIndices.size() / 3u;
    std::vector<GNg::sVEC3> faceNormal(triangleCount);
    std::vector<float> faceArea(triangleCount);
    for (size_t triangle = 0; triangle < triangleCount; ++triangle) {
        const uint32_t i0 = mesh.aIndices[triangle * 3 + 0];
        const uint32_t i1 = mesh.aIndices[triangle * 3 + 1];
        const uint32_t i2 = mesh.aIndices[triangle * 3 + 2];
        if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount) {
            return false;
        }
        const float *p0 = &mesh.aVertices[i0 * 11u];
        const float *p1 = &mesh.aVertices[i1 * 11u];
        const float *p2 = &mesh.aVertices[i2 * 11u];
        const float ax = p1[0] - p0[0], ay = p1[1] - p0[1], az = p1[2] - p0[2];
        const float bx = p2[0] - p0[0], by = p2[1] - p0[1], bz = p2[2] - p0[2];
        const float nx = ay * bz - az * by;
        const float ny = az * bx - ax * bz;
        const float nz = ax * by - ay * bx;
        const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
        faceArea[triangle] = length;
        if (length > 1.0e-12f) {
            faceNormal[triangle] = {nx / length, ny / length, nz / length};
        } else {
            faceNormal[triangle] = {0.0f, 0.0f, 0.0f};
        }
    }
    std::vector<std::vector<uint32_t>> incident(vertexCount);
    for (size_t triangle = 0; triangle < triangleCount; ++triangle) {
        for (int corner = 0; corner < 3; ++corner) {
            incident[mesh.aIndices[triangle * 3 + corner]].push_back(static_cast<uint32_t>(triangle));
        }
    }
    // 同位置分组（量化到 0.1 mm）。
    std::map<std::tuple<int, int, int>, std::vector<uint32_t>> groups;
    for (size_t index = 0; index < vertexCount; ++index) {
        const float *vertex = &mesh.aVertices[index * 11u];
        const std::tuple<int, int, int> key(
            static_cast<int>(std::lround(vertex[0] * 10000.0f)),
            static_cast<int>(std::lround(vertex[1] * 10000.0f)),
            static_cast<int>(std::lround(vertex[2] * 10000.0f)));
        groups[key].push_back(static_cast<uint32_t>(index));
    }
    size_t smoothed = 0;
    for (auto &entry : groups) {
        std::vector<uint32_t> &members = entry.second;
        if (members.size() < 2) {
            continue;
        }
        // 面积加权平均 + 最大偏差检查。
        GNg::sVEC3 sum = {0.0f, 0.0f, 0.0f};
        std::vector<uint32_t> faces;
        for (size_t member = 0; member < members.size(); ++member) {
            const std::vector<uint32_t> &list = incident[members[member]];
            for (size_t f = 0; f < list.size(); ++f) {
                faces.push_back(list[f]);
            }
        }
        std::sort(faces.begin(), faces.end());
        faces.erase(std::unique(faces.begin(), faces.end()), faces.end());
        for (size_t f = 0; f < faces.size(); ++f) {
            const uint32_t triangle = faces[f];
            const float weight = faceArea[triangle];
            sum.x += faceNormal[triangle].x * weight;
            sum.y += faceNormal[triangle].y * weight;
            sum.z += faceNormal[triangle].z * weight;
        }
        const float length = std::sqrt(sum.x * sum.x + sum.y * sum.y + sum.z * sum.z);
        if (length <= 1.0e-12f) {
            continue;
        }
        const GNg::sVEC3 average = {sum.x / length, sum.y / length, sum.z / length};
        bool hardEdge = false;
        for (size_t f = 0; f < faces.size(); ++f) {
            const GNg::sVEC3 &normal = faceNormal[faces[f]];
            const float dot = normal.x * average.x + normal.y * average.y + normal.z * average.z;
            if (dot < thresholdCos) {
                hardEdge = true;
                break;
            }
        }
        if (hardEdge) {
            continue;
        }
        for (size_t member = 0; member < members.size(); ++member) {
            const uint32_t index = members[member];
            mesh.aVertices[index * 11u + 3] = average.x;
            mesh.aVertices[index * 11u + 4] = average.y;
            mesh.aVertices[index * 11u + 5] = average.z;
        }
        ++smoothed;
    }
    gSmoothedGroups += smoothed;
    return smoothed > 0;
}

// 烘焙一个 primitive 的顶点/索引；返回 false 表示该 primitive 不可用（已计入 skipped）。
bool bakePrimitive(const GN3d::sASSET &asset, const GN3d::sASSET_PRIMITIVE &primitive,
                   bool skinned, const std::vector<GNg::sMAT44> *palette,
                   const GNg::sMAT44 &nodeWorld, sModelData &out, const std::string &meshName,
                   int32_t followJoint) {
    if (primitive.aVertices.empty()) {
        return false;
    }
    sMeshData mesh;
    mesh.aName = meshName;
    mesh.aMaterial = primitive.aMaterial;
    mesh.aVertices.resize(primitive.aVertices.size() * VERTEX_FLOATS);
    mesh.aMin.x = mesh.aMin.y = mesh.aMin.z = 1.0e30f;
    mesh.aMax.x = mesh.aMax.y = mesh.aMax.z = -1.0e30f;

    for (size_t index = 0; index < primitive.aVertices.size(); ++index) {
        const GN3d::sASSET_VERTEX &source = primitive.aVertices[index];
        GNg::sVEC3 position = {source.aPosition.x, source.aPosition.y, source.aPosition.z};
        GNg::sVEC3 normal = {source.aNormal.x, source.aNormal.y, source.aNormal.z};
        if (skinned) {
            GNg::sVEC3 sum = {0.0f, 0.0f, 0.0f};
            GNg::sVEC3 normalSum = {0.0f, 0.0f, 0.0f};
            float weightSum = 0.0f;
            for (int slot = 0; slot < 4; ++slot) {
                const float weight = source.aWeights[slot];
                const uint32_t joint = source.aJoints[slot];
                if (weight <= 0.0f || joint >= palette->size()) {
                    continue;
                }
                const GNg::sMAT44 &matrix = (*palette)[joint];
                const GNg::sVEC3 p = transformPoint(matrix, position);
                sum.x += p.x * weight;
                sum.y += p.y * weight;
                sum.z += p.z * weight;
                const GNg::sVEC3 n = transformDirection(matrix, normal);
                normalSum.x += n.x * weight;
                normalSum.y += n.y * weight;
                normalSum.z += n.z * weight;
                weightSum += weight;
            }
            if (weightSum > MIN_WEIGHT_SUM) {
                position.x = sum.x / weightSum;
                position.y = sum.y / weightSum;
                position.z = sum.z / weightSum;
                normal = normalSum;
            } else {
                ++out.aZeroWeightVertices;
            }
        } else {
            position = transformPoint(nodeWorld, position);
            normal = transformDirection(nodeWorld, normal);
        }
        if (!isFiniteVec(position)) {
            position.x = position.y = position.z = 0.0f;
        }
        const float lengthSq = normal.x * normal.x + normal.y * normal.y + normal.z * normal.z;
        if (lengthSq > 1.0e-12f) {
            const float invLength = 1.0f / std::sqrt(lengthSq);
            normal.x *= invLength;
            normal.y *= invLength;
            normal.z *= invLength;
        } else {
            normal.x = normal.y = normal.z = 0.0f;
        }
        float *vertex = &mesh.aVertices[index * VERTEX_FLOATS];
        vertex[0] = position.x;
        vertex[1] = position.y;
        vertex[2] = position.z;
        vertex[3] = normal.x;
        vertex[4] = normal.y;
        vertex[5] = normal.z;
        vertex[6] = vertex[7] = vertex[8] = 0.0f; // 切线留给着色器回退
        vertex[9] = source.aUv[0];
        vertex[10] = source.aUv[1];
        if (skinned) {
            // 绑定姿势 + 关节/权重：动画时用同一份数据重算顶点（顶点本身在绑定时 == 世界位置）。
            sBindVertex bindVertex;
            bindVertex.aPosition = position;
            bindVertex.aNormal = normal;
            bindVertex.aUv[0] = source.aUv[0];
            bindVertex.aUv[1] = source.aUv[1];
            for (int slot = 0; slot < 4; ++slot) {
                bindVertex.aJoints[slot] = source.aJoints[slot];
                bindVertex.aWeights[slot] = source.aWeights[slot];
            }
            mesh.aBindVertices.push_back(bindVertex);
        } else if (!skinned) {
            // 静态网格：顶点已烘进世界坐标，先登记，循环结束后统一选一个关节刚性跟随。
            sBindVertex bindVertex;
            bindVertex.aPosition = position;
            bindVertex.aNormal = normal;
            bindVertex.aUv[0] = source.aUv[0];
            bindVertex.aUv[1] = source.aUv[1];
            mesh.aBindVertices.push_back(bindVertex);
        }
        if (position.x < mesh.aMin.x) mesh.aMin.x = position.x;
        if (position.y < mesh.aMin.y) mesh.aMin.y = position.y;
        if (position.z < mesh.aMin.z) mesh.aMin.z = position.z;
        if (position.x > mesh.aMax.x) mesh.aMax.x = position.x;
        if (position.y > mesh.aMax.y) mesh.aMax.y = position.y;
        if (position.z > mesh.aMax.z) mesh.aMax.z = position.z;
    }

    if (!skinned && !mesh.aBindVertices.empty()) {
        int32_t joint = followJoint;
        if (joint < 0) {
            joint = nearestJointToCentroid(asset, 0, mesh.aBindVertices);
        }
        if (joint >= 0) {
            for (size_t index = 0; index < mesh.aBindVertices.size(); ++index) {
                mesh.aBindVertices[index].aJoints[0] = static_cast<uint16_t>(joint);
                mesh.aBindVertices[index].aWeights[0] = 1.0f;
            }
        } else {
            mesh.aBindVertices.clear(); // 没有骨架可跟：保持原来的静态烘焙
        }
    }

    if (primitive.aIndices.empty()) {
        mesh.aIndices.resize(primitive.aVertices.size());
        for (size_t index = 0; index < primitive.aVertices.size(); ++index) {
            mesh.aIndices[index] = static_cast<uint32_t>(index);
        }
    } else {
        mesh.aIndices.assign(primitive.aIndices.begin(), primitive.aIndices.end());
    }
    // 法线平滑（默认 60° 阈值；IMO_SCENE_SMOOTH=0 关闭）。只处理带绑定顶点的角色网格，
    // 房间/静态场景保持原法线（建筑硬边被平滑会改观感）。
    const char *smoothEnv = std::getenv("IMO_SCENE_SMOOTH");
    if (!mesh.aBindVertices.empty() && !(smoothEnv != 0 && smoothEnv[0] == '0')) {
        const char *angleEnv = std::getenv("IMO_SCENE_SMOOTH_ANGLE");
        const float angleDeg =
            (angleEnv != 0) ? static_cast<float>(std::atof(angleEnv)) : 60.0f;
        const float thresholdCos =
            std::cos(angleDeg * 3.14159265358979f / 180.0f);
        smoothMeshNormals(mesh, thresholdCos);
    }
    // 同步到绑定顶点：动画时用同一份法线。
    for (size_t index = 0; index < mesh.aBindVertices.size() && index * 11u + 5 < mesh.aVertices.size();
         ++index) {
        mesh.aBindVertices[index].aNormal.x = mesh.aVertices[index * 11u + 3];
        mesh.aBindVertices[index].aNormal.y = mesh.aVertices[index * 11u + 4];
        mesh.aBindVertices[index].aNormal.z = mesh.aVertices[index * 11u + 5];
    }
    out.aVertexCount += static_cast<uint32_t>(primitive.aVertices.size());
    out.aTriangleCount += static_cast<uint32_t>(mesh.aIndices.size() / 3);
    out.aMeshes.push_back(mesh);
    return true;
}

// 逐节点烘焙：蒙皮节点用调色板，静态节点用节点世界矩阵（glTF 规范：蒙皮忽略节点变换）。
void bakeNodes(const GN3d::sASSET &asset, const std::vector<GNg::sMAT44> &world,
               const sConvertOptions &options, sModelData &out) {
    std::vector<std::vector<GNg::sMAT44>> palettes(asset.aSkins.size());
    std::vector<bool> paletteReady(asset.aSkins.size(), false);
    std::vector<GN3d::sANIM_TRANSFORM> localPose;
    if (!GN3d::sampleLocalPose(asset, 0, 0.0f, localPose)) {
        localPose.clear();
    }
    for (size_t nodeIndex = 0; nodeIndex < asset.aNodes.size(); ++nodeIndex) {
        const GN3d::sASSET_NODE &node = asset.aNodes[nodeIndex];
        if (node.aMesh < 0 || static_cast<size_t>(node.aMesh) >= asset.aMeshes.size()) {
            continue;
        }
        bool skinned = node.aSkin >= 0 && static_cast<size_t>(node.aSkin) < asset.aSkins.size();
        const std::vector<GNg::sMAT44> *palette = 0;
        if (skinned) {
            const size_t skinIndex = static_cast<size_t>(node.aSkin);
            if (!paletteReady[skinIndex]) {
                paletteReady[skinIndex] = true;
                if (!localPose.empty()) {
                    GN3d::computeSkinPalette(asset, static_cast<uint32_t>(skinIndex), localPose,
                                             palettes[skinIndex]);
                }
            }
            if (palettes[skinIndex].empty()) {
                skinned = false; // 调色板不可用：退回静态节点矩阵，至少不丢几何
            } else {
                palette = &palettes[skinIndex];
            }
        }
        const GN3d::sASSET_MESH &mesh = asset.aMeshes[static_cast<size_t>(node.aMesh)];
        for (size_t primitiveIndex = 0; primitiveIndex < mesh.aPrimitives.size();
             ++primitiveIndex) {
            std::string meshName = node.aName;
            if (mesh.aPrimitives.size() > 1) {
                char suffix[16];
                std::snprintf(suffix, sizeof(suffix), "#%zu", primitiveIndex);
                meshName += suffix;
            }
            const bool shown = options.aShowFilter.empty() ||
                               matchesNameFilter(meshName, options.aShowFilter);
            const bool hidden = !options.aHideFilter.empty() &&
                                matchesNameFilter(meshName, options.aHideFilter);
            if (!shown || hidden) {
                ++out.aFilteredMeshes;
                continue;
            }
            const int32_t followJoint =
                skinned ? -1 : nearestSkinJoint(asset, 0, static_cast<int32_t>(nodeIndex));
            if (!bakePrimitive(asset, mesh.aPrimitives[primitiveIndex], skinned, palette,
                               world[nodeIndex], out, meshName, followJoint)) {
                ++out.aSkippedPrimitives;
            }
        }
    }
}

// 贴图：复制压缩字节并统计 alpha。
void bakeTextures(const GN3d::sASSET &asset, sModelData &out) {
    out.aTextures.resize(asset.aTextures.size());
    for (size_t index = 0; index < asset.aTextures.size(); ++index) {
        const GN3d::sASSET_TEXTURE &source = asset.aTextures[index];
        sTextureData &texture = out.aTextures[index];
        texture.aName = source.aName;
        texture.aBytes = source.aImageBytes;
        texture.aWrapS = source.aWrapS;
        texture.aWrapT = source.aWrapT;
        measureTextureAlpha(source, texture);
    }
}

// 材质：贴图 alpha 决定 OPAQUE/MASK/BLEND；无贴图时保留 glTF 原声明（玻璃类仍可 BLEND）。
void bakeMaterials(const GN3d::sASSET &asset, const sConvertOptions &options,
                   sModelData &out) {
    out.aMaterials.resize(asset.aMaterials.size());
    for (size_t index = 0; index < asset.aMaterials.size(); ++index) {
        const GN3d::sASSET_MATERIAL &source = asset.aMaterials[index];
        sMaterialData &material = out.aMaterials[index];
        material.aName = source.aName;
        material.aDiffuse[0] = source.aBaseColorFactor[0];
        material.aDiffuse[1] = source.aBaseColorFactor[1];
        material.aDiffuse[2] = source.aBaseColorFactor[2];
        material.aMetallic = source.aMetallic;
        material.aRoughness = source.aRoughness;
        material.aAlbedoTexture = source.aAlbedoTexture;
        material.aNormalTexture = source.aNormalTexture;
        material.aAlphaCutoff = ALPHA_CUTOFF;
        const bool hasTexture = source.aAlbedoTexture >= 0 &&
                                static_cast<size_t>(source.aAlbedoTexture) < out.aTextures.size();
        // 静态场景的 glTF 材质全是 OPAQUE（Blender 导出未标混合），玻璃/窗纱/软垫的透明
        // 只能从贴图 alpha 反推；resolveAlphaMode 以 BLEND 为输入时才会走那套重判规则。
        const GN3d::eALPHA_MODE sourceMode =
            options.aClassifyAlpha ? GN3d::eALPHA_BLEND : source.aAlphaMode;
        if (options.aForceOpaque) {
            material.aAlphaMode = GN3d::eALPHA_OPAQUE;
        } else if (hasTexture) {
            material.aAlphaMode =
                resolveAlphaMode(out.aTextures[static_cast<size_t>(source.aAlbedoTexture)],
                                 sourceMode);
        } else {
            material.aAlphaMode = sourceMode;
        }
    }
    if (out.aMaterials.empty()) {
        sMaterialData fallback;
        fallback.aName = "fallback";
        out.aMaterials.push_back(fallback);
    }
}

// 只汇总 AABB（不重定心）：房间这类保持世界坐标的模型也要有可打印的包围盒。
void updateModelBounds(sModelData &out) {
    GNg::sVEC3 minimum = {1.0e30f, 1.0e30f, 1.0e30f};
    GNg::sVEC3 maximum = {-1.0e30f, -1.0e30f, -1.0e30f};
    for (size_t meshIndex = 0; meshIndex < out.aMeshes.size(); ++meshIndex) {
        const sMeshData &mesh = out.aMeshes[meshIndex];
        if (mesh.aVertices.empty()) {
            continue;
        }
        if (mesh.aMin.x < minimum.x) minimum.x = mesh.aMin.x;
        if (mesh.aMin.y < minimum.y) minimum.y = mesh.aMin.y;
        if (mesh.aMin.z < minimum.z) minimum.z = mesh.aMin.z;
        if (mesh.aMax.x > maximum.x) maximum.x = mesh.aMax.x;
        if (mesh.aMax.y > maximum.y) maximum.y = mesh.aMax.y;
        if (mesh.aMax.z > maximum.z) maximum.z = mesh.aMax.z;
    }
    if (minimum.x > maximum.x) {
        return;
    }
    out.aMin = minimum;
    out.aMax = maximum;
}

// 重定心：脚底 y=0、水平中心 xz=0；同步平移每个网格顶点与 AABB。
void recenterModel(sModelData &out) {
    GNg::sVEC3 minimum = {1.0e30f, 1.0e30f, 1.0e30f};
    GNg::sVEC3 maximum = {-1.0e30f, -1.0e30f, -1.0e30f};
    for (size_t meshIndex = 0; meshIndex < out.aMeshes.size(); ++meshIndex) {
        const sMeshData &mesh = out.aMeshes[meshIndex];
        if (mesh.aVertices.empty()) {
            continue;
        }
        if (mesh.aMin.x < minimum.x) minimum.x = mesh.aMin.x;
        if (mesh.aMin.y < minimum.y) minimum.y = mesh.aMin.y;
        if (mesh.aMin.z < minimum.z) minimum.z = mesh.aMin.z;
        if (mesh.aMax.x > maximum.x) maximum.x = mesh.aMax.x;
        if (mesh.aMax.y > maximum.y) maximum.y = mesh.aMax.y;
        if (mesh.aMax.z > maximum.z) maximum.z = mesh.aMax.z;
    }
    if (minimum.x > maximum.x) {
        return;
    }
    const GNg::sVEC3 offset = {-(minimum.x + maximum.x) * 0.5f, -minimum.y,
                               -(minimum.z + maximum.z) * 0.5f};
    out.aRecenterOffset = offset;
    for (size_t meshIndex = 0; meshIndex < out.aMeshes.size(); ++meshIndex) {
        sMeshData &mesh = out.aMeshes[meshIndex];
        for (size_t vertex = 0; vertex < mesh.aVertices.size(); vertex += VERTEX_FLOATS) {
            mesh.aVertices[vertex + 0] += offset.x;
            mesh.aVertices[vertex + 1] += offset.y;
            mesh.aVertices[vertex + 2] += offset.z;
        }
        mesh.aMin.x += offset.x;
        mesh.aMin.y += offset.y;
        mesh.aMin.z += offset.z;
        mesh.aMax.x += offset.x;
        mesh.aMax.y += offset.y;
        mesh.aMax.z += offset.z;
    }
    out.aMin.x = minimum.x + offset.x;
    out.aMin.y = minimum.y + offset.y;
    out.aMin.z = minimum.z + offset.z;
    out.aMax.x = maximum.x + offset.x;
    out.aMax.y = maximum.y + offset.y;
    out.aMax.z = maximum.z + offset.z;
}

// 从 sASSET 的 skin 0 建立蒙皮骨架（关节顺序 = skin.joints，父索引限定在关节内且早于自身）。
void buildSkinRig(const GN3d::sASSET &asset, sSkinRig &out) {
    out = sSkinRig();
    if (asset.aSkins.empty()) {
        return;
    }
    const GN3d::sASSET_SKIN &skin = asset.aSkins[0];
    if (skin.aJoints.empty() || skin.aInverseBinds.size() != skin.aJoints.size()) {
        return;
    }
    out.aJointNodes.assign(skin.aJoints.begin(), skin.aJoints.end());
    out.aInverseBinds.assign(skin.aInverseBinds.begin(), skin.aInverseBinds.end());
    out.aParents.assign(skin.aJoints.size(), -1);
    out.aBindLocal.resize(skin.aJoints.size());
    for (size_t joint = 0; joint < skin.aJoints.size(); ++joint) {
        const uint32_t node = skin.aJoints[joint];
        if (static_cast<size_t>(node) >= asset.aNodes.size()) {
            continue;
        }
        const GN3d::sASSET_NODE &source = asset.aNodes[node];
        GN3d::sANIM_TRANSFORM &local = out.aBindLocal[joint];
        local.aTranslation = source.aTranslation;
        local.aRotation = source.aRotation;
        local.aScale = source.aScale;
        const int32_t parentNode = source.aParent;
        if (parentNode < 0) {
            continue;
        }
        for (size_t other = 0; other < joint; ++other) {
            if (out.aJointNodes[other] == static_cast<uint32_t>(parentNode)) {
                out.aParents[joint] = static_cast<int32_t>(other);
                break;
            }
        }
    }
}

} // namespace

sConvertOptions::sConvertOptions()
    : aForceOpaque(false), aRecenter(true), aClassifyAlpha(false) {}

sBindVertex::sBindVertex() {
    aPosition.x = aPosition.y = aPosition.z = 0.0f;
    aNormal.x = aNormal.y = aNormal.z = 0.0f;
    aUv[0] = aUv[1] = 0.0f;
    for (int slot = 0; slot < 4; ++slot) {
        aJoints[slot] = 0;
        aWeights[slot] = 0.0f;
    }
}

sSkinRig::sSkinRig() {}

sMeshData::sMeshData() : aMaterial(-1) {
    aMin.x = aMin.y = aMin.z = 0.0f;
    aMax.x = aMax.y = aMax.z = 0.0f;
}

sMaterialData::sMaterialData()
    : aMetallic(0.0f), aRoughness(0.5f), aAlphaMode(GN3d::eALPHA_OPAQUE), aAlphaCutoff(0.5f),
      aAlbedoTexture(-1), aNormalTexture(-1) {
    aDiffuse[0] = aDiffuse[1] = aDiffuse[2] = 0.8f;
}

sTextureData::sTextureData()
    : aHasAlpha(false), aPartialAlphaRatio(0.0f), aWrapS(10497), aWrapT(10497) {}

sModelData::sModelData()
    : aVertexCount(0), aTriangleCount(0), aZeroWeightVertices(0), aSkippedPrimitives(0),
      aFilteredMeshes(0) {
    aMin.x = aMin.y = aMin.z = 0.0f;
    aMax.x = aMax.y = aMax.z = 0.0f;
    aRecenterOffset.x = aRecenterOffset.y = aRecenterOffset.z = 0.0f;
}

void sModelData::clear() {
    aMeshes.clear();
    aMaterials.clear();
    aTextures.clear();
    aVertexCount = 0;
    aTriangleCount = 0;
    aZeroWeightVertices = 0;
    aSkippedPrimitives = 0;
    aFilteredMeshes = 0;
    aMin.x = aMin.y = aMin.z = 0.0f;
    aMax.x = aMax.y = aMax.z = 0.0f;
    aRecenterOffset.x = aRecenterOffset.y = aRecenterOffset.z = 0.0f;
    aSkin = sSkinRig();
}

const char *alphaModeName(GN3d::eALPHA_MODE mode) {
    switch (mode) {
    case GN3d::eALPHA_MASK:
        return "MASK";
    case GN3d::eALPHA_BLEND:
        return "BLEND";
    default:
        return "OPAQUE";
    }
}

bool buildModelFromGltf(const char *path, sModelData &out, std::string &error,
                        const sConvertOptions &options) {
    out.clear();
    if (path == 0 || path[0] == '\0') {
        error = "模型路径为空";
        return false;
    }
    GN3d::sASSET asset;
    if (!asset.loadFromFile(path)) {
        error = std::string("glTF 导入失败: ") + path;
        return false;
    }
    std::vector<GNg::sMAT44> world;
    if (!GN3d::buildWorld(asset, world)) {
        error = "节点世界矩阵构建失败（父索引/TRS 非法）";
        return false;
    }
    bakeTextures(asset, out);
    bakeMaterials(asset, options, out);
    bakeNodes(asset, world, options, out);
    buildSkinRig(asset, out.aSkin);
    if (out.aMeshes.empty()) {
        out.clear();
        error = "glTF 中没有可用网格";
        return false;
    }
    for (size_t meshIndex = 0; meshIndex < out.aMeshes.size(); ++meshIndex) {
        sMeshData &mesh = out.aMeshes[meshIndex];
        if (mesh.aMaterial >= 0 && static_cast<size_t>(mesh.aMaterial) >= out.aMaterials.size()) {
            mesh.aMaterial = -1;
        }
    }
    if (std::getenv("IMO_SCENE_SMOOTH_DEBUG") != 0) {
        std::printf("[imoScene] 法线平滑: %zu 组（同位置顶点合并）\n", gSmoothedGroups);
    }
    if (options.aRecenter) {
        recenterModel(out);
    } else {
        updateModelBounds(out);
    }
    return true;
}

} // namespace imoScene