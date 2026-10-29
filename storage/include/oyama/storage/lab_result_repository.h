#pragma once

#include "connection.h"
#include "types.h"

#include <optional>
#include <string_view>
#include <vector>

namespace oyama::storage {

class LabResultRepository {
public:
    explicit LabResultRepository(Connection& connection);

    [[nodiscard]] std::vector<LabResult> list(Mode mode);
    [[nodiscard]] std::optional<LabResult> get(std::string_view id);
    void upsert(const LabResult& result);
    [[nodiscard]] bool remove(std::string_view id);

private:
    Connection& connection_;
};

}  // namespace oyama::storage
