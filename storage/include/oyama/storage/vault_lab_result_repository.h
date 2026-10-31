#pragma once

#include "types.h"

#include <optional>
#include <string_view>
#include <vector>

namespace oyama::storage {

class VaultStore;

class VaultLabResultRepository {
public:
    explicit VaultLabResultRepository(VaultStore& vault);

    [[nodiscard]] std::vector<LabResult> list(Mode mode);
    [[nodiscard]] std::optional<LabResult> get(std::string_view id);
    void upsert(const LabResult& result);
    [[nodiscard]] bool remove(std::string_view id);

private:
    VaultStore& vault_;
};

}  // namespace oyama::storage
