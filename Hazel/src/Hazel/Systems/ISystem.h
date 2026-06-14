#pragma once

#include "Hazel/Core/Timestep.h"
#include "Hazel/Renderer/EditorCamera.h"

namespace Hazel {

    class Scene;
    class Entity;

    /// @brief 所有 ECS System 的统一接口
    /// 
    /// 每个 System 负责一类独立的逻辑（物理、动画、UI、渲染等）。
    /// Scene 持有 System 列表，按顺序调用，自身不包含任何系统细节。
    /// 
    /// 新增系统步骤：
    ///   1. 继承 ISystem，实现需要的虚函数
    ///   2. 在 Scene::OnRuntimeStart() 里 push_back 新系统
    class ISystem
    {
    public:
        virtual ~ISystem() = default;

        /// 运行时启动（物理世界创建、脚本初始化等）
        virtual void OnRuntimeStart(Scene* scene) {}

        /// 运行时停止（释放运行时资源）
        virtual void OnRuntimeStop(Scene* scene) {}

        /// 运行时每帧更新
        virtual void OnUpdateRuntime(Scene* scene, Timestep ts) {}

        /// 编辑模式每帧更新（大多数系统不需要实现）
        virtual void OnUpdateEditor(Scene* scene, Timestep ts) {}

        /// 实体被 InstantiateFromPrefab 或运行时动态创建后调用
        virtual void OnEntityCreated(Scene* scene, Entity& entity) {}

        /// 实体进入销毁队列、registry.destroy 之前调用
        virtual void OnEntityDestroy(Scene* scene, Entity& entity) {}
    };

} // namespace Hazel