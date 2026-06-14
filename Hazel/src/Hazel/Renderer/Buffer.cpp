#include "hzpch.h"
#include "Buffer.h"

#include "Renderer.h"
#include "Platform/OpenGL/OpenGLBuffer.h"

namespace Hazel {

    // ©¤©¤ VertexBuffer ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤

    Ref<VertexBuffer> VertexBuffer::Create(uint32_t size)
    {
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::OpenGL:
            return CreateRef<OpenGLVertexBuffer>(size);
        case RendererAPI::API::None:
            HZ_CORE_ASSERT(false, "RendererAPI::None is not supported");
            return nullptr;
        }
        HZ_CORE_ASSERT(false, "Unknown RendererAPI");
        return nullptr;
    }

    Ref<VertexBuffer> VertexBuffer::Create(float* vertices, uint32_t size)
    {
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::OpenGL:
            return CreateRef<OpenGLVertexBuffer>(vertices, size);
        case RendererAPI::API::None:
            HZ_CORE_ASSERT(false, "RendererAPI::None is not supported");
            return nullptr;
        }
        HZ_CORE_ASSERT(false, "Unknown RendererAPI");
        return nullptr;
    }

    // ©¤©¤ IndexBuffer ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤

    Ref<IndexBuffer> IndexBuffer::Create(uint32_t* indices, uint32_t count)
    {
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::OpenGL:
            return CreateRef<OpenGLIndexBuffer>(indices, count);
        case RendererAPI::API::None:
            HZ_CORE_ASSERT(false, "RendererAPI::None is not supported");
            return nullptr;
        }
        HZ_CORE_ASSERT(false, "Unknown RendererAPI");
        return nullptr;
    }

} // namespace Hazel