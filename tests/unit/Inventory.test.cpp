#include "TestSupport.hpp"

#include "domain/Inventory.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace df;

TEST_CASE("inventory respects cargo capacity", "[inventory]") {
    const auto& db = dft::shipped_content();
    const auto ferrite = db.material("material.ferrite");
    REQUIRE(ferrite.valid());

    Inventory inv;
    inv.set_capacity(10.0f);
    CHECK(inv.add(db, ferrite.index, 4) == 4);
    CHECK(inv.add(db, ferrite.index, 20) == 6);
    CHECK(inv.count(ferrite.index) == 10);
    CHECK(inv.full(db));
    CHECK(inv.add(db, ferrite.index, 1) == 0);
}

TEST_CASE("inventory stacks and removes correctly", "[inventory]") {
    const auto& db = dft::shipped_content();
    const auto a = db.material("material.ferrite");
    const auto b = db.material("material.cuprite");
    Inventory inv;
    inv.set_capacity(100.0f);
    inv.add(db, a.index, 5);
    inv.add(db, b.index, 3);
    inv.add(db, a.index, 2);
    CHECK(inv.stacks().size() == 2);
    CHECK(inv.count(a.index) == 7);
    CHECK(inv.remove(a.index, 3) == 3);
    CHECK(inv.count(a.index) == 4);
    CHECK(inv.remove(a.index, 99) == 4);
    CHECK(inv.count(a.index) == 0);
    CHECK(inv.stacks().size() == 1);
}

TEST_CASE("inventory quantities never go negative", "[inventory]") {
    const auto& db = dft::shipped_content();
    const auto a = db.material("material.ferrite");
    Inventory inv;
    inv.set_capacity(50.0f);
    CHECK(inv.remove(a.index, 5) == 0);
    CHECK(inv.add(db, a.index, -3) == 0);
    CHECK(inv.add(db, -1, 5) == 0);
    CHECK(inv.count(a.index) == 0);
    for (const auto& s : inv.stacks()) {
        CHECK(s.amount >= 0);
    }
}

TEST_CASE("a failed transaction rolls the inventory back", "[inventory][transaction]") {
    const auto& db = dft::shipped_content();
    const auto a = db.material("material.ferrite");
    const auto b = db.material("material.cuprite");
    Inventory inv;
    inv.set_capacity(100.0f);
    inv.add(db, a.index, 10);
    inv.add(db, b.index, 2);
    const auto before = inv.stacks();
    {
        InventoryTransaction tx(db, inv);
        REQUIRE(tx.take(a.index, 5));
        REQUIRE_FALSE(tx.take(b.index, 99)); // insufficient: abort
        // No commit, so the destructor restores the snapshot.
    }
    CHECK(inv.stacks() == before);

    {
        InventoryTransaction tx(db, inv);
        REQUIRE(tx.take(a.index, 5));
        tx.commit();
    }
    CHECK(inv.count(a.index) == 5);
}

TEST_CASE("protected samples use their own storage", "[inventory]") {
    const auto& db = dft::shipped_content();
    const auto shard = db.material("material.echo_shard");
    REQUIRE(shard.valid());
    CHECK(db.material_def(shard).protectedSample);
    Inventory samples;
    samples.set_capacity(1.0f);
    CHECK(samples.add(db, shard.index, 1) == 1);
    CHECK(samples.add(db, shard.index, 1) == 0);
}

TEST_CASE("base value sums the whole hold", "[inventory]") {
    const auto& db = dft::shipped_content();
    const auto a = db.material("material.ferrite");
    const auto b = db.material("material.cuprite");
    Inventory inv;
    inv.set_capacity(1000.0f);
    inv.add(db, a.index, 10);
    inv.add(db, b.index, 4);
    const i64 expected = static_cast<i64>(db.material_def(a).baseValue) * 10 +
                         static_cast<i64>(db.material_def(b).baseValue) * 4;
    CHECK(inv.base_value(db) == expected);
}
