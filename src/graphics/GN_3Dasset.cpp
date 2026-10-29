// GN_3Dasset.cpp — glTF 2.0 导入（节点/网格/皮肤/动画/材质/纹理）到引擎 CPU 结构。
//
// cgltf v1.15（MIT，external/include/cgltf.h，SHA-256 见 external/README.md）：
// 实现宏只在本文件定义一次，其它文件只包含 GN_3Dasset.h。
// 不导入相机/灯光；材质/纹理只做 CPU 描述（不读图片字节）；CUBICSPLINE 与 morph 权重通道跳过。
#include <GN.h>
#include "GN_3Dasset.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-variable"
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#pragma GCC diagnostic pop

#include <cstdio>
#include <cstring>
#include <string>

namespace GN3d {

namespace {

// 读取整个文件（上限 64MB，图片足够）；失败返回空。
std::vector<uint8_t> readFileBytes(const char *path) {
    std::vector<uint8_t> bytes;
    if(path == 0) return bytes;
    FILE *fp = fopen(path, "rb");
    if(fp == 0) return bytes;
    fseek(fp, 0, SEEK_END);
    const long size = ftell(fp);
    if(size <= 0 || size > 64L * 1024L * 1024L) {
        fclose(fp);
        return bytes;
    }
    fseek(fp, 0, SEEK_SET);
    bytes.resize((size_t)size);
    const size_t got = fread(bytes.data(), 1, bytes.size(), fp);
    fclose(fp);
    if(got != bytes.size()) bytes.clear();
    return bytes;
}

// 取文件路径的目录部分（含末尾分隔符）；无分隔符返回空串。
std::string dirNameOf(const char *path) {
    std::string text = path ? path : "";
    const size_t slash = text.find_last_of("/\\");
    if(slash == std::string::npos) return std::string();
    return text.substr(0, slash + 1);
}

std::string joinPath(const std::string &dir, const std::string &name) {
    if(dir.empty()) return name;
    if(name.empty()) return dir;
    const char last = dir[dir.size() - 1];
    if(last == '/' || last == '\\') return dir + name;
    return dir + "/" + name;
}

GNg::sVEC3 makeVec3(float x, float y, float z) {
    GNg::sVEC3 v = {x, y, z};
    return v;
}

int32_t nodeIndexOf(const cgltf_data *data, const cgltf_node *node) {
    if(node == 0 || data->nodes == 0) return -1;
    return (int32_t)(node - data->nodes);
}

void importNodes(const cgltf_data *data, sASSET &out) {
    out.aNodes.resize(data->nodes_count);
    for(cgltf_size i = 0; i < data->nodes_count; ++i) {
        const cgltf_node &node = data->nodes[i];
        sASSET_NODE &target = out.aNodes[i];
        target.aName = node.name ? node.name : "";
        target.aParent = -1;
        target.aTranslation = node.has_translation ? makeVec3(node.translation[0], node.translation[1],
                                                              node.translation[2])
                                                   : makeVec3(0.0f, 0.0f, 0.0f);
        target.aRotation = node.has_rotation
                               ? GNg::sQUAT(node.rotation[0], node.rotation[1], node.rotation[2],
                                            node.rotation[3])
                               : GNg::sQUAT();
        target.aScale = node.has_scale
                            ? makeVec3(node.scale[0], node.scale[1], node.scale[2])
                            : makeVec3(1.0f, 1.0f, 1.0f);
        target.aMesh = node.mesh ? (int32_t)cgltf_mesh_index(data, node.mesh) : -1;
        target.aSkin = node.skin ? (int32_t)(node.skin - data->skins) : -1;
    }
    // 父索引：遍历每个节点的 children。
    for(cgltf_size i = 0; i < data->nodes_count; ++i) {
        const cgltf_node &node = data->nodes[i];
        for(cgltf_size c = 0; c < node.children_count; ++c) {
            const int32_t child = nodeIndexOf(data, node.children[c]);
            if(child >= 0) out.aNodes[child].aParent = (int32_t)i;
        }
    }
}

bool readVec3(const cgltf_accessor *accessor, cgltf_size index, GNg::sVEC3 &out) {
    if(accessor == 0) return false;
    cgltf_float values[3];
    if(!cgltf_accessor_read_float(accessor, index, values, 3)) return false;
    out = makeVec3(values[0], values[1], values[2]);
    return true;
}

void importMeshes(const cgltf_data *data, sASSET &out) {
    out.aMeshes.resize(data->meshes_count);
    for(cgltf_size m = 0; m < data->meshes_count; ++m) {
        const cgltf_mesh &mesh = data->meshes[m];
        sASSET_MESH &targetMesh = out.aMeshes[m];
        targetMesh.aPrimitives.resize(mesh.primitives_count);
        for(cgltf_size p = 0; p < mesh.primitives_count; ++p) {
            const cgltf_primitive &primitive = mesh.primitives[p];
            sASSET_PRIMITIVE &target = targetMesh.aPrimitives[p];
            target.aMaterial = primitive.material
                                   ? (int32_t)cgltf_material_index(data, primitive.material)
                                   : -1;
            const cgltf_accessor *position =
                cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
            if(position == 0) continue;
            const cgltf_accessor *normal =
                cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0);
            const cgltf_accessor *uv = cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, 0);
            const cgltf_accessor *joints =
                cgltf_find_accessor(&primitive, cgltf_attribute_type_joints, 0);
            const cgltf_accessor *weights =
                cgltf_find_accessor(&primitive, cgltf_attribute_type_weights, 0);
            const cgltf_size vertexCount = position->count;
            target.aVertices.resize(vertexCount);
            for(cgltf_size v = 0; v < vertexCount; ++v) {
                sASSET_VERTEX &vertex = target.aVertices[v];
                vertex.aNormal = makeVec3(0.0f, 1.0f, 0.0f);
                vertex.aUv[0] = 0.0f;
                vertex.aUv[1] = 0.0f;
                for(int k = 0; k < 4; ++k) {
                    vertex.aJoints[k] = 0;
                    vertex.aWeights[k] = 0.0f;
                }
                readVec3(position, v, vertex.aPosition);
                readVec3(normal, v, vertex.aNormal);
                if(uv) {
                    cgltf_float uvValues[2];
                    if(cgltf_accessor_read_float(uv, v, uvValues, 2)) {
                        vertex.aUv[0] = uvValues[0];
                        vertex.aUv[1] = uvValues[1];
                    }
                }
                if(joints) {
                    cgltf_uint jointValues[4] = {0, 0, 0, 0};
                    if(cgltf_accessor_read_uint(joints, v, jointValues, 4)) {
                        for(int k = 0; k < 4; ++k) vertex.aJoints[k] = (uint16_t)jointValues[k];
                    }
                }
                if(weights) {
                    cgltf_float weightValues[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                    if(cgltf_accessor_read_float(weights, v, weightValues, 4)) {
                        for(int k = 0; k < 4; ++k) vertex.aWeights[k] = weightValues[k];
                    }
                }
            }
            if(primitive.indices) {
                target.aIndices.resize(primitive.indices->count);
                for(cgltf_size i = 0; i < primitive.indices->count; ++i) {
                    target.aIndices[i] = (uint32_t)cgltf_accessor_read_index(primitive.indices, i);
                }
            } else {
                target.aIndices.resize(vertexCount);
                for(cgltf_size i = 0; i < vertexCount; ++i) target.aIndices[i] = (uint32_t)i;
            }
        }
    }
}

void importSkins(const cgltf_data *data, sASSET &out) {
    out.aSkins.resize(data->skins_count);
    for(cgltf_size s = 0; s < data->skins_count; ++s) {
        const cgltf_skin &skin = data->skins[s];
        sASSET_SKIN &target = out.aSkins[s];
        target.aJoints.resize(skin.joints_count);
        for(cgltf_size j = 0; j < skin.joints_count; ++j) {
            const int32_t index = nodeIndexOf(data, skin.joints[j]);
            target.aJoints[j] = (index >= 0) ? (uint32_t)index : 0u;
        }
        target.aInverseBinds.resize(skin.joints_count);
        for(cgltf_size j = 0; j < skin.joints_count; ++j) {
            GNg::sMAT44 &matrix = target.aInverseBinds[j];
            matrix.Init(true);
            if(skin.inverse_bind_matrices) {
                cgltf_float values[16];
                if(cgltf_accessor_read_float(skin.inverse_bind_matrices, j, values, 16)) {
                    for(int k = 0; k < 16; ++k) matrix.m[k] = values[k]; // glTF 与 sMAT44 都是列主序
                }
            }
        }
    }
}

void importTextures(const cgltf_data *data, sASSET &out, const char *baseDir) {
    out.aTextures.resize(data->textures_count);
    for(cgltf_size t = 0; t < data->textures_count; ++t) {
        const cgltf_texture &texture = data->textures[t];
        sASSET_TEXTURE &target = out.aTextures[t];
        target.aName = texture.name ? texture.name : "";
        target.aUri.clear();
        target.aMime.clear();
        target.aBufferView = -1;
        target.aImageBytes.clear();
        // glTF 默认值：wrap=REPEAT；filter 无规范默认（记为 0=未指定）。
        target.aWrapS = (int32_t)cgltf_wrap_mode_repeat;
        target.aWrapT = (int32_t)cgltf_wrap_mode_repeat;
        target.aMagFilter = (int32_t)cgltf_filter_type_undefined;
        target.aMinFilter = (int32_t)cgltf_filter_type_undefined;
        if(texture.image != 0) {
            target.aUri = texture.image->uri ? texture.image->uri : "";
            target.aMime = texture.image->mime_type ? texture.image->mime_type : "";
            const cgltf_buffer_view *view = texture.image->buffer_view;
            if(view != 0 && data->buffer_views != 0) {
                target.aBufferView = (int32_t)(view - data->buffer_views);
                // .glb / 内嵌 buffer：cgltf_load_buffers 已把字节读进 buffer->data。
                if(view->buffer != 0 && view->buffer->data != 0 && view->buffer->size >= view->offset &&
                   view->buffer->size - view->offset >= view->size) {
                    const uint8_t *start = view->buffer->data + view->offset;
                    target.aImageBytes.assign(start, start + view->size);
                }
            } else if(!target.aUri.empty() && target.aUri.compare(0, 5, "data:") != 0 && baseDir != 0) {
                // 外部图片：按 .gltf 所在目录解析（loadFromMemory 无目录，留空由解码器报不支持）。
                target.aImageBytes = readFileBytes(joinPath(baseDir, target.aUri).c_str());
            }
        }
        if(texture.sampler != 0) {
            target.aWrapS = (int32_t)texture.sampler->wrap_s;
            target.aWrapT = (int32_t)texture.sampler->wrap_t;
            target.aMagFilter = (int32_t)texture.sampler->mag_filter;
            target.aMinFilter = (int32_t)texture.sampler->min_filter;
        }
    }
}

int32_t textureIndexOf(const cgltf_data *data, const cgltf_texture *texture) {
    if(texture == 0 || data->textures == 0) return -1;
    return (int32_t)cgltf_texture_index(data, texture);
}

void importMaterials(const cgltf_data *data, sASSET &out) {
    out.aMaterials.resize(data->materials_count);
    for(cgltf_size m = 0; m < data->materials_count; ++m) {
        const cgltf_material &material = data->materials[m];
        sASSET_MATERIAL &target = out.aMaterials[m];
        // cgltf 解析时已按 glTF 规范填好 pbrMetallicRoughness 默认值，这里直接复制。
        const cgltf_pbr_metallic_roughness &pbr = material.pbr_metallic_roughness;
        target.aName = material.name ? material.name : "";
        for(int k = 0; k < 4; ++k) target.aBaseColorFactor[k] = pbr.base_color_factor[k];
        target.aMetallic = pbr.metallic_factor;
        target.aRoughness = pbr.roughness_factor;
        for(int k = 0; k < 3; ++k) target.aEmissive[k] = material.emissive_factor[k];
        target.aAlbedoTexture = textureIndexOf(data, pbr.base_color_texture.texture);
        target.aNormalTexture = textureIndexOf(data, material.normal_texture.texture);
    }
}

void importAnimations(const cgltf_data *data, sASSET &out) {
    out.aAnimations.resize(data->animations_count);
    for(cgltf_size a = 0; a < data->animations_count; ++a) {
        const cgltf_animation &animation = data->animations[a];
        sASSET_ANIMATION &target = out.aAnimations[a];
        target.aName = animation.name ? animation.name : "";
        for(cgltf_size c = 0; c < animation.channels_count; ++c) {
            const cgltf_animation_channel &channel = animation.channels[c];
            if(channel.target_node == 0 || channel.sampler == 0) continue;
            const cgltf_animation_path_type path = channel.target_path;
            if(path != cgltf_animation_path_type_translation && path != cgltf_animation_path_type_rotation &&
               path != cgltf_animation_path_type_scale) {
                continue; // morph weights 等：v1 跳过
            }
            const cgltf_interpolation_type interp = channel.sampler->interpolation;
            if(interp == cgltf_interpolation_type_cubic_spline) continue; // P2 v1 不支持
            const cgltf_accessor *input = channel.sampler->input;
            const cgltf_accessor *output = channel.sampler->output;
            if(input == 0 || output == 0) continue;
            sANIM_CHANNEL targetChannel;
            targetChannel.aNode = (uint32_t)nodeIndexOf(data, channel.target_node);
            targetChannel.aInterp = (interp == cgltf_interpolation_type_step) ? eANIM_STEP : eANIM_LINEAR;
            if(path == cgltf_animation_path_type_translation) {
                targetChannel.aPath = eANIM_TRANSLATION;
            } else if(path == cgltf_animation_path_type_rotation) {
                targetChannel.aPath = eANIM_ROTATION;
            } else {
                targetChannel.aPath = eANIM_SCALE;
            }
            targetChannel.aTimes.resize(input->count);
            for(cgltf_size t = 0; t < input->count; ++t) {
                cgltf_float time = 0.0f;
                cgltf_accessor_read_float(input, t, &time, 1);
                targetChannel.aTimes[t] = time;
            }
            if(path == cgltf_animation_path_type_rotation) {
                targetChannel.aQuatValues.resize(input->count);
                for(cgltf_size t = 0; t < input->count; ++t) {
                    cgltf_float values[4] = {0.0f, 0.0f, 0.0f, 1.0f};
                    cgltf_accessor_read_float(output, t, values, 4);
                    targetChannel.aQuatValues[t] =
                        GNg::sQUAT(values[0], values[1], values[2], values[3]);
                }
            } else {
                targetChannel.aVecValues.resize(input->count);
                for(cgltf_size t = 0; t < input->count; ++t) {
                    readVec3(output, t, targetChannel.aVecValues[t]);
                }
            }
            target.aClip.aChannels.push_back(targetChannel);
        }
        target.aClip.updateDuration();
    }
}

bool importGltf(const cgltf_data *data, sASSET &out, const char *baseDir) {
    out.clear();
    importNodes(data, out);
    importMeshes(data, out);
    importSkins(data, out);
    importAnimations(data, out);
    importTextures(data, out, baseDir);
    importMaterials(data, out);
    return true;
}

} // namespace

void sASSET::clear() {
    aMeshes.clear();
    aNodes.clear();
    aSkins.clear();
    aAnimations.clear();
    aMaterials.clear();
    aTextures.clear();
}

bool sASSET::loadFromFile(const char *path) {
    if(path == 0) return false;
    cgltf_options options;
    std::memset(&options, 0, sizeof(options));
    cgltf_data *data = 0;
    if(cgltf_parse_file(&options, path, &data) != cgltf_result_success) {
        clear();
        return false;
    }
    const cgltf_result buffers = cgltf_load_buffers(&options, data, path);
    const std::string baseDir = dirNameOf(path);
    const bool ok = (buffers == cgltf_result_success) && importGltf(data, *this, baseDir.c_str());
    cgltf_free(data);
    if(!ok) clear();
    return ok;
}

bool sASSET::loadFromMemory(const void *data, size_t size) {
    if(data == 0 || size == 0) return false;
    cgltf_options options;
    std::memset(&options, 0, sizeof(options));
    cgltf_data *gltf = 0;
    if(cgltf_parse(&options, data, size, &gltf) != cgltf_result_success) {
        clear();
        return false;
    }
    const cgltf_result buffers = cgltf_load_buffers(&options, gltf, 0);
    const bool ok = (buffers == cgltf_result_success) && importGltf(gltf, *this, 0);
    cgltf_free(gltf);
    if(!ok) clear();
    return ok;
}

} // namespace GN3d
