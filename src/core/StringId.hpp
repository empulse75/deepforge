// Stable string identifiers.
//
// Content data and save files always use the readable string form; runtime code
// resolves them once into typed handles (Section 15.2).
#pragma once

#include "core/Hash.hpp"
#include "core/Types.hpp"

#include <string>
#include <string_view>

namespace df {

/// Hashed, comparable identifier that remembers its text for diagnostics.
class StringId {
public:
    StringId() = default;

    explicit StringId(std::string_view text) : mText(text), mHash(fnv1a64(text)) {}

    const std::string& text() const noexcept { return mText; }

    u64 hash() const noexcept { return mHash; }

    bool empty() const noexcept { return mText.empty(); }

    friend bool operator==(const StringId& a, const StringId& b) noexcept { return a.mHash == b.mHash; }

    friend bool operator<(const StringId& a, const StringId& b) noexcept { return a.mText < b.mText; }

private:
    std::string mText;
    u64 mHash = 0;
};

/// Typed index handle into a definition table. Index -1 means unresolved.
template <typename Tag>
struct Handle {
    i32 index = -1;

    constexpr bool valid() const noexcept { return index >= 0; }

    friend constexpr bool operator==(const Handle&, const Handle&) = default;
};

struct MaterialTag {};

struct BiomeTag {};

struct EquipmentTag {};

struct ModuleTag {};

struct SkillTag {};

struct RecipeTag {};

struct FacilityTag {};

struct ContractTag {};

struct LandmarkTag {};

struct HazardTag {};

struct ResearchTag {};

using MaterialHandle = Handle<MaterialTag>;
using BiomeHandle = Handle<BiomeTag>;
using EquipmentHandle = Handle<EquipmentTag>;
using ModuleHandle = Handle<ModuleTag>;
using SkillHandle = Handle<SkillTag>;
using RecipeHandle = Handle<RecipeTag>;
using FacilityHandle = Handle<FacilityTag>;
using ContractHandle = Handle<ContractTag>;
using LandmarkHandle = Handle<LandmarkTag>;
using HazardHandle = Handle<HazardTag>;
using ResearchHandle = Handle<ResearchTag>;

} // namespace df

template <>
struct std::hash<df::StringId> {
    std::size_t operator()(const df::StringId& id) const noexcept {
        return static_cast<std::size_t>(id.hash());
    }
};
