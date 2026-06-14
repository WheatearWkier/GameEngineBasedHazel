#include "hzpch.h"
#include "Mesh.h"
#include "Hazel/Core/Log.h"
#include <glm/glm.hpp>

// tinyobjloader —— header-only，把实现放在这个 .cpp 里
// 需要在 premake/CMake 里把 tinyobjloader 的 include 路径加进来
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <unordered_map>
#include <cmath>

namespace Hazel {

    // -------------------------------------------------------------------------
    // 工厂方法
    // -------------------------------------------------------------------------

    Ref<Mesh> Mesh::Create(const std::string& filepath)
    {
        auto mesh = CreateRef<Mesh>();
        mesh->m_Filepath = filepath;

        std::vector<MeshVertex> vertices;
        std::vector<uint32_t>   indices;

        if (!mesh->LoadOBJ(filepath, vertices, indices))
        {
            HZ_CORE_ERROR("Mesh::Create — 加载失败: {0}", filepath);
            // 退化成一个默认立方体，让场景不崩溃
            return CreateCube();
        }

        mesh->UploadToGPU(vertices, indices);
        HZ_CORE_INFO("Mesh 加载成功: {0}  ({1} 顶点, {2} 三角形)",
            filepath, vertices.size(), indices.size() / 3);
        return mesh;
    }

    Ref<Mesh> Mesh::Create(const std::vector<MeshVertex>& vertices,
        const std::vector<uint32_t>& indices)
    {
        auto mesh = CreateRef<Mesh>();
        mesh->UploadToGPU(vertices, indices);
        return mesh;
    }

    // -------------------------------------------------------------------------
    // 内置几何体
    // -------------------------------------------------------------------------

    Ref<Mesh> Mesh::CreateCube()
    {
        // 每个面 4 个顶点（法线各不相同，不能共享顶点）
        // 顶点顺序：逆时针为正面（OpenGL 默认）
        std::vector<MeshVertex> vertices = {
            // 前面 (z = +0.5, normal = 0,0,1)
            {{ -0.5f, -0.5f,  0.5f }, { 0,0,1 }, { 0,0 }},
            {{  0.5f, -0.5f,  0.5f }, { 0,0,1 }, { 1,0 }},
            {{  0.5f,  0.5f,  0.5f }, { 0,0,1 }, { 1,1 }},
            {{ -0.5f,  0.5f,  0.5f }, { 0,0,1 }, { 0,1 }},
            // 后面 (z = -0.5, normal = 0,0,-1)
            {{  0.5f, -0.5f, -0.5f }, { 0,0,-1 }, { 0,0 }},
            {{ -0.5f, -0.5f, -0.5f }, { 0,0,-1 }, { 1,0 }},
            {{ -0.5f,  0.5f, -0.5f }, { 0,0,-1 }, { 1,1 }},
            {{  0.5f,  0.5f, -0.5f }, { 0,0,-1 }, { 0,1 }},
            // 左面 (x = -0.5, normal = -1,0,0)
            {{ -0.5f, -0.5f, -0.5f }, { -1,0,0 }, { 0,0 }},
            {{ -0.5f, -0.5f,  0.5f }, { -1,0,0 }, { 1,0 }},
            {{ -0.5f,  0.5f,  0.5f }, { -1,0,0 }, { 1,1 }},
            {{ -0.5f,  0.5f, -0.5f }, { -1,0,0 }, { 0,1 }},
            // 右面 (x = +0.5, normal = 1,0,0)
            {{  0.5f, -0.5f,  0.5f }, { 1,0,0 }, { 0,0 }},
            {{  0.5f, -0.5f, -0.5f }, { 1,0,0 }, { 1,0 }},
            {{  0.5f,  0.5f, -0.5f }, { 1,0,0 }, { 1,1 }},
            {{  0.5f,  0.5f,  0.5f }, { 1,0,0 }, { 0,1 }},
            // 上面 (y = +0.5, normal = 0,1,0)
            {{ -0.5f,  0.5f,  0.5f }, { 0,1,0 }, { 0,0 }},
            {{  0.5f,  0.5f,  0.5f }, { 0,1,0 }, { 1,0 }},
            {{  0.5f,  0.5f, -0.5f }, { 0,1,0 }, { 1,1 }},
            {{ -0.5f,  0.5f, -0.5f }, { 0,1,0 }, { 0,1 }},
            // 下面 (y = -0.5, normal = 0,-1,0)
            {{ -0.5f, -0.5f, -0.5f }, { 0,-1,0 }, { 0,0 }},
            {{  0.5f, -0.5f, -0.5f }, { 0,-1,0 }, { 1,0 }},
            {{  0.5f, -0.5f,  0.5f }, { 0,-1,0 }, { 1,1 }},
            {{ -0.5f, -0.5f,  0.5f }, { 0,-1,0 }, { 0,1 }},
        };

        // 每个面两个三角形，共 6 面 × 6 索引 = 36
        std::vector<uint32_t> indices;
        indices.reserve(36);
        for (uint32_t face = 0; face < 6; ++face)
        {
            uint32_t base = face * 4;
            indices.insert(indices.end(), {
                base + 0, base + 1, base + 2,
                base + 2, base + 3, base + 0
                });
        }

        return Create(vertices, indices);
    }

    Ref<Mesh> Mesh::CreateSphere(uint32_t sectorCount, uint32_t stackCount)
    {
        std::vector<MeshVertex> vertices;
        std::vector<uint32_t>   indices;
        BuildSphere(sectorCount, stackCount, vertices, indices);
        return Create(vertices, indices);
    }

    // -------------------------------------------------------------------------
    // GPU 上传（顶点布局：Position / Normal / TexCoord）
    // -------------------------------------------------------------------------

    void Mesh::UploadToGPU(const std::vector<MeshVertex>& vertices,
        const std::vector<uint32_t>& indices)
    {
        m_VertexArray = VertexArray::Create();

        // VBO
        auto vbo = VertexBuffer::Create(
            (uint32_t)(vertices.size() * sizeof(MeshVertex))
        );
        vbo->SetData(vertices.data(),
            (uint32_t)(vertices.size() * sizeof(MeshVertex)));
        vbo->SetLayout({
            { "a_Position", ShaderDataType::Float3 },
            { "a_Normal",   ShaderDataType::Float3 },
            { "a_TexCoord", ShaderDataType::Float2 },
            });
        m_VertexArray->AddVertexBuffer(vbo);

        // IBO
        auto ibo = IndexBuffer::Create(
            const_cast<uint32_t*>(indices.data()),
            (uint32_t)indices.size()
        );
        m_VertexArray->SetIndexBuffer(ibo);

        m_IndexCount = (uint32_t)indices.size();
    }

    // -------------------------------------------------------------------------
    // OBJ 加载（tinyobjloader）
    // -------------------------------------------------------------------------

    bool Mesh::LoadOBJ(const std::string& filepath,
        std::vector<MeshVertex>& outVertices,
        std::vector<uint32_t>& outIndices)
    {
        tinyobj::attrib_t                attrib;
        std::vector<tinyobj::shape_t>    shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        // 材质搜索目录 = 文件所在目录
        std::string baseDir = filepath.substr(0, filepath.find_last_of("/\\") + 1);

        // 1.0.6 的签名：warn 和 err 分开，filepath 直接传 c_str()
        bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials,
            &err,                  // 1.0.6 没有独立的 warn 参数
            filepath.c_str(),
            baseDir.c_str());
        if (!err.empty()) HZ_CORE_ERROR("OBJ: {0}", err);
        if (!ok) return false;

        // 用 index 去重：相同 (pos/normal/uv) 三元组 → 同一顶点
        // key = "posIdx/normalIdx/uvIdx"
        std::unordered_map<std::string, uint32_t> uniqueVertices;

        for (const auto& shape : shapes)
        {
            for (const auto& idx : shape.mesh.indices)
            {
                MeshVertex v{};

                // 位置（必有）
                v.Position = {
                    attrib.vertices[3 * idx.vertex_index + 0],
                    attrib.vertices[3 * idx.vertex_index + 1],
                    attrib.vertices[3 * idx.vertex_index + 2],
                };

                // 法线（可能没有 → 后续重新计算，先填 0）
                if (idx.normal_index >= 0)
                {
                    v.Normal = {
                        attrib.normals[3 * idx.normal_index + 0],
                        attrib.normals[3 * idx.normal_index + 1],
                        attrib.normals[3 * idx.normal_index + 2],
                    };
                }

                // UV（可能没有 → 填 0）
                if (idx.texcoord_index >= 0)
                {
                    v.TexCoord = {
                        attrib.texcoords[2 * idx.texcoord_index + 0],
                        1.0f - attrib.texcoords[2 * idx.texcoord_index + 1], // flip Y
                    };
                }

                // 去重
                std::string key = std::to_string(idx.vertex_index) + "/" +
                    std::to_string(idx.normal_index) + "/" +
                    std::to_string(idx.texcoord_index);
                if (uniqueVertices.count(key) == 0)
                {
                    uniqueVertices[key] = (uint32_t)outVertices.size();
                    outVertices.push_back(v);
                }
                outIndices.push_back(uniqueVertices[key]);
            }
        }

        // 如果 .obj 没有法线，按三角形面法线填充（flat shading）
        bool hasNormals = !attrib.normals.empty();
        if (!hasNormals)
        {
            for (size_t i = 0; i + 2 < outIndices.size(); i += 3)
            {
                auto& v0 = outVertices[outIndices[i + 0]];
                auto& v1 = outVertices[outIndices[i + 1]];
                auto& v2 = outVertices[outIndices[i + 2]];
                glm::vec3 n = glm::normalize(
                    glm::cross(v1.Position - v0.Position,
                        v2.Position - v0.Position)
                );
                v0.Normal = v1.Normal = v2.Normal = n;
            }
        }

        return true;
    }

    // -------------------------------------------------------------------------
    // 球体生成（UV sphere）
    // -------------------------------------------------------------------------

    void Mesh::BuildSphere(uint32_t sectorCount, uint32_t stackCount,
        std::vector<MeshVertex>& outV,
        std::vector<uint32_t>& outI)
    {
        const float PI = 3.14159265358979f;

        for (uint32_t i = 0; i <= stackCount; ++i)
        {
            float stackAngle = PI / 2.0f - PI * (float)i / (float)stackCount;
            float xz = std::cos(stackAngle);
            float y = std::sin(stackAngle);

            for (uint32_t j = 0; j <= sectorCount; ++j)
            {
                float sectorAngle = 2.0f * PI * (float)j / (float)sectorCount;
                float x = xz * std::cos(sectorAngle);
                float z = xz * std::sin(sectorAngle);

                MeshVertex v;
                v.Position = { x * 0.5f, y * 0.5f, z * 0.5f }; // 半径 0.5
                v.Normal = { x, y, z };
                v.TexCoord = { (float)j / sectorCount,
                               (float)i / stackCount };
                outV.push_back(v);
            }
        }

        for (uint32_t i = 0; i < stackCount; ++i)
        {
            uint32_t k1 = i * (sectorCount + 1);
            uint32_t k2 = k1 + sectorCount + 1;
            for (uint32_t j = 0; j < sectorCount; ++j, ++k1, ++k2)
            {
                if (i != 0)
                    outI.insert(outI.end(), { k1, k2, k1 + 1 });
                if (i != stackCount - 1)
                    outI.insert(outI.end(), { k1 + 1, k2, k2 + 1 });
            }
        }
    }

} // namespace Hazel