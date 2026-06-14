#include "hzpch.h"
#include "UISystem.h"

#include "Hazel/Scene/Scene.h"
#include "Hazel/Scene/Components.h"
#include "Hazel/Core/Input.h"
#include "Hazel/UI/UIRenderer.h"
#include "Hazel/UI/UIInputSystem.h"

#include <glm/glm.hpp>

namespace Hazel {

    void UISystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        UIInputSystem::OnUpdate(
            scene,
            Input::GetMouseX() - m_ViewportOffset.x,
            Input::GetMouseY() - m_ViewportOffset.y,
            scene->GetViewportWidth(),
            scene->GetViewportHeight());
    }

    void UISystem::OnUpdateEditor(Scene* scene, Timestep ts)
    {
        // 编辑模式下 UI 只渲染，不处理输入
        // 渲染由 RenderSystem 在合适时机调用 RenderUI()
    }

    void UISystem::RenderUI(Scene* scene)
    {
        auto& registry = scene->GetRegistry();

        // 收集并按 SortOrder 排序
        std::vector<std::pair<int, entt::entity>> entries;
        for (auto e : registry.view<UIWidgetComponent>())
            entries.emplace_back(registry.get<UIWidgetComponent>(e).SortOrder, e);

        std::sort(entries.begin(), entries.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        for (auto [order, e] : entries)
        {
            auto& widget = registry.get<UIWidgetComponent>(e);
            if (!widget.Visible) continue;
            int id = static_cast<int>(static_cast<uint32_t>(e));

            if (auto* pb = registry.try_get<UIProgressBarComponent>(e))
                UIRenderer::DrawUIProgressBar(widget, *pb, id);
            else if (auto* btn = registry.try_get<UIButtonComponent>(e))
                UIRenderer::DrawUIButton(widget, *btn, id);
            else if (auto* img = registry.try_get<UIImageComponent>(e))
                UIRenderer::DrawUIImage(widget, *img, id);
        }
    }

} // namespace Hazel