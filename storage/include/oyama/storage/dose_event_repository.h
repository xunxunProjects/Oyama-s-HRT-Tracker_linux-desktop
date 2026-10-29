#pragma once

#include "connection.h"
#include "types.h"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace oyama::storage {

class DoseEventRepository {
public:
    explicit DoseEventRepository(Connection& connection);

    [[nodiscard]] std::vector<DoseEvent> list(Mode mode);
    [[nodiscard]] std::optional<DoseEvent> get(std::string_view id);
    void upsert(const DoseEvent& event);
    [[nodiscard]] bool remove(std::string_view id);
    [[nodiscard]] std::size_t count(Mode mode);

private:
    Connection& connection_;
};

}  // namespace oyama::storage
