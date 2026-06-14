#include "hzpch.h"
#include "IBLPrecompute.h"
#include "Renderer.h"
#include "Platform/OpenGL/OpenGLIBL.h"  // 唯一知道OpenGL存在的地方

namespace Hazel {

    Ref<IBLResult> IBLPrecompute::Compute(const std::string& hdrPath)
    {
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::OpenGL:
            return OpenGLIBL::Compute(hdrPath);
        case RendererAPI::API::None:
            HZ_CORE_ASSERT(false, "RendererAPI::None not supported");
            return nullptr;
        }
        HZ_CORE_ASSERT(false, "Unknown RendererAPI");
        return nullptr;
    }

    Ref<IBLResult> IBLPrecompute::ComputeOrLoad(const std::string& hdrPath)
    {
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::OpenGL:
            return OpenGLIBL::ComputeOrLoad(hdrPath);
        case RendererAPI::API::None:
            HZ_CORE_ASSERT(false, "RendererAPI::None not supported");
            return nullptr;
        }
        HZ_CORE_ASSERT(false, "Unknown RendererAPI");
        return nullptr;
    }

    void IBLPrecompute::SaveToCache(const Ref<IBLResult>& ibl,
        const std::string& cachePath)
    {
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::OpenGL:
            OpenGLIBL::SaveToCache(ibl, cachePath); return;
        case RendererAPI::API::None:
            HZ_CORE_ASSERT(false, "RendererAPI::None not supported"); return;
        }
    }

    Ref<IBLResult> IBLPrecompute::LoadFromCache(const std::string& cachePath)
    {
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::OpenGL:
            return OpenGLIBL::LoadFromCache(cachePath);
        case RendererAPI::API::None:
            HZ_CORE_ASSERT(false, "RendererAPI::None not supported");
            return nullptr;
        }
        HZ_CORE_ASSERT(false, "Unknown RendererAPI");
        return nullptr;
    }

    uint32_t IBLPrecompute::GetBrdfLUT()
    {
        switch (Renderer::GetAPI())
        {
        case RendererAPI::API::OpenGL: return OpenGLIBL::GetBrdfLUT();
        default: return 0;
        }
    }

} // namespace Hazel