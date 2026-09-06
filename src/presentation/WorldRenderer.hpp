// Tile rendering with a per-chunk texture cache.
//
// Chunks are rasterised once into a texture and re-used until they change, so
// the frame cost is a dozen textured quads rather than thousands of tile draws
// (Section 20).
#pragma once

#include "app/GameSession.hpp"
#include "platform/Settings.hpp"
#include "presentation/Draw.hpp"

#include <unordered_map>

struct SDL_Texture;

namespace df {

struct Camera {
    Vec2 centre{}; ///< Tile-space centre of the view.
    float pixelsPerTile = 32.0f;
    float shake = 0.0f;
    Vec2 shakeOffset{};

    Vec2 world_to_screen(Vec2 world, float screenW, float screenH) const {
        return Vec2{(world.x - centre.x) * pixelsPerTile + screenW * 0.5f + shakeOffset.x,
                    (world.y - centre.y) * pixelsPerTile + screenH * 0.5f + shakeOffset.y};
    }
};

/// A short-lived visual effect. Purely cosmetic; never read by the simulation.
struct Particle {
    Vec2 position{};
    Vec2 velocity{};
    float life = 0.0f;
    float maxLife = 1.0f;
    Color color{};
    float size = 2.0f;
};

class WorldRenderer {
public:
    WorldRenderer() = default;
    ~WorldRenderer();

    WorldRenderer(const WorldRenderer&) = delete;
    WorldRenderer& operator=(const WorldRenderer&) = delete;

    void render(Draw& draw, const GameSession& session, const RenderSnapshot& snap, const Camera& cam,
                const Settings& settings, float screenW, float screenH);

    void spawn_break_particles(Vec2 tile, Color c, const Settings& settings);
    void spawn_pickup(Vec2 tile, Color c, const Settings& settings);
    void update_effects(float dt);
    void add_shake(float amount, const Settings& settings);

    float shake() const noexcept { return mShake; }

    Vec2 shake_offset() const noexcept { return mShakeOffset; }

    void invalidate_chunk(ChunkCoord c);

    void invalidate_all();

    usize cached_chunks() const noexcept { return mChunkTextures.size(); }

    i32 draw_calls() const noexcept { return mDrawCalls; }

    /// Material colour honouring the colour-blind palette option.
    static Color material_color(const MaterialDefinition& m, const Settings& settings);

private:
    struct CachedChunk {
        SDL_Texture* texture = nullptr;
        u64 hash = 0;
        i32 pixelsPerTile = 0;
    };

    SDL_Texture* build_chunk_texture(Draw& draw, const GameSession& session, ChunkCoord coord,
                                     const Settings& settings, int pixelsPerTile);

    std::unordered_map<u64, CachedChunk> mChunkTextures;
    std::vector<Particle> mParticles;
    float mShake = 0.0f;
    Vec2 mShakeOffset{};
    u64 mCosmeticState = 0x1234567890ABCDEFull;
    i32 mDrawCalls = 0;
};

} // namespace df
