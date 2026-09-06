#include "domain/Inventory.hpp"

#include <algorithm>

namespace df {

float Inventory::used_units(const ContentDatabase& db) const {
    float total = 0.0f;
    for (const auto& s : mStacks) {
        if (s.material < 0 || static_cast<usize>(s.material) >= db.materials().size()) {
            continue;
        }
        total += static_cast<float>(db.materials()[static_cast<usize>(s.material)].cargoUnits) *
                 static_cast<float>(s.amount);
    }
    return total;
}

i32 Inventory::add(const ContentDatabase& db, i32 material, i32 amount, bool respectCapacity) {
    if (material < 0 || amount <= 0 || static_cast<usize>(material) >= db.materials().size()) {
        return 0;
    }
    const auto& def = db.materials()[static_cast<usize>(material)];
    i32 storable = amount;
    if (respectCapacity && def.cargoUnits > 0) {
        const float free = free_units(db);
        storable = std::min(amount, static_cast<i32>(free / static_cast<float>(def.cargoUnits)));
        if (storable <= 0) {
            return 0;
        }
    }
    for (auto& s : mStacks) {
        if (s.material == material) {
            s.amount += storable;
            return storable;
        }
    }
    mStacks.push_back(InventoryStack{material, storable});
    return storable;
}

i32 Inventory::remove(i32 material, i32 amount) {
    if (amount <= 0) {
        return 0;
    }
    for (usize i = 0; i < mStacks.size(); ++i) {
        if (mStacks[i].material != material) {
            continue;
        }
        const i32 taken = std::min(amount, mStacks[i].amount);
        mStacks[i].amount -= taken;
        if (mStacks[i].amount <= 0) {
            mStacks.erase(mStacks.begin() + static_cast<std::ptrdiff_t>(i));
        }
        return taken;
    }
    return 0;
}

i32 Inventory::count(i32 material) const {
    for (const auto& s : mStacks) {
        if (s.material == material) {
            return s.amount;
        }
    }
    return 0;
}

i64 Inventory::base_value(const ContentDatabase& db) const {
    i64 total = 0;
    for (const auto& s : mStacks) {
        if (s.material < 0 || static_cast<usize>(s.material) >= db.materials().size()) {
            continue;
        }
        total += static_cast<i64>(db.materials()[static_cast<usize>(s.material)].baseValue) * s.amount;
    }
    return total;
}

bool InventoryTransaction::take(i32 material, i32 amount) {
    if (mInv->count(material) < amount) {
        return false;
    }
    return mInv->remove(material, amount) == amount;
}

bool InventoryTransaction::give(i32 material, i32 amount, bool respectCapacity) {
    return mInv->add(*mDb, material, amount, respectCapacity) == amount;
}

} // namespace df
