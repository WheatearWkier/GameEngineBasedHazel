#include "hzpch.h"
#include "ScriptGlue.h"

#include "Hazel/Scene/Scene.h"
#include "Hazel/Scene/Entity.h"
#include "Hazel/Scene/Components.h"
#include "Hazel/Core/Input.h"
#include "Hazel/Core/KeyCodes.h"
#include "Hazel/Audio/AudioEngine.h"
#include "ScriptEngine.h"

#include <box2d/b2_body.h>

#include <mono/jit/jit.h>
#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>

namespace Hazel {

#define HZ_ADD_INTERNAL_CALL(Name) \
    mono_add_internal_call("Hazel.InternalCalls::" #Name, (void*)Name)

    // =========================================================
    //  类型注册表
    //  用 std::unordered_map 代替一堆 if-else 字符串比较，
    //  新增组件只需在 RegisterFunctions() 末尾加一行 REGISTER 宏
    // =========================================================

    using HasComponentFn = bool(*)(Entity&);
    using AddComponentFn = void(*)(Entity&);

    static std::unordered_map<MonoType*, HasComponentFn> s_HasComponentFns;
    static std::unordered_map<MonoType*, AddComponentFn> s_AddComponentFns;

    // 注册辅助宏
#define REGISTER_COMPONENT(CppType, CSharpName)                                         \
    {                                                                                    \
        MonoType* mt = mono_reflection_type_from_name(                                  \
            (char*)(CSharpName), ScriptEngine::GetCoreAssemblyImage());                 \
        if (mt) {                                                                        \
            s_HasComponentFns[mt] = [](Entity& e) { return e.HasComponent<CppType>(); };\
            s_AddComponentFns[mt] = [](Entity& e) {                                     \
                if (!e.HasComponent<CppType>()) e.AddComponent<CppType>();              \
            };                                                                           \
        } else {                                                                         \
            HZ_CORE_WARN("REGISTER_COMPONENT: type '{}' not found", CSharpName);        \
        }                                                                                \
    }

    // =========================================================
    //  Entity
    // =========================================================

    static MonoString* Entity_GetTag(uint64_t entityID)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity)
            return mono_string_new(mono_domain_get(), "");
        return mono_string_new(mono_domain_get(),
            entity.GetComponent<TagComponent>().Tag.c_str());
    }

    static void Entity_Destroy(uint64_t entityID)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity) return;
        scene->DestroyEntity(entity);
    }

    static bool Entity_HasComponent(uint64_t entityID, MonoReflectionType* componentType)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity) return false;

        MonoType* monoType = mono_reflection_type_get_type(componentType);
        auto it = s_HasComponentFns.find(monoType);
        if (it != s_HasComponentFns.end())
            return it->second(entity);

        HZ_CORE_WARN("Entity_HasComponent: unregistered type '{}'",
            mono_type_get_name(monoType));
        return false;
    }

    static void Entity_AddComponent(uint64_t entityID, MonoReflectionType* componentType)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity) return;

        MonoType* monoType = mono_reflection_type_get_type(componentType);
        auto it = s_AddComponentFns.find(monoType);
        if (it != s_AddComponentFns.end())
        {
            it->second(entity);
            return;
        }

        HZ_CORE_WARN("Entity_AddComponent: unregistered type '{}'",
            mono_type_get_name(monoType));
    }

    // 在场景中按名字查找 Entity，返回其 UUID（找不到返回 0）
    // C# 端：ulong id = InternalCalls.Scene_FindEntityByName("Player");
    static uint64_t Scene_FindEntityByName(MonoString* name)
    {
        char* cStr = mono_string_to_utf8(name);
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByName(cStr);
        mono_free(cStr);
        if (!entity) return 0;
        return (uint64_t)entity.GetUUID();
    }

    // =========================================================
    //  Transform
    // =========================================================

    static void TransformComponent_GetTranslation(uint64_t entityID, glm::vec3* outTranslation)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        HZ_CORE_ASSERT(entity, "Entity not found!");
        *outTranslation = entity.GetComponent<TransformComponent>().Translation;
    }

    static void TransformComponent_SetTranslation(uint64_t entityID, glm::vec3* translation)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity) return;
        entity.GetComponent<TransformComponent>().Translation = *translation;
    }

    static void TransformComponent_GetRotation(uint64_t entityID, glm::vec3* outRotation)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity) return;
        *outRotation = entity.GetComponent<TransformComponent>().Rotation;
    }

    static void TransformComponent_SetRotation(uint64_t entityID, glm::vec3* rotation)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity) return;
        entity.GetComponent<TransformComponent>().Rotation = *rotation;
    }

    static void TransformComponent_GetScale(uint64_t entityID, glm::vec3* outScale)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity) return;
        *outScale = entity.GetComponent<TransformComponent>().Scale;
    }

    static void TransformComponent_SetScale(uint64_t entityID, glm::vec3* scale)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity) return;
        entity.GetComponent<TransformComponent>().Scale = *scale;
    }

    // =========================================================
    //  Rigidbody2D
    // =========================================================

    static void Rigidbody2DComponent_ApplyLinearImpulse(
        uint64_t entityID, glm::vec2* impulse, glm::vec2* point, bool wake)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<Rigidbody2DComponent>()) return;

        b2Body* body = static_cast<b2Body*>(
            entity.GetComponent<Rigidbody2DComponent>().RuntimeBody);
        if (!body) return;
        body->ApplyLinearImpulse(
            b2Vec2(impulse->x, impulse->y), b2Vec2(point->x, point->y), wake);
    }

    static void Rigidbody2DComponent_GetLinearVelocity(uint64_t entityID, glm::vec2* outVelocity)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<Rigidbody2DComponent>())
        {
            *outVelocity = {}; return;
        }

        b2Body* body = static_cast<b2Body*>(
            entity.GetComponent<Rigidbody2DComponent>().RuntimeBody);
        if (!body) { *outVelocity = {}; return; }

        const b2Vec2& vel = body->GetLinearVelocity();
        *outVelocity = { vel.x, vel.y };
    }

    static void Rigidbody2DComponent_SetLinearVelocity(uint64_t entityID, glm::vec2* velocity)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<Rigidbody2DComponent>()) return;

        b2Body* body = static_cast<b2Body*>(
            entity.GetComponent<Rigidbody2DComponent>().RuntimeBody);
        if (!body) return;
        body->SetLinearVelocity(b2Vec2(velocity->x, velocity->y));
    }

    // GravityScale —— 读取 / 设置重力缩放比例（0 = 不受重力）
    static float Rigidbody2DComponent_GetGravityScale(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<Rigidbody2DComponent>()) return 1.0f;

        b2Body* body = static_cast<b2Body*>(
            entity.GetComponent<Rigidbody2DComponent>().RuntimeBody);
        if (!body) return entity.GetComponent<Rigidbody2DComponent>().GravityScale;
        return body->GetGravityScale();
    }

    static void Rigidbody2DComponent_SetGravityScale(uint64_t entityID, float scale)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<Rigidbody2DComponent>()) return;

        auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
        rb2d.GravityScale = scale;   // 同时写回组件数据，以便序列化保存

        b2Body* body = static_cast<b2Body*>(rb2d.RuntimeBody);
        if (body) body->SetGravityScale(scale);
    }

    // FixedRotation —— 锁定旋转
    static bool Rigidbody2DComponent_GetFixedRotation(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<Rigidbody2DComponent>()) return false;
        return entity.GetComponent<Rigidbody2DComponent>().FixedRotation;
    }

    static void Rigidbody2DComponent_SetFixedRotation(uint64_t entityID, bool fixed)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<Rigidbody2DComponent>()) return;

        auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
        rb2d.FixedRotation = fixed;

        b2Body* body = static_cast<b2Body*>(rb2d.RuntimeBody);
        if (body) body->SetFixedRotation(fixed);
    }

    // BodyType
    static int Rigidbody2DComponent_GetBodyType(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<Rigidbody2DComponent>()) return 0;
        return static_cast<int>(entity.GetComponent<Rigidbody2DComponent>().Type);
    }

    static void Rigidbody2DComponent_SetBodyType(uint64_t entityID, int type)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<Rigidbody2DComponent>()) return;

        auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
        rb2d.Type = static_cast<Rigidbody2DComponent::BodyType>(type);

        b2Body* body = static_cast<b2Body*>(rb2d.RuntimeBody);
        if (body)
        {
            b2BodyType b2Type = b2_staticBody;
            switch (rb2d.Type)
            {
            case Rigidbody2DComponent::BodyType::Dynamic:   b2Type = b2_dynamicBody;   break;
            case Rigidbody2DComponent::BodyType::Kinematic: b2Type = b2_kinematicBody; break;
            default: break;
            }
            body->SetType(b2Type);
        }
    }

    // =========================================================
    //  Input
    // =========================================================

    static bool Input_IsKeyDown(int keycode)
    {
        return Input::IsKeyPressed(keycode);
    }

    // =========================================================
    //  SpriteRenderer
    // =========================================================
    static bool SpriteRendererComponent_GetFlipX(UUID entityID)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<SpriteRendererComponent>()) return false;
        return entity.GetComponent<SpriteRendererComponent>().FlipX;
    }

    static void SpriteRendererComponent_SetFlipX(UUID entityID, bool flipX)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<SpriteRendererComponent>()) return;
        entity.GetComponent<SpriteRendererComponent>().FlipX = flipX;
    }

    // =========================================================
    //  SpriteAnimator
    // =========================================================

    static void SpriteAnimatorComponent_Play(uint64_t entityID, MonoString* clipName)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<SpriteAnimatorComponent>()) return;
        char* cStr = mono_string_to_utf8(clipName);
        entity.GetComponent<SpriteAnimatorComponent>().Play(cStr);
        mono_free(cStr);
    }

    static void SpriteAnimatorComponent_Stop(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<SpriteAnimatorComponent>()) return;
        entity.GetComponent<SpriteAnimatorComponent>().IsPlaying = false;
    }

    static void SpriteAnimatorComponent_Resume(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<SpriteAnimatorComponent>()) return;
        entity.GetComponent<SpriteAnimatorComponent>().IsPlaying = true;
    }

    static MonoString* SpriteAnimatorComponent_GetCurrentClip(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<SpriteAnimatorComponent>())
            return mono_string_new(mono_domain_get(), "");
        const auto& name = entity.GetComponent<SpriteAnimatorComponent>().CurrentClipName;
        return mono_string_new(mono_domain_get(), name.c_str());
    }

    static bool SpriteAnimatorComponent_IsFinished(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<SpriteAnimatorComponent>()) return false;
        return entity.GetComponent<SpriteAnimatorComponent>().IsFinished;
    }

    // =========================================================
    //  Audio
    // =========================================================

    // 最简单的一次性播放，C# 里 Audio.PlaySound("path") 就走这里
    static void Audio_PlaySound(MonoString* filepath, float volume)
    {
        char* path = mono_string_to_utf8(filepath);
        AudioEngine::PlaySound(std::string(path), volume);
        mono_free(path);
    }

    // 播放并返回句柄，C# 里可以拿到 handle 再控制
    static uint32_t Audio_PlaySoundWithHandle(MonoString* filepath,
        float volume, bool loop)
    {
        char* path = mono_string_to_utf8(filepath);
        uint32_t handle = AudioEngine::PlaySoundWithHandle(
            std::string(path), volume, loop);
        mono_free(path);
        return handle;
    }

    static void Audio_StopSound(uint32_t handle)
    {
        AudioEngine::StopSound(handle);
    }

    static void Audio_PauseSound(uint32_t handle)
    {
        AudioEngine::PauseSound(handle);
    }

    static void Audio_ResumeSound(uint32_t handle)
    {
        AudioEngine::ResumeSound(handle);
    }

    static void Audio_SetVolume(uint32_t handle, float volume)
    {
        AudioEngine::SetVolume(handle, volume);
    }

    static bool Audio_IsPlaying(uint32_t handle)
    {
        return AudioEngine::IsPlaying(handle);
    }

    // ---- AudioSourceComponent 组件读写 ----
    static void AudioSourceComponent_Play(UUID entityID)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<AudioSourceComponent>()) return;

        auto& asc = entity.GetComponent<AudioSourceComponent>();
        if (asc.AudioFilePath.empty()) return;

        // 如果已经在播放，先停掉
        if (asc.RuntimeHandle != 0)
            AudioEngine::StopSound(asc.RuntimeHandle);

        asc.RuntimeHandle = AudioEngine::PlaySoundWithHandle(
            asc.AudioFilePath, asc.Volume, asc.Loop);
    }

    static void AudioSourceComponent_Stop(UUID entityID)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<AudioSourceComponent>()) return;

        auto& asc = entity.GetComponent<AudioSourceComponent>();
        if (asc.RuntimeHandle != 0)
        {
            AudioEngine::StopSound(asc.RuntimeHandle);
            asc.RuntimeHandle = 0;
        }
    }

    static bool AudioSourceComponent_IsPlaying(UUID entityID)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<AudioSourceComponent>()) return false;

        auto& asc = entity.GetComponent<AudioSourceComponent>();
        return AudioEngine::IsPlaying(asc.RuntimeHandle);
    }

    static float AudioSourceComponent_GetVolume(UUID entityID)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<AudioSourceComponent>()) return 1.0f;
        return entity.GetComponent<AudioSourceComponent>().Volume;
    }

    static void AudioSourceComponent_SetVolume(UUID entityID, float volume)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<AudioSourceComponent>()) return;

        auto& asc = entity.GetComponent<AudioSourceComponent>();
        asc.Volume = volume;
        // 如果正在播放，实时更新音量
        if (asc.RuntimeHandle != 0)
            AudioEngine::SetVolume(asc.RuntimeHandle, volume);
    }

    // =========================================================
    //  UI
    // =========================================================
    // ── UI: UIWidgetComponent ──────────────────────────────────────────────────

    static bool UIWidgetComponent_GetVisible(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIWidgetComponent>()) return false;
        return entity.GetComponent<UIWidgetComponent>().Visible;
    }

    static void UIWidgetComponent_SetVisible(uint64_t entityID, bool visible)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIWidgetComponent>()) return;
        entity.GetComponent<UIWidgetComponent>().Visible = visible;
    }

    static void UIWidgetComponent_GetPosition(uint64_t entityID, glm::vec2* outPos)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIWidgetComponent>()) return;
        *outPos = entity.GetComponent<UIWidgetComponent>().Position;
    }

    static void UIWidgetComponent_SetPosition(uint64_t entityID, glm::vec2* pos)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIWidgetComponent>()) return;
        entity.GetComponent<UIWidgetComponent>().Position = *pos;
    }

    static void UIWidgetComponent_GetSize(uint64_t entityID, glm::vec2* outSize)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIWidgetComponent>()) return;
        *outSize = entity.GetComponent<UIWidgetComponent>().Size;
    }

    static void UIWidgetComponent_SetSize(uint64_t entityID, glm::vec2* size)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIWidgetComponent>()) return;
        entity.GetComponent<UIWidgetComponent>().Size = *size;
    }

    // ── UI: UIImageComponent ───────────────────────────────────────────────────

    static void UIImageComponent_GetColor(uint64_t entityID, glm::vec4* outColor)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIImageComponent>()) return;
        *outColor = entity.GetComponent<UIImageComponent>().Color;
    }

    static void UIImageComponent_SetColor(uint64_t entityID, glm::vec4* color)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIImageComponent>()) return;
        entity.GetComponent<UIImageComponent>().Color = *color;
    }

    // ── UI: UIProgressBarComponent ────────────────────────────────────────────

    static float UIProgressBarComponent_GetValue(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIProgressBarComponent>()) return 0.0f;
        return entity.GetComponent<UIProgressBarComponent>().Value;
    }

    static void UIProgressBarComponent_SetValue(uint64_t entityID, float value)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIProgressBarComponent>()) return;
        entity.GetComponent<UIProgressBarComponent>().Value = value;
    }

    static float UIProgressBarComponent_GetMaxValue(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIProgressBarComponent>()) return 1.0f;
        return entity.GetComponent<UIProgressBarComponent>().MaxValue;
    }

    static void UIProgressBarComponent_SetMaxValue(uint64_t entityID, float maxValue)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIProgressBarComponent>()) return;
        entity.GetComponent<UIProgressBarComponent>().MaxValue = maxValue;
    }

    static void UIProgressBarComponent_GetForegroundColor(uint64_t entityID, glm::vec4* outColor)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIProgressBarComponent>()) return;
        *outColor = entity.GetComponent<UIProgressBarComponent>().ForegroundColor;
    }

    static void UIProgressBarComponent_SetForegroundColor(uint64_t entityID, glm::vec4* color)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIProgressBarComponent>()) return;
        entity.GetComponent<UIProgressBarComponent>().ForegroundColor = *color;
    }

    // ── UI: UIButtonComponent ─────────────────────────────────────────────────

    static bool UIButtonComponent_GetIsHovered(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIButtonComponent>()) return false;
        return entity.GetComponent<UIButtonComponent>().IsHovered;
    }

    static bool UIButtonComponent_GetIsPressed(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIButtonComponent>()) return false;
        return entity.GetComponent<UIButtonComponent>().IsPressed;
    }

    // Button点击事件的核心：C++检测到点击后，调用此函数触发C#回调
    // 这个函数由UIInputSystem调用，不是InternalCall

    static void UIButtonComponent_SetOnClickFunction(uint64_t entityID, MonoString* funcName)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIButtonComponent>()) return;
        char* cStr = mono_string_to_utf8(funcName);
        entity.GetComponent<UIButtonComponent>().OnClickFunction = cStr;
        mono_free(cStr);
    }

    static MonoString* UIButtonComponent_GetOnClickFunction(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UIButtonComponent>()) return nullptr;
        auto& func = entity.GetComponent<UIButtonComponent>().OnClickFunction;
        return mono_string_new(mono_domain_get(), func.c_str());
    }

    // ── UI: UITextComponent ───────────────────────────────────────────────────

    static MonoString* UITextComponent_GetText(uint64_t entityID)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UITextComponent>()) return nullptr;
        auto& text = entity.GetComponent<UITextComponent>().Text;
        return mono_string_new(mono_domain_get(), text.c_str());
    }

    static void UITextComponent_SetText(uint64_t entityID, MonoString* text)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UITextComponent>()) return;
        char* cStr = mono_string_to_utf8(text);
        entity.GetComponent<UITextComponent>().Text = cStr;
        mono_free(cStr);
    }

    static void UITextComponent_GetColor(uint64_t entityID, glm::vec4* outColor)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UITextComponent>()) return;
        *outColor = entity.GetComponent<UITextComponent>().Color;
    }

    static void UITextComponent_SetColor(uint64_t entityID, glm::vec4* color)
    {
        Entity entity = ScriptEngine::GetSceneContext()->GetEntityByUUID(entityID);
        if (!entity || !entity.HasComponent<UITextComponent>()) return;
        entity.GetComponent<UITextComponent>().Color = *color;
    }

    // =========================================================
    //  Scene
    // =========================================================

    static uint64_t Scene_InstantiateFromPrefab(MonoString* prefabPath, glm::vec3* position)
    {
        char* cStr = mono_string_to_utf8(prefabPath);
        std::filesystem::path path(cStr);
        mono_free(cStr);

        Scene* scene = ScriptEngine::GetSceneContext();
        Entity entity = scene->InstantiateFromPrefab(path, *position);
        if (!entity) return 0;
        return (uint64_t)entity.GetUUID();
    }

    // 跨脚本通信：通过 UUID 拿到另一个 Entity 的脚本实例（返回 object，C# 端强转）
    static MonoObject* Entity_GetScriptInstance(uint64_t entityID)
    {
        Ref<ScriptInstance> instance = ScriptEngine::GetEntityScriptInstance(entityID);
        if (!instance) return nullptr;
        return instance->GetMonoObject();
    }

    // =========================================================
    //  注册所有函数
    // =========================================================

    void ScriptGlue::RegisterFunctions()
    {
        // ── Entity ────────────────────────────────────────────
        HZ_ADD_INTERNAL_CALL(Entity_GetTag);
        HZ_ADD_INTERNAL_CALL(Entity_Destroy);
        HZ_ADD_INTERNAL_CALL(Entity_HasComponent);
        HZ_ADD_INTERNAL_CALL(Entity_AddComponent);
        HZ_ADD_INTERNAL_CALL(Entity_GetScriptInstance);

        // ── Scene ─────────────────────────────────────────────
        HZ_ADD_INTERNAL_CALL(Scene_FindEntityByName);
        HZ_ADD_INTERNAL_CALL(Scene_InstantiateFromPrefab);

        // ── Transform ─────────────────────────────────────────
        HZ_ADD_INTERNAL_CALL(TransformComponent_GetTranslation);
        HZ_ADD_INTERNAL_CALL(TransformComponent_SetTranslation);
        HZ_ADD_INTERNAL_CALL(TransformComponent_GetRotation);
        HZ_ADD_INTERNAL_CALL(TransformComponent_SetRotation);
        HZ_ADD_INTERNAL_CALL(TransformComponent_GetScale);
        HZ_ADD_INTERNAL_CALL(TransformComponent_SetScale);

        // ── Rigidbody2D ───────────────────────────────────────
        HZ_ADD_INTERNAL_CALL(Rigidbody2DComponent_ApplyLinearImpulse);
        HZ_ADD_INTERNAL_CALL(Rigidbody2DComponent_GetLinearVelocity);
        HZ_ADD_INTERNAL_CALL(Rigidbody2DComponent_SetLinearVelocity);
        HZ_ADD_INTERNAL_CALL(Rigidbody2DComponent_GetGravityScale);
        HZ_ADD_INTERNAL_CALL(Rigidbody2DComponent_SetGravityScale);
        HZ_ADD_INTERNAL_CALL(Rigidbody2DComponent_GetFixedRotation);
        HZ_ADD_INTERNAL_CALL(Rigidbody2DComponent_SetFixedRotation);
        HZ_ADD_INTERNAL_CALL(Rigidbody2DComponent_GetBodyType);
        HZ_ADD_INTERNAL_CALL(Rigidbody2DComponent_SetBodyType);

        // ── Input ─────────────────────────────────────────────
        HZ_ADD_INTERNAL_CALL(Input_IsKeyDown);

        // ── SpriteRenderer ────────────────────────────────────
        HZ_ADD_INTERNAL_CALL(SpriteRendererComponent_GetFlipX);
        HZ_ADD_INTERNAL_CALL(SpriteRendererComponent_SetFlipX);

        // ── SpriteAnimator ────────────────────────────────────
        HZ_ADD_INTERNAL_CALL(SpriteAnimatorComponent_Play);
        HZ_ADD_INTERNAL_CALL(SpriteAnimatorComponent_Stop);
        HZ_ADD_INTERNAL_CALL(SpriteAnimatorComponent_Resume);
        HZ_ADD_INTERNAL_CALL(SpriteAnimatorComponent_GetCurrentClip);
        HZ_ADD_INTERNAL_CALL(SpriteAnimatorComponent_IsFinished);

        // ── Audio ─────────────────────────────────────────────
        HZ_ADD_INTERNAL_CALL(Audio_PlaySound);
        HZ_ADD_INTERNAL_CALL(Audio_PlaySoundWithHandle);
        HZ_ADD_INTERNAL_CALL(Audio_StopSound);
        HZ_ADD_INTERNAL_CALL(Audio_PauseSound);
        HZ_ADD_INTERNAL_CALL(Audio_ResumeSound);
        HZ_ADD_INTERNAL_CALL(Audio_SetVolume);
        HZ_ADD_INTERNAL_CALL(Audio_IsPlaying);
        HZ_ADD_INTERNAL_CALL(AudioSourceComponent_Play);
        HZ_ADD_INTERNAL_CALL(AudioSourceComponent_Stop);
        HZ_ADD_INTERNAL_CALL(AudioSourceComponent_IsPlaying);
        HZ_ADD_INTERNAL_CALL(AudioSourceComponent_GetVolume);
        HZ_ADD_INTERNAL_CALL(AudioSourceComponent_SetVolume);

        // ── UI Components ─────────────────────────────────────────────────────────
        HZ_ADD_INTERNAL_CALL(UIWidgetComponent_GetVisible);
        HZ_ADD_INTERNAL_CALL(UIWidgetComponent_SetVisible);
        HZ_ADD_INTERNAL_CALL(UIWidgetComponent_GetPosition);
        HZ_ADD_INTERNAL_CALL(UIWidgetComponent_SetPosition);
        HZ_ADD_INTERNAL_CALL(UIWidgetComponent_GetSize);
        HZ_ADD_INTERNAL_CALL(UIWidgetComponent_SetSize);

        HZ_ADD_INTERNAL_CALL(UIImageComponent_GetColor);
        HZ_ADD_INTERNAL_CALL(UIImageComponent_SetColor);

        HZ_ADD_INTERNAL_CALL(UIProgressBarComponent_GetValue);
        HZ_ADD_INTERNAL_CALL(UIProgressBarComponent_SetValue);
        HZ_ADD_INTERNAL_CALL(UIProgressBarComponent_GetMaxValue);
        HZ_ADD_INTERNAL_CALL(UIProgressBarComponent_SetMaxValue);
        HZ_ADD_INTERNAL_CALL(UIProgressBarComponent_GetForegroundColor);
        HZ_ADD_INTERNAL_CALL(UIProgressBarComponent_SetForegroundColor);

        HZ_ADD_INTERNAL_CALL(UIButtonComponent_GetIsHovered);
        HZ_ADD_INTERNAL_CALL(UIButtonComponent_GetIsPressed);
        HZ_ADD_INTERNAL_CALL(UIButtonComponent_SetOnClickFunction);
        HZ_ADD_INTERNAL_CALL(UIButtonComponent_GetOnClickFunction);

        HZ_ADD_INTERNAL_CALL(UITextComponent_GetText);
        HZ_ADD_INTERNAL_CALL(UITextComponent_SetText);
        HZ_ADD_INTERNAL_CALL(UITextComponent_GetColor);
        HZ_ADD_INTERNAL_CALL(UITextComponent_SetColor);

        // ── 类型注册表（HasComponent / AddComponent 用）──────
        // 新增组件只需在这里加一行，无需修改 HasComponent/AddComponent 函数体
        REGISTER_COMPONENT(TransformComponent, "Hazel.TransformComponent");
        REGISTER_COMPONENT(Rigidbody2DComponent, "Hazel.Rigidbody2DComponent");
        REGISTER_COMPONENT(SpriteAnimatorComponent, "Hazel.SpriteAnimatorComponent");
        REGISTER_COMPONENT(SpriteRendererComponent, "Hazel.SpriteRendererComponent");
        REGISTER_COMPONENT(CircleRendererComponent, "Hazel.CircleRendererComponent");
        REGISTER_COMPONENT(BoxCollider2DComponent, "Hazel.BoxCollider2DComponent");
        REGISTER_COMPONENT(CircleCollider2DComponent, "Hazel.CircleCollider2DComponent");
        REGISTER_COMPONENT(CameraComponent, "Hazel.CameraComponent");
        REGISTER_COMPONENT(UICanvasComponent, "Hazel.UICanvasComponent");
        REGISTER_COMPONENT(UIWidgetComponent, "Hazel.UIWidgetComponent");
        REGISTER_COMPONENT(UIImageComponent, "Hazel.UIImageComponent");
        REGISTER_COMPONENT(UITextComponent, "Hazel.UITextComponent");
        REGISTER_COMPONENT(UIButtonComponent, "Hazel.UIButtonComponent");
        REGISTER_COMPONENT(UIProgressBarComponent, "Hazel.UIProgressBarComponent");
        REGISTER_COMPONENT(AudioSourceComponent, "Hazel.AudioSourceComponent");
    }

} // namespace Hazel
