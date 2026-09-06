# 4. Terrain storage and persistence deltas

Status: accepted

## Context

A world 840 tiles wide and 4200 deep is about 3.5 million tiles. It must stream,
persist what the player changed, survive eviction and reload, and never turn
into an entity explosion.

## Decision

A chunk is 32x32 tiles held as three fixed arrays — material index, cosmetic
variant, flags — plus small vectors for landmarks, hazards and *sparse* mining
progress. No tile is an entity. Rendering metadata is derived, not stored.

Persistence stores only divergence from generation:

- changed tiles as `(local index, material)`
- partial mining progress
- landmark activated/looted state
- fog as a 128-byte bitset per chunk

The fog bitset is the reason this stays small: walking through a chunk reveals
hundreds of tiles, and recording those individually would dwarf the actual
gameplay changes.

On load, a chunk is regenerated from the seed and the delta is replayed over it.
Deltas outlive the chunk cache, so an evicted chunk returns exactly as the player
left it.

An LRU cache bounds resident chunks (512 by default, roughly 6 MB of tile data).

## Consequences

- A save is small and stays proportional to what the player actually did.
- `WorldMap::ensure_chunk` is the single entry point for generation and delta
  replay, which is also the seam where a worker thread would publish results.
- Chunk textures in the renderer are keyed to a content hash, so a chunk is
  re-rasterised only when it actually changes.
- Changing generation invalidates the terrain a save expects. `kWorldGenVersion`
  is stored in every save so this is detectable rather than silent.
