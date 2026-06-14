#include "hzpch.h"
#include "OpenGLIBL.h"
#include <glad/glad.h>
#include <stb_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <fstream>

#include "Hazel/Renderer/Shader.h"
#include "Hazel/Renderer/VertexArray.h"
#include "Hazel/Renderer/Buffer.h"
#include "Hazel/Renderer/UniformBuffer.h"

namespace Hazel {

    // =========================================================================
    //  Static members
    // =========================================================================

    uint32_t OpenGLIBL::s_BrdfLUT = 0;

    static const glm::mat4 s_CaptureProjection =
        glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

    static const glm::mat4 s_CaptureViews[6] =
    {
        glm::lookAt(glm::vec3(0), { 1, 0, 0}, {0,-1, 0}),
        glm::lookAt(glm::vec3(0), {-1, 0, 0}, {0,-1, 0}),
        glm::lookAt(glm::vec3(0), { 0, 1, 0}, {0, 0, 1}),
        glm::lookAt(glm::vec3(0), { 0,-1, 0}, {0, 0,-1}),
        glm::lookAt(glm::vec3(0), { 0, 0, 1}, {0,-1, 0}),
        glm::lookAt(glm::vec3(0), { 0, 0,-1}, {0,-1, 0}),
    };

    // =========================================================================
    //  IBLResult 移动 / 析构（定义在这里，因为 glDeleteTextures 需要 glad）
    // =========================================================================

    void IBLResult::Release()
    {
        if (IrradianceMap) { glDeleteTextures(1, &IrradianceMap); IrradianceMap = 0; }
        if (PrefilterMap) { glDeleteTextures(1, &PrefilterMap);  PrefilterMap = 0; }
        // BrdfLUT 是共享资源，由 OpenGLIBL::s_BrdfLUT 持有，这里不删除
        BrdfLUT = 0;
    }

    IBLResult::IBLResult(IBLResult&& o) noexcept
        : IrradianceMap(o.IrradianceMap), PrefilterMap(o.PrefilterMap),
        BrdfLUT(o.BrdfLUT)
    {
        o.IrradianceMap = o.PrefilterMap = o.BrdfLUT = 0;
    }

    IBLResult& IBLResult::operator=(IBLResult&& o) noexcept
    {
        if (this != &o)
        {
            Release();
            IrradianceMap = o.IrradianceMap;
            PrefilterMap = o.PrefilterMap;
            BrdfLUT = o.BrdfLUT;
            o.IrradianceMap = o.PrefilterMap = o.BrdfLUT = 0;
        }
        return *this;
    }

    // =========================================================================
    //  Private helpers
    // =========================================================================

    // 单位正方体 VAO，给六面渲染pass用
    static Ref<VertexArray> CreateCubeVAO()
    {
        float verts[] = {
            -1,-1,-1,  1, 1,-1,  1,-1,-1,   1, 1,-1, -1,-1,-1, -1, 1,-1,
            -1,-1, 1,  1,-1, 1,  1, 1, 1,   1, 1, 1, -1, 1, 1, -1,-1, 1,
            -1, 1, 1, -1, 1,-1, -1,-1,-1,  -1,-1,-1, -1,-1, 1, -1, 1, 1,
             1, 1, 1,  1,-1,-1,  1, 1,-1,   1,-1,-1,  1, 1, 1,  1,-1, 1,
            -1,-1,-1,  1,-1, 1,  1,-1,-1,   1,-1, 1, -1,-1,-1, -1,-1, 1,
            -1, 1,-1,  1, 1,-1,  1, 1, 1,   1, 1, 1, -1, 1, 1, -1, 1,-1,
        };
        auto vbo = VertexBuffer::Create(verts, sizeof(verts));
        vbo->SetLayout({ { "a_Position", ShaderDataType::Float3 } });
        auto vao = VertexArray::Create();
        vao->AddVertexBuffer(vbo);
        return vao;
    }

    // CaptureCamera UBO（对应 IBL shader 里 binding = 1）
    struct CaptureCameraUBO { glm::mat4 ViewProjection; };

    uint32_t OpenGLIBL::LoadEquirect(const std::string& path)
    {
        stbi_set_flip_vertically_on_load(true);
        int w, h, c;
        float* data = stbi_loadf(path.c_str(), &w, &h, &c, 0);
        if (!data)
        {
            HZ_CORE_ERROR("OpenGLIBL: failed to load HDR '{0}'", path);
            return 0;
        }

        GLuint tex;
        glCreateTextures(GL_TEXTURE_2D, 1, &tex);
        glTextureStorage2D(tex, 1, GL_RGB16F, w, h);
        glTextureSubImage2D(tex, 0, 0, 0, w, h, GL_RGB, GL_FLOAT, data);
        glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
        return static_cast<uint32_t>(tex);
    }

    uint32_t OpenGLIBL::CreateCubemap(int size, uint32_t internalFormat, bool generateMips)
    {
        GLuint tex;
        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &tex);

        int mipLevels = generateMips
            ? 1 + static_cast<int>(glm::floor(glm::log2(static_cast<float>(size))))
            : 1;
        glTextureStorage2D(tex, mipLevels, static_cast<GLenum>(internalFormat), size, size);

        glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(tex, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER,
            generateMips ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        return static_cast<uint32_t>(tex);
    }

    static void RenderCubeFaces(const Ref<Shader>& shader,
        uint32_t captureFBO,
        uint32_t /*rbo*/,
        uint32_t targetCubemap,
        int      resolution,
        int      mip)
    {
        // binding = 1：六面渲染共用的 CaptureCamera UBO
        static Ref<UniformBuffer> s_CaptureUBO =
            UniformBuffer::Create(sizeof(CaptureCameraUBO), 1);

        auto cubeVAO = CreateCubeVAO();
        shader->Bind();

        glViewport(0, 0, resolution, resolution);
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(captureFBO));

        for (int face = 0; face < 6; face++)
        {
            CaptureCameraUBO camData;
            camData.ViewProjection = s_CaptureProjection * s_CaptureViews[face];
            s_CaptureUBO->SetData(&camData, sizeof(CaptureCameraUBO));

            glNamedFramebufferTextureLayer(
                static_cast<GLuint>(captureFBO),
                GL_COLOR_ATTACHMENT0,
                static_cast<GLuint>(targetCubemap),
                mip, face);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    uint32_t OpenGLIBL::CreateBrdfLUT()
    {
        GLuint lut;
        glCreateTextures(GL_TEXTURE_2D, 1, &lut);
        glTextureStorage2D(lut, 1, GL_RG16F, k_BrdfLUTSize, k_BrdfLUTSize);
        glTextureParameteri(lut, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(lut, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(lut, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(lut, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        GLuint fbo, rbo;
        glCreateFramebuffers(1, &fbo);
        glCreateRenderbuffers(1, &rbo);
        glNamedRenderbufferStorage(rbo, GL_DEPTH_COMPONENT24, k_BrdfLUTSize, k_BrdfLUTSize);
        glNamedFramebufferRenderbuffer(fbo, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
        glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, lut, 0);

        auto brdfShader = Shader::Create("assets/shaders/IBL_BrdfLUT.glsl");
        brdfShader->Bind();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, k_BrdfLUTSize, k_BrdfLUTSize);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 3); // fullscreen triangle，shader 用 gl_VertexID

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        glDeleteRenderbuffers(1, &rbo);

        return static_cast<uint32_t>(lut);
    }

    std::string OpenGLIBL::GetCachePath(const std::string& hdrPath)
    {
        std::filesystem::path p(hdrPath);
        return "assets/cache/ibl/" + p.stem().string() + ".iblcache";
    }

    // =========================================================================
    //  Cache 文件格式
    //  [4]  magic   0x4C424948 ("HIBL")
    //  [4]  version 1
    //  [4]  irradianceSize
    //  [4]  prefilterSize
    //  [4]  prefilterMips
    //  [4]  brdfLUTSize
    //  [...]  IrradianceMap 像素（6 face × 1 mip，RGB16F）
    //  [...]  PrefilterMap  像素（6 face × k_PrefilterMips mip，RGB16F）
    //  [...]  BrdfLUT       像素（RG16F，2D）
    // =========================================================================

    static constexpr uint32_t k_CacheMagic = 0x4C424948u;
    static constexpr uint32_t k_CacheVersion = 1u;

    static void WriteCubemap(std::ofstream& f, uint32_t tex,
        int baseSize, int mipLevels)
    {
        for (int mip = 0; mip < mipLevels; mip++)
        {
            int    size = std::max(1, baseSize >> mip);
            size_t faceBytes = static_cast<size_t>(size * size) * 3 * sizeof(uint16_t);
            std::vector<uint8_t> buf(faceBytes);

            for (int face = 0; face < 6; face++)
            {
                glGetTextureSubImage(
                    static_cast<GLuint>(tex),
                    mip,
                    0, 0, face,
                    size, size, 1,
                    GL_RGB, GL_HALF_FLOAT,
                    static_cast<GLsizei>(faceBytes),
                    buf.data());
                f.write(reinterpret_cast<char*>(buf.data()),
                    static_cast<std::streamsize>(faceBytes));
            }
        }
    }

    static void ReadCubemap(std::ifstream& f, uint32_t tex,
        int baseSize, int mipLevels)
    {
        for (int mip = 0; mip < mipLevels; mip++)
        {
            int    size = std::max(1, baseSize >> mip);
            size_t faceBytes = static_cast<size_t>(size * size) * 3 * sizeof(uint16_t);
            std::vector<uint8_t> buf(faceBytes);

            for (int face = 0; face < 6; face++)
            {
                f.read(reinterpret_cast<char*>(buf.data()),
                    static_cast<std::streamsize>(faceBytes));
                glTextureSubImage3D(
                    static_cast<GLuint>(tex),
                    mip,
                    0, 0, face,
                    size, size, 1,
                    GL_RGB, GL_HALF_FLOAT,
                    buf.data());
            }
        }
    }

    // =========================================================================
    //  Public API
    // =========================================================================

    uint32_t OpenGLIBL::GetBrdfLUT()
    {
        return s_BrdfLUT;
    }

    Ref<IBLResult> OpenGLIBL::Compute(const std::string& hdrPath)
    {
        HZ_CORE_INFO("OpenGLIBL: processing '{0}'", hdrPath);

        uint32_t equirectTex = LoadEquirect(hdrPath);
        if (!equirectTex) return nullptr;

        // 共享 FBO + RBO，每个 pass 前 resize RBO
        GLuint captureFBO, captureRBO;
        glCreateFramebuffers(1, &captureFBO);
        glCreateRenderbuffers(1, &captureRBO);
        glNamedRenderbufferStorage(captureRBO, GL_DEPTH_COMPONENT24,
            k_CubemapSize, k_CubemapSize);
        glNamedFramebufferRenderbuffer(captureFBO, GL_DEPTH_ATTACHMENT,
            GL_RENDERBUFFER, captureRBO);

        auto result = CreateRef<IBLResult>();

        // ── Pass 1: Equirect → Environment Cubemap ─────────────────────────
        uint32_t envCubemap = CreateCubemap(k_CubemapSize, GL_RGB16F, true);
        auto equirectShader = Shader::Create("assets/shaders/IBL_EquirectToCubemap.glsl");
        glBindTextureUnit(0, static_cast<GLuint>(equirectTex));
        glNamedRenderbufferStorage(captureRBO, GL_DEPTH_COMPONENT24,
            k_CubemapSize, k_CubemapSize);
        RenderCubeFaces(equirectShader, captureFBO, captureRBO,
            envCubemap, k_CubemapSize, 0);
        glGenerateTextureMipmap(static_cast<GLuint>(envCubemap));
        glDeleteTextures(1, reinterpret_cast<GLuint*>(&equirectTex));

        // ── Pass 2: Irradiance convolution ─────────────────────────────────
        result->IrradianceMap = CreateCubemap(k_IrradianceSize, GL_RGB16F, false);
        auto irradianceShader = Shader::Create("assets/shaders/IBL_Irradiance.glsl");
        glBindTextureUnit(0, static_cast<GLuint>(envCubemap));
        glNamedRenderbufferStorage(captureRBO, GL_DEPTH_COMPONENT24,
            k_IrradianceSize, k_IrradianceSize);
        RenderCubeFaces(irradianceShader, captureFBO, captureRBO,
            result->IrradianceMap, k_IrradianceSize, 0);

        // ── Pass 3: Specular prefilter（每个粗糙度一个 mip）───────────────
        result->PrefilterMap = CreateCubemap(k_PrefilterSize, GL_RGB16F, true);
        auto prefilterShader = Shader::Create("assets/shaders/IBL_Prefilter.glsl");
        glBindTextureUnit(0, static_cast<GLuint>(envCubemap));

        struct PrefilterParams { float Roughness; float Resolution; };
        auto prefilterUBO = UniformBuffer::Create(sizeof(PrefilterParams), 2);

        for (int mip = 0; mip < k_PrefilterMips; mip++)
        {
            int mipSize = std::max(1, static_cast<int>(
                k_PrefilterSize * glm::pow(0.5f, static_cast<float>(mip))));

            glNamedRenderbufferStorage(captureRBO, GL_DEPTH_COMPONENT24, mipSize, mipSize);

            PrefilterParams params;
            params.Roughness = static_cast<float>(mip) / (k_PrefilterMips - 1);
            params.Resolution = static_cast<float>(k_CubemapSize);
            prefilterUBO->SetData(&params, sizeof(PrefilterParams));

            RenderCubeFaces(prefilterShader, captureFBO, captureRBO,
                result->PrefilterMap, mipSize, mip);
        }

        // ── Pass 4: BRDF LUT（全局共享，只算一次）─────────────────────────
        if (!s_BrdfLUT)
            s_BrdfLUT = CreateBrdfLUT();
        result->BrdfLUT = s_BrdfLUT;

        // 清理临时资源
        glDeleteTextures(1, reinterpret_cast<GLuint*>(&envCubemap));
        glDeleteFramebuffers(1, &captureFBO);
        glDeleteRenderbuffers(1, &captureRBO);

        HZ_CORE_INFO("OpenGLIBL: compute done.");
        return result;
    }

    void OpenGLIBL::SaveToCache(const Ref<IBLResult>& ibl,
        const std::string& cachePath)
    {
        std::filesystem::create_directories(
            std::filesystem::path(cachePath).parent_path());

        std::ofstream f(cachePath, std::ios::binary);
        if (!f)
        {
            HZ_CORE_WARN("OpenGLIBL: cannot write cache '{0}'", cachePath);
            return;
        }

        // Header
        f.write(reinterpret_cast<const char*>(&k_CacheMagic), 4);
        f.write(reinterpret_cast<const char*>(&k_CacheVersion), 4);
        int32_t sizes[] = { k_IrradianceSize, k_PrefilterSize,
                            k_PrefilterMips,  k_BrdfLUTSize };
        f.write(reinterpret_cast<const char*>(sizes), sizeof(sizes));

        WriteCubemap(f, ibl->IrradianceMap, k_IrradianceSize, 1);
        WriteCubemap(f, ibl->PrefilterMap, k_PrefilterSize, k_PrefilterMips);

        // BRDF LUT（RG16F，2D）
        size_t lutBytes = static_cast<size_t>(k_BrdfLUTSize * k_BrdfLUTSize)
            * 2 * sizeof(uint16_t);
        std::vector<uint8_t> lutBuf(lutBytes);
        glGetTextureImage(static_cast<GLuint>(ibl->BrdfLUT), 0,
            GL_RG, GL_HALF_FLOAT,
            static_cast<GLsizei>(lutBytes), lutBuf.data());
        f.write(reinterpret_cast<char*>(lutBuf.data()),
            static_cast<std::streamsize>(lutBytes));

        HZ_CORE_INFO("OpenGLIBL: cache saved to '{0}'", cachePath);
    }

    Ref<IBLResult> OpenGLIBL::LoadFromCache(const std::string& cachePath)
    {
        std::ifstream f(cachePath, std::ios::binary);
        if (!f) return nullptr;

        uint32_t magic, version;
        f.read(reinterpret_cast<char*>(&magic), 4);
        f.read(reinterpret_cast<char*>(&version), 4);
        if (magic != k_CacheMagic || version != k_CacheVersion)
        {
            HZ_CORE_WARN("OpenGLIBL: stale/corrupt cache '{0}', recomputing", cachePath);
            return nullptr;
        }

        int32_t sizes[4];
        f.read(reinterpret_cast<char*>(sizes), sizeof(sizes));
        if (sizes[0] != k_IrradianceSize || sizes[1] != k_PrefilterSize ||
            sizes[2] != k_PrefilterMips || sizes[3] != k_BrdfLUTSize)
        {
            HZ_CORE_WARN("OpenGLIBL: size mismatch in cache '{0}', recomputing", cachePath);
            return nullptr;
        }

        auto result = CreateRef<IBLResult>();

        result->IrradianceMap = CreateCubemap(k_IrradianceSize, GL_RGB16F, false);
        ReadCubemap(f, result->IrradianceMap, k_IrradianceSize, 1);

        result->PrefilterMap = CreateCubemap(k_PrefilterSize, GL_RGB16F, true);
        ReadCubemap(f, result->PrefilterMap, k_PrefilterSize, k_PrefilterMips);

        // BRDF LUT — 共享，只建一次
        if (!s_BrdfLUT)
        {
            GLuint lut;
            glCreateTextures(GL_TEXTURE_2D, 1, &lut);
            glTextureStorage2D(lut, 1, GL_RG16F, k_BrdfLUTSize, k_BrdfLUTSize);
            glTextureParameteri(lut, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteri(lut, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTextureParameteri(lut, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTextureParameteri(lut, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            size_t lutBytes = static_cast<size_t>(k_BrdfLUTSize * k_BrdfLUTSize)
                * 2 * sizeof(uint16_t);
            std::vector<uint8_t> lutBuf(lutBytes);
            f.read(reinterpret_cast<char*>(lutBuf.data()),
                static_cast<std::streamsize>(lutBytes));
            glTextureSubImage2D(lut, 0, 0, 0,
                k_BrdfLUTSize, k_BrdfLUTSize,
                GL_RG, GL_HALF_FLOAT, lutBuf.data());
            s_BrdfLUT = static_cast<uint32_t>(lut);
        }
        result->BrdfLUT = s_BrdfLUT;

        HZ_CORE_INFO("OpenGLIBL: loaded from cache '{0}'", cachePath);
        return result;
    }

    Ref<IBLResult> OpenGLIBL::ComputeOrLoad(const std::string& hdrPath)
    {
        std::string cachePath = GetCachePath(hdrPath);

        auto result = LoadFromCache(cachePath);
        if (result) return result;

        result = Compute(hdrPath);
        if (result) SaveToCache(result, cachePath);
        return result;
    }

} // namespace Hazel