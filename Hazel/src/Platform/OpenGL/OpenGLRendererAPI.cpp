#include "hzpch.h"
#include "OpenGLRendererAPI.h"
#include <glad/glad.h>

namespace Hazel {

    void OpenGLRendererAPI::Init()
    {
        HZ_PROFILE_FUNCTION();

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_LINE_SMOOTH);
    }

    void OpenGLRendererAPI::SetClearColor(const glm::vec4& color)
    {
        glClearColor(color.r, color.g, color.b, color.a);
    }

    void OpenGLRendererAPI::Clear()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLRendererAPI::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
    {
        glViewport(x, y, width, height);
    }

    void OpenGLRendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount)
    {
        vertexArray->Bind();
        uint32_t count = indexCount ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
    }

    void OpenGLRendererAPI::DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount)
    {
        vertexArray->Bind();
        glDrawArrays(GL_LINES, 0, vertexCount);
    }

    void OpenGLRendererAPI::DrawArrays(const Ref<VertexArray>& vertexArray, uint32_t vertexCount)
    {
        vertexArray->Bind();
        glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    }

    void OpenGLRendererAPI::SetLineWidth(float width)
    {
        glLineWidth(width);
    }

    void OpenGLRendererAPI::EnableDepthTest()
    {
        glEnable(GL_DEPTH_TEST);
    }

    void OpenGLRendererAPI::DisableDepthTest()
    {
        glDisable(GL_DEPTH_TEST);
    }

    void OpenGLRendererAPI::SetDepthWrite(bool enabled)
    {
        glDepthMask(enabled ? GL_TRUE : GL_FALSE);
    }

    void OpenGLRendererAPI::SetDepthFunc(DepthFunc func)
    {
        switch (func)
        {
        case DepthFunc::Less:        glDepthFunc(GL_LESS);   break;
        case DepthFunc::LessOrEqual: glDepthFunc(GL_LEQUAL); break;
        }
    }

    void OpenGLRendererAPI::SetBlend(bool enabled)
    {
        if (enabled)
        {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        else
        {
            glDisable(GL_BLEND);
        }
    }

    uint32_t OpenGLRendererAPI::GetBoundFramebuffer()
    {
        GLint id;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &id);
        return static_cast<uint32_t>(id);
    }

    void OpenGLRendererAPI::BindFramebuffer(uint32_t id)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, id);
    }

    void OpenGLRendererAPI::BindTextureUnit(uint32_t slot, uint32_t textureID)
    {
        glBindTextureUnit(slot, static_cast<GLuint>(textureID));
    }

} // namespace Hazel