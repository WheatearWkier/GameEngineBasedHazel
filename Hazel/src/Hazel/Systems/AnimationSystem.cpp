#include "hzpch.h"
#include "AnimationSystem.h"

#include "Hazel/Scene/Scene.h"
#include "Hazel/Scene/Components.h"
#include "Hazel/Animation/AnimationClip.h"

#include <glm/glm.hpp>

namespace Hazel {

    void AnimationSystem::OnRuntimeStart(Scene* scene)
    {
        // PlayOnStart 初始化
        for (auto e : scene->GetRegistry().view<SpriteAnimatorComponent>())
        {
            auto& anim = scene->GetRegistry().get<SpriteAnimatorComponent>(e);
            anim.CurrentClipName = anim.DefaultClipName;
            anim.ElapsedTime = 0.0f;
            anim.IsPlaying = anim.PlayOnStart;
        }
    }

    void AnimationSystem::OnUpdateRuntime(Scene* scene, Timestep ts)
    {
        UpdateAnimations(scene, ts);
    }

    void AnimationSystem::OnUpdateEditor(Scene* scene, Timestep ts)
    {
        if (!m_EditorPreviewActive)
            SyncEditorPreviewFrame(scene);
    }

    // ── 私有实现 ──────────────────────────────────────────────────────────────

    void AnimationSystem::SyncEditorPreviewFrame(Scene* scene)
    {
        auto& registry = scene->GetRegistry();
        for (auto e : registry.view<SpriteRendererComponent, SpriteAnimatorComponent>())
        {
            auto& sr = registry.get<SpriteRendererComponent>(e);
            auto& anim = registry.get<SpriteAnimatorComponent>(e);

            if (anim.CurrentClipName.empty()) continue;
            auto it = anim.Clips.find(anim.CurrentClipName);
            if (it == anim.Clips.end()) continue;
            const auto& frames = it->second->GetFrames();
            if (frames.empty()) continue;

            sr.Texture = frames[0].Texture;
            sr.UVMin = frames[0].TexCoordMin;
            sr.UVMax = frames[0].TexCoordMax;
        }
    }

    void AnimationSystem::UpdateAnimations(Scene* scene, Timestep ts)
    {
        auto& registry = scene->GetRegistry();
        auto view = registry.view<SpriteAnimatorComponent, SpriteRendererComponent>();

        for (auto e : view)
        {
            auto& animator = view.get<SpriteAnimatorComponent>(e);
            auto& sprite = view.get<SpriteRendererComponent>(e);

            if (!animator.IsPlaying) continue;
            auto clip = animator.GetCurrentClip();
            if (!clip) continue;

            animator.ElapsedTime += ts;
            const float totalDur = clip->GetTotalDuration();

            // 非循环动画结束处理
            if (totalDur > 0.0f && !clip->IsLooping() && animator.ElapsedTime >= totalDur)
            {
                animator.ElapsedTime = totalDur;
                animator.IsPlaying = false;
                animator.IsFinished = true;
            }

            // 帧采样
            if (clip->GetFrameCount() > 0)
            {
                const auto& frames = clip->GetFrames();
                float t = animator.ElapsedTime;
                if (clip->IsLooping() && totalDur > 0.0f)
                    t = std::fmod(t, totalDur);

                int   frameIdx = static_cast<int>(frames.size()) - 1;
                float acc = 0.0f;
                for (int i = 0; i < static_cast<int>(frames.size()); ++i)
                {
                    acc += frames[i].Duration;
                    if (t < acc) { frameIdx = i; break; }
                }
                animator.CurrentFrameIndex = frameIdx;

                const auto& frame = frames[frameIdx];
                sprite.Texture = frame.Texture;
                sprite.UVMin = frame.TexCoordMin;
                sprite.UVMax = frame.TexCoordMax;
            }

            // 属性轨道采样
            TransformComponent* tc = registry.try_get<TransformComponent>(e);

            for (auto& trackBase : clip->GetPropertyTracks())
            {
                switch (trackBase->Property)
                {
                case AnimatedProperty::SpriteColorA:
                    std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [&sprite](const float& v) { sprite.Color.a = v; }; break;
                case AnimatedProperty::SpriteColorR:
                    std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [&sprite](const float& v) { sprite.Color.r = v; }; break;
                case AnimatedProperty::SpriteColorG:
                    std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [&sprite](const float& v) { sprite.Color.g = v; }; break;
                case AnimatedProperty::SpriteColorB:
                    std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [&sprite](const float& v) { sprite.Color.b = v; }; break;
                case AnimatedProperty::SpriteColor:
                    std::static_pointer_cast<PropertyTrack<glm::vec4>>(trackBase)->Writer =
                        [&sprite](const glm::vec4& v) { sprite.Color = v; }; break;
                case AnimatedProperty::PositionX:
                    if (tc) std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [tc](const float& v) { tc->Translation.x = v; }; break;
                case AnimatedProperty::PositionY:
                    if (tc) std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [tc](const float& v) { tc->Translation.y = v; }; break;
                case AnimatedProperty::PositionZ:
                    if (tc) std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [tc](const float& v) { tc->Translation.z = v; }; break;
                case AnimatedProperty::RotationZ:
                    if (tc) std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [tc](const float& v) { tc->Rotation.z = v; }; break;
                case AnimatedProperty::ScaleX:
                    if (tc) std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [tc](const float& v) { tc->Scale.x = v; }; break;
                case AnimatedProperty::ScaleY:
                    if (tc) std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [tc](const float& v) { tc->Scale.y = v; }; break;
                case AnimatedProperty::ScaleUniform:
                    if (tc) std::static_pointer_cast<PropertyTrack<float>>(trackBase)->Writer =
                        [tc](const float& v) { tc->Scale.x = tc->Scale.y = v; }; break;
                }
                trackBase->Sample(animator.ElapsedTime, clip->IsLooping(), totalDur);
            }
        }
    }

} // namespace Hazel