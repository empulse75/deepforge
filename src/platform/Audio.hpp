// Procedurally synthesised sound effects.
//
// The game ships with no external audio assets, so every cue is generated at
// runtime from simple oscillators and envelopes. Cosmetic variation is derived
// from a dedicated random stream and never feeds back into the simulation.
#pragma once

#include "core/Types.hpp"

#include <string>
#include <vector>

struct SDL_AudioStream;

namespace df {

enum class Sfx : u8 {
    DrillContact,
    TileBreak,
    OreBreak,
    CrystalBreak,
    VolatileBreak,
    Pickup,
    CargoFull,
    LowFuel,
    HeatWarning,
    HullDamage,
    Discovery,
    Purchase,
    Upgrade,
    Return,
    UiMove,
    UiSelect,
    UiBack,
    Scan,
    Count
};

/// Caption text for the subtitles/captions accessibility option.
std::string_view sfx_caption(Sfx s);

class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    bool available() const noexcept { return mStream != nullptr; }

    void set_volumes(float master, float sfx);

    void play(Sfx sound, float pitchVariation = 0.0f);

    /// Continuous drilling loop; call every frame while drilling.
    void set_drill_active(bool active, float intensity);

    void update();

private:
    void mix_into(std::vector<float>& out);

    struct Voice {
        Sfx sound = Sfx::UiMove;
        float t = 0.0f;
        float duration = 0.0f;
        float pitch = 1.0f;
        bool active = false;
    };

    SDL_AudioStream* mStream = nullptr;
    std::vector<Voice> mVoices;
    float mMaster = 0.8f;
    float mSfxVolume = 0.9f;
    bool mDrillActive = false;
    float mDrillIntensity = 0.0f;
    float mDrillPhase = 0.0f;
    u64 mNoiseState = 0x243F6A8885A308D3ull;
};

} // namespace df
