// The monetisation catalog: cosmetics and the entitlements that sell them.
//
// This is deliberately its own layer, not part of src/content: the non-P2W
// invariant (SPEC, "Monetisation - non-P2W, testable") closes the simulation
// layers - domain, world, content, core - against the store, and CI greps all
// four for store identifiers. The store is parsed here and read only by
// presentation, tools and tests; nothing below this layer may link it (ADR 10).
//
// The kind enum is deliberately wider than what ships: a future category must
// be expressible without a schema migration, while the shipped catalog stays
// cosmetic-only and a test enforces that.
#pragma once

#include "core/Result.hpp"
#include "core/StringId.hpp"
#include "core/Types.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace df {

enum class EntitlementKind : u8 {
    Cosmetic,    ///< Presentation only; the only kind the first release sells.
    Expansion,   ///< Reserved: content packs. Must never gate a balance fix.
    Convenience, ///< Reserved: non-simulation quality of life.
    Count
};

EntitlementKind entitlement_kind_from_string(std::string_view s);
std::string_view entitlement_kind_name(EntitlementKind k);

/// A cosmetic the player can own: palette/trail/decor data, applied at draw
/// time only. The palette is opaque to the loader on purpose - the renderer
/// interprets it; the simulation never sees it.
struct CosmeticDefinition {
    StringId id;
    std::string nameKey;
    std::string descriptionKey;
    std::string family; ///< livery | trail | decor.
    std::vector<std::string> palette;
};

/// A purchasable grant. `simVisibleEffects` counts effect entries on the
/// definition; the non-P2W invariant requires zero for every shipped entry.
struct EntitlementDefinition {
    StringId id;
    std::string nameKey;
    std::string descriptionKey;
    EntitlementKind kind = EntitlementKind::Cosmetic;
    std::vector<StringId> grants; ///< Cosmetic ids this unlocks.
    i32 simVisibleEffects = 0;
};

class StoreCatalog {
public:
    /// Loads cosmetics.json and store_catalog.json from `dataDir`.
    static Result<StoreCatalog> load(const std::filesystem::path& dataDir, ErrorList& errors);

    /// Builds from in-memory JSON text; used by tests and the validator.
    static Result<StoreCatalog> load_from_strings(const std::string& cosmeticsJson,
                                                  const std::string& storeJson, ErrorList& errors);

    const std::vector<CosmeticDefinition>& cosmetics() const noexcept { return mCosmetics; }

    const std::vector<EntitlementDefinition>& entitlements() const noexcept { return mEntitlements; }

private:
    /// Parses both JSON texts (invalid JSON is reported and treated as absent).
    Status load_texts(const std::string& cosmeticsJson, const std::string& storeJson, ErrorList& errors);
    std::vector<CosmeticDefinition> mCosmetics;
    std::vector<EntitlementDefinition> mEntitlements;
};

} // namespace df
