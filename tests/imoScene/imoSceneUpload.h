#pragma once
#ifndef GN_IMOSCENE_UPLOAD_H
#define GN_IMOSCENE_UPLOAD_H
// tests/imoScene/imoSceneUpload.h — CPU 场景数据 → 引擎 cMODEL / sIMAGE（依赖已初始化的引擎）。
//
// 边界：只做内存所有权转移与解码上传（ImageLoadFromMem），不做绘制；
//       贴图指针由 sSceneModel 持有，释放时必须先 delete 贴图再 Release 模型。
#include "imoSceneConvert.h"
#include "GN_3Dmodel.h"

#include <string>
#include <vector>

namespace imoScene {

// 上传后的场景：模型 + 贴图（与 sModelData::aTextures 下标对齐，解码失败的位置为 nullptr）。
struct sSceneModel {
    GN3d::cMODEL aModel;
    std::vector<GNg::sIMAGE *> aTextures;
    GNg::sVEC3 aMin;
    GNg::sVEC3 aMax;

    sSceneModel();
    ~sSceneModel();
    void release();
};

// 上传开关：用于在疑似驱动/显存问题上做最小化二分（默认全开）。
struct sUploadOptions {
    bool aLoadTextures; // false = 跳过全部贴图解码/上传（材质退化为纯色）
    uint32_t aMaxMeshes; // 0 = 全部；>0 只上传前 N 个网格

    sUploadOptions();
};

// 把 CPU 数据装进引擎对象并上传纹理；失败时 out 清空并写 error。
bool uploadSceneModel(const sModelData &data, sSceneModel &out, std::string &error,
                      const sUploadOptions &options = sUploadOptions());

} // namespace imoScene
#endif // GN_IMOSCENE_UPLOAD_H
