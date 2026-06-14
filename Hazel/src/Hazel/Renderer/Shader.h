#pragma once

#include <string>
#include <unordered_map>

#include <glm/glm.hpp>

namespace Hazel {

    class Shader
    {
    public:
        virtual ~Shader() = default;

        virtual void Bind()   const = 0;
        virtual void Unbind() const = 0;

        // ── Uniform 设置 ──────────────────────────────────
        virtual void SetInt(const std::string& name, int value) = 0;
        virtual void SetIntArray(const std::string& name, int* values,
            uint32_t count) = 0;
        virtual void SetFloat(const std::string& name, float value) = 0;
        virtual void SetFloat2(const std::string& name, const glm::vec2& value) = 0;
        virtual void SetFloat3(const std::string& name, const glm::vec3& value) = 0;
        virtual void SetFloat4(const std::string& name, const glm::vec4& value) = 0;
        virtual void SetMat4(const std::string& name, const glm::mat4& value) = 0;

        // ── 元信息 ────────────────────────────────────────
        virtual const std::string& GetName() const = 0;

        // 修复：允许外部覆盖名字，解决 filepath 创建后
        // ShaderLibrary 用不同 name 注册时两者不一致的问题
        virtual void SetName(const std::string& name) = 0;

        // ── 工厂方法 ──────────────────────────────────────
        static Ref<Shader> Create(const std::string& filepath);
        static Ref<Shader> Create(const std::string& name,
            const std::string& vertexSrc,
            const std::string& fragmentSrc);
    };

    // ═══════════════════════════════════════════════════════
    //  ShaderLibrary
    // ═══════════════════════════════════════════════════════

    class ShaderLibrary
    {
    public:
        // 用 shader 自身的 name 注册
        void Add(const Ref<Shader>& shader);

        // 用指定 name 注册（同时更新 shader->m_Name，保持一致）
        void Add(const std::string& name, const Ref<Shader>& shader);

        // 从文件加载，用路径中的文件名作为 key
        Ref<Shader> Load(const std::string& filepath);

        // 从文件加载，用指定 name 作为 key
        Ref<Shader> Load(const std::string& name, const std::string& filepath);

        Ref<Shader> Get(const std::string& name) const;
        bool        Exists(const std::string& name) const;

    private:
        std::unordered_map<std::string, Ref<Shader>> m_Shaders;
    };

} // namespace Hazel