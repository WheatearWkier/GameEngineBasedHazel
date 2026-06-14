#pragma once
#include <string>
#include <unordered_map>
#include <filesystem>

// 前向声明，避免把 miniaudio 暴露到全局
struct ma_engine;
struct ma_sound;

namespace Hazel {

    // 代表一个已加载的音频资源句柄
    struct AudioClip
    {
        std::string FilePath;
        bool        Loaded = false;
    };

    class AudioEngine
    {
    public:
        static void Init();
        static void Shutdown();

        // 播放一次性音效（fire-and-forget，适合子弹射击音效）
        static void PlaySound(const std::string& filepath, float volume = 1.0f);

        // 播放并返回句柄（适合需要控制的音效，比如背景音乐）
        static uint32_t PlaySoundWithHandle(const std::string& filepath,
            float volume = 1.0f,
            bool  loop = false);

        static void StopSound(uint32_t handle);
        static void PauseSound(uint32_t handle);
        static void ResumeSound(uint32_t handle);
        static void SetVolume(uint32_t handle, float volume);
        static bool IsPlaying(uint32_t handle);

        // 给 AudioSourceComponent 在 Scene 运行时调用
        static void OnSceneStop();

    private:
        static ma_engine* s_Engine;
        static std::unordered_map<uint32_t, ma_sound*> s_Sounds;
        static uint32_t                                 s_NextHandle;
    };

}