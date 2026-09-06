#include "platform/Audio.hpp"

#include "core/Log.hpp"
#include "core/Math.hpp"
#include "core/Rng.hpp"

#include <SDL3/SDL.h>

#include <cmath>

namespace df {
namespace {

constexpr int kSampleRate = 48000;
constexpr int kChunkFrames = 1024;

struct SfxSpec {
    float duration;
    float baseFreq;
    float endFreq;
    float noiseMix;
    float decay;
    const char* caption;
};

constexpr SfxSpec kSpecs[static_cast<usize>(Sfx::Count)] = {
    /* DrillContact  */ {0.06f, 190.0f, 150.0f, 0.55f, 26.0f, "[drill bites]"},
    /* TileBreak     */ {0.16f, 150.0f, 70.0f, 0.75f, 16.0f, "[rock breaks]"},
    /* OreBreak      */ {0.22f, 320.0f, 180.0f, 0.45f, 12.0f, "[ore breaks]"},
    /* CrystalBreak  */ {0.30f, 880.0f, 1320.0f, 0.2f, 9.0f, "[crystal shatters]"},
    /* VolatileBreak */ {0.36f, 120.0f, 60.0f, 0.85f, 7.0f, "[volatile discharge]"},
    /* Pickup        */ {0.12f, 660.0f, 990.0f, 0.0f, 18.0f, "[cargo stowed]"},
    /* CargoFull     */ {0.34f, 300.0f, 220.0f, 0.1f, 7.0f, "[cargo hold full]"},
    /* LowFuel       */ {0.40f, 440.0f, 330.0f, 0.0f, 6.0f, "[low fuel warning]"},
    /* HeatWarning   */ {0.45f, 240.0f, 200.0f, 0.25f, 5.0f, "[heat critical]"},
    /* HullDamage    */ {0.28f, 110.0f, 80.0f, 0.7f, 10.0f, "[hull impact]"},
    /* Discovery     */ {0.55f, 520.0f, 780.0f, 0.0f, 4.0f, "[discovery logged]"},
    /* Purchase      */ {0.20f, 520.0f, 700.0f, 0.0f, 12.0f, "[purchase confirmed]"},
    /* Upgrade       */ {0.45f, 380.0f, 760.0f, 0.05f, 6.0f, "[upgrade installed]"},
    /* Return        */ {0.60f, 260.0f, 520.0f, 0.0f, 4.0f, "[docked at surface]"},
    /* UiMove        */ {0.05f, 620.0f, 620.0f, 0.0f, 30.0f, ""},
    /* UiSelect      */ {0.09f, 780.0f, 940.0f, 0.0f, 22.0f, ""},
    /* UiBack        */ {0.09f, 420.0f, 320.0f, 0.0f, 22.0f, ""},
    /* Scan          */ {0.50f, 900.0f, 300.0f, 0.05f, 5.0f, "[scanner pulse]"},
};

} // namespace

std::string_view sfx_caption(Sfx s) {
    return kSpecs[static_cast<usize>(s)].caption;
}

AudioSystem::AudioSystem() {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        DF_LOG_WARN("audio", "audio unavailable: {}", SDL_GetError());
        return;
    }
    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_F32;
    spec.channels = 1;
    spec.freq = kSampleRate;
    mStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (mStream == nullptr) {
        DF_LOG_WARN("audio", "could not open audio stream: {}", SDL_GetError());
        return;
    }
    SDL_ResumeAudioStreamDevice(mStream);
    mVoices.resize(16);
    DF_LOG_INFO("audio", "synthesised audio ready at {} Hz", kSampleRate);
}

AudioSystem::~AudioSystem() {
    if (mStream != nullptr) {
        SDL_DestroyAudioStream(mStream);
    }
}

void AudioSystem::set_volumes(float master, float sfx) {
    mMaster = saturate(master);
    mSfxVolume = saturate(sfx);
}

void AudioSystem::play(Sfx sound, float pitchVariation) {
    if (mStream == nullptr) {
        return;
    }
    for (auto& v : mVoices) {
        if (v.active) {
            continue;
        }
        v.active = true;
        v.sound = sound;
        v.t = 0.0f;
        v.duration = kSpecs[static_cast<usize>(sound)].duration;
        v.pitch = 1.0f + pitchVariation;
        return;
    }
}

void AudioSystem::set_drill_active(bool active, float intensity) {
    mDrillActive = active;
    mDrillIntensity = saturate(intensity);
}

void AudioSystem::mix_into(std::vector<float>& out) {
    const float dt = 1.0f / static_cast<float>(kSampleRate);
    for (usize i = 0; i < out.size(); ++i) {
        float sample = 0.0f;
        for (auto& v : mVoices) {
            if (!v.active) {
                continue;
            }
            const auto& spec = kSpecs[static_cast<usize>(v.sound)];
            const float progress = v.duration > 0.0f ? v.t / v.duration : 1.0f;
            const float freq = lerp(spec.baseFreq, spec.endFreq, saturate(progress)) * v.pitch;
            const float env = std::exp(-spec.decay * v.t);
            mNoiseState = splitmix64(mNoiseState);
            const float noise =
                static_cast<float>(static_cast<i32>(mNoiseState >> 40) - 8388608) / 8388608.0f;
            const float tone = std::sin(6.2831853f * freq * v.t);
            sample += env * lerp(tone, noise, spec.noiseMix) * 0.30f;
            v.t += dt;
            if (v.t >= v.duration) {
                v.active = false;
            }
        }
        if (mDrillActive && mDrillIntensity > 0.0f) {
            mDrillPhase += dt;
            mNoiseState = splitmix64(mNoiseState);
            const float noise =
                static_cast<float>(static_cast<i32>(mNoiseState >> 40) - 8388608) / 8388608.0f;
            const float rumble = std::sin(6.2831853f * 62.0f * mDrillPhase);
            sample += (0.55f * rumble + 0.45f * noise) * 0.10f * mDrillIntensity;
        }
        out[i] = clamp(sample * mMaster * mSfxVolume, -1.0f, 1.0f);
    }
}

void AudioSystem::update() {
    if (mStream == nullptr) {
        return;
    }
    // Keep roughly 4 chunks queued: enough to avoid underruns, short enough that
    // cues stay responsive.
    const int queued = SDL_GetAudioStreamQueued(mStream);
    const int target = static_cast<int>(sizeof(float)) * kChunkFrames * 4;
    if (queued >= target) {
        return;
    }
    std::vector<float> buffer(static_cast<usize>(kChunkFrames));
    mix_into(buffer);
    SDL_PutAudioStreamData(mStream, buffer.data(), static_cast<int>(buffer.size() * sizeof(float)));
}

} // namespace df
