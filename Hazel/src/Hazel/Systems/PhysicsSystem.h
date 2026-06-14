#pragma once

#include "ISystem.h"

class b2World;

namespace Hazel {

    class ContactListener;

    /// @brief 2D 物理系统
    /// 
    /// 负责：
    ///   - Box2D 世界的创建与销毁
    ///   - 运行时刚体初始化
    ///   - 每帧物理步进
    ///   - 物理结果回写到 TransformComponent
    class PhysicsSystem : public ISystem
    {
    public:
        void OnRuntimeStart(Scene* scene) override;
        void OnRuntimeStop(Scene* scene) override;
        void OnUpdateRuntime(Scene* scene, Timestep ts) override;
        void OnEntityCreated(Scene* scene, Entity& entity) override;
        void OnEntityDestroy(Scene* scene, Entity& entity) override;

        /// 直接为单个实体初始化物理 body（OnEntityCreated 内部调用）
        void InitEntityPhysics(Scene* scene, Entity entity);

        b2World* GetPhysicsWorld() const { return m_PhysicsWorld; }

    private:
        b2World* m_PhysicsWorld = nullptr;
        ContactListener* m_ContactListener = nullptr;
    };

} // namespace Hazel