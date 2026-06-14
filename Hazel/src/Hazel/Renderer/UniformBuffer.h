#pragma once

#include "Hazel/Core/Core.h"

namespace Hazel {

    // UniformBuffer 对应 GPU 上的 UBO（Uniform Buffer Object）
    // 用于高效地向 Shader 传递每帧变化的数据（如相机矩阵）
    // binding 对应 Shader 里的 layout(binding = N)
    class UniformBuffer
    {
    public:
        virtual ~UniformBuffer() = default;

        // 上传数据到 GPU
        // data   : CPU 端数据指针
        // size   : 字节数
        // offset : UBO 内的字节偏移（默认从头开始）
        virtual void SetData(const void* data, uint32_t size,
            uint32_t offset = 0) = 0;

        static Ref<UniformBuffer> Create(uint32_t size, uint32_t binding);
    };

} // namespace Hazel