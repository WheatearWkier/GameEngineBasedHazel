#include "hzpch.h"
#include "Scene.h"
#include "Components.h"
#include "Entity.h"
#include "SceneSerializer.h"

#include "Hazel/Systems/PhysicsSystem.h"
#include "Hazel/Systems/ScriptSystem.h"
#include "Hazel/Systems/AnimationSystem.h"
#include "Hazel/Systems/AudioSystem.h"
#include "Hazel/Systems/UISystem.h"
#include "Hazel/Systems/RenderSystem.h"

#include <glm/glm.hpp>

// 一个bug：在play状态下在动画编辑器面板play动画后再按场景暂停键会程序崩溃

namespace Hazel {

    // ═══════════════════════════════════════════════════════════════════════════
    //  组件复制辅助（内部）
    // ═══════════════════════════════════════════════════════════════════════════

    namespace {

        template<typename... Ts>
        void CopyComponents(ComponentGroup<Ts...>,
            entt::registry& dst, entt::registry& src,
            const std::unordered_map<UUID, entt::entity>& map)
        {
            ([&] {
                for (auto e : src.view<Ts>())
                {
                    UUID uuid = src.get<IDComponent>(e).ID;
                    HZ_CORE_ASSERT(map.count(uuid), "Entity UUID not found in map");
                    dst.emplace_or_replace<Ts>(map.at(uuid), src.get<Ts>(e));
                }
                }(), ...);
        }

        template<typename... Ts>
        void CopyComponentsIfExist(ComponentGroup<Ts...>, Entity dst, Entity src)
        {
            ([&] {
                if (src.HasComponent<Ts>())
                    dst.AddOrReplaceComponent<Ts>(src.GetComponent<Ts>());
                }(), ...);
        }

        using AllCopyableComponents = ComponentGroup<
            TransformComponent,
            SpriteRendererComponent,
            SpriteAnimatorComponent,
            CircleRendererComponent,
            MeshRendererComponent,
            DirectionalLightComponent,
            PointLightComponent,
            CameraComponent,
            NativeScriptComponent,
            Rigidbody2DComponent,
            BoxCollider2DComponent,
            CircleCollider2DComponent,
            ScriptComponent,
            UICanvasComponent,
            UIWidgetComponent,
            UIImageComponent,
            UITextComponent,
            UIButtonComponent,
            UIProgressBarComponent,
            AudioSourceComponent
        >;

    } // anonymous namespace

    // ═══════════════════════════════════════════════════════════════════════════
    //  构造 / 析构
    // ═══════════════════════════════════════════════════════════════════════════

    Scene::Scene() = default;
    Scene::~Scene() = default;

    // ═══════════════════════════════════════════════════════════════════════════
    //  场景复制
    // ═══════════════════════════════════════════════════════════════════════════

    Ref<Scene> Scene::Copy(Ref<Scene> other)
    {
        Ref<Scene> newScene = CreateRef<Scene>();
        newScene->m_ViewportWidth = other->m_ViewportWidth;
        newScene->m_ViewportHeight = other->m_ViewportHeight;
        newScene->m_ViewportOffset = other->m_ViewportOffset;

        auto& src = other->m_Registry;
        auto& dst = newScene->m_Registry;

        std::unordered_map<UUID, entt::entity> enttMap;
        for (auto e : src.view<IDComponent>())
        {
            UUID        uuid = src.get<IDComponent>(e).ID;
            const auto& name = src.get<TagComponent>(e).Tag;
            enttMap[uuid] = static_cast<entt::entity>(
                newScene->CreateEntityWithUUID(uuid, name));
        }

        CopyComponents(AllCopyableComponents{}, dst, src, enttMap);
        return newScene;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  实体管理
    // ═══════════════════════════════════════════════════════════════════════════

    Entity Scene::CreateEntity(const std::string& name)
    {
        return CreateEntityWithUUID(UUID(), name);
    }

    Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
    {
        Entity entity = { m_Registry.create(), this };
        entity.AddComponent<IDComponent>(uuid);
        entity.AddComponent<TransformComponent>();
        entity.AddComponent<TagComponent>().Tag = name.empty() ? "Entity" : name;
        return entity;
    }

    void Scene::DestroyEntityImmediate(Entity entity)
    {
        m_Registry.destroy(entity);
    }

    void Scene::DestroyEntity(Entity entity)
    {
        m_DestroyQueue.insert(static_cast<entt::entity>(entity));
    }

    Entity Scene::DuplicateEntity(Entity entity)
    {
        Entity newEntity = CreateEntity(entity.GetName());
        CopyComponentsIfExist(AllCopyableComponents{}, newEntity, entity);
        return newEntity;
    }

    Entity Scene::InstantiateFromPrefab(const std::filesystem::path& prefabPath,
        const glm::vec3& position)
    {
        Entity entity = SceneSerializer::DeserializePrefab(prefabPath, this);
        if (!entity)
        {
            HZ_CORE_WARN("InstantiateFromPrefab: failed to load '{}'", prefabPath.string());
            return {};
        }

        if (entity.HasComponent<TransformComponent>())
            entity.GetComponent<TransformComponent>().Translation = position;

        // Scene 不知道任何系统细节，只广播事件
        for (auto& system : m_Systems)
            system->OnEntityCreated(this, entity);

        return entity;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  延迟销毁队列
    // ═══════════════════════════════════════════════════════════════════════════

    void Scene::FlushDestroyQueueEditor()
    {
        for (entt::entity e : m_DestroyQueue)
            if (m_Registry.valid(e))
                m_Registry.destroy(e);
        m_DestroyQueue.clear();
    }

    void Scene::FlushDestroyQueue()
    {
        for (entt::entity e : m_DestroyQueue)
        {
            if (!m_Registry.valid(e)) continue;
            Entity entity = { e, this };

            // 每个系统自己决定销毁时要做什么，Scene 完全不知道细节
            for (auto& system : m_Systems)
                system->OnEntityDestroy(this, entity);

            m_Registry.destroy(e);
        }
        m_DestroyQueue.clear();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  生命周期
    // ═══════════════════════════════════════════════════════════════════════════

    void Scene::OnRuntimeStart()
    {
        // ── 系统注册（顺序即执行顺序）──────────────────────────────────────
        // 需要新系统时在这里加一行，其他地方完全不用改
        m_Systems.push_back(CreateScope<PhysicsSystem>());
        m_Systems.push_back(CreateScope<ScriptSystem>());
        m_Systems.push_back(CreateScope<AnimationSystem>());
        m_Systems.push_back(CreateScope<AudioSystem>());
        m_Systems.push_back(CreateScope<UISystem>());
        m_Systems.push_back(CreateScope<RenderSystem>());

        for (auto& system : m_Systems)
            system->OnRuntimeStart(this);
    }

    void Scene::OnRuntimeStop()
    {
        // 逆序停止，保证依赖关系正确（渲染先停，物理后停）
        for (auto it = m_Systems.rbegin(); it != m_Systems.rend(); ++it)
            (*it)->OnRuntimeStop(this);
        m_Systems.clear();

        m_DestroyQueue.clear();
    }

    void Scene::OnUpdateRuntime(Timestep ts)
    {
        FlushDestroyQueue();

        for (auto& system : m_Systems)
            system->OnUpdateRuntime(this, ts);
    }

    void Scene::OnEditorStart()
    {
        m_Systems.clear();

        // 只注册编辑模式需要的系统
        m_Systems.push_back(CreateScope<AnimationSystem>());
        m_Systems.push_back(CreateScope<UISystem>());
        m_Systems.push_back(CreateScope<RenderSystem>());

        for (auto& system : m_Systems)
            system->OnRuntimeStart(this); // 可以复用
    }

    void Scene::OnEditorStop()
    {
        for (auto it = m_Systems.rbegin(); it != m_Systems.rend(); ++it)
            (*it)->OnRuntimeStop(this);

        m_Systems.clear();
    }

    void Scene::OnUpdateEditor(Timestep ts, EditorCamera& camera)
    {
        FlushDestroyQueueEditor();

        for (auto& system : m_Systems)
            system->OnUpdateEditor(this, ts);

        // 编辑模式渲染：使用编辑器相机而非游戏主相机
        if (auto* render = GetSystem<RenderSystem>())
            render->RenderWithEditorCamera(this, camera);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  视口
    // ═══════════════════════════════════════════════════════════════════════════

    void Scene::OnViewportResize(uint32_t width, uint32_t height)
    {
        m_ViewportWidth = width;
        m_ViewportHeight = height;

        for (auto e : m_Registry.view<CameraComponent>())
        {
            auto& cc = m_Registry.get<CameraComponent>(e);
            if (!cc.FixedAspectRatio)
                cc.Camera.SetViewportSize(width, height);
        }
    }

    void Scene::SetViewportOffset(float x, float y)
    {
        m_ViewportOffset = { x, y };

        if (auto* ui = GetSystem<UISystem>())
            ui->SetViewportOffset(x, y);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  动画编辑器预览
    // ═══════════════════════════════════════════════════════════════════════════

    void Scene::SetAnimationEditorPreviewActive(bool active)
    {
        if (auto* anim = GetSystem<AnimationSystem>())
            anim->SetEditorPreviewActive(active);
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  查询
    // ═══════════════════════════════════════════════════════════════════════════

    Entity Scene::GetPrimaryCameraEntity()
    {
        for (auto e : m_Registry.view<CameraComponent>())
            if (m_Registry.get<CameraComponent>(e).Primary)
                return { e, this };
        return {};
    }

    Entity Scene::GetEntityByUUID(UUID uuid)
    {
        for (auto e : m_Registry.view<IDComponent>())
            if (m_Registry.get<IDComponent>(e).ID == uuid)
                return { e, this };
        HZ_CORE_WARN("GetEntityByUUID: entity {} not found", (uint64_t)uuid);
        return {};
    }

    Entity Scene::GetEntityByName(const std::string& name)
    {
        for (auto e : m_Registry.view<TagComponent>())
            if (m_Registry.get<TagComponent>(e).Tag == name)
                return { e, this };
        return {};
    }

    // ═══════════════════════════════════════════════════════════════════════════
    //  OnComponentAdded
    // ═══════════════════════════════════════════════════════════════════════════

    template<typename T>
    void Scene::OnComponentAdded(Entity, T&) {}

    template<>
    void Scene::OnComponentAdded<CameraComponent>(Entity, CameraComponent& component)
    {
        if (m_ViewportWidth > 0 && m_ViewportHeight > 0)
            component.Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
    }

    template<>
    void Scene::OnComponentAdded<MeshRendererComponent>(Entity entity, MeshRendererComponent& component)
    {
        // MeshRendererComponent 不需要特殊初始化，空函数体即可
    }

    template<>
    void Scene::OnComponentAdded<DirectionalLightComponent>(Entity entity, DirectionalLightComponent& component)
    {
    }

    template<>
    void Scene::OnComponentAdded<PointLightComponent>(Entity entity, PointLightComponent& component)
    {
    }

} // namespace Hazel