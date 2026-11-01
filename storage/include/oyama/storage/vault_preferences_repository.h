#pragma once
#include <oyama/compat/all.h>

#include "types.h"
#include "vault_store.h"

#include <optional>
#include <string_view>
#include <vector>

namespace oyama { namespace storage {

class VaultPreferencesRepository {
public:
    explicit VaultPreferencesRepository(VaultStore& vault);

    OYAMA_NODISCARD oyama::optional<Preference> get(oyama::string_view key);
    OYAMA_NODISCARD std::vector<Preference> list();
    void set(const Preference& preference);
    OYAMA_NODISCARD bool remove(oyama::string_view key);

private:
    VaultStore& vault_;
};

} }  // namespace oyama::storage
