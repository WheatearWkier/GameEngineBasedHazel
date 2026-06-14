#pragma once

#include "ISystem.h"

namespace Hazel {

    /// @brief 音频系统
    /// 
    /// 负责：
    ///   - AudioSourceComponent 的 PlayOnStart 初始化
    ///   - 场景停止时的音频清理
    class AudioSystem : public ISystem
    {
    public:
        void OnRuntimeStart(Scene* scene) override;
        void OnRuntimeStop(Scene* scene) override;
    };

} // namespace Hazel