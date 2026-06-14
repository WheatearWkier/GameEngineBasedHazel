#include "hzpch.h"
// =============================================================
//  Renderer3D.cpp  (updated for IBL)
//
//  Key changes vs original:
//    - SkyboxUBO replaced by IBLDataUBO (binding=10) and
//      SkyboxGradientUBO (binding=14)
//    - Three IBL textures bound to slots 11, 12, 13
//    - SetIBL() / ClearIBL() / SetIBLIntensity()
//    - DrawMesh() uploads IBL textures when available
// =============================================================

#include "Renderer3D.h"
#include "RenderCommand.h"
#include "Shader.h"
#include "UniformBuffer.h"
#include "VertexArray.h"
#include <random>
#include <glm/gtc/matrix_transform.hpp>

namespace Hazel {

    // =========================================================================
    //  GPU-side data structs  (must match std140 layout in shaders exactly)
    // =========================================================================

    struct DirectionalLightData
    {
        glm::vec4 Direction = { -0.5f, -1.0f, -0.5f, 0.0f };
        glm::vec4 Color     = {  1.0f,  1.0f,  1.0f, 0.0f }; // w = intensity
    };

    struct PointLightData
    {
        glm::vec4 Position  = { 0, 0, 0, 0 };
        glm::vec4 Color     = { 1, 1, 1, 1 }; // w = intensity
        float Constant  = 1.0f;
        float Linear    = 0.09f;
        float Quadratic = 0.032f;
        float _pad      = 0.0f;
    };

    struct LightUBOData
    {
        DirectionalLightData DirLight;
        PointLightData       PointLights[k_MaxPointLights];
        int                  ActivePointLights = 0;
        float                _pad[3] = {};
    };

    struct CameraUBOData      // binding=1, 208 bytes
    {
        glm::mat4 ViewProjection;
        glm::mat4 View;
        glm::mat4 Projection;
        glm::vec3 CameraPosition;
        float     Near = 0.1f;
    };

    struct TransformUBOData   // binding=2, 192 bytes
    {
        glm::mat4 Model;
        glm::mat4 NormalMatrix;
        glm::mat4 LightSpaceMatrix;
    };

    struct MaterialUBOData    // binding=3, 48 bytes
    {
        glm::vec4 Albedo;
        float     Metallic;
        float     Roughness;
        int       EntityID;
        int       FlipNormals;
        int       HasNormalMap;
        int       HasRoughnessMap;
        int       HasMetallicMap;
        int       HasShadowMap;
    };

    // NEW: replaces the old Skybox UBO at binding=10
    struct IBLDataUBO         // binding=10, 16 bytes
    {
        float IBLIntensity = 1.0f;
        int   HasIBL       = 0;
        int   HasSSAO = 0;
        float _pad = 0.0f;
    };

    // SSAO 参数 UBO（binding=15）
    // 与 SSAO.glsl 中 SSAOParams UBO 的 std140 布局完全一致
    struct SSAOParamsUBO      // 32 bytes
    {
        glm::vec4 NoiseScale = { 0, 0, 0, 0 }; // xy=(w/4, h/4)，zw 填充
        float     Radius = 0.5f;
        float     Bias = 0.025f;
        float     Power = 1.5f;
        float     _pad = 0.0f;
    };

    // SSAO 采样核 UBO（binding=16）
    // std140 中 vec3 数组每个元素对齐到 vec4，所以直接用 vec4
    struct SSAOKernelUBO      // 64 * 16 = 1024 bytes
    {
        glm::vec4 Samples[64] = {};
    };

    // NEW: gradient colours now at binding=14 (skybox shader only)
    struct SkyboxGradientUBO  // binding=14, 48 bytes
    {
        glm::vec4 TopColor;
        glm::vec4 HorizonColor;
        glm::vec4 BottomColor;
    };

    // =========================================================================
    //  Internal state
    // =========================================================================

    struct Renderer3DData
    {
        // --- UBOs ---
        CameraUBOData      CameraBuffer;
        Ref<UniformBuffer> CameraUBO;           // binding=1

        TransformUBOData   TransformBuffer;
        Ref<UniformBuffer> TransformUBO;        // binding=2

        MaterialUBOData    MaterialBuffer;
        Ref<UniformBuffer> MaterialUBO;         // binding=3

        LightUBOData       LightBuffer;
        Ref<UniformBuffer> LightUBO;            // binding=5
        bool               LightsDirty = true;

        IBLDataUBO         IBLBuffer;
        Ref<UniformBuffer> IBLUBO;              // binding=10

        SkyboxGradientUBO  SkyboxGradientBuffer;
        Ref<UniformBuffer> SkyboxGradientUBO;   // binding=14

        // --- Shaders ---
        Ref<Shader> MeshShader;
        Ref<Shader> ShadowShader;
        Ref<Shader> EditorSkyboxShader;
        Ref<Shader> EditorGridShader;

        // --- Textures ---
        Ref<Texture2D>   WhiteTexture;
        Ref<IBLResult>   CurrentIBL;            // null when no IBL loaded

        // --- Shadow map ---
        Ref<Framebuffer> ShadowMapFB;
        glm::mat4        LightSpaceMatrix = glm::mat4(1.0f);
        static constexpr uint32_t k_ShadowMapSize = 2048;

        // --- Skybox geometry ---
        Ref<VertexArray>  SkyboxVAO;
        Ref<VertexBuffer> SkyboxVBO;
        glm::vec3         SkyTopColor     = { 0.15f, 0.45f, 0.82f };
        glm::vec3         SkyHorizonColor = { 0.60f, 0.78f, 0.95f };
        glm::vec3         SkyBottomColor  = { 0.25f, 0.20f, 0.15f };

        // --- Editor grid ---
        Ref<VertexArray> GridVAO;

        struct SSAOData
        {
            Ref<Framebuffer>   RawFBO;
            Ref<Framebuffer>   BlurFBO;
            Ref<Shader>        SSAOShader;
            Ref<Shader>        BlurShader;
            Ref<VertexArray>   QuadVAO;
            Ref<Texture2D>     NoiseTex;
            SSAOParamsUBO      ParamsBuffer;
            Ref<UniformBuffer> ParamsUBO;
            SSAOKernelUBO      KernelBuffer;
            Ref<UniformBuffer> KernelUBO;
            bool  Enabled = true;
            float Radius  = 0.5f;
            float Bias    = 0.025f;
            float Power   = 1.5f;
        } SSAO;

        Renderer3D::Statistics Stats;
    };

    static Renderer3DData s_Data;

    // =========================================================================
    //  Init / Shutdown
    // =========================================================================

    void Renderer3D::Init()
    {
        // 1×1 white fallback texture
        s_Data.WhiteTexture = Texture2D::Create(1, 1);
        constexpr uint32_t white = 0xffffffff;
        s_Data.WhiteTexture->SetData(const_cast<uint32_t*>(&white), sizeof(uint32_t));

        // UBOs
        s_Data.CameraUBO        = UniformBuffer::Create(sizeof(CameraUBOData),      1);
        s_Data.TransformUBO     = UniformBuffer::Create(sizeof(TransformUBOData),   2);
        s_Data.MaterialUBO      = UniformBuffer::Create(sizeof(MaterialUBOData),    3);
        s_Data.LightUBO         = UniformBuffer::Create(sizeof(LightUBOData),       5);
        s_Data.IBLUBO           = UniformBuffer::Create(sizeof(IBLDataUBO),        10);
        s_Data.SkyboxGradientUBO= UniformBuffer::Create(sizeof(SkyboxGradientUBO), 14);

        // Shaders
        s_Data.MeshShader        = Shader::Create("assets/shaders/Renderer3D_Mesh_PBR.glsl");
        s_Data.ShadowShader      = Shader::Create("assets/shaders/Renderer3D_Shadow.glsl");
        s_Data.EditorSkyboxShader= Shader::Create("assets/shaders/Renderer3D_EditorSkybox.glsl");
        s_Data.EditorGridShader  = Shader::Create("assets/shaders/Renderer3D_EditorGrid.glsl");

        // Shadow framebuffer
        FramebufferSpecification shadowSpec;
        shadowSpec.Width       = Renderer3DData::k_ShadowMapSize;
        shadowSpec.Height      = Renderer3DData::k_ShadowMapSize;
        shadowSpec.Attachments = { FramebufferTextureFormat::DEPTH32F };
        s_Data.ShadowMapFB     = Framebuffer::Create(shadowSpec);

        // Skybox cube geometry
        float skyVerts[] = {
            -1,-1,-1,  1, 1,-1,  1,-1,-1,   1, 1,-1, -1,-1,-1, -1, 1,-1,
            -1,-1, 1,  1,-1, 1,  1, 1, 1,   1, 1, 1, -1, 1, 1, -1,-1, 1,
            -1, 1, 1, -1, 1,-1, -1,-1,-1,  -1,-1,-1, -1,-1, 1, -1, 1, 1,
             1, 1, 1,  1,-1,-1,  1, 1,-1,   1,-1,-1,  1, 1, 1,  1,-1, 1,
            -1,-1,-1,  1,-1, 1,  1,-1,-1,   1,-1, 1, -1,-1,-1, -1,-1, 1,
            -1, 1,-1,  1, 1,-1,  1, 1, 1,   1, 1, 1, -1, 1, 1, -1, 1,-1,
        };
        s_Data.SkyboxVBO = VertexBuffer::Create(skyVerts, sizeof(skyVerts));
        s_Data.SkyboxVBO->SetLayout({ { "a_Position", ShaderDataType::Float3 } });
        s_Data.SkyboxVAO = VertexArray::Create();
        s_Data.SkyboxVAO->AddVertexBuffer(s_Data.SkyboxVBO);

        // Grid VAO (shader generates verts from gl_VertexID)
        s_Data.GridVAO = VertexArray::Create();

        // Initial IBL UBO (no IBL)
        s_Data.IBLBuffer = IBLDataUBO{};
        s_Data.IBLUBO->SetData(&s_Data.IBLBuffer, sizeof(IBLDataUBO));

        // Initial gradient
        s_Data.SkyboxGradientBuffer.TopColor     = glm::vec4(s_Data.SkyTopColor,     1.0f);
        s_Data.SkyboxGradientBuffer.HorizonColor = glm::vec4(s_Data.SkyHorizonColor, 1.0f);
        s_Data.SkyboxGradientBuffer.BottomColor  = glm::vec4(s_Data.SkyBottomColor,  1.0f);
        s_Data.SkyboxGradientUBO->SetData(&s_Data.SkyboxGradientBuffer, sizeof(SkyboxGradientUBO));
    }

    void Renderer3D::Shutdown()
    {
        s_Data.MeshShader.reset();
        s_Data.ShadowShader.reset();
        s_Data.EditorSkyboxShader.reset();
        s_Data.EditorGridShader.reset();
        s_Data.WhiteTexture.reset();
        s_Data.ShadowMapFB.reset();
        s_Data.SkyboxVAO.reset();
        s_Data.SkyboxVBO.reset();
        s_Data.GridVAO.reset();
        s_Data.CurrentIBL.reset();
        s_Data.CameraUBO.reset();
        s_Data.TransformUBO.reset();
        s_Data.MaterialUBO.reset();
        s_Data.LightUBO.reset();
        s_Data.IBLUBO.reset();
        s_Data.SkyboxGradientUBO.reset();
    }

    // =========================================================================
    //  BeginScene / EndScene
    // =========================================================================

    void Renderer3D::BeginScene(const Camera& camera, const glm::mat4& cameraTransform)
    {
        glm::mat4 view = glm::inverse(cameraTransform);
        s_Data.CameraBuffer.ViewProjection = camera.GetProjection() * view;
        s_Data.CameraBuffer.View           = view;
        s_Data.CameraBuffer.Projection     = camera.GetProjection();
        s_Data.CameraBuffer.CameraPosition = glm::vec3(cameraTransform[3]);
        s_Data.CameraBuffer.Near           = 0.1f;
        s_Data.CameraUBO->SetData(&s_Data.CameraBuffer, sizeof(CameraUBOData));

        s_Data.LightBuffer = {};
        FlushLights();
        s_Data.LightsDirty = false;
        ResetStats();
    }

    void Renderer3D::BeginScene(const EditorCamera& camera)
    {
        s_Data.CameraBuffer.ViewProjection = camera.GetViewProjection();
        s_Data.CameraBuffer.View           = camera.GetViewMatrix();
        s_Data.CameraBuffer.Projection     = camera.GetProjection();
        s_Data.CameraBuffer.CameraPosition = camera.GetPosition();
        s_Data.CameraBuffer.Near           = 0.1f;
        s_Data.CameraUBO->SetData(&s_Data.CameraBuffer, sizeof(CameraUBOData));

        s_Data.LightBuffer = {};
        FlushLights();
        s_Data.LightsDirty = false;
        ResetStats();
    }

    void Renderer3D::EndScene()
    {
        if (s_Data.LightsDirty) FlushLights();
    }

    // =========================================================================
    //  Lights
    // =========================================================================

    void Renderer3D::SetDirectionalLight(const glm::vec3& direction,
                                         const glm::vec3& color,
                                         float intensity)
    {
        glm::vec3 dir = glm::normalize(direction);
        s_Data.LightBuffer.DirLight.Direction = { dir.x, dir.y, dir.z, 0.0f };
        s_Data.LightBuffer.DirLight.Color     = { color.x, color.y, color.z, intensity };
        s_Data.LightsDirty = true;
    }

    void Renderer3D::AddPointLight(const glm::vec3& position, const glm::vec3& color,
                                   float intensity, float constant, float linear, float quadratic)
    {
        int idx = s_Data.LightBuffer.ActivePointLights;
        if (idx >= static_cast<int>(k_MaxPointLights))
        {
            HZ_CORE_WARN("Renderer3D: point light count exceeded (max {0})", k_MaxPointLights);
            return;
        }
        auto& pl     = s_Data.LightBuffer.PointLights[idx];
        pl.Position  = { position.x, position.y, position.z, 0.0f };
        pl.Color     = { color.x, color.y, color.z, intensity };
        pl.Constant  = constant;
        pl.Linear    = linear;
        pl.Quadratic = quadratic;
        s_Data.LightBuffer.ActivePointLights++;
        s_Data.LightsDirty = true;
    }

    void Renderer3D::FlushLights()
    {
        s_Data.LightUBO->SetData(&s_Data.LightBuffer, sizeof(LightUBOData));
        s_Data.LightsDirty = false;
    }

    // =========================================================================
    //  IBL
    // =========================================================================

    void Renderer3D::SetIBL(const Ref<IBLResult>& ibl)
    {
        s_Data.CurrentIBL = ibl;

        if (ibl && ibl->IsValid())
        {
            s_Data.IBLBuffer.HasIBL = 1;
        }
        else
        {
            s_Data.IBLBuffer.HasIBL = 0;
        }
        s_Data.IBLUBO->SetData(&s_Data.IBLBuffer, sizeof(IBLDataUBO));
    }

    void Renderer3D::ClearIBL()
    {
        s_Data.CurrentIBL = nullptr;
        s_Data.IBLBuffer.HasIBL = 0;
        s_Data.IBLUBO->SetData(&s_Data.IBLBuffer, sizeof(IBLDataUBO));
    }

    void Renderer3D::SetIBLIntensity(float intensity)
    {
        s_Data.IBLBuffer.IBLIntensity = intensity;
        s_Data.IBLUBO->SetData(&s_Data.IBLBuffer, sizeof(IBLDataUBO));
    }

    // =========================================================================
    //  InitSSAO
    // =========================================================================

    void Renderer3D::InitSSAO(uint32_t width, uint32_t height)
    {
        // ── 生成 64 个半球采样核 ──────────────────────────────────────────────
        std::default_random_engine            engine(42u);
        std::uniform_real_distribution<float> rnd(0.0f, 1.0f);

        for (uint32_t i = 0; i < 64; ++i)
        {
            glm::vec3 sample(
                rnd(engine) * 2.0f - 1.0f,  // x: [-1, 1]
                rnd(engine) * 2.0f - 1.0f,  // y: [-1, 1]
                rnd(engine)                  // z: [ 0, 1] 保证在法线半球内
            );
            sample = glm::normalize(sample) * rnd(engine);

            // 加速插值：让采样点集中在原点附近（近处遮蔽更重要）
            float scale = float(i) / 64.0f;
            scale = glm::mix(0.1f, 1.0f, scale * scale);
            sample *= scale;

            s_Data.SSAO.KernelBuffer.Samples[i] = glm::vec4(sample, 0.0f);
        }

        // ── 生成 4x4 随机旋转噪声纹理 ─────────────────────────────────────────
        // 用 Texture2D::Create(4, 4) + SetData(uint8*, size)
        // 将 [-1,1] 的 float 映射到 [0,255] 的 uint8
        // z 分量固定为 128（对应 float 0，旋转只在 xy 平面）
        {
            uint8_t noiseData[16 * 4]; // 16 像素 × RGBA
            for (int p = 0; p < 16; ++p)
            {
                float nx = rnd(engine) * 2.0f - 1.0f;
                float ny = rnd(engine) * 2.0f - 1.0f;
                noiseData[p * 4 + 0] = static_cast<uint8_t>((nx * 0.5f + 0.5f) * 255.0f);
                noiseData[p * 4 + 1] = static_cast<uint8_t>((ny * 0.5f + 0.5f) * 255.0f);
                noiseData[p * 4 + 2] = 128;  // z = 0.0
                noiseData[p * 4 + 3] = 255;
            }

            s_Data.SSAO.NoiseTex = Texture2D::Create(4, 4);
            s_Data.SSAO.NoiseTex->SetData(noiseData, sizeof(noiseData));

            // 注意：Texture2D::Create(w,h) 默认可能是 CLAMP_TO_EDGE。
            // 噪声纹理需要 REPEAT 平铺才能正确工作。
            // 如果你的 OpenGLTexture2D::Create(w,h) 没有 REPEAT，
            // 可以在 OpenGLTexture2D 里加一个带 WrapMode 参数的重载，
            // 或者临时调大 NoiseScale 的比例来近似平铺效果。
        }

        // ── SSAO 专用 FBO（R8 单通道，不需要深度）────────────────────────────
        FramebufferSpecification ssaoSpec;
        ssaoSpec.Width = width;
        ssaoSpec.Height = height;
        ssaoSpec.Attachments = { FramebufferTextureFormat::R8 };

        s_Data.SSAO.RawFBO = Framebuffer::Create(ssaoSpec);
        s_Data.SSAO.BlurFBO = Framebuffer::Create(ssaoSpec);

        // ── 空 VAO（gl_VertexID 方式，和 EditorGrid 一样）────────────────────
        s_Data.SSAO.QuadVAO = VertexArray::Create();

        // ── UBO ──────────────────────────────────────────────────────────────
        s_Data.SSAO.ParamsUBO = UniformBuffer::Create(sizeof(SSAOParamsUBO), 15);
        s_Data.SSAO.KernelUBO = UniformBuffer::Create(sizeof(SSAOKernelUBO), 16);

        // 采样核上传一次即可，运行中不变
        s_Data.SSAO.KernelUBO->SetData(&s_Data.SSAO.KernelBuffer,
            sizeof(SSAOKernelUBO));

        // ── Shader ───────────────────────────────────────────────────────────
        s_Data.SSAO.SSAOShader = Shader::Create("assets/shaders/SSAO.glsl");
        s_Data.SSAO.BlurShader = Shader::Create("assets/shaders/SSAO_Blur.glsl");

        // 初始化设置
        s_Data.SSAO.Radius = 0.5f;
        s_Data.SSAO.Bias = 0.025f;
        s_Data.SSAO.Power = 1.5f;
        s_Data.SSAO.Enabled = true;
    }

    // =========================================================================
    //  ResizeSSAO
    // =========================================================================

    void Renderer3D::ResizeSSAO(uint32_t width, uint32_t height)
    {
        if (!s_Data.SSAO.RawFBO) return;
        s_Data.SSAO.RawFBO->Resize(width, height);
        s_Data.SSAO.BlurFBO->Resize(width, height);
    }

    // =========================================================================
    //  ComputeSSAO
    // =========================================================================

    void Renderer3D::ComputeSSAO(uint32_t normalTex, uint32_t depthTex,
        const glm::mat4& projection)
    {
        if (!s_Data.SSAO.Enabled || !s_Data.SSAO.RawFBO) return;

        const auto& spec = s_Data.SSAO.RawFBO->GetSpecification();

        // 更新参数 UBO
        s_Data.SSAO.ParamsBuffer.NoiseScale = glm::vec4(
            float(spec.Width) / 4.0f,
            float(spec.Height) / 4.0f,
            0.0f, 0.0f
        );
        s_Data.SSAO.ParamsBuffer.Radius = s_Data.SSAO.Radius;
        s_Data.SSAO.ParamsBuffer.Bias = s_Data.SSAO.Bias;
        s_Data.SSAO.ParamsBuffer.Power = s_Data.SSAO.Power;
        s_Data.SSAO.ParamsUBO->SetData(&s_Data.SSAO.ParamsBuffer,
            sizeof(SSAOParamsUBO));

        // Camera UBO（binding=1）已由 BeginScene 上传，SSAO shader 直接复用

        // ── Pass 1：原始 AO ──────────────────────────────────────────────────
        {
            uint32_t prevFBO = RenderCommand::GetBoundFramebuffer();

            s_Data.SSAO.RawFBO->Bind();
            RenderCommand::SetClearColor({ 1.0f, 1.0f, 1.0f, 1.0f }); // AO 默认 1.0
            RenderCommand::Clear();

            // 绑定输入纹理（平台抽象 API）
            RenderCommand::BindTextureUnit(20, normalTex);
            RenderCommand::BindTextureUnit(21, depthTex);
            RenderCommand::BindTextureUnit(22, s_Data.SSAO.NoiseTex->GetRendererID());

            s_Data.SSAO.SSAOShader->Bind();
            // SSAO.glsl 用 layout(binding=N) 声明纹理，SPIR-V 路径下
            // binding 由 shader 自身指定，不需要 SetInt 传 slot
            // 如果你的 shader 编译后 binding 失效，再加：
            // s_Data.SSAO.SSAOShader->SetInt("u_NormalTex", 20);
            // s_Data.SSAO.SSAOShader->SetInt("u_DepthTex",  21);
            // s_Data.SSAO.SSAOShader->SetInt("u_NoiseTex",  22);

            RenderCommand::DrawArrays(s_Data.SSAO.QuadVAO, 6);

            s_Data.SSAO.RawFBO->Unbind();
            RenderCommand::BindFramebuffer(prevFBO);
        }

        // ── Pass 2：Blur ─────────────────────────────────────────────────────
        {
            uint32_t prevFBO = RenderCommand::GetBoundFramebuffer();

            s_Data.SSAO.BlurFBO->Bind();
            RenderCommand::SetClearColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            RenderCommand::Clear();

            RenderCommand::BindTextureUnit(20,
                s_Data.SSAO.RawFBO->GetColorAttachmentRendererID(0));

            s_Data.SSAO.BlurShader->Bind();

            RenderCommand::DrawArrays(s_Data.SSAO.QuadVAO, 6);

            s_Data.SSAO.BlurFBO->Unbind();
            RenderCommand::BindFramebuffer(prevFBO);
        }

        // 更新 IBL UBO 的 HasSSAO 标志，让 PBR shader 知道要采样
        s_Data.IBLBuffer.HasSSAO = 1;
        s_Data.IBLUBO->SetData(&s_Data.IBLBuffer, sizeof(IBLDataUBO));
    }

    // =========================================================================
    //  BindSSAOResult（在 DrawMesh 内、Shader::Bind() 之前调用）
    // =========================================================================

    void Renderer3D::BindSSAOResult()
    {
        if (s_Data.SSAO.Enabled && s_Data.SSAO.BlurFBO)
        {
            RenderCommand::BindTextureUnit(
                14, s_Data.SSAO.BlurFBO->GetColorAttachmentRendererID(0));
        }
        else
        {
            // 禁用时绑定白色纹理（AO=1，无遮蔽）
            s_Data.WhiteTexture->Bind(14);
        }
    }

    // =========================================================================
    //  设置接口（EditorLayer3D::OnImGuiExtra 直接引用）
    // =========================================================================

    bool& Renderer3D::SSAOEnabled() { return s_Data.SSAO.Enabled; }
    float& Renderer3D::SSAORadius() { return s_Data.SSAO.Radius; }
    float& Renderer3D::SSAOBias() { return s_Data.SSAO.Bias; }
    float& Renderer3D::SSAOPower() { return s_Data.SSAO.Power; }

    // =========================================================================
    //  Shadow pass
    // =========================================================================

    void Renderer3D::BeginShadowPass(const glm::mat4& lightSpaceMatrix)
    {
        s_Data.LightSpaceMatrix = lightSpaceMatrix;
        s_Data.ShadowShader->Bind();
    }

    void Renderer3D::EndShadowPass() {}

    void Renderer3D::DrawMeshShadow(const glm::mat4& transform, const Ref<Mesh>& mesh)
    {
        if (!mesh) return;
        s_Data.TransformBuffer.Model            = transform;
        s_Data.TransformBuffer.LightSpaceMatrix = s_Data.LightSpaceMatrix;
        s_Data.TransformUBO->SetData(&s_Data.TransformBuffer, sizeof(TransformUBOData));
        RenderCommand::DrawIndexed(mesh->GetVertexArray(), mesh->GetIndexCount());
    }

    uint32_t          Renderer3D::GetShadowMapID()      { return s_Data.ShadowMapFB->GetDepthAttachmentRendererID(); }
    const glm::mat4&  Renderer3D::GetLightSpaceMatrix() { return s_Data.LightSpaceMatrix; }
    Ref<Framebuffer>& Renderer3D::GetShadowMapFB()      { return s_Data.ShadowMapFB; }

    // =========================================================================
    //  Mesh rendering
    // =========================================================================

    void Renderer3D::DrawMesh(const glm::mat4& transform,
                              const Ref<Mesh>& mesh,
                              const Ref<Material>& material,
                              int entityID)
    {
        if (!mesh) return;
        if (s_Data.LightsDirty) FlushLights();

        // Transform UBO
        s_Data.TransformBuffer.Model           = transform;
        s_Data.TransformBuffer.NormalMatrix    = glm::transpose(glm::inverse(transform));
        s_Data.TransformBuffer.LightSpaceMatrix= s_Data.LightSpaceMatrix;
        s_Data.TransformUBO->SetData(&s_Data.TransformBuffer, sizeof(TransformUBOData));

        // Material UBO
        static Ref<Material> s_DefaultMaterial = Material::Create();
        const Material& mat = material ? *material : *s_DefaultMaterial;

        s_Data.MaterialBuffer.Albedo          = mat.Albedo;
        s_Data.MaterialBuffer.Metallic        = mat.Metallic;
        s_Data.MaterialBuffer.Roughness       = mat.Roughness;
        s_Data.MaterialBuffer.EntityID        = entityID;
        s_Data.MaterialBuffer.FlipNormals     = mat.FlipNormals ? 1 : 0;
        s_Data.MaterialBuffer.HasNormalMap    = mat.NormalMap    ? 1 : 0;
        s_Data.MaterialBuffer.HasRoughnessMap = mat.RoughnessMap ? 1 : 0;
        s_Data.MaterialBuffer.HasMetallicMap  = mat.MetallicMap  ? 1 : 0;
        s_Data.MaterialBuffer.HasShadowMap    = 1;
        s_Data.MaterialUBO->SetData(&s_Data.MaterialBuffer, sizeof(MaterialUBOData));

        // PBR textures (slots 4, 6-9)
        (mat.AlbedoMap    ? mat.AlbedoMap    : s_Data.WhiteTexture)->Bind(4);
        (mat.NormalMap    ? mat.NormalMap    : s_Data.WhiteTexture)->Bind(6);
        (mat.RoughnessMap ? mat.RoughnessMap : s_Data.WhiteTexture)->Bind(7);
        (mat.MetallicMap  ? mat.MetallicMap  : s_Data.WhiteTexture)->Bind(8);
        s_Data.ShadowMapFB->BindDepthAttachment(9);

        // IBL textures (slots 11, 12, 13) — bind fallbacks when no IBL
        if (s_Data.CurrentIBL && s_Data.CurrentIBL->IsValid())
        {
            RenderCommand::BindTextureUnit(11, s_Data.CurrentIBL->IrradianceMap);
            RenderCommand::BindTextureUnit(12, s_Data.CurrentIBL->PrefilterMap);
            RenderCommand::BindTextureUnit(13, s_Data.CurrentIBL->BrdfLUT);
        }
        // When HasIBL == 0 the shader uses the hemisphere fallback,
        // so we don't need to bind real textures — they'll just be ignored.

        BindSSAOResult();

        s_Data.MeshShader->Bind();
        RenderCommand::DrawIndexed(mesh->GetVertexArray(), mesh->GetIndexCount());
        s_Data.Stats.DrawCalls++;
        s_Data.Stats.MeshCount++;
    }

    // =========================================================================
    //  Editor visuals
    // =========================================================================

    void Renderer3D::SetSkyboxColors(const glm::vec3& top,
                                     const glm::vec3& horizon,
                                     const glm::vec3& bottom)
    {
        s_Data.SkyTopColor     = top;
        s_Data.SkyHorizonColor = horizon;
        s_Data.SkyBottomColor  = bottom;

        s_Data.SkyboxGradientBuffer.TopColor     = glm::vec4(top,     1.0f);
        s_Data.SkyboxGradientBuffer.HorizonColor = glm::vec4(horizon, 1.0f);
        s_Data.SkyboxGradientBuffer.BottomColor  = glm::vec4(bottom,  1.0f);
        s_Data.SkyboxGradientUBO->SetData(&s_Data.SkyboxGradientBuffer, sizeof(SkyboxGradientUBO));
    }

    void Renderer3D::DrawEditorSkybox()
    {
        // Bind IBL prefilter for the HDR skybox path (slot 12)
        if (s_Data.CurrentIBL && s_Data.CurrentIBL->IsValid())
            RenderCommand::BindTextureUnit(12, s_Data.CurrentIBL->PrefilterMap);

        RenderCommand::SetDepthWrite(false);
        RenderCommand::SetDepthFunc(DepthFunc::LessOrEqual);

        s_Data.EditorSkyboxShader->Bind();
        RenderCommand::DrawArrays(s_Data.SkyboxVAO, 36);

        RenderCommand::SetDepthWrite(true);
        RenderCommand::SetDepthFunc(DepthFunc::Less);
    }

    void Renderer3D::DrawEditorGrid()
    {
        RenderCommand::SetBlend(true);
        s_Data.EditorGridShader->Bind();
        RenderCommand::DrawArrays(s_Data.GridVAO, 6);
        RenderCommand::SetBlend(false);
    }

    // =========================================================================
    //  Stats
    // =========================================================================

    Renderer3D::Statistics Renderer3D::GetStats()  { return s_Data.Stats; }
    void                   Renderer3D::ResetStats() { s_Data.Stats = {}; }

} // namespace Hazel
