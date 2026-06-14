#pragma once

#include "ISystem.h"

namespace Hazel {

    /// @brief 脚本系统
    /// 
    /// 负责：
    ///   - NativeScript（C++ 脚本）的实例化与每帧更新
    ///   - C# 脚本（Mono）的运行时启动、每帧更新、实体销毁回调
    class ScriptSystem : public ISystem
    {
    public:
        void OnRuntimeStart(Scene* scene) override;
        void OnRuntimeStop(Scene* scene) override;
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;
        void OnEntityCreated(Scene* scene, Entity& entity) override;
        void OnEntityDestroy(Scene* scene, Entity& entity) override;

    };

} // namespace Hazel