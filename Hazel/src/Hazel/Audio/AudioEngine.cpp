#include "hzpch.h"
#include "AudioEngine.h"
#include "Hazel/Core/Application.h"

// miniaudio implementation — must be defined in only one .cpp
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

namespace Hazel {

    ma_engine* AudioEngine::s_Engine = nullptr;
    std::unordered_map<uint32_t, ma_sound*> AudioEngine::s_Sounds = {};
    uint32_t AudioEngine::s_NextHandle = 1;

    // 放在 namespace Hazel { 里面，其他函数上面
    static std::string ResolvePath(const std::string& filepath)
    {
        // 如果已经是绝对路径，直接用
        if (std::filesystem::path(filepath).is_absolute())
            return filepath;

        // 否则拼接工作目录（Hazelnut 运行时工作目录就是 Hazelnut/ 文件夹）
        std::filesystem::path base = std::filesystem::current_path();
        std::filesystem::path full = base / "assets" / filepath;

        // 如果加了 assets 还不对，试试直接拼
        if (!std::filesystem::exists(full))
            full = base / filepath;

        return full.string();
    }

    void AudioEngine::Init()
    {
        s_Engine = new ma_engine();

        ma_result result = ma_engine_init(nullptr, s_Engine);
        if (result != MA_SUCCESS)
        {
            HZ_CORE_ERROR("AudioEngine: initialization failed! error code = {0}", (int)result);
            delete s_Engine;
            s_Engine = nullptr;
            return;
        }

        HZ_CORE_INFO("AudioEngine: initialization successful");
    }

    void AudioEngine::Shutdown()
    {
        // Stop and release all sounds first
        for (auto& [handle, sound] : s_Sounds)
        {
            ma_sound_stop(sound);
            ma_sound_uninit(sound);
            delete sound;
        }
        s_Sounds.clear();

        if (s_Engine)
        {
            ma_engine_uninit(s_Engine);
            delete s_Engine;
            s_Engine = nullptr;
        }

        HZ_CORE_INFO("AudioEngine: shutdown complete");
    }

    void AudioEngine::PlaySound(const std::string& filepath, float volume)
    {
        if (!s_Engine) return;

        std::string resolvedPath = ResolvePath(filepath);

        ma_result result = ma_engine_play_sound(s_Engine, resolvedPath.c_str(), nullptr);
        if (result != MA_SUCCESS)
            HZ_CORE_WARN("AudioEngine: failed to play [{0}], error code = {1}", filepath, (int)result);
    }

    uint32_t AudioEngine::PlaySoundWithHandle(const std::string& filepath,
        float volume, bool loop)
    {
        if (!s_Engine) return 0;

        std::string resolvedPath = ResolvePath(filepath);

        ma_sound* sound = new ma_sound();
        ma_result result = ma_sound_init_from_file(
            s_Engine,
            resolvedPath.c_str(),
            MA_SOUND_FLAG_DECODE,
            nullptr, nullptr,
            sound
        );

        if (result != MA_SUCCESS)
        {
            HZ_CORE_WARN("AudioEngine: failed to load [{0}], error code = {1}", filepath, (int)result);
            delete sound;
            return 0;
        }

        ma_sound_set_volume(sound, volume);
        ma_sound_set_looping(sound, loop ? MA_TRUE : MA_FALSE);
        ma_sound_start(sound);

        uint32_t handle = s_NextHandle++;
        s_Sounds[handle] = sound;
        return handle;
    }

    void AudioEngine::StopSound(uint32_t handle)
    {
        auto it = s_Sounds.find(handle);
        if (it == s_Sounds.end()) return;

        ma_sound_stop(it->second);
        ma_sound_uninit(it->second);
        delete it->second;
        s_Sounds.erase(it);
    }

    void AudioEngine::PauseSound(uint32_t handle)
    {
        auto it = s_Sounds.find(handle);
        if (it != s_Sounds.end())
            ma_sound_stop(it->second);
    }

    void AudioEngine::ResumeSound(uint32_t handle)
    {
        auto it = s_Sounds.find(handle);
        if (it != s_Sounds.end())
            ma_sound_start(it->second);
    }

    void AudioEngine::SetVolume(uint32_t handle, float volume)
    {
        auto it = s_Sounds.find(handle);
        if (it != s_Sounds.end())
            ma_sound_set_volume(it->second, volume);
    }

    bool AudioEngine::IsPlaying(uint32_t handle)
    {
        auto it = s_Sounds.find(handle);
        if (it == s_Sounds.end()) return false;
        return ma_sound_is_playing(it->second) == MA_TRUE;
    }

    void AudioEngine::OnSceneStop()
    {
        for (auto& [handle, sound] : s_Sounds)
        {
            ma_sound_stop(sound);
            ma_sound_uninit(sound);
            delete sound;
        }
        s_Sounds.clear();
        s_NextHandle = 1;
    }

}