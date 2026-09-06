// Cargo and vault storage with explicit, rollback-safe transactions.
#pragma once

#include "content/ContentDatabase.hpp"
#include "core/Result.hpp"

#include <vector>

namespace df {

struct InventoryStack {
    i32 material = -1;
    i32 amount = 0;

    friend bool operator==(const InventoryStack&, const InventoryStack&) = default;
};

/// Holds material stacks. Cargo has a unit capacity; the surface vault does not.
class Inventory {
public:
    Inventory() = default;

    void set_capacity(float units) noexcept { mCapacity = units; }

    float capacity() const noexcept { return mCapacity; }

    /// Cargo units currently used (sum of amount * cargoUnits).
    float used_units(const ContentDatabase& db) const;

    float free_units(const ContentDatabase& db) const { return mCapacity - used_units(db); }

    bool full(const ContentDatabase& db) const { return free_units(db) < 1e-3f; }

    /// Adds as much as fits and returns the amount actually stored.
    i32 add(const ContentDatabase& db, i32 material, i32 amount, bool respectCapacity = true);

    /// Removes up to `amount`; returns the amount actually removed.
    i32 remove(i32 material, i32 amount);

    i32 count(i32 material) const;

    bool has(i32 material, i32 amount) const { return count(material) >= amount; }

    const std::vector<InventoryStack>& stacks() const noexcept { return mStacks; }

    void clear() { mStacks.clear(); }

    bool empty() const noexcept { return mStacks.empty(); }

    /// Total sale value at base prices, ignoring modifiers.
    i64 base_value(const ContentDatabase& db) const;

    void set_stacks(std::vector<InventoryStack> s) { mStacks = std::move(s); }

private:
    std::vector<InventoryStack> mStacks;
    float mCapacity = 1e9f;
};

/// A multi-step change that either applies completely or not at all.
class InventoryTransaction {
public:
    InventoryTransaction(const ContentDatabase& db, Inventory& inv) : mDb(&db), mInv(&inv), mBackup(inv) {}

    bool take(i32 material, i32 amount);
    bool give(i32 material, i32 amount, bool respectCapacity = true);

    void commit() noexcept { mCommitted = true; }

    ~InventoryTransaction() {
        if (!mCommitted) {
            *mInv = mBackup;
        }
    }

    InventoryTransaction(const InventoryTransaction&) = delete;
    InventoryTransaction& operator=(const InventoryTransaction&) = delete;

private:
    const ContentDatabase* mDb;
    Inventory* mInv;
    Inventory mBackup;
    bool mCommitted = false;
};

} // namespace df
