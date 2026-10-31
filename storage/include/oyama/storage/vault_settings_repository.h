#pragma once

#include "types.h"
#include "vault_store.h"

namespace oyama::storage {

class VaultSettingsRepository {
public:
    explicit VaultSettingsRepository(VaultStore& vault);

    [[nodiscard]] Settings get();
    void save(const Settings& settings);

private:
    VaultStore& vault_;
};

}  // namespace oyama::storage
