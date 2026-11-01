#pragma once
#include <oyama/compat/all.h>

#include "types.h"

#include <optional>
#include <string_view>
#include <vector>

namespace oyama { namespace storage {

class VaultStore;

class VaultLabResultRepository {
public:
    explicit VaultLabResultRepository(VaultStore& vault);

    OYAMA_NODISCARD std::vector<LabResult> list(Mode mode);
    OYAMA_NODISCARD oyama::optional<LabResult> get(oyama::string_view id);
    void upsert(const LabResult& result);
    OYAMA_NODISCARD bool remove(oyama::string_view id);

private:
    VaultStore& vault_;
};

} }  // namespace oyama::storage
