#pragma once

#include "types.h"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace oyama::storage {

class VaultStore;

class VaultDoseEventRepository {
public:
    explicit VaultDoseEventRepository(VaultStore& vault);

    [[nodiscard]] std::vector<DoseEvent> list(Mode mode);
    [[nodiscard]] std::optional<DoseEvent> get(std::string_view id);
    void upsert(const DoseEvent& event);
    [[nodiscard]] bool remove(std::string_view id);
    [[nodiscard]] std::size_t count(Mode mode);

private:
    VaultStore& vault_;
};

}  // namespace oyama::storage
