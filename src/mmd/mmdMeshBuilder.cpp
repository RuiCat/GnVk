// MMD 模型 → 引擎网格实现：材质切段、顶点压缩、绕序修正、CPU 数据生成。
#include "mmdMeshBuilder.h"

#include <cstring>

namespace mmd {

namespace {

struct MeshSpec {
    std::string name;
    uint32_t offset;
    uint32_t count;
    float diffuse[3];
    float specular[3];
    float shininess;
    std::string texture;
};

bool fail(std::string *error, const char *message) {
    if (error) {
        *error = message;
    }
    return false;
}

float clampShininess(float value) {
    if (value < 1.0f) {
        return 1.0f;
    }
    return value > 256.0f ? 256.0f : value;
}

// 镜像 X 的行列式是 -1，会把每个三角形的绕序反转。原始 MMD 数据的「绕序法线」与
// 顶点法线是一致的（CPU 探针实测 mode.pmd：agree=6189 / disagree=40），镜像后若不再翻一次，
// 世界 CCW 正面（GN_Space::frontFaceForWorldPass(true)）会把模型判成内表面：看到内壁、
// 光照像「法线反了」。这里每 3 个索引交换后两位，把绕序恢复成与法线一致的 CCW。
void reverseTriangleWinding(std::vector<int32_t> &indices) {
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const int32_t tmp = indices[i + 1];
        indices[i + 1] = indices[i + 2];
        indices[i + 2] = tmp;
    }
}

// 把位置/法线/UV 三个扁平数组按材质段压缩成 sMESH。
bool buildModel(const std::vector<MeshSpec> &specs, const std::vector<int32_t> &indices,
                const std::vector<float> &positions, const std::vector<float> &normals,
                const std::vector<float> &uvs, GN3d::cMODEL &outModel,
                std::vector<MeshTextureBinding> &outTextures, std::string *error) {
    outModel.Release();
    outTextures.clear();

    const size_t vertexCount = positions.size() / 3;
    if (vertexCount == 0 || indices.empty()) {
        return fail(error, "MMD 模型没有顶点或索引");
    }
    if (normals.size() != vertexCount * 3 || uvs.size() != vertexCount * 2) {
        return fail(error, "MMD 顶点数组长度不一致");
    }

    struct MeshRange {
        const MeshSpec *spec;
        size_t begin;
        size_t end;
    };
    std::vector<MeshRange> ranges;
    for (size_t i = 0; i < specs.size(); ++i) {
        const MeshSpec &spec = specs[i];
        if (spec.count == 0 || spec.offset >= indices.size()) {
            continue;
        }
        MeshRange range;
        range.spec = &spec;
        range.begin = spec.offset;
        range.end = spec.offset + spec.count;
        if (range.end > indices.size()) {
            range.end = indices.size();
        }
        if (range.end > range.begin) {
            ranges.push_back(range);
        }
    }
    if (ranges.empty()) {
        return fail(error, "MMD 材质索引数之和为 0");
    }

    outModel.aMeshCount = static_cast<uint32_t>(ranges.size());
    outModel.aMeshes = new GN3d::sMESH[ranges.size()];

    for (size_t meshIndex = 0; meshIndex < ranges.size(); ++meshIndex) {
        const MeshRange &range = ranges[meshIndex];
        GN3d::sMESH &mesh = outModel.aMeshes[meshIndex];
        std::strncpy(mesh.aName, range.spec->name.c_str(), sizeof(mesh.aName) - 1);
        mesh.aName[sizeof(mesh.aName) - 1] = '\0';
        mesh.aDiffuse[0] = range.spec->diffuse[0];
        mesh.aDiffuse[1] = range.spec->diffuse[1];
        mesh.aDiffuse[2] = range.spec->diffuse[2];
        mesh.aSpecular[0] = range.spec->specular[0];
        mesh.aSpecular[1] = range.spec->specular[1];
        mesh.aSpecular[2] = range.spec->specular[2];
        mesh.aShininess = clampShininess(range.spec->shininess);

        std::vector<int32_t> remap(vertexCount, -1);
        std::vector<float> vertices;
        vertices.reserve((range.end - range.begin) * 11 + 11);
        std::vector<uint32_t> meshIndices;
        meshIndices.reserve(range.end - range.begin);

        bool ok = true;
        // 保持文件原始绕序（PMX 为 CW）。世界绕序统一为 CCW 的迁移见
        // docs/AXIS_UNIFICATION_PLAN.md U5：手性镜像后自然变 CCW，不要在这里单独反绕序。
        auto emitVertex = [&](int32_t vertexIndex) -> bool {
            if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= vertexCount) {
                ok = false;
                return false;
            }
            if (remap[static_cast<size_t>(vertexIndex)] < 0) {
                remap[static_cast<size_t>(vertexIndex)] = static_cast<int32_t>(vertices.size() / 11);
                const size_t vi = static_cast<size_t>(vertexIndex);
                vertices.push_back(positions[vi * 3 + 0]);
                vertices.push_back(positions[vi * 3 + 1]);
                vertices.push_back(positions[vi * 3 + 2]);
                vertices.push_back(normals[vi * 3 + 0]);
                vertices.push_back(normals[vi * 3 + 1]);
                vertices.push_back(normals[vi * 3 + 2]);
                vertices.push_back(1.0f);
                vertices.push_back(0.0f);
                vertices.push_back(0.0f);
                vertices.push_back(uvs[vi * 2 + 0]);
                vertices.push_back(uvs[vi * 2 + 1]);
            }
            meshIndices.push_back(static_cast<uint32_t>(remap[static_cast<size_t>(vertexIndex)]));
            return true;
        };

        size_t cursor = range.begin;
        for (; cursor + 3 <= range.end; cursor += 3) {
            // 保持文件原始绕序（PMX CW）；顶点镜像 X 后自然变 CCW；
            // 3D pass 的 frontFace 为 CW（世界 CCW + 投影 Y 翻转，见 GN_Space.h）。
            if (!emitVertex(indices[cursor]) || !emitVertex(indices[cursor + 1]) ||
                !emitVertex(indices[cursor + 2])) {
                break;
            }
        }
        // 非 3 的倍数（坏数据）按原顺序收尾，不丢顶点。
        for (; ok && cursor < range.end; ++cursor) {
            emitVertex(indices[cursor]);
        }
        if (!ok) {
            outModel.Release();
            outTextures.clear();
            return fail(error, "MMD 顶点索引越界");
        }

        mesh.aVertexCount = static_cast<uint32_t>(vertices.size() / 11);
        mesh.aIndexCount = static_cast<uint32_t>(meshIndices.size());
        mesh.aVertices = new float[vertices.size()];
        std::memcpy(mesh.aVertices, vertices.data(), vertices.size() * sizeof(float));
        if (!meshIndices.empty()) {
            mesh.aIndices = new uint32_t[meshIndices.size()];
            std::memcpy(mesh.aIndices, meshIndices.data(), meshIndices.size() * sizeof(uint32_t));
        }

        if (!range.spec->texture.empty()) {
            MeshTextureBinding binding;
            binding.meshIndex = static_cast<uint32_t>(meshIndex);
            binding.texturePath = range.spec->texture;
            outTextures.push_back(binding);
        }
    }

    outModel.computeWorldAABB();
    return true;
}

} // namespace

bool buildPmxModel(const PmxModel &source, GN3d::cMODEL &outModel,
                   std::vector<MeshTextureBinding> &outTextures, std::string *error) {
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> uvs;
    positions.reserve(source.vertices.size() * 3);
    normals.reserve(source.vertices.size() * 3);
    uvs.reserve(source.vertices.size() * 2);
    for (size_t i = 0; i < source.vertices.size(); ++i) {
        const PmxVertex &vertex = source.vertices[i];
        positions.push_back(vertex.position[0]);
        positions.push_back(vertex.position[1]);
        positions.push_back(vertex.position[2]);
        normals.push_back(vertex.normal[0]);
        normals.push_back(vertex.normal[1]);
        normals.push_back(vertex.normal[2]);
        uvs.push_back(vertex.uv[0]);
        uvs.push_back(vertex.uv[1]);
    }
    // 镜像 X 转右手系：正面保持 +Z，但镜像会把绕序反转，必须再反一次恢复
    // 「CCW 绕序 + 外向法线」的一致关系（否则模型内外翻转、光照像法线反了）。
    std::vector<int32_t> indices = source.indices;
    GN3d::mirrorVectorsX(positions.data(), static_cast<uint32_t>(source.vertices.size()), 3);
    GN3d::mirrorVectorsX(normals.data(), static_cast<uint32_t>(source.vertices.size()), 3);
    reverseTriangleWinding(indices);

    std::vector<MeshSpec> specs;
    uint32_t offset = 0;
    for (size_t i = 0; i < source.materials.size(); ++i) {
        const PmxMaterial &material = source.materials[i];
        MeshSpec spec;
        spec.name = material.name;
        spec.offset = offset;
        spec.count = material.indexCount > 0 ? static_cast<uint32_t>(material.indexCount) : 0;
        offset += spec.count;
        spec.diffuse[0] = material.diffuse[0];
        spec.diffuse[1] = material.diffuse[1];
        spec.diffuse[2] = material.diffuse[2];
        spec.specular[0] = material.specular[0];
        spec.specular[1] = material.specular[1];
        spec.specular[2] = material.specular[2];
        spec.shininess = material.specularity;
        if (material.diffuseTextureIndex >= 0 &&
            static_cast<size_t>(material.diffuseTextureIndex) < source.textures.size()) {
            spec.texture = source.textures[static_cast<size_t>(material.diffuseTextureIndex)];
        }
        specs.push_back(spec);
    }
    return buildModel(specs, indices, positions, normals, uvs, outModel, outTextures, error);
}

bool buildPmdModel(const PmdModel &source, GN3d::cMODEL &outModel,
                   std::vector<MeshTextureBinding> &outTextures, std::string *error) {
    std::vector<float> positions;
    std::vector<float> normals;
    std::vector<float> uvs;
    positions.reserve(source.vertices.size() * 3);
    normals.reserve(source.vertices.size() * 3);
    uvs.reserve(source.vertices.size() * 2);
    for (size_t i = 0; i < source.vertices.size(); ++i) {
        const PmdVertex &vertex = source.vertices[i];
        positions.push_back(vertex.position[0]);
        positions.push_back(vertex.position[1]);
        positions.push_back(vertex.position[2]);
        normals.push_back(vertex.normal[0]);
        normals.push_back(vertex.normal[1]);
        normals.push_back(vertex.normal[2]);
        uvs.push_back(vertex.uv[0]);
        uvs.push_back(vertex.uv[1]);
    }
    // PMD 与 PMX 同为左手系（+Y 上、+Z 前）；镜像 X 转右手系后正面保持 +Z，
    // 但镜像反转绕序，需再反一次，恢复「CCW + 外向法线」的一致关系。
    GN3d::mirrorVectorsX(positions.data(), static_cast<uint32_t>(source.vertices.size()), 3);
    GN3d::mirrorVectorsX(normals.data(), static_cast<uint32_t>(source.vertices.size()), 3);

    std::vector<int32_t> indices;
    indices.reserve(source.indices.size());
    for (size_t i = 0; i < source.indices.size(); ++i) {
        indices.push_back(static_cast<int32_t>(source.indices[i]));
    }
    reverseTriangleWinding(indices);

    std::vector<MeshSpec> specs;
    uint32_t offset = 0;
    for (size_t i = 0; i < source.materials.size(); ++i) {
        const PmdMaterial &material = source.materials[i];
        MeshSpec spec;
        spec.name = material.textureFilename;
        spec.offset = offset;
        spec.count = material.indexCount;
        offset += spec.count;
        spec.diffuse[0] = material.diffuse[0];
        spec.diffuse[1] = material.diffuse[1];
        spec.diffuse[2] = material.diffuse[2];
        spec.specular[0] = material.specular[0];
        spec.specular[1] = material.specular[1];
        spec.specular[2] = material.specular[2];
        spec.shininess = material.power;
        spec.texture = material.textureFilename;
        specs.push_back(spec);
    }
    return buildModel(specs, indices, positions, normals, uvs, outModel, outTextures, error);
}

} // namespace mmd
