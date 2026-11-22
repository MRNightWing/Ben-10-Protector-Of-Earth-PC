#pragma once
#include "psp/types.h"
#include <string>

namespace Audio {

enum SoundID {
    SFX_OMNITRIX_SELECT = 0,
    SFX_OMNITRIX_TRANSFORM = 1,
    SFX_PLAYER_PUNCH = 2,
    SFX_PLAYER_SMASH = 3,
    SFX_PLAYER_JUMP = 4,
    SFX_DRONE_LASER = 5,
    SFX_DRONE_HIT = 6,
    SFX_DRONE_EXPLODE = 7,
    SFX_PORTAL_ACTIVE = 8,
    SFX_LEVEL_COMPLETE = 9,
    SFX_COUNT
};

class AudioEngine {
public:
    static AudioEngine& Instance();

    bool Init();
    void Shutdown();

    void PlaySFX(SoundID id);
    void SetMusicEnabled(bool enabled);
    bool IsMusicEnabled() const { return m_MusicEnabled; }

    // Decompiled PSP Quantum Engine compatibility hooks
    void StreamTrack(const char* trackName);
    void PlayDecompiledSFX(u32 sfxId);

private:
    AudioEngine() = default;
    ~AudioEngine() = default;

    bool m_Initialized = false;
    bool m_MusicEnabled = true;
    u32  m_DeviceID = 0;
};

} // namespace Audio

// Decompiled C API hooks
extern "C" {
    void Engine_AudioAllocPool(u32 poolSize);
    void Sound_PlaySFX(u32 sfxId);
    void Music_StreamTrack(const char* name);
}
