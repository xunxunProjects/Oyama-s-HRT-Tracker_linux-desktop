#pragma once
#include <oyama/compat/all.h>

#include "types.h"
#include "vault_store.h"

namespace oyama { namespace storage {

class VaultSettingsRepository {
public:
    explicit VaultSettingsRepository(VaultStore& vault);

    OYAMA_NODISCARD Settings get();
    void save(const Settings& settings);

private:
    VaultStore& vault_;
};

} }  // namespace oyama::storage
