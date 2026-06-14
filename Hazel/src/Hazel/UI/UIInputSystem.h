#pragma once
#include "Hazel/Scene/Scene.h"
#include "Hazel/Scene/Components.h"

namespace Hazel {

    class UIInputSystem
    {
    public:
        // 每帧在Scene::OnUpdateRuntime里调用
        // mouseX/Y 是鼠标在viewport里的像素坐标（左上角为原点）
        static void OnUpdate(Scene* scene,
            float mouseX, float mouseY,
            uint32_t viewportWidth,
            uint32_t viewportHeight);

        // 鼠标按下/抬起事件（从EditorLayer或GameLayer转发过来）
        static void OnMousePressed(Scene* scene);
        static void OnMouseReleased(Scene* scene);

    private:
        // 判断归一化鼠标坐标是否在widget的AABB内
        static bool HitTest(const UIWidgetComponent& widget,
            float normMouseX, float normMouseY);

        // 触发C#脚本里的OnClick回调
        static void FireOnClick(Scene* scene, entt::entity entity);

        static bool s_MouseWasPressed;
    };

} // namespace Hazel