#pragma once

#include "Scene.h"
#include <filesystem>

namespace Hazel {

    class SceneSerializer
    {
    public:
        explicit SceneSerializer(const Ref<Scene>& scene);

        // 序列化
        void SerializeYaml(const std::filesystem::path& filepath);

        // 反序列化，成功返回 true
        bool DeserializeYaml(const std::filesystem::path& filepath);

        // 供 PrefabSerializer 复用
        static bool SerializePrefab(Entity entity, const std::filesystem::path& filepath);
        static Entity DeserializePrefab(const std::filesystem::path& filepath, Scene* scene);

    private:
        Ref<Scene> m_Scene;
    };

    /// @brief 编译期类型包，用于将一组组件类型打包成单个模板参数。
    /// 自身不含任何数据或逻辑，仅作为类型标签在模板推导时展开成 Ts...
    /// 
    /// 用法示例：
    ///   using MyComponents = ComponentGroup<TransformComponent, CameraComponent>;
    ///   SomeFunction(MyComponents{}, ...);  // 函数通过偏特化/参数推导展开 Ts...
    template<typename T>
    struct ComponentSerializer; // 只声明，不实现

} // namespace Hazel