#pragma once

#include <glm/glm.hpp>

namespace Hazel {

    /// 所有 2D 批处理顶点结构定义
    /// 新增图元时在这里加新的顶点结构体即可

    struct QuadVertex
    {
        glm::vec3 Position;
        glm::vec4 Color;
        glm::vec2 TexCoord;
        float     TexIndex;
        float     TilingFactor;
        int       EntityID;
    };

    struct CircleVertex
    {
        glm::vec3 WorldPosition;
        glm::vec3 LocalPosition;
        glm::vec4 Color;
        float     Thickness;
        float     Fade;
        int       EntityID;
    };

    struct LineVertex
    {
        glm::vec3 Position;
        glm::vec4 Color;
        int       EntityID;
    };

    // ── 未来扩展示例 ──────────────────────────────────────────────────────────
    // struct TriangleVertex { ... };
    // struct MeshVertex     { ... };

} // namespace Hazel