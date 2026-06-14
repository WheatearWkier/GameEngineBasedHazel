#include "hzpch.h"
#include "SceneCamera.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Hazel {

    SceneCamera::SceneCamera()
    {
        RecalculateProjection();
    }

    void SceneCamera::SetPerspective(float verticalFOV, float nearClip, float farClip)
    {
        m_ProjectionType = ProjectionType::Perspective;
        m_PerspectiveFOV = verticalFOV;
        m_PerspectiveNear = nearClip;
        m_PerspectiveFar = farClip;
        RecalculateProjection();
    }

    void SceneCamera::SetOrthographic(float size, float nearClip, float farClip)
    {
        m_ProjectionType = ProjectionType::Orthographic;
        m_OrthographicSize = size;
        m_OrthographicNear = nearClip;
        m_OrthographicFar = farClip;
        RecalculateProjection();
    }

    void SceneCamera::SetViewportSize(uint32_t width, uint32_t height)
    {
        HZ_CORE_ASSERT(width > 0 && height > 0, "Viewport size must be greater than zero");
        m_AspectRatio = static_cast<float>(width) / static_cast<float>(height);
        RecalculateProjection();
    }

    void SceneCamera::RecalculateProjection()
    {
        switch (m_ProjectionType)
        {
        case ProjectionType::Perspective:
        {
            m_Projection = glm::perspective(
                m_PerspectiveFOV,
                m_AspectRatio,
                m_PerspectiveNear,
                m_PerspectiveFar
            );
            break;
        }
        case ProjectionType::Orthographic:
        {
            const float halfSize = m_OrthographicSize * 0.5f;
            const float halfWidth = halfSize * m_AspectRatio;
            m_Projection = glm::ortho(
                -halfWidth, halfWidth,
                -halfSize, halfSize,
                m_OrthographicNear, m_OrthographicFar
            );
            break;
        }
        default:
            HZ_CORE_ASSERT(false, "Unknown ProjectionType");
            break;
        }
    }

} // namespace Hazel