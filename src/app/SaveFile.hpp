// Versioned, atomic, checksummed save files with explicit migrations.
//
// Save data is a product feature: a failed write must never destroy a good
// save, and a save from any released schema version must load (Section 19).
#pragma once

#include "content/ContentDatabase.hpp"
#include "core/Result.hpp"
#include "domain/Expedition.hpp"
#include "domain/PlayerProgress.hpp"
#include "domain/RigState.hpp"
#include "world/WorldMap.hpp"

#include <filesystem>
#include <string>

namespace df {

/// Everything a slot persists.
struct SaveData {
    u64 worldSeed = 0;
    std::string worldName;
    PlayerProgress progress;
    RigState rig;
    Inventory cargo;
    Inventory samples;
    std::vector<ChunkDelta> worldDeltas;
    double playSeconds = 0.0;
    i32 saveSchemaVersion = 0;
    i32 contentVersion = 0;
    i32 worldGenVersion = 0;
    /// Which content pack generated this world. The base game is "base";
    /// expansion packs record their own id so a later pack cannot silently
    /// reinterpret a save carved from different content.
    std::string contentPackId = "base";
    std::string buildVersion;
    i64 timestamp = 0;
    std::string summary;
};

/// Metadata shown in the load menu without deserialising the whole save.
struct SaveSlotInfo {
    int slot = 0;
    bool exists = false;
    bool damaged = false;
    bool backupAvailable = false;
    std::string summary;
    std::string buildVersion;
    i32 schemaVersion = 0;
    i64 timestamp = 0;
    double playSeconds = 0.0;
    i64 credits = 0;
    i64 deepestDepth = 0;
};

class SaveSystem {
public:
    explicit SaveSystem(std::filesystem::path saveDir) : mDir(std::move(saveDir)) {}

    const std::filesystem::path& directory() const noexcept { return mDir; }

    std::filesystem::path slot_path(int slot) const;
    std::filesystem::path backup_path(int slot) const;

    /// Serialises to a string. Exposed so tests can round trip without disk.
    Result<std::string> serialise(const ContentDatabase& db, const SaveData& data) const;

    /// Parses and migrates. Never throws; every failure carries context.
    Result<SaveData> deserialise(const ContentDatabase& db, const std::string& text) const;

    /// Writes atomically: temp file, flush, validate, rotate backup, replace.
    Status write(const ContentDatabase& db, int slot, const SaveData& data) const;

    /// Loads a slot, falling back to the backup only when asked.
    Result<SaveData> read(const ContentDatabase& db, int slot, bool useBackup = false) const;

    SaveSlotInfo inspect(const ContentDatabase& db, int slot) const;

    std::vector<SaveSlotInfo> list_slots(const ContentDatabase& db, int maxSlots = 6) const;

    static u64 checksum(std::string_view payload);

private:
    std::filesystem::path mDir;
};

/// Platform-appropriate writable directory for saves and logs.
std::filesystem::path default_user_data_dir();

} // namespace df
