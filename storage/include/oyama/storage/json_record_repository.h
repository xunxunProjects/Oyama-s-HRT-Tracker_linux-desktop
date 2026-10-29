#pragma once

#include "connection.h"
#include "types.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oyama::storage {

/// Repository for the JSON-carrying tables: dose templates, quick doses,
/// supplies and plan items.  Payload shape stays with the upstream backup
/// format until the fields settle.
class JsonRecordRepository {
public:
    JsonRecordRepository(Connection& connection, JsonRecordKind kind);

    [[nodiscard]] JsonRecordKind kind() const noexcept;
    [[nodiscard]] std::vector<JsonRecord> list(Mode mode);
    [[nodiscard]] std::optional<JsonRecord> get(std::string_view id);
    void upsert(const JsonRecord& record);
    [[nodiscard]] bool remove(std::string_view id);

private:
    Connection& connection_;
    JsonRecordKind kind_;
    std::string table_;
};

}  // namespace oyama::storage
