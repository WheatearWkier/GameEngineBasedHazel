#pragma once

// GLM_ENABLE_EXPERIMENTAL 必须在包含 gtx 头文件之前定义
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Hazel/Core/UUID.h"
#include "Hazel/Renderer/SceneCamera.h"
#include "Hazel/Renderer/Texture.h"
#include "Hazel/Renderer/Mesh.h"
#include "Hazel/Renderer/Material.h"
#include "Hazel/Renderer/Renderer3D.h"

#include "Hazel/Animation/AnimationClip.h"
#include "Hazel/Audio/AudioEngine.h"

namespace Hazel {

    // ═══════════════════════════════════════════════════════
    //  基础组件
    // ═══════════════════════════════════════════════════════

    struct IDComponent
    {
        UUID ID;

        IDComponent() = default;
        IDComponent(const IDComponent&) = default;
        IDComponent(const UUID& id) : ID(id) {}  // 修复：Scene.cpp 编译错误的根源
    };

    struct TagComponent
    {
        std::string Tag;

        TagComponent() = default;
        TagComponent(const TagComponent&) = default;
        TagComponent(const std::string& tag) : Tag(tag) {}
    };

    struct TransformComponent
    {
        glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
        glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };
        glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

        TransformComponent() = default;
        TransformComponent(const TransformComponent&) = default;
        TransformComponent(const glm::vec3& translation) : Translation(translation) {}

        glm::mat4 GetTransform() const
        {
            return glm::translate(glm::mat4(1.0f), Translation)
                * glm::toMat4(glm::quat(Rotation))
                * glm::scale(glm::mat4(1.0f), Scale);
        }
    };

    // ═══════════════════════════════════════════════════════
    //  渲染组件
    // ═══════════════════════════════════════════════════════

    struct SpriteRendererComponent
    {
        glm::vec4      Color{ 1.0f, 1.0f, 1.0f, 1.0f };
        Ref<Texture2D> Texture;
        float          TilingFactor = 1.0f;

        // 动画系统写入这两个字段，默认是整张贴图
        glm::vec2      UVMin = { 0.0f, 0.0f };
        glm::vec2      UVMax = { 1.0f, 1.0f };
        bool           FlipX = false;

        SpriteRendererComponent() = default;
        SpriteRendererComponent(const SpriteRendererComponent&) = default;
        SpriteRendererComponent(const glm::vec4& color) : Color(color) {}
    };

    struct CircleRendererComponent
    {
        glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
        float     Thickness = 1.0f;
        float     Fade = 0.005f;

        CircleRendererComponent() = default;
        CircleRendererComponent(const CircleRendererComponent&) = default;
    };

    // ═══════════════════════════════════════════════════════
    //  相机组件
    // ═══════════════════════════════════════════════════════

    struct CameraComponent
    {
        SceneCamera Camera;
        bool        Primary = true;
        bool        FixedAspectRatio = false;

        CameraComponent() = default;
        CameraComponent(const CameraComponent&) = default;
    };

    // ═══════════════════════════════════════════════════════
    //  脚本组件
    // ═══════════════════════════════════════════════════════

    class ScriptableEntity;

    // C++原生脚本组件
    struct NativeScriptComponent
    {
        ScriptableEntity* Instance = nullptr;

        // 函数指针初始化为 nullptr，防止未绑定时调用崩溃
        ScriptableEntity* (*InstantiateScript)() = nullptr;
        void              (*DestroyScript)(NativeScriptComponent*) = nullptr;

        template<typename T>
        void Bind()
        {
            InstantiateScript = []() -> ScriptableEntity*
                {
                    return static_cast<ScriptableEntity*>(new T());
                };
            DestroyScript = [](NativeScriptComponent* nsc)
                {
                    delete nsc->Instance;
                    nsc->Instance = nullptr;
                };
        }
    };

    // C#脚本组件
    struct ScriptComponent
    {
        std::string ClassName;

        ScriptComponent() = default;
        ScriptComponent(const ScriptComponent&) = default;
    };

    // ═══════════════════════════════════════════════════════
    //  物理组件
    // ═══════════════════════════════════════════════════════

    struct Rigidbody2DComponent
    {
        enum class BodyType { Static = 0, Dynamic, Kinematic };

        BodyType Type = BodyType::Static;
        bool     FixedRotation = false;
        float    GravityScale = 1.0f;
        void*    RuntimeBody = nullptr;   // 运行时由 Box2D 填充，不参与序列化

        Rigidbody2DComponent() = default;
        Rigidbody2DComponent(const Rigidbody2DComponent&) = default;
    };

    struct BoxCollider2DComponent
    {
        glm::vec2 Offset = { 0.0f, 0.0f };
        glm::vec2 Size = { 0.5f, 0.5f };
        float     Density = 1.0f;
        float     Friction = 0.5f;
        float     Restitution = 0.0f;
        float     RestitutionThreshold = 0.5f;
        void* RuntimeFixture = nullptr;   // 运行时由 Box2D 填充

        BoxCollider2DComponent() = default;
        BoxCollider2DComponent(const BoxCollider2DComponent&) = default;
    };

    struct CircleCollider2DComponent
    {
        glm::vec2 Offset = { 0.0f, 0.0f };
        float     Radius = 0.5f;
        float     Density = 1.0f;
        float     Friction = 0.5f;
        float     Restitution = 0.0f;
        float     RestitutionThreshold = 0.5f;
        void* RuntimeFixture = nullptr;   // 运行时由 Box2D 填充

        CircleCollider2DComponent() = default;
        CircleCollider2DComponent(const CircleCollider2DComponent&) = default;
    };

    // 3D Mesh 渲染组件
    struct MeshRendererComponent
    {
        Ref<Mesh>     Mesh;
        Ref<Material> Material;   // ← 从 MeshMaterial 换成 Ref<Material>

        MeshRendererComponent()
        {
            Material = Material::Create();  // 默认创建一个空材质
        }
        MeshRendererComponent(const MeshRendererComponent&) = default;
    };

    // 平行光：模拟太阳，方向由 Entity 的 Rotation 决定，不衰减
    struct DirectionalLightComponent
    {
        glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
        float     Intensity = 1.0f;

        DirectionalLightComponent() = default;
        DirectionalLightComponent(const DirectionalLightComponent&) = default;
    };

    // 点光源：有位置，强度随距离衰减
    struct PointLightComponent
    {
        glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
        float     Intensity = 1.0f;
        float     Constant = 1.0f;
        float     Linear = 0.09f;
        float     Quadratic = 0.032f;

        PointLightComponent() = default;
        PointLightComponent(const PointLightComponent&) = default;
    };

    // ═══════════════════════════════════════════════════════
    //  动画控制器组件，管理多个AnimationClip
    // ═══════════════════════════════════════════════════════
    struct SpriteAnimatorComponent
    {
        // 已注册的所有 clip，key 是 clip 名字
        std::unordered_map<std::string, Ref<AnimationClip>> Clips;
        std::string DefaultClipName;
        bool PlayOnStart = true;

        // --- 运行时状态（不需要序列化）---
        std::string  CurrentClipName;
        int          CurrentFrameIndex = 0;
        float        ElapsedTime = 0.0f;   // 当前帧已过时间
        bool         IsPlaying = false;
        bool         IsFinished = false;  // 非循环动画播完后为 true

        SpriteAnimatorComponent() = default;
        SpriteAnimatorComponent(const SpriteAnimatorComponent&) = default;

        // 注册一个 clip
        void AddClip(const Ref<AnimationClip>& clip)
        {
            Clips[clip->GetName()] = clip;
        }

        // 切换到指定 clip（名字相同时不重置，避免同帧多次调用抖动）
        void Play(const std::string& clipName)
        {
            if (CurrentClipName == clipName) return;
            auto it = Clips.find(clipName);
            if (it == Clips.end()) return;

            CurrentClipName = clipName;
            CurrentFrameIndex = 0;
            ElapsedTime = 0.0f;
            IsPlaying = true;
            IsFinished = false;
        }

        // 获取当前正在播放的 Clip（可能为 nullptr）
        Ref<AnimationClip> GetCurrentClip() const
        {
            auto it = Clips.find(CurrentClipName);
            return (it != Clips.end()) ? it->second : nullptr;
        }

        // 获取当前帧（可能为 nullptr）
        const AnimationFrame* GetCurrentFrame() const
        {
            auto clip = GetCurrentClip();
            if (!clip || clip->GetFrameCount() == 0) return nullptr;
            return &clip->GetFrames()[CurrentFrameIndex];
        }
    };

    // ─────────────────────────────────────────────
    //  UI System Components
    // ─────────────────────────────────────────────

    // UI锚点枚举，决定UI元素相对屏幕的对齐方式
    enum class UIAnchor
    {
        TopLeft, TopCenter, TopRight,
        MiddleLeft, MiddleCenter, MiddleRight,
        BottomLeft, BottomCenter, BottomRight
    };

    // ── Canvas：挂在一个根Entity上，代表整个UI层 ──
    struct UICanvasComponent
    {
        bool  Visible = true;
        // 参考分辨率，用于把归一化坐标缩放到实际像素
        float ReferenceWidth = 1920.0f;
        float ReferenceHeight = 1080.0f;

        UICanvasComponent() = default;
        UICanvasComponent(const UICanvasComponent&) = default;
    };

    // ── UIWidget：所有UI元素共用的Transform信息 ──
    // 坐标含义：相对于Canvas的归一化坐标 (0~1)
    // 例如 Position={0.5, 0.5} = 屏幕正中心
    struct UIWidgetComponent
    {
        bool      Visible = true;
        glm::vec2 Position = { 0.5f, 0.5f };  // 归一化屏幕坐标
        glm::vec2 Size = { 0.1f, 0.05f }; // 归一化尺寸
        float     Rotation = 0.0f;
        UIAnchor  Anchor = UIAnchor::MiddleCenter;
        int       SortOrder = 0; // 越大越靠前

        UIWidgetComponent() = default;
        UIWidgetComponent(const UIWidgetComponent&) = default;
    };

    // ── UIImage：纯色矩形 或 贴图 ──────────────────
    struct UIImageComponent
    {
        glm::vec4      Color = { 1.0f, 1.0f, 1.0f, 1.0f };
        Ref<Texture2D> Texture = nullptr;

        UIImageComponent() = default;
        UIImageComponent(const UIImageComponent&) = default;
    };

    // ── UIText：屏幕文字 ────────────────────────────
    // 注意：这里先用一个简单的占位实现
    // 后续Step 2会接入真正的字体渲染
    struct UITextComponent
    {
        std::string Text = "Text";
        glm::vec4   Color = { 1.0f, 1.0f, 1.0f, 1.0f };
        float       FontSize = 24.0f;
        // 后续接入字体时填这个
        // Ref<Font>  Font = nullptr;

        UITextComponent() = default;
        UITextComponent(const UITextComponent&) = default;
        explicit UITextComponent(const std::string& text) : Text(text) {}
    };

    // ── UIButton：可点击的区域 + 颜色状态 ───────────
    struct UIButtonComponent
    {
        glm::vec4 NormalColor = { 0.3f, 0.3f, 0.8f, 1.0f };
        glm::vec4 HoverColor = { 0.4f, 0.4f, 1.0f, 1.0f };
        glm::vec4 PressedColor = { 0.2f, 0.2f, 0.6f, 1.0f };

        // 运行时状态（不序列化）
        bool IsHovered = false;
        bool IsPressed = false;

        // C#回调函数名（ScriptEngine会用到）
        std::string OnClickFunction = "";

        UIButtonComponent() = default;
        UIButtonComponent(const UIButtonComponent&) = default;
    };

    // ── UIProgressBar：血条/进度条 ──────────────────
    struct UIProgressBarComponent
    {
        float     Value = 1.0f;  // 0.0 ~ 1.0
        float     MaxValue = 1.0f;
        glm::vec4 ForegroundColor = { 0.2f, 0.8f, 0.2f, 1.0f }; // 绿色血条
        glm::vec4 BackgroundColor = { 0.2f, 0.2f, 0.2f, 1.0f }; // 深灰背景

        float GetNormalized() const
        {
            if (MaxValue <= 0.0f) return 0.0f;
            return glm::clamp(Value / MaxValue, 0.0f, 1.0f);
        }

        UIProgressBarComponent() = default;
        UIProgressBarComponent(const UIProgressBarComponent&) = default;
    };

    // ── miniaudio音频系统 ──────────────────────────
    struct AudioSourceComponent
    {
        std::string AudioFilePath = "";   // 音频文件路径，相对于 assets/
        float       Volume = 1.0f;
        bool        Loop = false;
        bool        PlayOnStart = false; // Scene 启动时自动播放

        // 运行时句柄，不参与序列化
        uint32_t    RuntimeHandle = 0;

        AudioSourceComponent() = default;
        AudioSourceComponent(const AudioSourceComponent&) = default;
    };

    template<typename... T>
    struct ComponentGroup {};

} // namespace Hazel