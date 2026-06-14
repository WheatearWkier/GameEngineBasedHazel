#include "hzpch.h"
#include "ScriptEngine.h"
#include "ScriptGlue.h"

#include "Hazel/Scene/Scene.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/Components.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/object.h>
#include <mono/metadata/tabledefs.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/threads.h>

namespace Hazel {

    // ════════════════════════════════════════════════════════════
    //  内部数据结构
    // ════════════════════════════════════════════════════════════

    struct ScriptEngineData
    {
        // ── Mono 运行时 ──────────────────────────────────────────
        MonoDomain* RootDomain = nullptr;
        MonoDomain* AppDomain = nullptr;
        MonoAssembly* CoreAssembly = nullptr;
        MonoImage* CoreAssemblyImage = nullptr;

        // ── 脚本类注册表 ─────────────────────────────────────────
        // key = 完整类名（如 "MyGame.PlayerController"）
        std::unordered_map<std::string, Ref<ScriptClass>> EntityClasses;

        // ── 运行时状态 ───────────────────────────────────────────
        Scene* SceneContext = nullptr;
        std::unordered_map<UUID, Ref<ScriptInstance>> EntityInstances;
    };

    static ScriptEngineData* s_Data = nullptr;

    // 编辑态字段暂存表（运行时也保留，Stop 后继续显示上次的值）
    // key = 实体 UUID，value = { 字段名 → ScriptFieldInstance }
    static std::unordered_map<UUID, ScriptFieldMap> s_EntityScriptFields;

    // ════════════════════════════════════════════════════════════
    //  内部工具函数
    // ════════════════════════════════════════════════════════════

    static MonoAssembly* LoadMonoAssembly(const std::filesystem::path& filepath)
    {
        std::ifstream stream(filepath, std::ios::binary | std::ios::ate);
        HZ_CORE_ASSERT(stream, "Failed to open assembly file: {}", filepath.string());

        auto end = stream.tellg();
        stream.seekg(0, std::ios::beg);
        uint32_t size = static_cast<uint32_t>(end - stream.tellg());

        std::vector<char> buffer(size);
        stream.read(buffer.data(), size);
        stream.close();

        MonoImageOpenStatus status;
        MonoImage* image = mono_image_open_from_data_full(
            buffer.data(), size, true, &status, false);
        HZ_CORE_ASSERT(status == MONO_IMAGE_OK, mono_image_strerror(status));

        MonoAssembly* assembly = mono_assembly_load_from_full(
            image, filepath.string().c_str(), &status, false);
        mono_image_close(image);
        return assembly;
    }

    // 在继承链上精确查找"第一个参数类型为 ulong (MONO_TYPE_U8)"的方法。
    // 用于区分 Entity 基类里的 OnCollisionEnter(ulong id) 和用户可能重写的
    // OnCollisionEnter(Entity e) 两个重载，避免错误调用。
    static MonoMethod* FindMethodWithULongParam(MonoClass* klass, const char* methodName)
    {
        for (MonoClass* current = klass; current; current = mono_class_get_parent(current))
        {
            void* iter = nullptr;
            MonoMethod* method = nullptr;
            while ((method = mono_class_get_methods(current, &iter)) != nullptr)
            {
                if (strcmp(mono_method_get_name(method), methodName) != 0)
                    continue;

                MonoMethodSignature* sig = mono_method_signature(method);
                if (mono_signature_get_param_count(sig) != 1)
                    continue;

                void* paramIter = nullptr;
                MonoType* paramType = mono_signature_get_params(sig, &paramIter);
                if (mono_type_get_type(paramType) == MONO_TYPE_U8)
                    return method;
            }
        }
        return nullptr;
    }

    // MonoType 名称 → ScriptFieldType 的映射
    static ScriptFieldType MonoTypeToScriptFieldType(MonoType* monoType)
    {
        static const std::unordered_map<std::string, ScriptFieldType> s_TypeMap =
        {
            { "System.Single",  ScriptFieldType::Float   },
            { "System.Double",  ScriptFieldType::Double  },
            { "System.Boolean", ScriptFieldType::Bool    },
            { "System.Char",    ScriptFieldType::Char    },
            { "System.Int16",   ScriptFieldType::Short   },
            { "System.Int32",   ScriptFieldType::Int     },
            { "System.Int64",   ScriptFieldType::Long    },
            { "Hazel.Vector2",  ScriptFieldType::Vector2 },
            { "Hazel.Vector3",  ScriptFieldType::Vector3 },
        };

        std::string typeName = mono_type_get_name(monoType);
        auto it = s_TypeMap.find(typeName);
        return it != s_TypeMap.end() ? it->second : ScriptFieldType::None;
    }

    // ════════════════════════════════════════════════════════════
    //  ScriptEngine：引擎初始化 / 关闭
    // ════════════════════════════════════════════════════════════

    void ScriptEngine::Init()
    {
        s_Data = new ScriptEngineData();
        InitMono();
        LoadAssembly("assets/scripts/Hazel-ScriptCore.dll");
        ScriptGlue::RegisterFunctions();
    }

    void ScriptEngine::Shutdown()
    {
        ShutdownMono();
        delete s_Data;
        s_Data = nullptr;
    }

    void ScriptEngine::InitMono()
    {
        mono_set_assemblies_path("mono/lib");

        MonoDomain* rootDomain = mono_jit_init("HazelJITRuntime");
        HZ_CORE_ASSERT(rootDomain, "Failed to initialize Mono JIT");
        s_Data->RootDomain = rootDomain;
    }

    void ScriptEngine::ShutdownMono()
    {
        if (s_Data->AppDomain)
        {
            mono_domain_set(mono_get_root_domain(), false);
            mono_domain_unload(s_Data->AppDomain);
            s_Data->AppDomain = nullptr;
        }
        mono_jit_cleanup(s_Data->RootDomain);
        s_Data->RootDomain = nullptr;
    }

    void ScriptEngine::LoadAssembly(const std::filesystem::path& filepath)
    {
        s_Data->AppDomain = mono_domain_create_appdomain(
            const_cast<char*>("HazelScriptRuntime"), nullptr);
        mono_domain_set(s_Data->AppDomain, true);

        s_Data->CoreAssembly = LoadMonoAssembly(filepath);
        s_Data->CoreAssemblyImage = mono_assembly_get_image(s_Data->CoreAssembly);

        LoadEntityClasses();
    }

    // ════════════════════════════════════════════════════════════
    //  ScriptEngine：场景生命周期
    // ════════════════════════════════════════════════════════════

    void ScriptEngine::OnRuntimeStart(Scene* scene)
    {
        s_Data->SceneContext = scene;
    }

    void ScriptEngine::OnRuntimeStop()
    {
        s_Data->SceneContext = nullptr;
        s_Data->EntityInstances.clear();
        // 注意：不清除 s_EntityScriptFields。
        // Stop 后回到编辑态，Inspector 里的值应该还在。
    }

    // ════════════════════════════════════════════════════════════
    //  ScriptEngine：Entity 生命周期
    // ════════════════════════════════════════════════════════════

    void ScriptEngine::OnCreateEntity(Entity entity)
    {
        const auto& sc = entity.GetComponent<ScriptComponent>();
        if (!EntityClassExists(sc.ClassName))
            return;

        UUID entityID = entity.GetUUID();

        // 1. 创建实例
        Ref<ScriptClass>    scriptClass = GetEntityClass(sc.ClassName);
        Ref<ScriptInstance> instance = CreateRef<ScriptInstance>(scriptClass, entity);
        s_Data->EntityInstances[entityID] = instance;

        // 2. 将编辑态填写的字段值注入到刚创建的实例里
        auto fieldsIt = s_EntityScriptFields.find(entityID);
        if (fieldsIt != s_EntityScriptFields.end())
        {
            for (const auto& [name, fieldInst] : fieldsIt->second)
            {
                switch (fieldInst.Field.Type)
                {
                case ScriptFieldType::Float:
                    instance->SetFieldValue<float>(name, fieldInst.GetValue<float>());
                    break;
                case ScriptFieldType::Double:
                    instance->SetFieldValue<double>(name, fieldInst.GetValue<double>());
                    break;
                case ScriptFieldType::Bool:
                    instance->SetFieldValue<bool>(name, fieldInst.GetValue<bool>());
                    break;
                case ScriptFieldType::Int:
                    instance->SetFieldValue<int>(name, fieldInst.GetValue<int>());
                    break;
                case ScriptFieldType::Long:
                    instance->SetFieldValue<int64_t>(name, fieldInst.GetValue<int64_t>());
                    break;
                case ScriptFieldType::Vector2:
                    instance->SetFieldValue<glm::vec2>(name, fieldInst.GetValue<glm::vec2>());
                    break;
                case ScriptFieldType::Vector3:
                    instance->SetFieldValue<glm::vec3>(name, fieldInst.GetValue<glm::vec3>());
                    break;
                case ScriptFieldType::Vector4:
                    instance->SetFieldValue<glm::vec4>(name, fieldInst.GetValue<glm::vec4>());
                    break;
                default:
                    break;
                }
            }
        }

        // 3. 调用脚本的 OnCreate
        instance->InvokeOnCreate();
    }

    void ScriptEngine::OnUpdateEntity(Entity entity, Timestep ts)
    {
        UUID uuid = entity.GetUUID();
        auto it = s_Data->EntityInstances.find(uuid);
        if (it == s_Data->EntityInstances.end())
        {
            HZ_CORE_ERROR("ScriptEngine::OnUpdateEntity - no instance for UUID {}", (uint64_t)uuid);
            return;
        }
        it->second->InvokeOnUpdate(ts);
    }

    void ScriptEngine::OnDestroyEntity(Entity entity)
    {
        s_Data->EntityInstances.erase(entity.GetUUID());
    }

    // ════════════════════════════════════════════════════════════
    //  ScriptEngine：碰撞事件
    // ════════════════════════════════════════════════════════════

    void ScriptEngine::OnCollisionBegin(Entity entity, Entity other)
    {
        if (!entity.HasComponent<ScriptComponent>()) return;

        auto it = s_Data->EntityInstances.find(entity.GetUUID());
        if (it == s_Data->EntityInstances.end()) return;

        it->second->InvokeOnCollisionEnter(other);
    }

    void ScriptEngine::OnCollisionEnd(Entity entity, Entity other)
    {
        if (!entity.HasComponent<ScriptComponent>()) return;

        auto it = s_Data->EntityInstances.find(entity.GetUUID());
        if (it == s_Data->EntityInstances.end()) return;

        it->second->InvokeOnCollisionExit(other);
    }

    // ════════════════════════════════════════════════════════════
    //  ScriptEngine：手动调用脚本方法（供 UIButton 等使用）
    // ════════════════════════════════════════════════════════════

    void ScriptEngine::InvokeMethod(Entity entity, const std::string& methodName)
    {
        UUID id = entity.GetUUID();
        auto it = s_Data->EntityInstances.find(id);
        if (it == s_Data->EntityInstances.end()) return;

        Ref<ScriptInstance> instance = it->second;
        MonoClass* klass = instance->GetMonoClass();

        MonoMethod* method = mono_class_get_method_from_name(klass, methodName.c_str(), 0);
        if (!method)
        {
            HZ_CORE_WARN("ScriptEngine::InvokeMethod - '{}' not found on script", methodName);
            return;
        }

        MonoObject* exception = nullptr;
        mono_runtime_invoke(method, instance->GetMonoObject(), nullptr, &exception);
        if (exception)
            HZ_CORE_ERROR("ScriptEngine::InvokeMethod - exception in '{}'", methodName);
    }

    // ════════════════════════════════════════════════════════════
    //  ScriptEngine：编辑态字段暂存
    // ════════════════════════════════════════════════════════════

    ScriptFieldMap& ScriptEngine::GetScriptFieldMap(Entity entity)
    {
        // operator[] 在 key 不存在时会自动插入一个空 map，正好是我们想要的行为
        return s_EntityScriptFields[entity.GetUUID()];
    }

    bool ScriptEngine::HasScriptFieldMap(UUID entityID)
    {
        return s_EntityScriptFields.count(entityID) > 0;
    }

    // ════════════════════════════════════════════════════════════
    //  ScriptEngine：查询接口
    // ════════════════════════════════════════════════════════════

    bool ScriptEngine::EntityClassExists(const std::string& fullClassName)
    {
        return s_Data->EntityClasses.count(fullClassName) > 0;
    }

    Ref<ScriptClass> ScriptEngine::GetEntityClass(const std::string& fullClassName)
    {
        auto it = s_Data->EntityClasses.find(fullClassName);
        return it != s_Data->EntityClasses.end() ? it->second : nullptr;
    }

    Ref<ScriptInstance> ScriptEngine::GetEntityScriptInstance(UUID entityID)
    {
        auto it = s_Data->EntityInstances.find(entityID);
        return it != s_Data->EntityInstances.end() ? it->second : nullptr;
    }

    Scene* ScriptEngine::GetSceneContext() { return s_Data->SceneContext; }
    MonoImage* ScriptEngine::GetCoreAssemblyImage() { return s_Data->CoreAssemblyImage; }

    // ════════════════════════════════════════════════════════════
    //  ScriptEngine：程序集扫描
    // ════════════════════════════════════════════════════════════

    void ScriptEngine::LoadEntityClasses()
    {
        s_Data->EntityClasses.clear();

        MonoImage* image = s_Data->CoreAssemblyImage;
        MonoClass* entityClass = mono_class_from_name(image, "Hazel", "Entity");

        const MonoTableInfo* table = mono_image_get_table_info(image, MONO_TABLE_TYPEDEF);
        int32_t              numRows = mono_table_info_get_rows(table);

        for (int32_t i = 0; i < numRows; i++)
        {
            uint32_t cols[MONO_TYPEDEF_SIZE];
            mono_metadata_decode_row(table, i, cols, MONO_TYPEDEF_SIZE);

            const char* ns = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAMESPACE]);
            const char* name = mono_metadata_string_heap(image, cols[MONO_TYPEDEF_NAME]);

            MonoClass* monoClass = mono_class_from_name(image, ns, name);
            if (!monoClass || monoClass == entityClass)
                continue;
            if (!mono_class_is_subclass_of(monoClass, entityClass, false))
                continue;

            std::string fullName = (strlen(ns) > 0)
                ? std::string(ns) + "." + name
                : name;

            s_Data->EntityClasses[fullName] = CreateRef<ScriptClass>(ns, name);
            HZ_CORE_TRACE("ScriptEngine: found class '{}'", fullName);
        }
    }

    // ════════════════════════════════════════════════════════════
    //  ScriptClass 实现
    // ════════════════════════════════════════════════════════════

    ScriptClass::ScriptClass(const std::string& classNamespace, const std::string& className)
        : m_ClassNamespace(classNamespace), m_ClassName(className)
    {
        m_MonoClass = mono_class_from_name(
            s_Data->CoreAssemblyImage,
            classNamespace.c_str(),
            className.c_str());

        // 扫描所有 public 实例字段
        void* iter = nullptr;
        while (MonoClassField* field = mono_class_get_fields(m_MonoClass, &iter))
        {
            if (!(mono_field_get_flags(field) & FIELD_ATTRIBUTE_PUBLIC))
                continue;

            const char* fieldName = mono_field_get_name(field);
            MonoType* fieldType = mono_field_get_type(field);

            ScriptFieldType type = MonoTypeToScriptFieldType(fieldType);
            if (type == ScriptFieldType::None)
                continue;  // 忽略不支持的类型（如 string、自定义类等）

            m_Fields[fieldName] = { type, fieldName, field };
        }
    }

    MonoObject* ScriptClass::Instantiate()
    {
        // 只分配内存，不调用 C# 构造函数（由 ScriptInstance 手动调用基类 .ctor）
        return mono_object_new(s_Data->AppDomain, m_MonoClass);
    }

    MonoMethod* ScriptClass::GetMethod(const std::string& name, int parameterCount)
    {
        // 沿继承链向上查找，确保能找到定义在基类里的方法
        for (MonoClass* klass = m_MonoClass; klass; klass = mono_class_get_parent(klass))
        {
            MonoMethod* method = mono_class_get_method_from_name(
                klass, name.c_str(), parameterCount);
            if (method)
                return method;
        }
        return nullptr;
    }

    MonoObject* ScriptClass::InvokeMethod(MonoObject* instance, MonoMethod* method, void** params)
    {
        MonoObject* exception = nullptr;
        MonoObject* result = mono_runtime_invoke(method, instance, params, &exception);
        if (exception)
        {
            MonoString* msg = mono_object_to_string(exception, nullptr);
            HZ_CORE_ERROR("C# Exception in {}.{}: {}",
                m_ClassNamespace, m_ClassName, mono_string_to_utf8(msg));
        }
        return result;
    }

    // ════════════════════════════════════════════════════════════
    //  ScriptInstance 实现
    // ════════════════════════════════════════════════════════════

    ScriptInstance::ScriptInstance(Ref<ScriptClass> scriptClass, Entity entity)
        : m_ScriptClass(scriptClass)
    {
        m_Instance = scriptClass->Instantiate();

        // 查找生命周期方法
        m_OnCreateMethod = scriptClass->GetMethod("OnCreate", 0);
        m_OnUpdateMethod = scriptClass->GetMethod("OnUpdate", 1);

        // 精确查找接收 ulong 的碰撞回调（定义在 Entity 基类里）
        // 避免与用户可能重写的 OnCollisionEnter(Entity) 混淆
        m_OnCollisionEnterMethod = FindMethodWithULongParam(
            scriptClass->GetMonoClass(), "OnCollisionEnter");
        m_OnCollisionExitMethod = FindMethodWithULongParam(
            scriptClass->GetMonoClass(), "OnCollisionExit");

        if (!m_OnCollisionEnterMethod)
            HZ_CORE_WARN("ScriptInstance: OnCollisionEnter(ulong) not found in '{}'",
                scriptClass->GetName());

        // 调用 Entity 基类的 .ctor(ulong id)，初始化 this.ID
        MonoClass* entityClass = mono_class_from_name(s_Data->CoreAssemblyImage, "Hazel", "Entity");
        MonoMethod* constructor = mono_class_get_method_from_name(entityClass, ".ctor", 1);
        HZ_CORE_ASSERT(constructor, "Entity .ctor(ulong) not found in core assembly!");

        UUID  entityID = entity.GetUUID();
        void* ctorParam = &entityID;
        mono_runtime_invoke(constructor, m_Instance, &ctorParam, nullptr);
    }

    void ScriptInstance::InvokeOnCreate()
    {
        if (m_OnCreateMethod)
            m_ScriptClass->InvokeMethod(m_Instance, m_OnCreateMethod);
    }

    void ScriptInstance::InvokeOnUpdate(float ts)
    {
        if (m_OnUpdateMethod)
        {
            void* param = &ts;
            m_ScriptClass->InvokeMethod(m_Instance, m_OnUpdateMethod, &param);
        }
    }

    void ScriptInstance::InvokeOnCollisionEnter(Entity other)
    {
        if (!m_OnCollisionEnterMethod) return;
        UUID  otherID = other.GetUUID();
        void* param = &otherID;
        m_ScriptClass->InvokeMethod(m_Instance, m_OnCollisionEnterMethod, &param);
    }

    void ScriptInstance::InvokeOnCollisionExit(Entity other)
    {
        if (!m_OnCollisionExitMethod) return;
        UUID  otherID = other.GetUUID();
        void* param = &otherID;
        m_ScriptClass->InvokeMethod(m_Instance, m_OnCollisionExitMethod, &param);
    }

    bool ScriptInstance::GetFieldValueInternal(const std::string& name, void* outBuffer)
    {
        const auto& fields = m_ScriptClass->GetFields();
        auto it = fields.find(name);
        if (it == fields.end())
            return false;
        mono_field_get_value(m_Instance, it->second.ClassField, outBuffer);
        return true;
    }

    bool ScriptInstance::SetFieldValueInternal(const std::string& name, const void* value)
    {
        const auto& fields = m_ScriptClass->GetFields();
        auto it = fields.find(name);
        if (it == fields.end())
            return false;
        mono_field_set_value(m_Instance, it->second.ClassField, const_cast<void*>(value));
        return true;
    }

} // namespace Hazel