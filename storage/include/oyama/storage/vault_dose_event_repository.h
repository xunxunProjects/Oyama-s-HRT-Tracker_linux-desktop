#pragma once
#include <oyama/compat/all.h>

#include "types.h"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace oyama { namespace storage {

class VaultStore;

class VaultDoseEventRepository {
public:
    explicit VaultDoseEventRepository(VaultStore& vault);

    OYAMA_NODISCARD std::vector<DoseEvent> list(Mode mode);
    OYAMA_NODISCARD oyama::optional<DoseEvent> get(oyama::string_view id);
    void upsert(const DoseEvent& event);
    OYAMA_NODISCARD bool remove(oyama::string_view id);
    OYAMA_NODISCARD std::size_t count(Mode mode);

private:
    VaultStore& vault_;
};

} }  // namespace oyama::storage
