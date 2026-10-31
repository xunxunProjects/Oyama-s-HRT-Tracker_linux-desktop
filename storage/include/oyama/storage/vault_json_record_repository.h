#pragma once

#include "types.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oyama::storage {

class VaultStore;

class VaultJsonRecordRepository {
public:
    VaultJsonRecordRepository(VaultStore& vault, JsonRecordKind kind);
    [[nodiscard]] JsonRecordKind kind() const noexcept;
    [[nodiscard]] std::vector<JsonRecord> list(Mode mode);
    [[nodiscard]] std::optional<JsonRecord> get(std::string_view id);
    void upsert(const JsonRecord& record);
    [[nodiscard]] bool remove(std::string_view id);

private:
    VaultStore& vault_;
    JsonRecordKind kind_;
    std::string vault_kind_;
};

}  // namespace oyama::storage
