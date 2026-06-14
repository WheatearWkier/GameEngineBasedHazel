#pragma once
// GLM_ENABLE_EXPERIMENTAL 是为了用 gtx 里的函数，通常放在 pch 里，这里显式加一遍保险
#define GLM_ENABLE_EXPERIMENTAL
#include "Hazel/Core/Timestep.h"
#include "Hazel/Events/Event.h"
#include "Hazel/Events/MouseEvent.h"
#include "Hazel/Renderer/Camera.h"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Hazel {

    class EditorCamera : public Camera
    {
    public:
        // 相机模式
        enum class Mode
        {
            Orbit,  // 轨道模式（默认）：绕焦点旋转，Alt+鼠标控制
            Fly     // 飞行模式：按住鼠标右键激活，WASD移动
        };

    public:
        EditorCamera() = default;
        EditorCamera(float fovDegrees, float aspectRatio, float nearClip, float farClip);

        void OnUpdate(Timestep ts);
        void OnEvent(Event& e);

        // 视口
        void SetViewportSize(float width, float height);

        // Getters
        float GetDistance() const { return m_Distance; }
        void  SetDistance(float distance) { m_Distance = distance; }

        float GetPitch() const { return m_Pitch; }
        float GetYaw()   const { return m_Yaw; }

        Mode  GetMode()  const { return m_Mode; }

        const glm::mat4& GetViewMatrix()     const { return m_ViewMatrix; }
        glm::mat4        GetViewProjection() const { return m_Projection * m_ViewMatrix; }

        const glm::vec3& GetPosition()         const { return m_Position; }
        glm::quat        GetOrientation()      const;
        glm::vec3        GetUpDirection()      const;
        glm::vec3        GetRightDirection()   const;
        glm::vec3        GetForwardDirection() const;

    private:
        void UpdateProjection();
        void UpdateView();

        // 事件处理
        bool OnMouseScroll(MouseScrolledEvent& e);

        // 轨道模式操作
        void MousePan(const glm::vec2& delta);
        void MouseRotate(const glm::vec2& delta);
        void MouseZoom(float delta);

        // 辅助
        glm::vec3 CalculateOrbitPosition() const;
        std::pair<float, float> PanSpeed()    const;
        float                   RotationSpeed() const;
        float                   ZoomSpeed()     const;

    private:
        // 投影参数
        float m_FOV = 45.0f;
        float m_AspectRatio = 1.778f;
        float m_NearClip = 0.1f;
        float m_FarClip = 1000.0f;

        // 视图矩阵
        glm::mat4 m_ViewMatrix = glm::mat4(1.0f);

        // 相机位置与朝向（两种模式共用）
        glm::vec3 m_Position = { 0.0f, 8.0f, 15.0f };
        float     m_Pitch = 0.0f;   // 俯仰角（弧度）
        float     m_Yaw = 0.0f;   // 偏航角（弧度）

        // 轨道模式专用
        glm::vec3 m_FocalPoint = { 0.0f, 0.0f, 0.0f };
        float     m_Distance = 10.0f;

        // 飞行模式专用
        float m_FlySpeed = 5.0f;  // 单位/秒，Shift 加速 x3

        // 当前模式
        Mode m_Mode = Mode::Orbit;

        // 鼠标
        glm::vec2 m_InitialMousePosition = { 0.0f, 0.0f };

        // 视口尺寸
        float m_ViewportWidth = 1920.0f;
        float m_ViewportHeight = 1080.0f;
    };

} // namespace Hazel