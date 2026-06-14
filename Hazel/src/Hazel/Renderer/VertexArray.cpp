#include "hzpch.h"
#include "VertexArray.h"

#include "Renderer.h"
#include "Platform/OpenGL/OpenGLVertexArray.h"

namespace Hazel {

    Ref<VertexArray> VertexArray::Create()
    {
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::OpenGL:
            return CreateRef<OpenGLVertexArray>();
        case RendererAPI::API::None:
            HZ_CORE_ASSERT(false, "RendererAPI::None is not supported");
            return nullptr;
        }
        HZ_CORE_ASSERT(false, "Unknown RendererAPI");
        return nullptr;
    }

} // namespace Hazel