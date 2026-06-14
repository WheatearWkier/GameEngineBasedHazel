#include "hzpch.h"
#include "OpenGLShader.h"

#include <fstream>
#include <filesystem>

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>

#include "Hazel/Core/Timer.h"

// =============================================================================
//  OpenGLShader.cpp
//
//  Compile pipeline (current ¡ª OpenGL only):
//
//    Your GLSL  -->  shaderc (target: OpenGL 4.5)  -->  OpenGL SPIR-V
//                                                         ©¦
//                                                   glShaderBinary
//                                                   glSpecializeShader
//
//  When Vulkan support is added later:
//    ~ CompileOrGetVulkanBinaries  -> change target to shaderc_target_env_vulkan
//    ~ CompileOrGetOpenGLBinaries  -> keep its own separate OpenGL compilation
//    ~ The two pipelines run independently; m_VulkanSPIRV feeds the Vulkan backend,
//      m_OpenGLSPIRV feeds this OpenGL backend.
//
//  Why no spirv_cross round-trip?
//    Compiling to Vulkan SPIR-V and then converting back to GLSL via spirv_cross
//    causes interface-variable name mismatches (e.g. "_589" vs "__defaultname_589")
//    because the Vulkan optimizer renames variables and spirv_cross generates
//    different names per stage.  Compiling directly to OpenGL SPIR-V avoids the
//    round-trip entirely and keeps the SPIR-V driver-ready in one step.
// =============================================================================

namespace Hazel {

    // =========================================================================
    //  Internal utilities
    // =========================================================================

    namespace Utils {

        static GLenum ShaderTypeFromString(const std::string& type)
        {
            if (type == "vertex")                      return GL_VERTEX_SHADER;
            if (type == "fragment" || type == "pixel") return GL_FRAGMENT_SHADER;
            HZ_CORE_ASSERT(false, "Unknown shader type!");
            return 0;
        }

        static shaderc_shader_kind GLShaderStageToShaderC(GLenum stage)
        {
            switch (stage)
            {
            case GL_VERTEX_SHADER:   return shaderc_glsl_vertex_shader;
            case GL_FRAGMENT_SHADER: return shaderc_glsl_fragment_shader;
            }
            HZ_CORE_ASSERT(false, "Unknown shader stage");
            return static_cast<shaderc_shader_kind>(0);
        }

        static const char* GLShaderStageToString(GLenum stage)
        {
            switch (stage)
            {
            case GL_VERTEX_SHADER:   return "GL_VERTEX_SHADER";
            case GL_FRAGMENT_SHADER: return "GL_FRAGMENT_SHADER";
            }
            HZ_CORE_ASSERT(false, "Unknown shader stage");
            return nullptr;
        }

        static const char* GetCacheDirectory()
        {
            return "assets/cache/shader/opengl";
        }

        static void CreateCacheDirectoryIfNeeded()
        {
            std::string dir = GetCacheDirectory();
            if (!std::filesystem::exists(dir))
                std::filesystem::create_directories(dir);
        }

        // Cache file extensions ¡ª opengl and vulkan kept separate so that
        // adding the Vulkan backend later doesn't invalidate OpenGL caches.
        static const char* GLShaderStageCachedOpenGLFileExtension(uint32_t stage)
        {
            switch (stage)
            {
            case GL_VERTEX_SHADER:   return ".cached_opengl.vert";
            case GL_FRAGMENT_SHADER: return ".cached_opengl.frag";
            }
            HZ_CORE_ASSERT(false);
            return "";
        }

        static const char* GLShaderStageCachedVulkanFileExtension(uint32_t stage)
        {
            switch (stage)
            {
            case GL_VERTEX_SHADER:   return ".cached_vulkan.vert";
            case GL_FRAGMENT_SHADER: return ".cached_vulkan.frag";
            }
            HZ_CORE_ASSERT(false);
            return "";
        }

    } // namespace Utils

    // =========================================================================
    //  Construction / destruction
    // =========================================================================

    OpenGLShader::OpenGLShader(const std::string& filepath)
        : m_FilePath(filepath)
    {
        HZ_PROFILE_FUNCTION();
        Utils::CreateCacheDirectoryIfNeeded();

        std::string source = ReadFile(filepath);
        auto        shaderSrcs = PreProcess(source);

        Timer timer;
        CompileOrGetVulkanBinaries(shaderSrcs);   // fills m_OpenGLSPIRV (see note above)
        CreateProgram();
        HZ_CORE_WARN("Shader creation took {0} ms", timer.ElapsedMillis());

        // Extract name from filepath  (e.g. "assets/shaders/Foo.glsl" -> "Foo")
        auto lastSlash = filepath.find_last_of("/\\");
        lastSlash = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
        auto lastDot = filepath.rfind('.');
        auto count = (lastDot == std::string::npos)
            ? filepath.size() - lastSlash
            : lastDot - lastSlash;
        m_Name = filepath.substr(lastSlash, count);
    }

    OpenGLShader::OpenGLShader(const std::string& name,
        const std::string& vertexSrc,
        const std::string& fragmentSrc)
        : m_Name(name)
    {
        HZ_PROFILE_FUNCTION();
        std::unordered_map<GLenum, std::string> sources;
        sources[GL_VERTEX_SHADER] = vertexSrc;
        sources[GL_FRAGMENT_SHADER] = fragmentSrc;

        CompileOrGetVulkanBinaries(sources);
        CreateProgram();
    }

    OpenGLShader::~OpenGLShader()
    {
        HZ_PROFILE_FUNCTION();
        glDeleteProgram(m_RendererID);
    }

    // =========================================================================
    //  File I/O
    // =========================================================================

    std::string OpenGLShader::ReadFile(const std::string& filepath)
    {
        HZ_PROFILE_FUNCTION();
        std::string   result;
        std::ifstream in(filepath, std::ios::in | std::ios::binary);
        if (in)
        {
            in.seekg(0, std::ios::end);
            result.resize(static_cast<size_t>(in.tellg()));
            in.seekg(0, std::ios::beg);
            in.read(result.data(), static_cast<std::streamsize>(result.size()));
        }
        else
        {
            HZ_CORE_ERROR("Could not open shader file '{0}'", filepath);
        }
        return result;
    }

    // =========================================================================
    //  Pre-processing  (#type vertex / #type fragment splitting)
    // =========================================================================

    std::unordered_map<GLenum, std::string>
        OpenGLShader::PreProcess(const std::string& source)
    {
        HZ_PROFILE_FUNCTION();
        std::unordered_map<GLenum, std::string> shaderSources;

        const char* typeToken = "#type";
        size_t      typeTokenLength = strlen(typeToken);
        size_t      pos = source.find(typeToken, 0);

        while (pos != std::string::npos)
        {
            size_t eol = source.find_first_of("\r\n", pos);
            HZ_CORE_ASSERT(eol != std::string::npos, "Syntax error in shader");
            size_t begin = pos + typeTokenLength + 1;
            std::string type = source.substr(begin, eol - begin);
            HZ_CORE_ASSERT(Utils::ShaderTypeFromString(type), "Invalid shader type");

            size_t nextLinePos = source.find_first_not_of("\r\n", eol);
            pos = source.find(typeToken, nextLinePos);
            shaderSources[Utils::ShaderTypeFromString(type)] =
                (pos == std::string::npos)
                ? source.substr(nextLinePos)
                : source.substr(nextLinePos, pos - nextLinePos);
        }
        return shaderSources;
    }

    // =========================================================================
    //  CompileOrGetVulkanBinaries
    //
    //  CURRENT BEHAVIOUR (OpenGL only):
    //    Compiles directly to OpenGL SPIR-V and stores in m_OpenGLSPIRV.
    //    m_VulkanSPIRV is left empty and unused.
    //    Cache extension used: .cached_opengl.vert / .cached_opengl.frag
    //
    //  FUTURE (when adding Vulkan):
    //    Change SetTargetEnvironment back to shaderc_target_env_vulkan.
    //    Store result in m_VulkanSPIRV.
    //    Use .cached_vulkan.vert / .cached_vulkan.frag.
    //    Restore CompileOrGetOpenGLBinaries as a separate step.
    // =========================================================================

    void OpenGLShader::CompileOrGetVulkanBinaries(
        const std::unordered_map<GLenum, std::string>& shaderSources)
    {
        shaderc::Compiler      compiler;
        shaderc::CompileOptions options;

        // ©¤©¤ Target ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤
        // Compiling straight to OpenGL 4.5 SPIR-V avoids the spirv_cross
        // round-trip (Vulkan SPIR-V -> GLSL -> OpenGL SPIR-V) that caused
        // interface-variable name mismatches between shader stages.
        //
        // To add Vulkan later: change to shaderc_target_env_vulkan +
        // shaderc_env_version_vulkan_1_2 and restore CompileOrGetOpenGLBinaries.
        options.SetTargetEnvironment(shaderc_target_env_opengl,
            shaderc_env_version_opengl_4_5);

        // Disable optimisation ¡ª the optimiser renames interface variables
        // which breaks the OpenGL linker's name-based interface matching.
        options.SetOptimizationLevel(shaderc_optimization_level_zero);

        std::filesystem::path cacheDir = Utils::GetCacheDirectory();

        // Store directly into m_OpenGLSPIRV (m_VulkanSPIRV unused for now)
        auto& shaderData = m_OpenGLSPIRV;
        shaderData.clear();

        for (auto&& [stage, source] : shaderSources)
        {
            std::filesystem::path shaderFilePath = m_FilePath;
            std::filesystem::path cachedPath =
                cacheDir / (shaderFilePath.filename().string()
                    + Utils::GLShaderStageCachedOpenGLFileExtension(stage));

            std::ifstream in(cachedPath, std::ios::in | std::ios::binary);
            if (in.is_open())
            {
                // Cache hit ¡ª load directly
                in.seekg(0, std::ios::end);
                auto size = in.tellg();
                in.seekg(0, std::ios::beg);
                auto& data = shaderData[stage];
                data.resize(static_cast<size_t>(size) / sizeof(uint32_t));
                in.read(reinterpret_cast<char*>(data.data()),
                    static_cast<std::streamsize>(size));
            }
            else
            {
                // Cache miss ¡ª compile and cache
                shaderc::SpvCompilationResult module =
                    compiler.CompileGlslToSpv(source,
                        Utils::GLShaderStageToShaderC(stage),
                        m_FilePath.c_str(),
                        options);

                if (module.GetCompilationStatus() != shaderc_compilation_status_success)
                {
                    HZ_CORE_ERROR("Shader compilation failed ({0} ¡ª {1}):\n{2}",
                        m_FilePath,
                        Utils::GLShaderStageToString(stage),
                        module.GetErrorMessage());
                    HZ_CORE_ASSERT(false);
                    continue;
                }

                shaderData[stage] = std::vector<uint32_t>(module.cbegin(),
                    module.cend());

                std::ofstream out(cachedPath, std::ios::out | std::ios::binary);
                if (out.is_open())
                {
                    auto& data = shaderData[stage];
                    out.write(reinterpret_cast<const char*>(data.data()),
                        static_cast<std::streamsize>(
                            data.size() * sizeof(uint32_t)));
                    out.flush();
                }
            }
        }

        // Reflect on the compiled SPIR-V for diagnostic logging
        for (auto&& [stage, data] : shaderData)
            Reflect(stage, data);
    }

    // =========================================================================
    //  CompileOrGetOpenGLBinaries
    //
    //  Currently unused ¡ª OpenGL SPIR-V is produced directly in
    //  CompileOrGetVulkanBinaries above.
    //
    //  Restore this function when adding Vulkan:
    //    Use spirv_cross to cross-compile m_VulkanSPIRV -> GLSL,
    //    then recompile to OpenGL SPIR-V and store in m_OpenGLSPIRV.
    // =========================================================================

    void OpenGLShader::CompileOrGetOpenGLBinaries()
    {
        // No-op: OpenGL SPIR-V already in m_OpenGLSPIRV from CompileOrGetVulkanBinaries.
        // Restore the spirv_cross conversion here when a Vulkan backend is added.
    }

    // =========================================================================
    //  CreateProgram  ¡ª upload SPIR-V, specialise entry point, link
    // =========================================================================

    void OpenGLShader::CreateProgram()
    {
        GLuint program = glCreateProgram();

        std::vector<GLuint> shaderIDs;
        for (auto&& [stage, spirv] : m_OpenGLSPIRV)
        {
            GLuint shaderID = shaderIDs.emplace_back(glCreateShader(stage));
            glShaderBinary(1, &shaderID,
                GL_SHADER_BINARY_FORMAT_SPIR_V,
                spirv.data(),
                static_cast<GLsizei>(spirv.size() * sizeof(uint32_t)));
            glSpecializeShader(shaderID, "main", 0, nullptr, nullptr);
            glAttachShader(program, shaderID);
        }

        glLinkProgram(program);

        GLint isLinked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
        if (isLinked == GL_FALSE)
        {
            GLint maxLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
            std::vector<GLchar> infoLog(maxLength);
            glGetProgramInfoLog(program, maxLength, &maxLength, infoLog.data());
            HZ_CORE_ERROR("Shader linking failed ({0}):\n{1}",
                m_FilePath, infoLog.data());
            glDeleteProgram(program);
            for (auto id : shaderIDs)
                glDeleteShader(id);
            return;
        }

        for (auto id : shaderIDs)
        {
            glDetachShader(program, id);
            glDeleteShader(id);
        }

        m_RendererID = program;
    }

    // =========================================================================
    //  Reflect  ¡ª log UBO / sampler info from SPIR-V
    // =========================================================================

    void OpenGLShader::Reflect(GLenum stage, const std::vector<uint32_t>& shaderData)
    {
        spirv_cross::Compiler        compiler(shaderData);
        spirv_cross::ShaderResources resources = compiler.get_shader_resources();

        HZ_CORE_TRACE("OpenGLShader::Reflect - {0} {1}",
            Utils::GLShaderStageToString(stage), m_FilePath);
        HZ_CORE_TRACE("    {0} uniform buffers", resources.uniform_buffers.size());
        HZ_CORE_TRACE("    {0} resources", resources.sampled_images.size());
        HZ_CORE_TRACE("Uniform buffers:");

        for (const auto& resource : resources.uniform_buffers)
        {
            const auto& bufferType = compiler.get_type(resource.base_type_id);
            uint32_t    bufferSize = compiler.get_declared_struct_size(bufferType);
            uint32_t    binding = compiler.get_decoration(resource.id,
                spv::DecorationBinding);
            int         memberCount = static_cast<int>(bufferType.member_types.size());

            HZ_CORE_TRACE("  {0}", resource.name);
            HZ_CORE_TRACE("    Size = {0}", bufferSize);
            HZ_CORE_TRACE("    Binding = {0}", binding);
            HZ_CORE_TRACE("    Members = {0}", memberCount);
        }
    }

    // =========================================================================
    //  Shader uniform setters  (pass-through to OpenGL, unchanged)
    // =========================================================================

    void OpenGLShader::Bind()   const { HZ_PROFILE_FUNCTION(); glUseProgram(m_RendererID); }
    void OpenGLShader::Unbind() const { HZ_PROFILE_FUNCTION(); glUseProgram(0); }

    void OpenGLShader::SetInt(const std::string& name, int value)
    {
        HZ_PROFILE_FUNCTION(); UploadUniformInt(name, value);
    }
    void OpenGLShader::SetIntArray(const std::string& name, int* values, uint32_t count)
    {
        HZ_PROFILE_FUNCTION(); UploadUniformIntArray(name, values, count);
    }
    void OpenGLShader::SetFloat(const std::string& name, float value)
    {
        HZ_PROFILE_FUNCTION(); UploadUniformFloat(name, value);
    }

    void OpenGLShader::SetFloat2(const std::string& name, const glm::vec2& value)
    {
        HZ_PROFILE_FUNCTION(); UploadUniformFloat2(name, value);
    }

    void OpenGLShader::SetFloat3(const std::string& name, const glm::vec3& value)
    {
        HZ_PROFILE_FUNCTION(); UploadUniformFloat3(name, value);
    }
    void OpenGLShader::SetFloat4(const std::string& name, const glm::vec4& value)
    {
        HZ_PROFILE_FUNCTION(); UploadUniformFloat4(name, value);
    }
    void OpenGLShader::SetMat4(const std::string& name, const glm::mat4& value)
    {
        HZ_PROFILE_FUNCTION(); UploadUniformMat4(name, value);
    }

    uint32_t OpenGLShader::GetRendererID() const { return m_RendererID; }

    void OpenGLShader::UploadUniformInt(const std::string& name, int value)
    {
        glUniform1i(glGetUniformLocation(m_RendererID, name.c_str()), value);
    }
    void OpenGLShader::UploadUniformIntArray(const std::string& name, int* values, uint32_t count)
    {
        glUniform1iv(glGetUniformLocation(m_RendererID, name.c_str()), count, values);
    }
    void OpenGLShader::UploadUniformFloat(const std::string& name, float value)
    {
        glUniform1f(glGetUniformLocation(m_RendererID, name.c_str()), value);
    }
    void OpenGLShader::UploadUniformFloat2(const std::string& name, const glm::vec2& value)
    {
        glUniform2f(glGetUniformLocation(m_RendererID, name.c_str()), value.x, value.y);
    }
    void OpenGLShader::UploadUniformFloat3(const std::string& name, const glm::vec3& value)
    {
        glUniform3f(glGetUniformLocation(m_RendererID, name.c_str()), value.x, value.y, value.z);
    }
    void OpenGLShader::UploadUniformFloat4(const std::string& name, const glm::vec4& v)
    {
        glUniform4f(glGetUniformLocation(m_RendererID, name.c_str()), v.x, v.y, v.z, v.w);
    }
    void OpenGLShader::UploadUniformMat3(const std::string& name, const glm::mat3& matrix)
    {
        glUniformMatrix3fv(glGetUniformLocation(m_RendererID, name.c_str()), 1, GL_FALSE, glm::value_ptr(matrix));
    }
    void OpenGLShader::UploadUniformMat4(const std::string& name, const glm::mat4& matrix)
    {
        glUniformMatrix4fv(glGetUniformLocation(m_RendererID, name.c_str()), 1, GL_FALSE, glm::value_ptr(matrix));
    }

} // namespace Hazel