#include "hzpch.h"
#include "UIInputSystem.h"
#include "Hazel/Scene/Components.h"
#include "Hazel/Scripting/ScriptEngine.h"

namespace Hazel {

    bool UIInputSystem::s_MouseWasPressed = false;

    // ─────────────────────────────────────────────────────────────────────
    // HitTest
    // widget.Position 为中心点（归一化 0~1，左上角为原点）
    // normMouseX/Y   为同样坐标系下的鼠标坐标（由调用方减去视口偏移并归一化）
    // ─────────────────────────────────────────────────────────────────────
    bool UIInputSystem::HitTest(const UIWidgetComponent& widget,
        float normMouseX, float normMouseY)
    {
        float halfW = widget.Size.x * 0.5f;
        float halfH = widget.Size.y * 0.5f;
        float left = widget.Position.x - halfW;
        float right = widget.Position.x + halfW;
        float top = widget.Position.y - halfH;
        float bottom = widget.Position.y + halfH;

        return normMouseX >= left && normMouseX <= right
            && normMouseY >= top && normMouseY <= bottom;
    }

    void UIInputSystem::FireOnClick(Scene* scene, entt::entity e)
    {
        Entity entity{ e, scene };
        if (!entity.HasComponent<UIButtonComponent>()) return;

        auto& button = entity.GetComponent<UIButtonComponent>();
        if (button.OnClickFunction.empty()) return;

        // 要求该 Entity 同时拥有 ScriptComponent
        // OnClickFunction 格式："MethodName"
        ScriptEngine::InvokeMethod(entity, button.OnClickFunction);
    }

    // ─────────────────────────────────────────────────────────────────────
    // OnUpdate
    // 由 Scene::OnUpdateRuntime 调用，mouseX/Y 已减去视口偏移（BUG FIX #4）。
    // ─────────────────────────────────────────────────────────────────────
    void UIInputSystem::OnUpdate(Scene* scene,
        float mouseX, float mouseY,
        uint32_t viewportWidth,
        uint32_t viewportHeight)
    {
        if (viewportWidth == 0 || viewportHeight == 0) return;

        // 归一化到 [0,1]（左上角为原点）
        float normX = mouseX / static_cast<float>(viewportWidth);
        float normY = mouseY / static_cast<float>(viewportHeight);

        // BUG FIX: 鼠标在视口外时不做命中判断，防止悬空触发 hover/press
        bool mouseInViewport = (normX >= 0.0f && normX <= 1.0f &&
            normY >= 0.0f && normY <= 1.0f);

        auto view = scene->GetRegistry().view<UIWidgetComponent, UIButtonComponent>();
        for (auto e : view)
        {
            auto [widget, button] = view.get<UIWidgetComponent, UIButtonComponent>(e);
            if (!widget.Visible) continue;

            bool hit = mouseInViewport && HitTest(widget, normX, normY);
            button.IsHovered = hit;
            button.IsPressed = hit && s_MouseWasPressed;
        }
    }

    void UIInputSystem::OnMousePressed(Scene* scene)
    {
        s_MouseWasPressed = true;
    }

    void UIInputSystem::OnMouseReleased(Scene* scene)
    {
        if (!s_MouseWasPressed) return;
        s_MouseWasPressed = false;

        // 鼠标抬起时，对当前 hovered 的按钮触发 OnClick
        auto view = scene->GetRegistry().view<UIWidgetComponent, UIButtonComponent>();
        for (auto e : view)
        {
            auto [widget, button] = view.get<UIWidgetComponent, UIButtonComponent>(e);
            if (button.IsHovered)
            {
                button.IsPressed = false;
                FireOnClick(scene, e);
            }
        }
    }

} // namespace Hazel