#pragma once

#include "types.h"
#include "vault_store.h"

#include <optional>
#include <string_view>
#include <vector>

namespace oyama::storage {

class VaultPreferencesRepository {
public:
    explicit VaultPreferencesRepository(VaultStore& vault);

    [[nodiscard]] std::optional<Preference> get(std::string_view key);
    [[nodiscard]] std::vector<Preference> list();
    void set(const Preference& preference);
    [[nodiscard]] bool remove(std::string_view key);

private:
    VaultStore& vault_;
};

}  // namespace oyama::storage
