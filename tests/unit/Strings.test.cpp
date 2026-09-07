// The string table is what every screen reads. A gap in it does not crash
// anything - it quietly degrades a name into a title-cased identifier - so it
// has to be checked rather than noticed.
#include "TestSupport.hpp"

#include "content/ContentDatabase.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

using namespace df;

namespace {

/// Every key the shipped content points a screen at.
std::vector<std::string> referenced_keys(const ContentDatabase& db) {
    std::vector<std::string> keys;
    auto add = [&keys](const std::string& k) {
        if (!k.empty()) {
            keys.push_back(k);
        }
    };
    for (const auto& m : db.materials()) {
        add(m.nameKey);
        add(m.descriptionKey);
    }
    for (const auto& b : db.biomes()) {
        add(b.nameKey);
        add(b.descriptionKey);
    }
    for (const auto& e : db.equipment()) {
        add(e.nameKey);
        add(e.descriptionKey);
        for (const auto& t : e.tiers) {
            add(t.nameKey);
            add(t.summaryKey);
        }
    }
    for (const auto& m : db.modules()) {
        add(m.nameKey);
        add(m.descriptionKey);
    }
    for (const auto& s : db.skills()) {
        add(s.nameKey);
        add(s.descriptionKey);
    }
    for (const auto& r : db.recipes()) {
        add(r.nameKey);
        add(r.descriptionKey);
    }
    for (const auto& f : db.facilities()) {
        add(f.nameKey);
        add(f.descriptionKey);
    }
    for (const auto& c : db.contracts()) {
        add(c.nameKey);
        add(c.descriptionKey);
    }
    for (const auto& l : db.landmarks()) {
        add(l.nameKey);
        add(l.descriptionKey);
    }
    for (const auto& h : db.hazards()) {
        add(h.nameKey);
        add(h.descriptionKey);
    }
    for (const auto& r : db.research()) {
        add(r.nameKey);
        add(r.descriptionKey);
    }
    for (const auto& f : db.factions()) {
        add(f.nameKey);
        add(f.descriptionKey);
    }
    return keys;
}

} // namespace

TEST_CASE("every content key has authored text") {
    const auto& db = dft::shipped_content();
    std::string missing;
    for (const auto& key : referenced_keys(db)) {
        if (db.text(key).empty()) {
            missing += "  " + key + "\n";
        }
    }
    INFO("keys with no entry in strings.en.json:\n" << missing);
    CHECK(missing.empty());
}

TEST_CASE("the string table has no unreferenced entries") {
    const auto& db = dft::shipped_content();
    const auto refs = referenced_keys(db);
    const std::unordered_set<std::string> referenced(refs.begin(), refs.end());
    std::string orphans;
    for (const auto& [key, value] : db.strings()) {
        if (referenced.count(key) == 0) {
            orphans += "  " + key + "\n";
        }
    }
    INFO("entries no screen can reach:\n" << orphans);
    CHECK(orphans.empty());
}

TEST_CASE("authored text is presentable") {
    const auto& db = dft::shipped_content();
    for (const auto& [key, value] : db.strings()) {
        INFO("key: " << key);
        CHECK_FALSE(value.empty());
        // A leading or trailing space is invisible in the file and misaligns
        // every label that uses it.
        CHECK(value.front() != ' ');
        CHECK(value.back() != ' ');
        // Names are labels, not sentences; descriptions are the other way round.
        if (key.ends_with(".name")) {
            CHECK(value.size() <= 40);
            CHECK(value.find('\n') == std::string::npos);
        }
        // The Ui face covers printable ASCII only. Anything outside it draws as
        // a blank, which is the sort of thing that ships unnoticed.
        for (const char raw : value) {
            const auto ch = static_cast<unsigned char>(raw);
            CHECK(ch >= 32);
            CHECK(ch <= 126);
        }
    }
}
