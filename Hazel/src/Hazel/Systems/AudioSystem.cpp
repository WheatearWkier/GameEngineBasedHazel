#include "hzpch.h"
#include "AudioSystem.h"

#include "Hazel/Scene/Scene.h"
#include "Hazel/Scene/Components.h"
#include "Hazel/Audio/AudioEngine.h"

namespace Hazel {

    void AudioSystem::OnRuntimeStart(Scene* scene)
    {
        for (auto e : scene->GetRegistry().view<AudioSourceComponent>())
        {
            auto& asc = scene->GetRegistry().get<AudioSourceComponent>(e);
            if (asc.PlayOnStart && !asc.AudioFilePath.empty())
                asc.RuntimeHandle = AudioEngine::PlaySoundWithHandle(
                    asc.AudioFilePath, asc.Volume, asc.Loop);
        }
    }

    void AudioSystem::OnRuntimeStop(Scene* scene)
    {
        AudioEngine::OnSceneStop();
    }

} // namespace Hazel