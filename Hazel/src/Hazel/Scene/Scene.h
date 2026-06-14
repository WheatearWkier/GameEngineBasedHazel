#pragma once

#include "entt.hpp"
#include "Hazel/Core/UUID.h"
#include "Hazel/Core/Timestep.h"
#include "Hazel/Renderer/EditorCamera.h"
#include "Hazel/Systems/ISystem.h"

#include <glm/glm.hpp>
#include <filesystem>
#include <unordered_set>
#include <vector>
#include <memory>

class b2World;

namespace Hazel {

    class Entity;

    /// @brief 场景类：实体注册表 + 系统调度器
    /// 
    /// Scene 自身不包含任何游戏逻辑细节。
    /// 所有系统（物理、动画、UI、渲染等）通过 ISystem 接口注册，
    /// Scene 只负责按顺序驱动它们。
    /// 
    /// 新增系统：在 OnRuntimeStart() 里 push_back 新系统即可，无需修改 Scene 其他代码。
    class Scene
    {
    public:
        Scene();
        ~Scene();

        static Ref<Scene> Copy(Ref<Scene> other);

        // ── 实体管理 ──────────────────────────────────────────────────────────
        Entity CreateEntity(const std::string& name = {});
        Entity CreateEntityWithUUID(UUID uuid, const std::string& name = {});

        /// 立即销毁，遍历中请勿调用
        void   DestroyEntityImmediate(Entity entity);
        /// 延迟销毁：加入队列，帧首统一处理
        void   DestroyEntity(Entity entity);

        Entity DuplicateEntity(Entity entity);
        Entity InstantiateFromPrefab(const std::filesystem::path& prefabPath,
            const glm::vec3& position = {});

        // ── 生命周期 ──────────────────────────────────────────────────────────
        void OnRuntimeStart();
        void OnRuntimeStop();
        void OnUpdateRuntime(Timestep ts);
        void OnEditorStart();
        void OnEditorStop();
        void OnUpdateEditor(Timestep ts, EditorCamera& camera);

        // ── 视口 ──────────────────────────────────────────────────────────────
        void OnViewportResize(uint32_t width, uint32_t height);

        /// 通知视口在窗口内的左上角像素偏移，供 UISystem 正确归一化鼠标坐标
        void SetViewportOffset(float x, float y);

        uint32_t GetViewportWidth()  const { return m_ViewportWidth; }
        uint32_t GetViewportHeight() const { return m_ViewportHeight; }

        // ── 查询 ──────────────────────────────────────────────────────────────
        Entity GetPrimaryCameraEntity();
        Entity GetEntityByUUID(UUID uuid);
        Entity GetEntityByName(const std::string& name);

        // ── Registry 访问 ─────────────────────────────────────────────────────
        entt::registry& GetRegistry() { return m_Registry; }
        const entt::registry& GetRegistry() const { return m_Registry; }

        template<typename... Components>
        auto GetAllEntitiesWith() { return m_Registry.view<Components...>(); }

        // ── 系统访问 ──────────────────────────────────────────────────────────
        /// 按类型查找已注册的系统，找不到返回 nullptr
        template<typename T>
        T* GetSystem()
        {
            for (auto& s : m_Systems)
                if (auto* p = dynamic_cast<T*>(s.get())) return p;
            return nullptr;
        }

        // ── 动画编辑器预览 ────────────────────────────────────────────────────
        void SetAnimationEditorPreviewActive(bool active);

    private:
        void FlushDestroyQueue();
        void FlushDestroyQueueEditor();

        template<typename T>
        void OnComponentAdded(Entity entity, T& component);

    private:
        entt::registry   m_Registry;

        uint32_t         m_ViewportWidth = 0;
        uint32_t         m_ViewportHeight = 0;
        glm::vec2        m_ViewportOffset = { 0.0f, 0.0f };

        /// 系统列表，顺序即执行顺序
        std::vector<Scope<ISystem>> m_Systems;

        std::unordered_set<entt::entity> m_DestroyQueue;

        friend class Entity;
        friend class SceneSerializer;
        friend class SceneHierarchyPanel;
    };

} // namespace Hazel
