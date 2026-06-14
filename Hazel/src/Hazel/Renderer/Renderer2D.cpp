#include "hzpch.h"
#include "Renderer2D.h"
#include "RenderBatch.h"
#include "Renderer2DVertices.h"
#include "UniformBuffer.h"
#include "RenderCommand.h"
#include "Shader.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Hazel {

    // ═══════════════════════════════════════════════════════════════════════════
    //  常量 & 顶点模板
    // ═══════════════════════════════════════════════════════════════════════════

    static constexpr uint32_t k_MaxQuads = 10000;
    static constexpr uint32_t k_MaxVertices = k_MaxQuads * 4;
    static constexpr uint32_t k_MaxIndices = k_MaxQuads * 6;
    static constexpr uint32_t k_MaxTexSlots = 32;

    static constexpr glm::vec4 k_QuadPositions[4] = {
        { -0.5f, -0.5f, 0.0f, 1.0f },
        {  0.5f, -0.5f, 0.0f, 1.0f },
        {  0.5f,  0.5f, 0.0f, 1.0f },
        { -0.5f,  0.5f, 0.0f, 1.0f },
    };

    static constexpr glm::vec2 k_DefaultUV[4] = {
        { 0.0f, 0.0f }, { 1.0f, 0.0f },
        { 1.0f, 1.0f }, { 0.0f, 1.0f },
    };

    // ═══════════════════════════════════════════════════════════════════════════
    //  Renderer2DData
    //  新增图元：在这里加一个 RenderBatch<新Vertex> 字段，其余代码自动适配
    // ═══════════════════════════════════════════════════════════════════════════

    struct Renderer2DData
    {
        // ── 纹理槽（Quad / AnimFrame 共用）───────────────────────────────────
        std::array<Ref<Texture2D>, k_MaxTexSlots> TextureSlots;
        uint32_t TextureSlotIndex = 1;          // slot 0 = 白色纹理
        Ref<Texture2D> WhiteTexture;

        // ── 批处理实例 ────────────────────────────────────────────────────────
        Scope<RenderBatch<QuadVertex>>   QuadBatch;
        Scope<RenderBatch<CircleVertex>> CircleBatch;
        Scope<RenderBatch<LineVertex>>   LineBatch;
        // 未来只需在这里加新的 Scope<RenderBatch<XxxVertex>> XxxBatch;

        // ── 相机 UBO ──────────────────────────────────────────────────────────
        struct CameraData { glm::mat4 ViewProjection; };
        CameraData         CameraBuffer;
        Ref<UniformBuffer> CameraUniformBuffer;

        // ── 统计 ──────────────────────────────────────────────────────────────
        Renderer2D::Statistics Stats;
    };

    static Renderer2DData s_Data;

    // ═══════════════════════════════════════════════════════════════════════════
    //  初始化 / 关闭
    // ═══════════════════════════════════════════════════════════════════════════

    void Renderer2D::Init()
    {
        HZ_PROFILE_FUNCTION();

        // ── 共用索引缓冲（Quad + Circle 用）──────────────────────────────────
        auto* indices = new uint32_t[k_MaxIndices];
        for (uint32_t i = 0, offset = 0; i < k_MaxIndices; i += 6, offset += 4)
        {
            indices[i + 0] = offset + 0; indices[i + 1] = offset + 1; indices[i + 2] = offset + 2;
            indices[i + 3] = offset + 2; indices[i + 4] = offset + 3; indices[i + 5] = offset + 0;
        }
        Ref<IndexBuffer> sharedIBO = IndexBuffer::Create(indices, k_MaxIndices);
        delete[] indices;

        // ── 白色纹理 ──────────────────────────────────────────────────────────
        s_Data.WhiteTexture = Texture2D::Create(1, 1);
        constexpr uint32_t white = 0xffffffff;
        s_Data.WhiteTexture->SetData(const_cast<uint32_t*>(&white), sizeof(uint32_t));
        s_Data.TextureSlots[0] = s_Data.WhiteTexture;

        // ── QuadBatch ─────────────────────────────────────────────────────────
        s_Data.QuadBatch = CreateScope<RenderBatch<QuadVertex>>(
            k_MaxVertices,
            Shader::Create("assets/shaders/Renderer2D_Quad.glsl"),
            BufferLayout{
                { "a_Position",     ShaderDataType::Float3 },
                { "a_Color",        ShaderDataType::Float4 },
                { "a_TexCoord",     ShaderDataType::Float2 },
                { "a_TexIndex",     ShaderDataType::Float  },
                { "a_TilingFactor", ShaderDataType::Float  },
                { "a_EntityID",     ShaderDataType::Int    }
            },
            BatchDrawMode::Triangles,
            sharedIBO
        );
        // Quad Flush 前绑定所有纹理槽
        s_Data.QuadBatch->SetPreDrawCallback([]()
            {
                for (uint32_t i = 0; i < s_Data.TextureSlotIndex; i++)
                    s_Data.TextureSlots[i]->Bind(i);
            });

        // ── CircleBatch ───────────────────────────────────────────────────────
        s_Data.CircleBatch = CreateScope<RenderBatch<CircleVertex>>(
            k_MaxVertices,
            Shader::Create("assets/shaders/Renderer2D_Circle.glsl"),
            BufferLayout{
                { "a_WorldPosition", ShaderDataType::Float3 },
                { "a_LocalPosition", ShaderDataType::Float3 },
                { "a_Color",         ShaderDataType::Float4 },
                { "a_Thickness",     ShaderDataType::Float  },
                { "a_Fade",          ShaderDataType::Float  },
                { "a_EntityID",      ShaderDataType::Int    }
            },
            BatchDrawMode::Triangles,
            sharedIBO
        );

        // ── LineBatch ─────────────────────────────────────────────────────────
        s_Data.LineBatch = CreateScope<RenderBatch<LineVertex>>(
            k_MaxVertices,
            Shader::Create("assets/shaders/Renderer2D_Line.glsl"),
            BufferLayout{
                { "a_Position", ShaderDataType::Float3 },
                { "a_Color",    ShaderDataType::Float4 },
                { "a_EntityID", ShaderDataType::Int    }
            },
            BatchDrawMode::Lines
        );

        // ── Camera UBO ────────────────────────────────────────────────────────
        s_Data.CameraUniformBuffer = UniformBuffer::Create(
            sizeof(Renderer2DData::CameraData), 0);
    }

    void Renderer2D::Shutdown()
    {
        HZ_PROFILE_FUNCTION();
        // RenderBatch 析构时自动 delete[] CPU 缓冲，Scope 析构时自动调用
        s_Data.QuadBatch.reset();
        s_Data.CircleBatch.reset();
        s_Data.LineBatch.reset();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  场景开始 / 结束
    // ═══════════════════════════════════════════════════════════════════════════

    static void UploadCameraAndReset(const glm::mat4& vp)
    {
        s_Data.CameraBuffer.ViewProjection = vp;
        s_Data.CameraUniformBuffer->SetData(
            &s_Data.CameraBuffer, sizeof(Renderer2DData::CameraData));

        s_Data.QuadBatch->Reset();
        s_Data.CircleBatch->Reset();
        s_Data.LineBatch->Reset();
        s_Data.TextureSlotIndex = 1;
    }

    void Renderer2D::BeginScene(const Camera& camera, const glm::mat4& transform)
    {
        HZ_PROFILE_FUNCTION();
        UploadCameraAndReset(camera.GetProjection() * glm::inverse(transform));
    }

    void Renderer2D::BeginScene(const EditorCamera& camera)
    {
        HZ_PROFILE_FUNCTION();
        UploadCameraAndReset(camera.GetViewProjection());
    }

    void Renderer2D::EndScene()
    {
        HZ_PROFILE_FUNCTION();
        Flush();
    }

    void Renderer2D::SetViewProjection(const glm::mat4& vp)
    {
        s_Data.CameraBuffer.ViewProjection = vp;
        s_Data.CameraUniformBuffer->SetData(
            &s_Data.CameraBuffer, sizeof(Renderer2DData::CameraData));
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  Flush
    //  新增图元：在这里加一行 s_Data.XxxBatch->Flush(s_Data.Stats);
    // ═══════════════════════════════════════════════════════════════════════════

    void Renderer2D::Flush()
    {
        HZ_PROFILE_FUNCTION();
        s_Data.QuadBatch->Flush(s_Data.Stats);
        s_Data.CircleBatch->Flush(s_Data.Stats);
        s_Data.LineBatch->Flush(s_Data.Stats);
    }

    static void FlushAndReset()
    {
        Renderer2D::Flush();
        s_Data.QuadBatch->Reset();
        s_Data.CircleBatch->Reset();
        s_Data.LineBatch->Reset();
        s_Data.TextureSlotIndex = 1;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  内部辅助
    // ═══════════════════════════════════════════════════════════════════════════

    static float GetOrAllocTextureSlot(const Ref<Texture2D>& texture)
    {
        for (uint32_t i = 1; i < s_Data.TextureSlotIndex; i++)
            if (*s_Data.TextureSlots[i] == *texture)
                return static_cast<float>(i);

        if (s_Data.TextureSlotIndex >= k_MaxTexSlots)
            FlushAndReset();

        const float index = static_cast<float>(s_Data.TextureSlotIndex);
        HZ_CORE_ASSERT(s_Data.TextureSlotIndex < k_MaxTexSlots, "Texture slot overflow");
        s_Data.TextureSlots[s_Data.TextureSlotIndex++] = texture;
        return index;
    }

    static void SubmitQuadVertices(
        const glm::mat4& transform,
        const glm::vec4& color,
        const glm::vec2  uvs[4],
        float texIndex,
        float tilingFactor,
        int   entityID)
    {
        if (s_Data.QuadBatch->IsFull(4))
            FlushAndReset();

        for (int i = 0; i < 4; i++)
        {
            auto* v = s_Data.QuadBatch->AllocVertex();
            v->Position = transform * k_QuadPositions[i];
            v->Color = color;
            v->TexCoord = uvs[i];
            v->TexIndex = texIndex;
            v->TilingFactor = tilingFactor;
            v->EntityID = entityID;
        }
        s_Data.QuadBatch->AddIndexCount(6);
        s_Data.Stats.QuadCount++;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  DrawQuad
    // ═══════════════════════════════════════════════════════════════════════════

    void Renderer2D::DrawQuad(const glm::mat4& transform,
        const glm::vec4& color, int entityID)
    {
        HZ_PROFILE_FUNCTION();
        SubmitQuadVertices(transform, color, k_DefaultUV, 0.0f, 1.0f, entityID);
    }

    void Renderer2D::DrawQuad(const glm::mat4& transform,
        const Ref<Texture2D>& texture,
        float tilingFactor, const glm::vec4& tintColor,
        int entityID)
    {
        HZ_PROFILE_FUNCTION();
        const float texIndex = GetOrAllocTextureSlot(texture);
        SubmitQuadVertices(transform, tintColor, k_DefaultUV,
            texIndex, tilingFactor, entityID);
    }

    void Renderer2D::DrawQuad(const glm::vec2& pos, const glm::vec2& size,
        const glm::vec4& color)
    {
        DrawQuad(glm::vec3(pos, 0.0f), size, color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& pos, const glm::vec2& size,
        const glm::vec4& color)
    {
        DrawQuad(
            glm::scale(glm::translate(glm::mat4(1.0f), pos),
                { size.x, size.y, 1.0f }),
            color);
    }

    void Renderer2D::DrawQuad(const glm::vec3& pos, const glm::vec2& size,
        const Ref<Texture2D>& texture,
        float tilingFactor, const glm::vec4& tintColor)
    {
        DrawQuad(
            glm::scale(glm::translate(glm::mat4(1.0f), pos),
                { size.x, size.y, 1.0f }),
            texture, tilingFactor, tintColor);
    }

    void Renderer2D::DrawQuad(const glm::vec3& pos, const glm::vec2& size,
        const Ref<SubTexture2D>& subTexture,
        float tilingFactor, const glm::vec4& tintColor)
    {
        const glm::mat4 transform =
            glm::scale(glm::translate(glm::mat4(1.0f), pos),
                { size.x, size.y, 1.0f });
        const float texIndex = GetOrAllocTextureSlot(subTexture->GetTexture());
        SubmitQuadVertices(transform, tintColor,
            subTexture->GetTexCoords(), texIndex, tilingFactor, -1);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  DrawCircle
    // ═══════════════════════════════════════════════════════════════════════════

    void Renderer2D::DrawCircle(const glm::mat4& transform,
        const glm::vec4& color,
        float thickness, float fade, int entityID)
    {
        HZ_PROFILE_FUNCTION();

        if (s_Data.CircleBatch->IsFull(4))
            FlushAndReset();

        for (int i = 0; i < 4; i++)
        {
            auto* v = s_Data.CircleBatch->AllocVertex();
            v->WorldPosition = transform * k_QuadPositions[i];
            v->LocalPosition = k_QuadPositions[i] * 2.0f;
            v->Color = color;
            v->Thickness = thickness;
            v->Fade = fade;
            v->EntityID = entityID;
        }
        s_Data.CircleBatch->AddIndexCount(6);
        s_Data.Stats.QuadCount++;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  DrawSprite / DrawAnimationFrame
    // ═══════════════════════════════════════════════════════════════════════════

    void Renderer2D::DrawSprite(const glm::mat4& transform,
        SpriteRendererComponent& src, int entityID)
    {
        if (src.Texture)
            DrawAnimationFrame(transform, src.Texture,
                src.UVMin, src.UVMax, src.FlipX,
                src.Color, entityID);
        else
            DrawQuad(transform, src.Color, entityID);
    }

    void Renderer2D::DrawAnimationFrame(const glm::mat4& transform,
        const Ref<Texture2D>& texture,
        const glm::vec2& uvMin,
        const glm::vec2& uvMax,
        bool flipX,
        const glm::vec4& tintColor,
        int entityID)
    {
        HZ_PROFILE_FUNCTION();

        float uL = flipX ? uvMax.x : uvMin.x;
        float uR = flipX ? uvMin.x : uvMax.x;

        const glm::vec2 uvs[4] = {
            { uL, uvMin.y }, { uR, uvMin.y },
            { uR, uvMax.y }, { uL, uvMax.y },
        };

        const float texIndex = GetOrAllocTextureSlot(texture);
        SubmitQuadVertices(transform, tintColor, uvs, texIndex, 1.0f, entityID);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  DrawLine / DrawRect
    // ═══════════════════════════════════════════════════════════════════════════

    void Renderer2D::DrawLine(const glm::vec3& p0, const glm::vec3& p1,
        const glm::vec4& color, int entityID)
    {
        if (s_Data.LineBatch->IsFull(2))
            FlushAndReset();

        auto* v0 = s_Data.LineBatch->AllocVertex();
        v0->Position = p0; v0->Color = color; v0->EntityID = entityID;

        auto* v1 = s_Data.LineBatch->AllocVertex();
        v1->Position = p1; v1->Color = color; v1->EntityID = entityID;

        s_Data.LineBatch->AddVertexCount(2);
    }

    void Renderer2D::DrawRect(const glm::vec3& pos, const glm::vec2& size,
        const glm::vec4& color, int entityID)
    {
        const float hx = size.x * 0.5f, hy = size.y * 0.5f, z = pos.z;
        DrawLine({ pos.x - hx, pos.y - hy, z }, { pos.x + hx, pos.y - hy, z }, color, entityID);
        DrawLine({ pos.x + hx, pos.y - hy, z }, { pos.x + hx, pos.y + hy, z }, color, entityID);
        DrawLine({ pos.x + hx, pos.y + hy, z }, { pos.x - hx, pos.y + hy, z }, color, entityID);
        DrawLine({ pos.x - hx, pos.y + hy, z }, { pos.x - hx, pos.y - hy, z }, color, entityID);
    }

    void Renderer2D::DrawRect(const glm::mat4& transform,
        const glm::vec4& color, int entityID)
    {
        glm::vec3 corners[4];
        for (int i = 0; i < 4; i++)
            corners[i] = transform * k_QuadPositions[i];

        DrawLine(corners[0], corners[1], color, entityID);
        DrawLine(corners[1], corners[2], color, entityID);
        DrawLine(corners[2], corners[3], color, entityID);
        DrawLine(corners[3], corners[0], color, entityID);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  线宽 / 统计
    // ═══════════════════════════════════════════════════════════════════════════

    float Renderer2D::GetLineWidth() { return s_Data.LineBatch->GetLineWidth(); }
    void  Renderer2D::SetLineWidth(float w) { s_Data.LineBatch->SetLineWidth(w); }

    void Renderer2D::ResetStats()
    {
        std::memset(&s_Data.Stats, 0, sizeof(Statistics));
    }

    Renderer2D::Statistics Renderer2D::GetStats()
    {
        return s_Data.Stats;
    }

} // namespace Hazel
