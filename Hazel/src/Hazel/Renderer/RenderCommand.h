#pragma once
#include "RendererAPI.h"

namespace Hazel {

    class RenderCommand
    {
    public:
        static void Init()
        {
            s_RendererAPI->Init();
        }

        static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
        {
            s_RendererAPI->SetViewport(x, y, width, height);
        }

        static void SetClearColor(const glm::vec4& color)
        {
            s_RendererAPI->SetClearColor(color);
        }

        static void Clear()
        {
            s_RendererAPI->Clear();
        }

        static void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0)
        {
            s_RendererAPI->DrawIndexed(vertexArray, indexCount);
        }

        static void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount = 0)
        {
            s_RendererAPI->DrawLines(vertexArray, vertexCount);
        }

        // Draw non-indexed geometry (e.g. skybox cube, grid quads).
        static void DrawArrays(const Ref<VertexArray>& vertexArray, uint32_t vertexCount)
        {
            s_RendererAPI->DrawArrays(vertexArray, vertexCount);
        }

        static void SetLineWidth(float width)
        {
            s_RendererAPI->SetLineWidth(width);
        }

        static void EnableDepthTest()
        {
            s_RendererAPI->EnableDepthTest();
        }

        static void DisableDepthTest()
        {
            s_RendererAPI->DisableDepthTest();
        }

        static void SetDepthWrite(bool enabled)
        {
            s_RendererAPI->SetDepthWrite(enabled);
        }

        static void SetDepthFunc(DepthFunc func)
        {
            s_RendererAPI->SetDepthFunc(func);
        }

        // Enable or disable alpha blending.
        static void SetBlend(bool enabled)
        {
            s_RendererAPI->SetBlend(enabled);
        }

        // Save/restore the active framebuffer around passes that bind their own FBO.
        static uint32_t GetBoundFramebuffer()
        {
            return s_RendererAPI->GetBoundFramebuffer();
        }

        static void BindFramebuffer(uint32_t id)
        {
            s_RendererAPI->BindFramebuffer(id);
        }

        static void BindTextureUnit(uint32_t slot, uint32_t textureID)
        {
            s_RendererAPI->BindTextureUnit(slot, textureID);
        }

    private:
        static Scope<RendererAPI> s_RendererAPI;
    };

} // namespace Hazel