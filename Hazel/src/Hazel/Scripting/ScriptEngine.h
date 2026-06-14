#pragma once

#include "Hazel/Scene/Scene.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/Components.h"

#include <filesystem>
#include <string>
#include <unordered_map>

extern "C" {
    typedef struct _MonoClass      MonoClass;
    typedef struct _MonoObject     MonoObject;
    typedef struct _MonoMethod     MonoMethod;
    typedef struct _MonoAssembly   MonoAssembly;
    typedef struct _MonoDomain     MonoDomain;
    typedef struct _MonoImage      MonoImage;
    typedef struct _MonoClassField MonoClassField;
    typedef struct _MonoType       MonoType;
}

namespace Hazel {

    // ════════════════════════════════════════════════════════════
    //  脚本字段类型枚举
    // ════════════════════════════════════════════════════════════

    enum class ScriptFieldType
    {
        None = 0,
        Float, Double,
        Bool, Char,
        Byte, Short, Int, Long,
        Vector2, Vector3, Vector4
    };

    // ════════════════════════════════════════════════════════════
    //  ScriptField：描述一个 C# public 字段的元信息
    // ════════════════════════════════════════════════════════════

    struct ScriptField
    {
        ScriptFieldType Type = ScriptFieldType::None;
        std::string     Name;
        MonoClassField* ClassField = nullptr;  // Mono 内部字段句柄，用于 get/set value
    };

    // ════════════════════════════════════════════════════════════
    //  ScriptFieldInstance：编辑态字段值的暂存容器
    //
    //  在场景没有运行、ScriptInstance 还不存在时，用这个结构体
    //  保存用户在 Inspector 里填写的值。点击运行时再将这些值
    //  "注入"到刚创建的 ScriptInstance 里。
    // ════════════════════════════════════════════════════════════

    struct ScriptFieldInstance
    {
        ScriptField Field;  // 字段元信息（类型、名称、Mono 句柄）

        template<typename T>
        T GetValue() const
        {
            // BUG FIX: 原 ScriptInstance::s_FieldValueBuffer 只有 8 字节，
            // vec3 (12 字节) 会越界。这里用 16 字节，足够所有支持的类型。
            static_assert(sizeof(T) <= 16, "ScriptFieldInstance: T exceeds 16-byte buffer");
            return *reinterpret_cast<const T*>(m_Buffer);
        }

        template<typename T>
        void SetValue(const T& value)
        {
            static_assert(sizeof(T) <= 16, "ScriptFieldInstance: T exceeds 16-byte buffer");
            memcpy(m_Buffer, &value, sizeof(T));
        }

    private:
        uint8_t m_Buffer[16] = {};
    };

    // 每个实体的 "字段名 → 暂存值" 映射表
    using ScriptFieldMap = std::unordered_map<std::string, ScriptFieldInstance>;

    // ════════════════════════════════════════════════════════════
    //  ScriptClass：封装一个 Mono C# 类
    // ════════════════════════════════════════════════════════════

    class ScriptClass
    {
    public:
        ScriptClass() = default;
        ScriptClass(const std::string& classNamespace, const std::string& className);

        // 在 AppDomain 内分配对象内存（不调用 C# 构造函数）
        MonoObject* Instantiate();

        // 按名称和参数数量查找方法，沿继承链向上搜索
        MonoMethod* GetMethod(const std::string& name, int parameterCount);

        // 调用方法；params 为参数指针数组，无参传 nullptr
        MonoObject* InvokeMethod(MonoObject* instance, MonoMethod* method,
            void** params = nullptr);

        const std::string& GetNamespace() const { return m_ClassNamespace; }
        const std::string& GetName()      const { return m_ClassName; }
        MonoClass* GetMonoClass() const { return m_MonoClass; }

        const std::unordered_map<std::string, ScriptField>& GetFields() const
        {
            return m_Fields;
        }

    private:
        std::string m_ClassNamespace;
        std::string m_ClassName;
        MonoClass* m_MonoClass = nullptr;

        // 该类所有 public 实例字段（在构造时扫描一次）
        std::unordered_map<std::string, ScriptField> m_Fields;

        friend class ScriptEngine;
    };

    // ════════════════════════════════════════════════════════════
    //  ScriptInstance：C# 类的一个运行时实例
    // ════════════════════════════════════════════════════════════

    class ScriptInstance
    {
    public:
        ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity);

        void InvokeOnCreate();
        void InvokeOnUpdate(float ts);
        void InvokeOnCollisionEnter(Entity other);
        void InvokeOnCollisionExit(Entity other);

        // 通过字段名读取 C# 字段的值
        template<typename T>
        T GetFieldValue(const std::string& name)
        {
            // BUG FIX: 原 buffer 只有 8 字节，vec3 读取时越界。
            // 改用 16 字节局部 buffer，与 ScriptFieldInstance 保持一致。
            uint8_t buffer[16] = {};
            if (!GetFieldValueInternal(name, buffer))
                return T{};
            return *reinterpret_cast<T*>(buffer);
        }

        // 通过字段名写入 C# 字段的值
        template<typename T>
        void SetFieldValue(const std::string& name, const T& value)
        {
            SetFieldValueInternal(name, &value);
        }

        Ref<ScriptClass> GetScriptClass()  const { return m_ScriptClass; }
        MonoObject* GetMonoObject()   const { return m_Instance; }
        MonoClass* GetMonoClass()    const { return m_ScriptClass->GetMonoClass(); }

    private:
        bool GetFieldValueInternal(const std::string& name, void* outBuffer);
        bool SetFieldValueInternal(const std::string& name, const void* value);

    private:
        Ref<ScriptClass> m_ScriptClass;
        MonoObject* m_Instance = nullptr;

        MonoMethod* m_OnCreateMethod = nullptr;
        MonoMethod* m_OnUpdateMethod = nullptr;
        MonoMethod* m_OnCollisionEnterMethod = nullptr;
        MonoMethod* m_OnCollisionExitMethod = nullptr;
    };

    // ════════════════════════════════════════════════════════════
    //  ScriptEngine：全局脚本系统
    // ════════════════════════════════════════════════════════════

    class ScriptEngine
    {
    public:
        static void Init();
        static void Shutdown();

        // 加载 C# 程序集，同时扫描并注册所有 Entity 子类
        static void LoadAssembly(const std::filesystem::path& filepath);

        // ── 查询 ────────────────────────────────────────────────
        static bool                EntityClassExists(const std::string& fullClassName);
        static Ref<ScriptClass>    GetEntityClass(const std::string& fullClassName);
        static Ref<ScriptInstance> GetEntityScriptInstance(UUID entityID);
        static Scene* GetSceneContext();
        static MonoImage* GetCoreAssemblyImage();

        // ── 场景生命周期 ─────────────────────────────────────────
        static void OnRuntimeStart(Scene* scene);
        static void OnRuntimeStop();

        // ── Entity 生命周期 ──────────────────────────────────────
        static void OnCreateEntity(Entity entity);
        static void OnUpdateEntity(Entity entity, Timestep ts);
        static void OnDestroyEntity(Entity entity);

        // ── 碰撞事件 ─────────────────────────────────────────────
        static void OnCollisionBegin(Entity entity, Entity other);
        static void OnCollisionEnd(Entity entity, Entity other);

        // ── 手动调用脚本方法（供 UIButton 等使用）──────────────────
        static void InvokeMethod(Entity entity, const std::string& methodName);

        // ── 编辑态字段暂存 ───────────────────────────────────────
        //
        //  GetScriptFieldMap: 返回某实体的字段暂存表，不存在则自动创建。
        //  HasScriptFieldMap: 判断某实体是否已有暂存数据（用于运行时初始化前检查）。
        static ScriptFieldMap& GetScriptFieldMap(Entity entity);
        static bool            HasScriptFieldMap(UUID entityID);

    private:
        static void InitMono();
        static void ShutdownMono();
        static void LoadEntityClasses();
    };

} // namespace Hazel