#include "hzpch.h"
#include "EditorCamera.h"
#include "Hazel/Core/Input.h"
#include "Hazel/Core/KeyCodes.h"
#include "Hazel/Core/MouseButtonCodes.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Hazel {

    // -------------------------------------------------------------------------
    // 构造 / 投影
    // -------------------------------------------------------------------------

    EditorCamera::EditorCamera(float fovDegrees, float aspectRatio,
        float nearClip, float farClip)
        : Camera(glm::perspective(glm::radians(fovDegrees), aspectRatio, nearClip, farClip))
        , m_FOV(fovDegrees)
        , m_AspectRatio(aspectRatio)
        , m_NearClip(nearClip)
        , m_FarClip(farClip)
    {
        UpdateView();
    }

    void EditorCamera::SetViewportSize(float width, float height)
    {
        m_ViewportWidth = width;
        m_ViewportHeight = height;
        UpdateProjection();
    }

    void EditorCamera::UpdateProjection()
    {
        m_AspectRatio = m_ViewportWidth / m_ViewportHeight;
        m_Projection = glm::perspective(glm::radians(m_FOV),
            m_AspectRatio,
            m_NearClip, m_FarClip);
    }

    // -------------------------------------------------------------------------
    // 视图矩阵
    // 两种模式位置来源不同：
    //   Orbit -> 从焦点 + 距离反算
    //   Fly   -> 直接用 m_Position
    // -------------------------------------------------------------------------

    void EditorCamera::UpdateView()
    {
        if (m_Mode == Mode::Orbit)
            m_Position = CalculateOrbitPosition();
        // Fly 模式下 m_Position 由 OnUpdate 直接修改，这里不覆盖

        const glm::quat orientation = GetOrientation();
        m_ViewMatrix = glm::inverse(
            glm::translate(glm::mat4(1.0f), m_Position) * glm::toMat4(orientation)
        );
    }

    // -------------------------------------------------------------------------
    // 每帧更新
    // -------------------------------------------------------------------------

    void EditorCamera::OnUpdate(Timestep ts)
    {
        const bool rightMouseHeld = Input::IsMouseButtonPressed(HZ_MOUSE_BUTTON_RIGHT);

        // ---- 模式切换 ----
        // 按下鼠标右键 -> 进入飞行模式，记录焦点位置（让两模式之间位置连续）
        if (rightMouseHeld && m_Mode == Mode::Orbit)
        {
            m_Mode = Mode::Fly;
            // 进入飞行时，把焦点拉到相机前方一段距离，
            // 这样下次切回轨道模式时不会跳变
            m_FocalPoint = m_Position + GetForwardDirection() * m_Distance;
            m_InitialMousePosition = { Input::GetMouseX(), Input::GetMouseY() };
        }
        else if (!rightMouseHeld && m_Mode == Mode::Fly)
        {
            m_Mode = Mode::Orbit;
            // 退出飞行时，用当前位置反算轨道距离，使轨道模式平滑接管
            m_FocalPoint = m_Position + GetForwardDirection() * m_Distance;
        }

        // ---- 飞行模式 ----
        if (m_Mode == Mode::Fly)
        {
            // 鼠标转向（右键按住期间）
            const glm::vec2 mouse = { Input::GetMouseX(), Input::GetMouseY() };
            const glm::vec2 delta = (mouse - m_InitialMousePosition) * 0.003f;
            m_InitialMousePosition = mouse;

            // delta.x -> Yaw，delta.y -> Pitch
            const float yawSign = (GetUpDirection().y < 0.0f) ? -1.0f : 1.0f;
            m_Yaw += yawSign * delta.x * RotationSpeed();
            m_Pitch += delta.y * RotationSpeed();

            // Pitch 夹角，避免翻转（±89°）
            const float pitchLimit = glm::radians(89.0f);
            m_Pitch = glm::clamp(m_Pitch, -pitchLimit, pitchLimit);

            // WASD / QE 移动
            const float speed = m_FlySpeed * (float)ts;
            const float fastMult = Input::IsKeyPressed(HZ_KEY_LEFT_SHIFT) ? 3.0f : 1.0f;
            const float slowMult = Input::IsKeyPressed(HZ_KEY_LEFT_CONTROL) ? 0.25f : 1.0f;
            const float finalSpeed = speed * fastMult * slowMult;

            if (Input::IsKeyPressed(HZ_KEY_W)) m_Position += GetForwardDirection() * finalSpeed;
            if (Input::IsKeyPressed(HZ_KEY_S)) m_Position -= GetForwardDirection() * finalSpeed;
            if (Input::IsKeyPressed(HZ_KEY_A)) m_Position -= GetRightDirection() * finalSpeed;
            if (Input::IsKeyPressed(HZ_KEY_D)) m_Position += GetRightDirection() * finalSpeed;
            if (Input::IsKeyPressed(HZ_KEY_Q)) m_Position -= GetUpDirection() * finalSpeed;  // 下降
            if (Input::IsKeyPressed(HZ_KEY_E)) m_Position += GetUpDirection() * finalSpeed;  // 上升
        }

        // ---- 轨道模式：Alt + 鼠标（保持原逻辑不变）----
        if (m_Mode == Mode::Orbit && Input::IsKeyPressed(HZ_KEY_LEFT_ALT))
        {
            const glm::vec2 mouse = { Input::GetMouseX(), Input::GetMouseY() };
            const glm::vec2 delta = (mouse - m_InitialMousePosition) * 0.003f;
            m_InitialMousePosition = mouse;

            if (Input::IsMouseButtonPressed(HZ_MOUSE_BUTTON_MIDDLE)) MousePan(delta);
            else if (Input::IsMouseButtonPressed(HZ_MOUSE_BUTTON_LEFT))   MouseRotate(delta);
            else if (Input::IsMouseButtonPressed(HZ_MOUSE_BUTTON_RIGHT))  MouseZoom(delta.y);
        }

        UpdateView();
    }

    // -------------------------------------------------------------------------
    // 事件：滚轮只在轨道模式生效（飞行模式用键盘控速）
    // -------------------------------------------------------------------------

    void EditorCamera::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<MouseScrolledEvent>(HZ_BIND_EVENT_FN(EditorCamera::OnMouseScroll));
    }

    bool EditorCamera::OnMouseScroll(MouseScrolledEvent& e)
    {
        if (m_Mode == Mode::Orbit)
        {
            MouseZoom(e.GetYOffset() * 0.1f);
            UpdateView();
        }
        else
        {
            // 飞行模式：滚轮调整飞行速度
            m_FlySpeed = glm::max(0.5f, m_FlySpeed + e.GetYOffset() * 0.5f);
        }
        return false;
    }

    // -------------------------------------------------------------------------
    // 轨道模式操作（原逻辑保留）
    // -------------------------------------------------------------------------

    void EditorCamera::MousePan(const glm::vec2& delta)
    {
        auto [xSpeed, ySpeed] = PanSpeed();
        m_FocalPoint -= GetRightDirection() * delta.x * xSpeed * m_Distance;
        m_FocalPoint += GetUpDirection() * delta.y * ySpeed * m_Distance;
    }

    void EditorCamera::MouseRotate(const glm::vec2& delta)
    {
        const float yawSign = (GetUpDirection().y < 0.0f) ? -1.0f : 1.0f;
        m_Yaw += yawSign * delta.x * RotationSpeed();
        m_Pitch += delta.y * RotationSpeed();
    }

    void EditorCamera::MouseZoom(float delta)
    {
        m_Distance -= delta * ZoomSpeed();
        if (m_Distance < 1.0f)
        {
            m_FocalPoint += GetForwardDirection();
            m_Distance = 1.0f;
        }
    }

    // -------------------------------------------------------------------------
    // 速度参数（原逻辑保留）
    // -------------------------------------------------------------------------

    std::pair<float, float> EditorCamera::PanSpeed() const
    {
        const float x = std::min(m_ViewportWidth / 1000.0f, 2.4f);
        const float y = std::min(m_ViewportHeight / 1000.0f, 2.4f);
        auto factor = [](float v) {
            return 0.0366f * (v * v) - 0.1778f * v + 0.3021f;
            };
        return { factor(x), factor(y) };
    }

    float EditorCamera::RotationSpeed() const
    {
        return 0.8f;
    }

    float EditorCamera::ZoomSpeed() const
    {
        const float distance = std::max(m_Distance * 0.2f, 0.0f);
        return std::min(distance * distance, 100.0f);
    }

    // -------------------------------------------------------------------------
    // 方向向量 & 位置计算
    // -------------------------------------------------------------------------

    glm::quat EditorCamera::GetOrientation() const
    {
        return glm::quat(glm::vec3(-m_Pitch, -m_Yaw, 0.0f));
    }

    glm::vec3 EditorCamera::GetUpDirection() const
    {
        return glm::rotate(GetOrientation(), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::vec3 EditorCamera::GetRightDirection() const
    {
        return glm::rotate(GetOrientation(), glm::vec3(1.0f, 0.0f, 0.0f));
    }

    glm::vec3 EditorCamera::GetForwardDirection() const
    {
        return glm::rotate(GetOrientation(), glm::vec3(0.0f, 0.0f, -1.0f));
    }

    glm::vec3 EditorCamera::CalculateOrbitPosition() const
    {
        return m_FocalPoint - GetForwardDirection() * m_Distance;
    }

} // namespace Hazel