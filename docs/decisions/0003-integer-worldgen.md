# 3. World generation is integer-only

Status: accepted

## Context

The brief requires that the same seed and content version reproduce the same
unmodified chunk **on every supported platform**, and warns against
implementation-dependent standard-library distributions.

Floating-point is a real risk here. `std::sin`, `std::pow` and friends are not
bit-identical across libm implementations; x87 excess precision, FMA contraction
and compiler fast-math flags can all change a low bit. A single bit difference
in a noise sample moves an ore vein, and a save made on Windows would then
disagree with the same seed on Linux.

## Decision

Every operation in world generation is integer-only.

- Noise is value noise on an integer lattice. Lattice values come from a
  64-bit hash of `(seed, x, y)`; interpolation is integer smoothstep in Q16.16
  fixed point (`3t^2 - 2t^3` computed with `i64` intermediates and shifts).
- The PRNG is xoshiro256\*\* with a SplitMix64 seeding step: shifts, XORs and
  multiplies only. Bounded integers use rejection sampling so the result is
  unbiased and identical everywhere.
- Streams are derived by hashing `(world seed, named stream, coordinates)`
  rather than by sharing one sequence, so consumers are order-independent.
- Vein and landmark placement is keyed to **world-space cells**, not chunks.
  Every chunk considers all cells overlapping its bounds plus a margin, and
  processes them in a fixed global order, so a vein is identical regardless of
  which chunk generated it first.

Floating point is still used freely in simulation and rendering, where
determinism is only required within a single platform and build.

## Consequences

- Cross-platform reproducibility is a property of the code, not a hope. The
  tests lock in known PRNG vectors, same-seed chunk equality, order-independence
  and border stability.
- Noise code is less readable than the float equivalent, so it is confined to
  `core/Noise.hpp` with the fixed-point contract documented at the top.
- `noise_to_bp` converts a Q16.16 sample to basis points, which is also the unit
  content data uses for thresholds and probabilities. Authors work in one scale.
- Changing any generation constant changes every existing seed, which is what
  `kWorldGenVersion` records.
