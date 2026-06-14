#pragma once

#include "ISystem.h"

namespace Hazel {

    /// @brief UI 系统
    /// 
    /// 负责：
    ///   - 运行时 UI 输入处理（鼠标事件分发到 UIButton 等）
    ///   - 运行时 + 编辑模式下的 UI 渲染
    ///   - 视口偏移的管理（鼠标坐标修正）
    class UISystem : public ISystem
    {
    public:
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;
        void OnUpdateEditor(Scene* scene, Timestep ts) override;

        /// 由编辑器通知视口左上角像素偏移，用于正确归一化鼠标坐标
        void SetViewportOffset(float x, float y) { m_ViewportOffset = { x, y }; }

        /// 渲染所有 UI Widget（运行时和编辑模式共用）
        void RenderUI(Scene* scene);

    private:
        glm::vec2 m_ViewportOffset = { 0.0f, 0.0f };
    };

} // namespace Hazel