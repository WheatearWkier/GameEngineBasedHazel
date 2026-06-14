#pragma once

#include <glm/glm.hpp>

#include "Texture.h"

namespace Hazel {

    // SubTexture2D 表示一张大纹理图集（atlas）中的一个子区域
    // UV 坐标在构造时归一化到 [0, 1]
    class SubTexture2D
    {
    public:
        SubTexture2D(const Ref<Texture2D>& texture,
            const glm::vec2& min,
            const glm::vec2& max);

        const Ref<Texture2D>& GetTexture()   const { return m_Texture; }
        const glm::vec2* GetTexCoords() const { return m_TexCoords; }

        // 从图集坐标（格子索引）创建子纹理
        // coords    : 格子索引，如 {2, 3} 表示第2列第3行
        // cellSize  : 每个格子的像素尺寸，如 {16, 16}
        // spriteSize: 占用格子数，如 {1, 2} 表示横1格竖2格，默认 {1, 1}
        static Ref<SubTexture2D> CreateFromCoords(
            const Ref<Texture2D>& texture,
            const glm::vec2& coords,
            const glm::vec2& cellSize,
            const glm::vec2& spriteSize = { 1.0f, 1.0f }
        );

    private:
        Ref<Texture2D> m_Texture;
        glm::vec2      m_TexCoords[4];
    };

} // namespace Hazel