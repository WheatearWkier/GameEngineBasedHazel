#pragma once

#include "Camera.h"
#include "EditorCamera.h"
#include "Texture.h"
#include "SubTexture2D.h"
#include "Hazel/Scene/Components.h"

#include <glm/glm.hpp>

namespace Hazel {

    /// 2D 批处理渲染器
    /// 
    /// 对外接口与原版完全一致，内部改为基于 RenderBatch<T> 的可扩展架构。
    /// 
    /// 新增图元步骤：
    ///   1. 在 Renderer2DVertices.h 里加顶点结构体
    ///   2. 在 Renderer2DData（内部）里加 RenderBatch<新Vertex> 实例
    ///   3. 在 Init() 里初始化 batch
    ///   4. 在 Flush() 里 batch.Flush()（已是循环，自动处理）
    ///   5. 写 DrawXxx 公共函数
    ///   以上改动全部局限在 Renderer2D.cpp，.h 只需加 DrawXxx 声明
    class Renderer2D
    {
    public:
        static void Init();
        static void Shutdown();

        // ── 场景开始/结束 ──────────────────────────────────────────────────
        static void BeginScene(const Camera& camera, const glm::mat4& transform);
        static void BeginScene(const EditorCamera& camera);
        static void EndScene();
        static void Flush();

        static void SetViewProjection(const glm::mat4& viewProjection);

        // ── Quad ───────────────────────────────────────────────────────────
        static void DrawQuad(const glm::mat4& transform,
            const glm::vec4& color, int entityID = -1);
        static void DrawQuad(const glm::mat4& transform,
            const Ref<Texture2D>& texture,
            float tilingFactor = 1.0f,
            const glm::vec4& tintColor = glm::vec4(1.0f),
            int entityID = -1);

        // 便捷重载（内部转为 mat4）
        static void DrawQuad(const glm::vec2& position, const glm::vec2& size,
            const glm::vec4& color);
        static void DrawQuad(const glm::vec3& position, const glm::vec2& size,
            const glm::vec4& color);
        static void DrawQuad(const glm::vec3& position, const glm::vec2& size,
            const Ref<Texture2D>& texture,
            float tilingFactor = 1.0f,
            const glm::vec4& tintColor = glm::vec4(1.0f));
        static void DrawQuad(const glm::vec3& position, const glm::vec2& size,
            const Ref<SubTexture2D>& subTexture,
            float tilingFactor = 1.0f,
            const glm::vec4& tintColor = glm::vec4(1.0f));

        // ── Circle ─────────────────────────────────────────────────────────
        static void DrawCircle(const glm::mat4& transform,
            const glm::vec4& color,
            float thickness = 1.0f,
            float fade = 0.005f,
            int entityID = -1);

        // ── Sprite（由场景系统调用）────────────────────────────────────────
        static void DrawSprite(const glm::mat4& transform,
            SpriteRendererComponent& src,
            int entityID);

        // ── Animation Frame（自定义 UV）────────────────────────────────────
        static void DrawAnimationFrame(const glm::mat4& transform,
            const Ref<Texture2D>& texture,
            const glm::vec2& uvMin,
            const glm::vec2& uvMax,
            bool flipX,
            const glm::vec4& tintColor = glm::vec4(1.0f),
            int entityID = -1);

        // ── Line / Rect ────────────────────────────────────────────────────
        static void DrawLine(const glm::vec3& p0, const glm::vec3& p1,
            const glm::vec4& color, int entityID = -1);
        static void DrawRect(const glm::vec3& position, const glm::vec2& size,
            const glm::vec4& color, int entityID = -1);
        static void DrawRect(const glm::mat4& transform,
            const glm::vec4& color, int entityID = -1);

        static float GetLineWidth();
        static void  SetLineWidth(float width);

        // ── 统计 ───────────────────────────────────────────────────────────
        struct Statistics
        {
            uint32_t DrawCalls = 0;
            uint32_t QuadCount = 0;

            uint32_t GetTotalVertexCount() const { return QuadCount * 4; }
            uint32_t GetTotalIndexCount()  const { return QuadCount * 6; }
        };

        static void       ResetStats();
        static Statistics GetStats();

    private:
        
    };

} // namespace Hazel
