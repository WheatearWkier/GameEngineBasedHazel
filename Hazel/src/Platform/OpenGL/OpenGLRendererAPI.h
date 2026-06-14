#pragma once
#include "Hazel/Renderer/RendererAPI.h"

namespace Hazel {

    class OpenGLRendererAPI : public RendererAPI
    {
    public:
        virtual void Init() override;

        virtual void SetClearColor(const glm::vec4& color) override;
        virtual void Clear() override;

        virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

        virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0) override;
        virtual void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount = 0) override;
        virtual void DrawArrays(const Ref<VertexArray>& vertexArray, uint32_t vertexCount) override;

        virtual void SetLineWidth(float width) override;

        virtual void EnableDepthTest()  override;
        virtual void DisableDepthTest() override;
        virtual void SetDepthWrite(bool enabled) override;
        virtual void SetDepthFunc(DepthFunc func) override;

        virtual void SetBlend(bool enabled) override;

        virtual uint32_t GetBoundFramebuffer() override;
        virtual void BindFramebuffer(uint32_t id) override;
        virtual void BindTextureUnit(uint32_t slot, uint32_t textureID) override;
    };

} // namespace Hazel