#pragma once

#include "ISystem.h"

namespace Hazel {

    /// @brief 精灵动画系统
    /// 
    /// 负责：
    ///   - 运行时动画帧推进与采样
    ///   - 属性轨道（位移、旋转、缩放、颜色）插值写回
    ///   - 编辑模式下的首帧预览同步
    class AnimationSystem : public ISystem
    {
    public:
        void OnRuntimeStart(Scene* scene) override;
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;
        void OnUpdateEditor(Scene* scene, Timestep ts) override;

        void SetEditorPreviewActive(bool active) { m_EditorPreviewActive = active; }

    private:
        void UpdateAnimations(Scene* scene, Timestep ts);
        void SyncEditorPreviewFrame(Scene* scene);

        bool m_EditorPreviewActive = false;
    };

} // namespace Hazel