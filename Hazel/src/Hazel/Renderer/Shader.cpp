#include "hzpch.h"
#include "Shader.h"

#include "Renderer.h"
#include "Platform/OpenGL/OpenGLShader.h"

namespace Hazel {

    // ═══════════════════════════════════════════════════════
    //  Shader 工厂
    // ═══════════════════════════════════════════════════════

    Ref<Shader> Shader::Create(const std::string& filepath)
    {
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::OpenGL:
            return CreateRef<OpenGLShader>(filepath);
        case RendererAPI::API::None:
            HZ_CORE_ASSERT(false, "RendererAPI::None is not supported");
            return nullptr;
        }
        HZ_CORE_ASSERT(false, "Unknown RendererAPI");
        return nullptr;
    }

    Ref<Shader> Shader::Create(const std::string& name,
        const std::string& vertexSrc,
        const std::string& fragmentSrc)
    {
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::OpenGL:
            return CreateRef<OpenGLShader>(name, vertexSrc, fragmentSrc);
        case RendererAPI::API::None:
            HZ_CORE_ASSERT(false, "RendererAPI::None is not supported");
            return nullptr;
        }
        HZ_CORE_ASSERT(false, "Unknown RendererAPI");
        return nullptr;
    }

    // ═══════════════════════════════════════════════════════
    //  ShaderLibrary
    // ═══════════════════════════════════════════════════════

    void ShaderLibrary::Add(const Ref<Shader>& shader)
    {
        Add(shader->GetName(), shader);
    }

    void ShaderLibrary::Add(const std::string& name, const Ref<Shader>& shader)
    {
        HZ_CORE_ASSERT(!Exists(name), "Shader '{0}' already exists in library", name);

        // 修复：统一 shader 内部的 name 和库里注册的 key
        // 避免 filepath 创建后 GetName() 和库的 key 不一致
        shader->SetName(name);
        m_Shaders[name] = shader;
    }

    Ref<Shader> ShaderLibrary::Load(const std::string& filepath)
    {
        auto shader = Shader::Create(filepath);
        Add(shader); // 用 shader 自身从路径解析的 name
        return shader;
    }

    Ref<Shader> ShaderLibrary::Load(const std::string& name,
        const std::string& filepath)
    {
        auto shader = Shader::Create(filepath);
        Add(name, shader); // 用指定 name，同时会更新 shader->m_Name
        return shader;
    }

    Ref<Shader> ShaderLibrary::Get(const std::string& name) const
    {
        HZ_CORE_ASSERT(Exists(name), "Shader '{0}' not found in library", name);
        return m_Shaders.at(name); // 用 at() 而非 [] 保持 const 正确性
    }

    bool ShaderLibrary::Exists(const std::string& name) const
    {
        return m_Shaders.find(name) != m_Shaders.end();
    }

} // namespace Hazel