// tests/imoScene/imoSceneUpload.cpp — sModelData → cMODEL / sIMAGE（Vulkan 侧只做上传）。
//
// 职责：分配网格/材质/贴图对象、把 CPU 顶点拷进 sMESH、解码 PNG/JPEG 贴图并绑定到材质。
// 不做：glTF 解析与蒙皮烘焙（imoSceneConvert）、任何绘制与场景装配（imoSceneMain）。

#include "imoSceneUpload.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace imoScene {

namespace {

// 网格名缓冲长度与 sMESH::aName 一致。
const size_t MESH_NAME_MAX = 128;

// 贴图下标 → sIMAGE；越界/解码失败返回 nullptr（材质按无贴图处理）。
GNg::sIMAGE *textureAt(const sSceneModel &model, int32_t index) {
    if (index < 0 || static_cast<size_t>(index) >= model.aTextures.size()) {
        return 0;
    }
    return model.aTextures[static_cast<size_t>(index)];
}

// 把一个网格的顶点/索引拷进引擎 sMESH（所有权交给 sMESH，析构时 delete[]）。
void fillMesh(GN3d::sMESH &mesh, const sMeshData &data, GN3d::sMATERIAL *material) {
    std::snprintf(mesh.aName, MESH_NAME_MAX, "%s", data.aName.c_str());
    const size_t floatCount = data.aVertices.size();
    if (floatCount > 0) {
        mesh.aVertices = new float[floatCount];
        std::memcpy(mesh.aVertices, data.aVertices.data(), floatCount * sizeof(float));
        mesh.aVertexCount = static_cast<uint32_t>(floatCount / 11);
    }
    if (!data.aIndices.empty()) {
        mesh.aIndices = new uint32_t[data.aIndices.size()];
        std::memcpy(mesh.aIndices, data.aIndices.data(),
                    data.aIndices.size() * sizeof(uint32_t));
        mesh.aIndexCount = static_cast<uint32_t>(data.aIndices.size());
    }
    mesh.aMaterial = material;
    if (material != 0) {
        mesh.aDiffuse[0] = material->aDiffuse.x;
        mesh.aDiffuse[1] = material->aDiffuse.y;
        mesh.aDiffuse[2] = material->aDiffuse.z;
        mesh.aAlbedoTex = material->aAlbedoTex;
        mesh.aNormalTex = material->aNormalTex;
    }
    const float dx = data.aMax.x - data.aMin.x;
    const float dy = data.aMax.y - data.aMin.y;
    const float dz = data.aMax.z - data.aMin.z;
    mesh.aBoundRadius = 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz);
}

} // namespace

sUploadOptions::sUploadOptions() : aLoadTextures(true), aMaxMeshes(0) {}

sSceneModel::sSceneModel() {
    aMin.x = aMin.y = aMin.z = 0.0f;
    aMax.x = aMax.y = aMax.z = 0.0f;
}

sSceneModel::~sSceneModel() {
    release();
}

void sSceneModel::release() {
    for (size_t index = 0; index < aTextures.size(); ++index) {
        delete aTextures[index];
    }
    aTextures.clear();
    aModel.Release();
    aMin.x = aMin.y = aMin.z = 0.0f;
    aMax.x = aMax.y = aMax.z = 0.0f;
}

bool uploadSceneModel(const sModelData &data, sSceneModel &out, std::string &error,
                      const sUploadOptions &options) {
    out.release();
    if (data.aMeshes.empty()) {
        error = "没有可上传的网格";
        return false;
    }

    out.aTextures.assign(data.aTextures.size(), static_cast<GNg::sIMAGE *>(0));
    for (size_t index = 0; options.aLoadTextures && index < data.aTextures.size(); ++index) {
        const sTextureData &source = data.aTextures[index];
        if (source.aBytes.empty()) {
            continue;
        }
        GNg::sIMAGE *image = new GNg::sIMAGE();
        if (!GNg::ImageLoadFromMem(source.aBytes.data(), static_cast<int>(source.aBytes.size()),
                                   image)) {
            std::fprintf(stderr, "[imoScene] 贴图解码失败（下标 %zu）\n", index);
            delete image;
            continue;
        }
        out.aTextures[index] = image;
    }

    size_t meshCount = data.aMeshes.size();
    if (options.aMaxMeshes > 0 && meshCount > options.aMaxMeshes) {
        meshCount = options.aMaxMeshes;
    }
    out.aModel.aMeshes = new GN3d::sMESH[meshCount];
    out.aModel.aMeshCount = static_cast<uint32_t>(meshCount);
    out.aModel.aMaterials = new GN3d::sMATERIAL[meshCount];
    out.aModel.aMaterialCount = static_cast<uint32_t>(meshCount);
    std::snprintf(out.aModel.aName, sizeof(out.aModel.aName), "%s", "imo_chan");

    for (size_t index = 0; index < meshCount; ++index) {
        const sMeshData &meshData = data.aMeshes[index];
        GN3d::sMATERIAL &material = out.aModel.aMaterials[index];
        const sMaterialData *source = 0;
        if (meshData.aMaterial >= 0 &&
            static_cast<size_t>(meshData.aMaterial) < data.aMaterials.size()) {
            source = &data.aMaterials[static_cast<size_t>(meshData.aMaterial)];
        }
        if (source != 0) {
            material.SetDefault();
            material.aDiffuse.x = source->aDiffuse[0];
            material.aDiffuse.y = source->aDiffuse[1];
            material.aDiffuse.z = source->aDiffuse[2];
            material.aMetallic = source->aMetallic;
            material.aRoughness = source->aRoughness;
            material.aAlphaMode = source->aAlphaMode;
            material.aAlphaCutoff = source->aAlphaCutoff;
            material.aAlbedoTex = textureAt(out, source->aAlbedoTexture);
            material.aNormalTex = textureAt(out, source->aNormalTexture);
        } else {
            material.SetWhite();
        }
        material.aSpecular.x = 1.0f;
        material.aSpecular.y = 1.0f;
        material.aSpecular.z = 1.0f;
        fillMesh(out.aModel.aMeshes[index], meshData, &material);
    }
    out.aMin = data.aMin;
    out.aMax = data.aMax;
    out.aModel.UpdateTransform();
    out.aModel.computeWorldAABB();
    return true;
}

} // namespace imoScene
