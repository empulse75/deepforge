#include "presentation/WorldRenderer.hpp"

#include "core/Hash.hpp"
#include "core/Log.hpp"
#include "presentation/Pretty.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace df {
namespace {

/// Deterministic per-tile shade so flat colours do not read as one slab.
float tile_shade(TileCoord t, u8 variant) {
    const u64 h = hash_coords(0xA5A5A5ull, t.x, t.y) ^ variant;
    return 0.86f + static_cast<float>(h & 0x3Fu) / 255.0f * 0.5f;
}

Color category_marker(MaterialCategory c) {
    switch (c) {
        case MaterialCategory::Commodity:
            return theme::kCargo;
        case MaterialCategory::Strategic:
            return theme::kAmber;
        case MaterialCategory::Volatile:
            return theme::kDanger;
        case MaterialCategory::Artifact:
            return Color{236, 150, 236};
        default:
            return theme::kTextMuted;
    }
}

} // namespace

WorldRenderer::~WorldRenderer() {
    invalidate_all();
}

void WorldRenderer::invalidate_all() {
    for (auto& [key, cached] : mChunkTextures) {
        if (cached.texture != nullptr) {
            SDL_DestroyTexture(cached.texture);
        }
    }
    mChunkTextures.clear();
}

void WorldRenderer::invalidate_chunk(ChunkCoord c) {
    const auto it = mChunkTextures.find(chunk_key(c));
    if (it != mChunkTextures.end()) {
        if (it->second.texture != nullptr) {
            SDL_DestroyTexture(it->second.texture);
        }
        mChunkTextures.erase(it);
    }
}

Color WorldRenderer::material_color(const MaterialDefinition& m, const Settings& settings) {
    Color c{m.colorR, m.colorG, m.colorB};
    if (!settings.access.colorBlindPalette) {
        return c;
    }
    // Category-keyed palette chosen for deuteranopia/protanopia separation:
    // blue/yellow/white contrast rather than red/green.
    switch (m.category) {
        case MaterialCategory::Commodity:
            return Color{240, 200, 70};
        case MaterialCategory::Strategic:
            return Color{80, 160, 230};
        case MaterialCategory::Volatile:
            return Color{250, 250, 250};
        case MaterialCategory::Artifact:
            return Color{190, 120, 240};
        case MaterialCategory::Structural:
            return Color{120, 120, 130};
        case MaterialCategory::Rock:
            return Color{78, 70, 66};
        default:
            return c;
    }
}

SDL_Texture* WorldRenderer::build_chunk_texture(Draw& draw, const GameSession& session, ChunkCoord coord,
                                                const Settings& settings, int pixelsPerTile) {
    SDL_Renderer* renderer = draw.renderer();
    const int size = kChunkSize * pixelsPerTile;
    SDL_Texture* tex =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, size, size);
    if (tex == nullptr) {
        DF_LOG_WARN("render", "could not create chunk texture: {}", SDL_GetError());
        return nullptr;
    }
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_Texture* previous = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, tex);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    const auto& db = session.content();
    const Chunk& chunk = session.world().chunk(coord);
    const float p = static_cast<float>(pixelsPerTile);
    for (i32 i = 0; i < kChunkTileCount; ++i) {
        const i32 mi = chunk.material_at(i);
        if (mi < 0) {
            continue;
        }
        const auto& def = db.materials()[static_cast<usize>(mi)];
        if (def.category == MaterialCategory::Empty) {
            continue;
        }
        const TileCoord t = tile_of(coord, i);
        const float x = static_cast<float>(i % kChunkSize) * p;
        const float y = static_cast<float>(i / kChunkSize) * p;
        Color c = material_color(def, settings);
        c = c.shade(tile_shade(t, chunk.variant_at(i)));

        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
        const SDL_FRect r{x, y, p, p};
        SDL_RenderFillRect(renderer, &r);

        // Subtle top highlight and bottom shadow give the tiles readable relief.
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 22);
        const SDL_FRect top{x, y, p, std::max(1.0f, p * 0.09f)};
        SDL_RenderFillRect(renderer, &top);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 46);
        const SDL_FRect bottom{x, y + p - std::max(1.0f, p * 0.09f), p, std::max(1.0f, p * 0.09f)};
        SDL_RenderFillRect(renderer, &bottom);

        // Valuable materials carry a category marker so they are readable
        // without relying on hue alone (Section 12.2).
        if (def.category != MaterialCategory::Rock && def.category != MaterialCategory::Boundary &&
            def.category != MaterialCategory::Structural) {
            const Color marker = category_marker(def.category);
            SDL_SetRenderDrawColor(renderer, marker.r, marker.g, marker.b, 210);
            const float m = std::max(2.0f, p * 0.22f);
            switch (def.category) {
                case MaterialCategory::Commodity: {
                    const SDL_FRect dot{x + p * 0.5f - m * 0.5f, y + p * 0.5f - m * 0.5f, m, m};
                    SDL_RenderFillRect(renderer, &dot);
                    break;
                }
                case MaterialCategory::Strategic: {
                    const SDL_FRect a{x + p * 0.5f - m * 0.5f, y + p * 0.28f, m, m * 0.4f};
                    const SDL_FRect b{x + p * 0.5f - m * 0.5f, y + p * 0.58f, m, m * 0.4f};
                    SDL_RenderFillRect(renderer, &a);
                    SDL_RenderFillRect(renderer, &b);
                    break;
                }
                case MaterialCategory::Volatile: {
                    const SDL_FRect a{x + p * 0.5f - m * 0.5f, y + p * 0.2f, m * 0.35f, m * 1.4f};
                    SDL_RenderFillRect(renderer, &a);
                    break;
                }
                case MaterialCategory::Artifact: {
                    const SDL_FRect a{x + p * 0.3f, y + p * 0.3f, p * 0.4f, p * 0.4f};
                    SDL_RenderFillRect(renderer, &a);
                    break;
                }
                default:
                    break;
            }
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }
    SDL_SetRenderTarget(renderer, previous);
    return tex;
}

void WorldRenderer::add_shake(float amount, const Settings& settings) {
    mShake = std::min(1.4f, mShake + amount * settings.access.screenShake);
}

void WorldRenderer::spawn_break_particles(Vec2 tile, Color c, const Settings& settings) {
    const int count = static_cast<int>(9.0f * settings.access.particleDensity);
    for (int i = 0; i < count; ++i) {
        mCosmeticState = splitmix64(mCosmeticState);
        const float a = static_cast<float>(mCosmeticState % 1000u) / 1000.0f * 6.2831853f;
        const float s = 1.5f + static_cast<float>((mCosmeticState >> 12) % 100u) / 40.0f;
        Particle p;
        p.position = tile;
        p.velocity = Vec2{std::cos(a) * s, std::sin(a) * s - 1.0f};
        p.maxLife = 0.35f + static_cast<float>((mCosmeticState >> 20) % 50u) / 120.0f;
        p.life = p.maxLife;
        p.color = c;
        p.size = 2.0f + static_cast<float>((mCosmeticState >> 30) % 3u);
        mParticles.push_back(p);
    }
}

void WorldRenderer::spawn_pickup(Vec2 tile, Color c, const Settings& settings) {
    const int count = static_cast<int>(4.0f * settings.access.particleDensity);
    for (int i = 0; i < count; ++i) {
        mCosmeticState = splitmix64(mCosmeticState);
        Particle p;
        p.position = tile;
        p.velocity = Vec2{0.0f, -2.0f - static_cast<float>(mCosmeticState % 20u) / 10.0f};
        p.maxLife = 0.5f;
        p.life = p.maxLife;
        p.color = c;
        p.size = 2.0f;
        mParticles.push_back(p);
    }
}

void WorldRenderer::update_effects(float dt) {
    mShake = std::max(0.0f, mShake - dt * 3.2f);
    mCosmeticState = splitmix64(mCosmeticState);
    const float ax = static_cast<float>(mCosmeticState % 200u) / 100.0f - 1.0f;
    const float ay = static_cast<float>((mCosmeticState >> 16) % 200u) / 100.0f - 1.0f;
    mShakeOffset = Vec2{ax * mShake * 7.0f, ay * mShake * 7.0f};
    for (usize i = 0; i < mParticles.size();) {
        mParticles[i].life -= dt;
        if (mParticles[i].life <= 0.0f) {
            mParticles[i] = mParticles.back();
            mParticles.pop_back();
            continue;
        }
        mParticles[i].velocity.y += 12.0f * dt;
        mParticles[i].position += mParticles[i].velocity * dt;
        ++i;
    }
}

void WorldRenderer::render(Draw& draw, const GameSession& session, const RenderSnapshot& snap,
                           const Camera& cam, const Settings& settings, float screenW, float screenH) {
    mDrawCalls = 0;
    const auto& db = session.content();
    const auto& biome = db.biomes()[static_cast<usize>(snap.biomeIndex)];

    // Background: biome sky above ground, fog colour below.
    const bool aboveGround = snap.depth < 4;
    const Color bg =
        aboveGround ? Color{biome.skyR, biome.skyG, biome.skyB} : Color{biome.fogR, biome.fogG, biome.fogB};
    draw.clear(bg);
    ++mDrawCalls;

    // Parallax strata behind the terrain: cheap, readable depth cue.
    for (int layer = 0; layer < 3; ++layer) {
        const float factor = 0.25f + static_cast<float>(layer) * 0.18f;
        const float offset = -cam.centre.y * cam.pixelsPerTile * factor;
        const float bandHeight = 140.0f + static_cast<float>(layer) * 60.0f;
        const float y = std::fmod(offset, bandHeight * 2.0f);
        const Color c =
            Color{biome.fogR, biome.fogG, biome.fogB}.shade(1.0f + 0.10f * static_cast<float>(layer + 1));
        for (float py = y - bandHeight * 2.0f; py < screenH + bandHeight; py += bandHeight * 2.0f) {
            draw.rect(0.0f, py, screenW, bandHeight, c.with_alpha(70));
            ++mDrawCalls;
        }
    }

    const int ppt = static_cast<int>(std::lround(cam.pixelsPerTile));
    const i64 halfTilesX = static_cast<i64>(screenW / cam.pixelsPerTile) / 2 + 2;
    const i64 halfTilesY = static_cast<i64>(screenH / cam.pixelsPerTile) / 2 + 2;
    const ChunkCoord c0 =
        chunk_of(TileCoord{ifloor(cam.centre.x) - halfTilesX, ifloor(cam.centre.y) - halfTilesY});
    const ChunkCoord c1 =
        chunk_of(TileCoord{ifloor(cam.centre.x) + halfTilesX, ifloor(cam.centre.y) + halfTilesY});

    for (i64 cy = c0.y; cy <= c1.y; ++cy) {
        for (i64 cx = c0.x; cx <= c1.x; ++cx) {
            const ChunkCoord coord{cx, cy};
            const u64 key = chunk_key(coord);
            const u64 hash = session.world().chunk(coord).content_hash();
            auto it = mChunkTextures.find(key);
            if (it != mChunkTextures.end() && (it->second.hash != hash || it->second.pixelsPerTile != ppt)) {
                if (it->second.texture != nullptr) {
                    SDL_DestroyTexture(it->second.texture);
                }
                mChunkTextures.erase(it);
                it = mChunkTextures.end();
            }
            if (it == mChunkTextures.end()) {
                CachedChunk cached;
                cached.texture = build_chunk_texture(draw, session, coord, settings, ppt);
                cached.hash = hash;
                cached.pixelsPerTile = ppt;
                it = mChunkTextures.emplace(key, cached).first;
            }
            if (it->second.texture == nullptr) {
                continue;
            }
            const Vec2 origin = cam.world_to_screen(
                Vec2{static_cast<float>(cx * kChunkSize), static_cast<float>(cy * kChunkSize)}, screenW,
                screenH);
            const SDL_FRect dst{origin.x, origin.y, static_cast<float>(kChunkSize * ppt),
                                static_cast<float>(kChunkSize * ppt)};
            SDL_RenderTexture(draw.renderer(), it->second.texture, nullptr, &dst);
            ++mDrawCalls;
        }
    }

    // Fog of war. Unexplored ground is not blanked out: solid rock keeps a dull
    // silhouette so the player can read cave shapes and plan a route, while the
    // material itself stays hidden until it is seen or scanned. That is what
    // leaves the scanner something worth revealing.
    const i64 minX = ifloor(cam.centre.x) - halfTilesX;
    const i64 maxX = ifloor(cam.centre.x) + halfTilesX;
    const i64 minY = ifloor(cam.centre.y) - halfTilesY;
    const i64 maxY = ifloor(cam.centre.y) + halfTilesY;
    const Color fogSolid{54, 50, 56, 246};
    const Color fogAir{12, 12, 17, 250};
    for (i64 y = minY; y <= maxY; ++y) {
        i64 runStart = 0;
        bool inRun = false;
        bool runSolid = false;
        auto flush = [&](i64 endX) {
            const Vec2 p = cam.world_to_screen(Vec2{static_cast<float>(runStart), static_cast<float>(y)},
                                               screenW, screenH);
            draw.rect(p.x, p.y, static_cast<float>(endX - runStart) * cam.pixelsPerTile, cam.pixelsPerTile,
                      runSolid ? fogSolid : fogAir);
            ++mDrawCalls;
        };
        for (i64 x = minX; x <= maxX + 1; ++x) {
            const bool inside = x <= maxX;
            const bool hidden = inside && !session.world().revealed(TileCoord{x, y});
            const bool solid = hidden && session.world().is_solid(TileCoord{x, y});
            if (hidden && (!inRun || solid != runSolid)) {
                if (inRun) {
                    flush(x);
                }
                runStart = x;
                runSolid = solid;
                inRun = true;
            } else if (!hidden && inRun) {
                flush(x);
                inRun = false;
            }
        }
    }

    // Landmarks the player has discovered, framed with corner brackets and a
    // label so they read as places rather than as boxes.
    for (const auto* lm : session.world().landmarks_near(
             TileCoord{ifloor(cam.centre.x), ifloor(cam.centre.y)}, static_cast<i32>(halfTilesX) + 8)) {
        const auto& def = db.landmarks()[static_cast<usize>(lm->definition)];
        const Vec2 p = cam.world_to_screen(
            Vec2{static_cast<float>(lm->origin.x), static_cast<float>(lm->origin.y)}, screenW, screenH);
        const float w = static_cast<float>(lm->width) * cam.pixelsPerTile;
        const float h = static_cast<float>(lm->height) * cam.pixelsPerTile;
        const Color c = lm->activated ? theme::kGood : theme::kCyan;
        const float arm = std::min(w, h) * 0.28f;
        const float t = 2.0f;
        auto bracket = [&](float bx, float by, float dx, float dy) {
            draw.rect(bx, by, arm * dx > 0 ? arm : t, t, c.with_alpha(210));
            draw.rect(bx, by, t, arm * dy > 0 ? arm : t, c.with_alpha(210));
        };
        bracket(p.x, p.y, 1.0f, 1.0f);
        draw.rect(p.x + w - arm, p.y, arm, t, c.with_alpha(210));
        draw.rect(p.x + w - t, p.y, t, arm, c.with_alpha(210));
        draw.rect(p.x, p.y + h - t, arm, t, c.with_alpha(210));
        draw.rect(p.x, p.y + h - arm, t, arm, c.with_alpha(210));
        draw.rect(p.x + w - arm, p.y + h - t, arm, t, c.with_alpha(210));
        draw.rect(p.x + w - t, p.y + h - arm, t, arm, c.with_alpha(210));

        const std::string name = pretty_key(def.nameKey);
        const float labelW = draw.measure(name, text_style::kCaption) + 10.0f;
        const float labelH = draw.line_height(text_style::kCaption) + 6.0f;
        draw.rect(p.x, p.y - labelH - 3.0f, labelW, labelH, Color{10, 12, 16, 215});
        draw.rect(p.x, p.y - labelH - 3.0f, 2.0f, labelH, c);
        draw.text(name, p.x + 6.0f, p.y - labelH, c, text_style::kCaption);
        mDrawCalls += 9;
    }

    // Mining target highlight and progress.
    if (snap.mining) {
        const Vec2 p = cam.world_to_screen(
            Vec2{static_cast<float>(snap.mineTarget.x), static_cast<float>(snap.mineTarget.y)}, screenW,
            screenH);
        draw.outline_xywh(p.x, p.y, cam.pixelsPerTile, cam.pixelsPerTile, theme::kAmber.with_alpha(220),
                          2.0f);
        // Crack overlay grows with progress so the player can read the timing.
        const float inset = cam.pixelsPerTile * 0.5f * (1.0f - snap.mineProgress);
        draw.rect(p.x + inset, p.y + inset, cam.pixelsPerTile - inset * 2.0f,
                  cam.pixelsPerTile - inset * 2.0f, Color{255, 255, 255, 60});
        ++mDrawCalls;
    }

    // Particles.
    for (const auto& p : mParticles) {
        const Vec2 s = cam.world_to_screen(p.position, screenW, screenH);
        const float alpha = clamp(p.life / p.maxLife, 0.0f, 1.0f);
        draw.rect(s.x, s.y, p.size, p.size, p.color.with_alpha(static_cast<u8>(alpha * 230.0f)));
    }
    mDrawCalls += static_cast<i32>(mParticles.size());

    // The rig. Drawn as a small machine rather than a square: a hull with a
    // lit cockpit, tread blocks and a drill head that points where it cuts.
    const Vec2 rigScreen = cam.world_to_screen(snap.rigPosition, screenW, screenH);
    const float p = cam.pixelsPerTile;
    const float bodyX = rigScreen.x + p * 0.07f;
    const float bodyY = rigScreen.y + p * 0.10f;
    const float bodyW = p * 0.86f;
    const float bodyH = p * 0.68f;
    // Shadow under the rig grounds it against the terrain.
    draw.rect(bodyX + p * 0.04f, rigScreen.y + p * 0.86f, bodyW, p * 0.10f, Color{0, 0, 0, 110});
    draw.rect(bodyX, bodyY, bodyW, bodyH, Color{150, 158, 176});
    draw.rect(bodyX, bodyY, bodyW, bodyH * 0.30f, Color{196, 204, 220});
    draw.rect(bodyX, bodyY + bodyH - p * 0.08f, bodyW, p * 0.08f, Color{86, 92, 106});
    // Cockpit glass, brighter while the drill is engaged.
    const Color glass = snap.mining ? Color{150, 226, 246} : Color{96, 168, 200};
    draw.rect(bodyX + bodyW * 0.12f, bodyY + bodyH * 0.18f, bodyW * 0.42f, bodyH * 0.44f, glass);
    draw.rect(bodyX + bodyW * 0.12f, bodyY + bodyH * 0.18f, bodyW * 0.42f, bodyH * 0.14f, glass.shade(1.4f));
    // Tread blocks.
    for (int i = 0; i < 3; ++i) {
        draw.rect(bodyX + p * 0.06f + static_cast<float>(i) * bodyW * 0.30f, rigScreen.y + p * 0.78f,
                  bodyW * 0.22f, p * 0.10f, Color{72, 78, 92});
    }
    // Drill head.
    Vec2 drill{0.0f, 0.42f};
    switch (snap.facing) {
        case MineDirection::Left:
            drill = Vec2{-0.44f, 0.05f};
            break;
        case MineDirection::Right:
            drill = Vec2{0.44f, 0.05f};
            break;
        case MineDirection::Up:
            drill = Vec2{0.0f, -0.40f};
            break;
        default:
            drill = Vec2{0.0f, 0.42f};
            break;
    }
    const float dsz = p * 0.26f;
    const float dx = rigScreen.x + p * (0.5f + drill.x) - dsz * 0.5f;
    const float dy = rigScreen.y + p * (0.5f + drill.y) - dsz * 0.5f;
    draw.rect(dx, dy, dsz, dsz, snap.mining ? theme::kAmberBright : Color{132, 138, 152});
    draw.rect(dx + dsz * 0.25f, dy + dsz * 0.25f, dsz * 0.5f, dsz * 0.5f,
              snap.mining ? theme::kWarn : Color{92, 98, 112});
    if (snap.mining) {
        // A soft glow at the bit while cutting.
        draw.rect(dx - dsz * 0.3f, dy - dsz * 0.3f, dsz * 1.6f, dsz * 1.6f, theme::kAmber.with_alpha(45));
    }
    mDrawCalls += 12;
}

} // namespace df
