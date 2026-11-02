#include "oyama/storage/vault_preferences_repository.h"

#include "oyama/storage/vault_codec.h"
#include "oyama/storage/vault_store.h"

#include <oyama/vault.h>

#include <algorithm>

namespace oyama { namespace storage {
namespace {

constexpr const char kPreferencesId[] =
    "00000000-0000-0000-0000-000000000002";

std::vector<Preference> load_preferences(VaultStore& vault) {
    const auto records = vault.scan("preferences");
    if (records.empty()) {
        return {};
    }
    if (records.size() != 1) {
        throw VaultError(OYAMA_VAULT_ERR_CORRUPT,
                         "vault contains multiple preferences records");
    }
    return decode_preferences(records.front().payload);
}

void save_preferences(VaultStore& vault, std::vector<Preference> preferences) {
    std::sort(preferences.begin(), preferences.end(),
              [](const Preference& left, const Preference& right) {
                  return left.key < right.key;
              });
    std::int64_t stamp = 0;
    for (const auto& preference : preferences) {
        stamp = std::max(stamp, preference.updated_at);
    }
    const auto envelope_stamp = stamp < 0
                                    ? std::uint64_t{0}
                                    : static_cast<std::uint64_t>(stamp);
    vault.put(VaultRecord{kPreferencesId, "preferences", envelope_stamp,
                          encode_preferences(preferences)});
}

}  // namespace

VaultPreferencesRepository::VaultPreferencesRepository(VaultStore& vault)
    : vault_(vault) {}

oyama::optional<Preference> VaultPreferencesRepository::get(
    oyama::string_view key) {
    const auto preferences = list();
    const auto found = std::find_if(
        preferences.begin(), preferences.end(),
        [key](const Preference& preference) { return preference.key == key; });
    if (found == preferences.end()) {
        return oyama::nullopt;
    }
    return *found;
}

std::vector<Preference> VaultPreferencesRepository::list() {
    auto preferences = load_preferences(vault_);
    std::sort(preferences.begin(), preferences.end(),
              [](const Preference& left, const Preference& right) {
                  return left.key < right.key;
              });
    return preferences;
}

void VaultPreferencesRepository::set(const Preference& preference) {
    auto preferences = load_preferences(vault_);
    const auto found = std::find_if(
        preferences.begin(), preferences.end(),
        [&preference](const Preference& existing) {
            return existing.key == preference.key;
        });
    if (found == preferences.end()) {
        preferences.push_back(preference);
    } else {
        *found = preference;
    }
    save_preferences(vault_, std::move(preferences));
}

bool VaultPreferencesRepository::remove(oyama::string_view key) {
    auto preferences = load_preferences(vault_);
    const auto found = std::find_if(
        preferences.begin(), preferences.end(),
        [key](const Preference& preference) { return preference.key == key; });
    if (found == preferences.end()) {
        return false;
    }
    preferences.erase(found);
    save_preferences(vault_, std::move(preferences));
    return true;
}

} }  // namespace oyama::storage
