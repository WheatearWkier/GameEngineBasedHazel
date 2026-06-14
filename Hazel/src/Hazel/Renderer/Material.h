#pragma once
#include "Texture.h"
#include "Hazel/Core/Core.h"
#include <glm/glm.hpp>
#include <string>

namespace Hazel {

    class Material
    {
    public:
        glm::vec4      Albedo = { 1.0f, 1.0f, 1.0f, 1.0f };
        float          Metallic = 0.0f;
        float          Roughness = 0.5f;
        bool           FlipNormals = false;
        Ref<Texture2D> AlbedoMap = nullptr;
        Ref<Texture2D> NormalMap = nullptr;
        Ref<Texture2D> RoughnessMap = nullptr;
        Ref<Texture2D> MetallicMap = nullptr;

        const std::string& GetPath() const { return m_Path; }
        bool IsDirty() const { return m_Dirty; }
        void MarkDirty() { m_Dirty = true; }
        void ClearDirty() { m_Dirty = false; }

        static Ref<Material> Create();
        static Ref<Material> Load(const std::string& path);
        void Save(const std::string& path);
        void Save(); // 保存到原路径

    private:
        std::string m_Path;
        bool        m_Dirty = false;
    };

} // namespace Hazel