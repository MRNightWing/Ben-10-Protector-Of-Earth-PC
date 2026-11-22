#include "audio/audio.h"
#include <SDL2/SDL.h>
#include <vector>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <mutex>

namespace Audio {

static const int SAMPLE_RATE = 44100;
static const int CHANNELS = 2;

struct ActiveSound {
    SoundID id;
    int samplePos = 0;
    int totalSamples = 0;
    float volume = 1.0f;
};

static std::vector<std::vector<int16_t>> g_PrebakedSFX;
static std::vector<ActiveSound> g_ActiveSounds;
static std::mutex g_AudioMutex;
static SDL_AudioDeviceID g_AudioDevice = 0;

// Music synth state
static float g_MusicPhase = 0.0f;
static float g_MusicBassPhase = 0.0f;
static int g_MusicStep = 0;
static int g_MusicSampleTimer = 0;

static const float NOTE_FREQ[] = {
    130.81f, // C3
    146.83f, // D3
    155.56f, // Eb3
    174.61f, // F3
    196.00f, // G3
    207.65f, // Ab3
    233.08f, // Bb3
    261.63f, // C4
    293.66f, // D4
    311.13f, // Eb4
    349.23f, // F4
    392.00f, // G4
};

// 16-step heroic action arpeggio pattern
static const int BGM_MELODY[16] = {
    0, 7, 2, 7, 3, 7, 4, 7,
    0, 7, 2, 7, 6, 4, 3, 2
};
static const int BGM_BASS[16] = {
    0, 0, 0, 0, 3, 3, 3, 3,
    4, 4, 4, 4, 6, 6, 4, 4
};

static std::vector<int16_t> SynthesizeSFX(SoundID id) {
    std::vector<int16_t> pcm;
    float duration = 0.1f;

    switch (id) {
    case SFX_OMNITRIX_SELECT: duration = 0.08f; break;
    case SFX_OMNITRIX_TRANSFORM: duration = 0.55f; break;
    case SFX_PLAYER_PUNCH: duration = 0.16f; break;
    case SFX_PLAYER_SMASH: duration = 0.32f; break;
    case SFX_PLAYER_JUMP: duration = 0.18f; break;
    case SFX_DRONE_LASER: duration = 0.22f; break;
    case SFX_DRONE_HIT: duration = 0.18f; break;
    case SFX_DRONE_EXPLODE: duration = 0.65f; break;
    case SFX_PORTAL_ACTIVE: duration = 0.80f; break;
    case SFX_LEVEL_COMPLETE: duration = 1.20f; break;
    default: duration = 0.1f; break;
    }

    int numSamples = (int)(duration * SAMPLE_RATE);
    pcm.resize(numSamples * CHANNELS);

    for (int i = 0; i < numSamples; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float progress = (float)i / numSamples;
        float sampleVal = 0.0f;

        switch (id) {
        case SFX_OMNITRIX_SELECT: {
            // High frequency crisp ping
            float f = 800.0f + 600.0f * progress;
            sampleVal = std::sin(2.0f * 3.14159f * f * t) * (1.0f - progress);
            break;
        }
        case SFX_OMNITRIX_TRANSFORM: {
            // Omnitrix alien energy charging sweep + electric buzz
            float f = 150.0f + 850.0f * std::pow(progress, 0.7f);
            float sine = std::sin(2.0f * 3.14159f * f * t);
            float noise = ((float)(rand() % 2000 - 1000) / 1000.0f) * 0.25f;
            float env = progress < 0.2f ? (progress / 0.2f) : (1.0f - progress);
            sampleVal = (sine * 0.75f + noise) * env;
            break;
        }
        case SFX_PLAYER_PUNCH: {
            // Rapid punch whoosh + impact
            float f = 240.0f * (1.0f - progress * 0.8f);
            float punch = std::sin(2.0f * 3.14159f * f * t);
            float noise = ((float)(rand() % 2000 - 1000) / 1000.0f) * 0.4f * (1.0f - progress);
            sampleVal = (punch * 0.6f + noise) * (1.0f - progress);
            break;
        }
        case SFX_PLAYER_SMASH: {
            // Heavy earth smash impact
            float f = 110.0f * (1.0f - progress * 0.7f);
            float thud = std::sin(2.0f * 3.14159f * f * t);
            float noise = ((float)(rand() % 2000 - 1000) / 1000.0f) * 0.6f * (1.0f - progress);
            sampleVal = (thud * 0.7f + noise) * (1.0f - std::pow(progress, 0.5f));
            break;
        }
        case SFX_PLAYER_JUMP: {
            // Ascending whoosh
            float f = 180.0f + 320.0f * progress;
            sampleVal = std::sin(2.0f * 3.14159f * f * t) * (1.0f - progress * progress);
            break;
        }
        case SFX_DRONE_LASER: {
            // Futuristic laser shot (frequency drops rapidly)
            float f = 1600.0f * std::exp(-progress * 8.0f) + 120.0f;
            sampleVal = std::sin(2.0f * 3.14159f * f * t) * (1.0f - progress);
            break;
        }
        case SFX_DRONE_HIT: {
            // Metallic clink
            float f = 850.0f * (1.0f - progress * 0.5f);
            float chime = std::sin(2.0f * 3.14159f * f * t) + 0.5f * std::sin(2.0f * 3.14159f * f * 2.3f * t);
            sampleVal = chime * 0.5f * (1.0f - progress);
            break;
        }
        case SFX_DRONE_EXPLODE: {
            // Low rumbling blast
            float noise = ((float)(rand() % 2000 - 1000) / 1000.0f);
            float lowRumble = std::sin(2.0f * 3.14159f * 65.0f * t);
            sampleVal = (noise * 0.7f + lowRumble * 0.5f) * (1.0f - progress);
            break;
        }
        case SFX_PORTAL_ACTIVE: {
            // Mystical energy chime
            float f1 = 440.0f, f2 = 660.0f, f3 = 880.0f;
            float chime = std::sin(2.0f * 3.14159f * f1 * t) + 
                          0.6f * std::sin(2.0f * 3.14159f * f2 * t) +
                          0.4f * std::sin(2.0f * 3.14159f * f3 * t);
            sampleVal = chime * 0.33f * (1.0f - progress);
            break;
        }
        case SFX_LEVEL_COMPLETE: {
            // Heroic fanfare
            float f = 330.0f;
            if (progress > 0.3f) f = 440.0f;
            if (progress > 0.6f) f = 660.0f;
            float horn = std::sin(2.0f * 3.14159f * f * t);
            sampleVal = horn * 0.5f * (1.0f - progress * 0.5f);
            break;
        }
        }

        int16_t s = (int16_t)(std::clamp(sampleVal, -1.0f, 1.0f) * 28000.0f);
        pcm[i * 2 + 0] = s;
        pcm[i * 2 + 1] = s;
    }

    return pcm;
}

static void AudioCallback(void* userdata, Uint8* stream, int len) {
    int16_t* out = (int16_t*)stream;
    int samplesToGenerate = len / (sizeof(int16_t) * CHANNELS);

    std::fill(out, out + samplesToGenerate * CHANNELS, 0);

    std::lock_guard<std::mutex> lock(g_AudioMutex);

    // 1. Synthesize background action theme
    if (AudioEngine::Instance().IsMusicEnabled()) {
        const int samplesPerBeat = SAMPLE_RATE / 8; // ~120 BPM 16th notes

        for (int i = 0; i < samplesToGenerate; ++i) {
            g_MusicSampleTimer++;
            if (g_MusicSampleTimer >= samplesPerBeat) {
                g_MusicSampleTimer = 0;
                g_MusicStep = (g_MusicStep + 1) % 16;
            }

            int leadNote = BGM_MELODY[g_MusicStep];
            int bassNote = BGM_BASS[g_MusicStep];

            float leadFreq = NOTE_FREQ[leadNote];
            float bassFreq = NOTE_FREQ[bassNote] * 0.5f;

            g_MusicPhase += (leadFreq * 2.0f * 3.14159f) / SAMPLE_RATE;
            if (g_MusicPhase > 6.28318f) g_MusicPhase -= 6.28318f;

            g_MusicBassPhase += (bassFreq * 2.0f * 3.14159f) / SAMPLE_RATE;
            if (g_MusicBassPhase > 6.28318f) g_MusicBassPhase -= 6.28318f;

            // Arp lead with envelope
            float beatProgress = (float)g_MusicSampleTimer / samplesPerBeat;
            float env = (1.0f - beatProgress);
            float leadVal = std::sin(g_MusicPhase) * env * 0.08f;

            // Punchy bass
            float bassVal = (std::sin(g_MusicBassPhase) > 0.0f ? 0.05f : -0.05f) * (1.0f - beatProgress * 0.7f);

            // Subtle hi-hat on off-beats
            float hat = 0.0f;
            if (g_MusicStep % 2 == 1 && beatProgress < 0.2f) {
                hat = ((float)(rand() % 2000 - 1000) / 1000.0f) * 0.03f * (1.0f - beatProgress / 0.2f);
            }

            int16_t musicSample = (int16_t)((leadVal + bassVal + hat) * 32767.0f);
            out[i * 2 + 0] = musicSample;
            out[i * 2 + 1] = musicSample;
        }
    }

    // 2. Mix Active SFX
    for (auto it = g_ActiveSounds.begin(); it != g_ActiveSounds.end(); ) {
        const auto& sfxPcm = g_PrebakedSFX[it->id];
        int sfxSamples = (int)sfxPcm.size() / CHANNELS;

        for (int i = 0; i < samplesToGenerate; ++i) {
            if (it->samplePos >= sfxSamples) break;

            int sfxIndex = it->samplePos * CHANNELS;
            int32_t left = out[i * 2 + 0] + (int32_t)(sfxPcm[sfxIndex + 0] * it->volume);
            int32_t right = out[i * 2 + 1] + (int32_t)(sfxPcm[sfxIndex + 1] * it->volume);

            out[i * 2 + 0] = (int16_t)std::clamp(left, -32768, 32767);
            out[i * 2 + 1] = (int16_t)std::clamp(right, -32768, 32767);

            it->samplePos++;
        }

        if (it->samplePos >= sfxSamples) {
            it = g_ActiveSounds.erase(it);
        } else {
            ++it;
        }
    }
}

AudioEngine& AudioEngine::Instance() {
    static AudioEngine s_Instance;
    return s_Instance;
}

bool AudioEngine::Init() {
    if (m_Initialized) return true;

    printf("[Audio] Initializing SDL Audio Subsystem...\n");
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "[Audio] SDL_InitSubSystem failed: %s\n", SDL_GetError());
        return false;
    }

    // Pre-bake all SFX procedural wave tables
    g_PrebakedSFX.resize(SFX_COUNT);
    for (int i = 0; i < SFX_COUNT; ++i) {
        g_PrebakedSFX[i] = SynthesizeSFX((SoundID)i);
    }
    printf("[Audio] Pre-baked %d procedural sound effects.\n", SFX_COUNT);

    SDL_AudioSpec desired, obtained;
    SDL_zero(desired);
    desired.freq = SAMPLE_RATE;
    desired.format = AUDIO_S16SYS;
    desired.channels = CHANNELS;
    desired.samples = 1024;
    desired.callback = AudioCallback;

    g_AudioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (g_AudioDevice == 0) {
        fprintf(stderr, "[Audio] SDL_OpenAudioDevice failed: %s (Continuing in silent mode)\n", SDL_GetError());
        m_Initialized = false;
        return false;
    }

    m_DeviceID = g_AudioDevice;
    SDL_PauseAudioDevice(g_AudioDevice, 0); // Start playback
    m_Initialized = true;
    printf("[Audio] Audio device opened successfully (44.1kHz 16-bit Stereo).\n");
    return true;
}

void AudioEngine::Shutdown() {
    if (!m_Initialized) return;
    printf("[Audio] Shutting down audio device...\n");
    if (g_AudioDevice != 0) {
        SDL_CloseAudioDevice(g_AudioDevice);
        g_AudioDevice = 0;
    }
    m_Initialized = false;
}

void AudioEngine::PlaySFX(SoundID id) {
    if (!m_Initialized || id < 0 || id >= SFX_COUNT) return;

    std::lock_guard<std::mutex> lock(g_AudioMutex);
    ActiveSound sound;
    sound.id = id;
    sound.samplePos = 0;
    sound.totalSamples = (int)g_PrebakedSFX[id].size() / CHANNELS;
    sound.volume = 1.0f;

    // Cap concurrent sounds to prevent clipping
    if (g_ActiveSounds.size() > 16) {
        g_ActiveSounds.erase(g_ActiveSounds.begin());
    }
    g_ActiveSounds.push_back(sound);
}

void AudioEngine::SetMusicEnabled(bool enabled) {
    m_MusicEnabled = enabled;
}

void AudioEngine::StreamTrack(const char* trackName) {
    printf("[Audio] Music_StreamTrack: '%s'\n", trackName ? trackName : "(null)");
    // Starts or switches procedural background music theme
    SetMusicEnabled(true);
}

void AudioEngine::PlayDecompiledSFX(u32 sfxId) {
    SoundID id = (SoundID)(sfxId % SFX_COUNT);
    PlaySFX(id);
}

} // namespace Audio

extern "C" {

void Engine_AudioAllocPool(u32 poolSize) {
    printf("[Engine] Engine_AudioAllocPool: allocated %u bytes for audio heap\n", poolSize);
    Audio::AudioEngine::Instance().Init();
}

void Sound_PlaySFX(u32 sfxId) {
    Audio::AudioEngine::Instance().PlayDecompiledSFX(sfxId);
}

void Music_StreamTrack(const char* name) {
    Audio::AudioEngine::Instance().StreamTrack(name);
}

}
