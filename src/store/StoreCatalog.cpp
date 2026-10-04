#include "store/StoreCatalog.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

namespace df {

using json = nlohmann::json;

EntitlementKind entitlement_kind_from_string(std::string_view s) {
    if (s == "cosmetic")
        return EntitlementKind::Cosmetic;
    if (s == "expansion")
        return EntitlementKind::Expansion;
    if (s == "convenience")
        return EntitlementKind::Convenience;
    return EntitlementKind::Count;
}

std::string_view entitlement_kind_name(EntitlementKind k) {
    switch (k) {
        case EntitlementKind::Cosmetic:
            return "cosmetic";
        case EntitlementKind::Expansion:
            return "expansion";
        case EntitlementKind::Convenience:
            return "convenience";
        case EntitlementKind::Count:
            break;
    }
    return "unknown";
}

Status StoreCatalog::load_texts(const std::string& cosmeticsText, const std::string& storeText,
                                ErrorList& errors) {
    if (!cosmeticsText.empty()) {
        json root = json::parse(cosmeticsText, nullptr, false, true);
        if (root.is_discarded()) {
            errors.add(Error{"file is not valid JSON", "cosmetics.json", {}, {}, {}});
        } else if (!root.contains("cosmetics") || !root["cosmetics"].is_array()) {
            errors.add(Error{"expected a \"cosmetics\" array", "cosmetics.json", {}, "cosmetics", {}});
        } else {
            for (const auto& j : root["cosmetics"]) {
                CosmeticDefinition def;
                def.id = StringId{j.value("id", "")};
                if (def.id.empty()) {
                    errors.add(Error{"cosmetic without an id", "cosmetics.json", {}, "id", {}});
                    continue;
                }
                for (const auto& other : mCosmetics) {
                    if (other.id == def.id) {
                        errors.add(
                            Error{"duplicate cosmetic id '" + def.id.text() + "'", "cosmetics.json",
                                  def.id.text(), "id", {}});
                    }
                }
                def.nameKey = j.value("name_key", def.id.text() + ".name");
                def.descriptionKey = j.value("description_key", def.id.text() + ".description");
                def.family = j.value("family", "");
                if (def.family != "livery" && def.family != "trail" && def.family != "decor") {
                    errors.add(Error{"unknown cosmetic family '" + def.family + "'", "cosmetics.json",
                                     def.id.text(), "family", "use livery, trail or decor"});
                    continue;
                }
                if (j.contains("palette") && j["palette"].is_array()) {
                    for (const auto& p : j["palette"]) {
                        if (p.is_string()) {
                            def.palette.push_back(p.get<std::string>());
                        }
                    }
                }
                if (def.palette.empty()) {
                    errors.add(Error{"a cosmetic needs at least one palette entry", "cosmetics.json",
                                     def.id.text(), "palette",
                                     "the renderer draws from this list; an empty one draws nothing"});
                }
                mCosmetics.push_back(std::move(def));
            }
        }
    }

    if (!storeText.empty()) {
        json root = json::parse(storeText, nullptr, false, true);
        if (root.is_discarded()) {
            errors.add(Error{"file is not valid JSON", "store_catalog.json", {}, {}, {}});
        } else if (!root.contains("entitlements") || !root["entitlements"].is_array()) {
            errors.add(
                Error{"expected an \"entitlements\" array", "store_catalog.json", {}, "entitlements",
                      {}});
        } else {
            for (const auto& j : root["entitlements"]) {
                EntitlementDefinition def;
                def.id = StringId{j.value("id", "")};
                if (def.id.empty()) {
                    errors.add(Error{"entitlement without an id", "store_catalog.json", {}, "id", {}});
                    continue;
                }
                for (const auto& other : mEntitlements) {
                    if (other.id == def.id) {
                        errors.add(Error{"duplicate entitlement id '" + def.id.text() + "'",
                                         "store_catalog.json", def.id.text(), "id", {}});
                    }
                }
                def.nameKey = j.value("name_key", def.id.text() + ".name");
                def.descriptionKey = j.value("description_key", def.id.text() + ".description");
                def.kind = entitlement_kind_from_string(j.value("kind", "cosmetic"));
                if (def.kind == EntitlementKind::Count) {
                    errors.add(Error{"unknown entitlement kind '" + j.value("kind", "") + "'",
                                     "store_catalog.json", def.id.text(), "kind",
                                     "see the kind list in store/StoreCatalog.hpp"});
                    continue;
                }
                if (j.contains("grants") && j["grants"].is_array()) {
                    for (const auto& g : j["grants"]) {
                        if (g.is_string()) {
                            def.grants.emplace_back(g.get<std::string>());
                        }
                    }
                }
                if (def.grants.empty()) {
                    errors.add(Error{"an entitlement must grant at least one cosmetic",
                                     "store_catalog.json", def.id.text(), "grants",
                                     "an entitlement that grants nothing is a charge for nothing"});
                }
                for (const auto& g : def.grants) {
                    bool found = false;
                    for (const auto& c : mCosmetics) {
                        if (c.id == g) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        errors.add(Error{"grants an unknown cosmetic '" + g.text() + "'",
                                         "store_catalog.json", def.id.text(), "grants",
                                         "add it to cosmetics.json or remove the grant"});
                    }
                }
                // The non-P2W invariant counts sim-visible effects per
                // definition; the shipped catalog must carry none.
                def.simVisibleEffects =
                    (j.contains("effects") && j["effects"].is_array())
                        ? static_cast<i32>(j["effects"].size())
                        : 0;
                mEntitlements.push_back(std::move(def));
            }
        }
    }

    if (!errors.empty()) {
        return fail(errors.all().back());
    }
    return {};
}

Result<StoreCatalog> StoreCatalog::load(const std::filesystem::path& dataDir, ErrorList& errors) {
    auto read = [&dataDir](const char* name) -> std::string {
        std::ifstream in(dataDir / name, std::ios::binary);
        if (!in) {
            return {}; // both files are optional content; an empty catalog is valid
        }
        std::ostringstream os;
        os << in.rdbuf();
        return os.str();
    };
    StoreCatalog out;
    if (!out.load_texts(read("cosmetics.json"), read("store_catalog.json"), errors)) {
        return fail(errors.all().back());
    }
    return out;
}

Result<StoreCatalog> StoreCatalog::load_from_strings(const std::string& cosmeticsJson,
                                                     const std::string& storeJson, ErrorList& errors) {
    StoreCatalog out;
    if (!out.load_texts(cosmeticsJson, storeJson, errors)) {
        return fail(errors.all().back());
    }
    return out;
}

} // namespace df
