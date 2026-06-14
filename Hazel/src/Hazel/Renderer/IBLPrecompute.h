#pragma once
#include "Hazel/Core/Core.h"
#include <string>

namespace Hazel {

    struct IBLResult
    {
        uint32_t IrradianceMap = 0;
        uint32_t PrefilterMap = 0;
        uint32_t BrdfLUT = 0;
        bool IsValid() const { return IrradianceMap && PrefilterMap && BrdfLUT; }

        void Release();
        ~IBLResult() { Release(); }

        IBLResult() = default;
        IBLResult(const IBLResult&) = delete;
        IBLResult& operator=(const IBLResult&) = delete;
        IBLResult(IBLResult&& o) noexcept;
        IBLResult& operator=(IBLResult&& o) noexcept;
    };

    // 平台无关的工厂接口，和 Texture2D::Create() 是同一个套路
    class IBLPrecompute
    {
    public:
        static Ref<IBLResult> ComputeOrLoad(const std::string& hdrPath);
        static Ref<IBLResult> Compute(const std::string& hdrPath);
        static void           SaveToCache(const Ref<IBLResult>& ibl,
            const std::string& cachePath);
        static Ref<IBLResult> LoadFromCache(const std::string& cachePath);
        static uint32_t       GetBrdfLUT();
    };

} // namespace Hazel