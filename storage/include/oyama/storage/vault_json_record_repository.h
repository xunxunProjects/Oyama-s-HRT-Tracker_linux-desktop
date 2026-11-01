#pragma once
#include <oyama/compat/all.h>

#include "types.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oyama { namespace storage {

class VaultStore;

class VaultJsonRecordRepository {
public:
    VaultJsonRecordRepository(VaultStore& vault, JsonRecordKind kind);
    OYAMA_NODISCARD JsonRecordKind kind() const noexcept;
    OYAMA_NODISCARD std::vector<JsonRecord> list(Mode mode);
    OYAMA_NODISCARD oyama::optional<JsonRecord> get(oyama::string_view id);
    void upsert(const JsonRecord& record);
    OYAMA_NODISCARD bool remove(oyama::string_view id);

private:
    VaultStore& vault_;
    JsonRecordKind kind_;
    std::string vault_kind_;
};

} }  // namespace oyama::storage
