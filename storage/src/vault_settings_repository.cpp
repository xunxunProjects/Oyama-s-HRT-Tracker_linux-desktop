#include "oyama/storage/vault_settings_repository.h"

#include "oyama/storage/vault_codec.h"
#include "oyama/storage/vault_store.h"

#include <oyama/vault.h>

namespace oyama::storage {
namespace {

constexpr const char kSettingsId[] =
    "00000000-0000-0000-0000-000000000001";

}  // namespace

VaultSettingsRepository::VaultSettingsRepository(VaultStore& vault)
    : vault_(vault) {}

Settings VaultSettingsRepository::get() {
    const auto records = vault_.scan("settings");
    if (records.empty()) {
        return Settings{};
    }
    if (records.size() != 1) {
        throw VaultError(OYAMA_VAULT_ERR_CORRUPT,
                         "vault contains multiple settings records");
    }
    return decode_settings(records.front().payload);
}

void VaultSettingsRepository::save(const Settings& settings) {
    const auto stamp = settings.updated_at < 0
                           ? std::uint64_t{0}
                           : static_cast<std::uint64_t>(settings.updated_at);
    vault_.put(VaultRecord{kSettingsId, "settings", stamp,
                           encode_settings(settings)});
}

}  // namespace oyama::storage
