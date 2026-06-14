#pragma once
#include "Hazel/Core/Core.h"
#include "Hazel/Renderer/Buffer.h"
#include "Hazel/Renderer/VertexArray.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Hazel {

    // -------------------------------------------------------------------------
    // 顶点格式：位置 + 法线 + UV
    // -------------------------------------------------------------------------
    struct MeshVertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 TexCoord;
    };

    // -------------------------------------------------------------------------
    // Mesh：持有一个模型的几何数据，上传到 GPU 后可多次 Draw
    //
    // 使用方式：
    //   auto mesh = Mesh::Create("assets/models/cube.obj");
    //   Renderer3D::DrawMesh(transform, mesh, material, entityID);
    // -------------------------------------------------------------------------
    class Mesh
    {
    public:
        Mesh() = default;

        // 从文件加载（支持 .obj，需要 tinyobjloader）
        static Ref<Mesh> Create(const std::string& filepath);

        // 从内存数据创建（运行时生成的几何体，如默认立方体）
        static Ref<Mesh> Create(const std::vector<MeshVertex>& vertices,
            const std::vector<uint32_t>& indices);

        // 创建内置几何体
        static Ref<Mesh> CreateCube();
        static Ref<Mesh> CreateSphere(uint32_t sectorCount = 36,
            uint32_t stackCount = 18);

        // ---- Getters ----
        const Ref<VertexArray>& GetVertexArray() const { return m_VertexArray; }
        uint32_t                GetIndexCount()  const { return m_IndexCount; }
        const std::string& GetFilepath()    const { return m_Filepath; }

    private:
        // 把顶点/索引数据上传到 GPU
        void UploadToGPU(const std::vector<MeshVertex>& vertices,
            const std::vector<uint32_t>& indices);

        // 从 .obj 解析顶点数据
        bool LoadOBJ(const std::string& filepath,
            std::vector<MeshVertex>& outVertices,
            std::vector<uint32_t>& outIndices);

        // 生成球体顶点
        static void BuildSphere(uint32_t sectorCount, uint32_t stackCount,
            std::vector<MeshVertex>& outVertices,
            std::vector<uint32_t>& outIndices);

    private:
        Ref<VertexArray> m_VertexArray;
        uint32_t         m_IndexCount = 0;
        std::string      m_Filepath;  // 空 = 程序化生成
    };

} // namespace Hazel