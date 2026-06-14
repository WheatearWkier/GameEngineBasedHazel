#include "hzpch.h"
#include "RendererAPI.h"

#include "Platform/OpenGL/OpenGLRendererAPI.h"

namespace Hazel {

    RendererAPI::API RendererAPI::s_API = RendererAPI::API::OpenGL;

    Scope<RendererAPI> RendererAPI::Create()
    {
        switch (s_API)
        {
            case API::OpenGL: return CreateScope<OpenGLRendererAPI>();
            case API::None:
                HZ_CORE_ASSERT(false, "RendererAPI::None is not supported");
                return nullptr;
        }
        HZ_CORE_ASSERT(false, "Unknown RendererAPI");
        return nullptr;
    }

} // namespace Hazel