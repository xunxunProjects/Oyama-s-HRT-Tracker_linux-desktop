#pragma once

#include "connection.h"
#include "types.h"

#include <optional>
#include <string_view>
#include <vector>

namespace oyama::storage {

class PreferencesRepository {
public:
    explicit PreferencesRepository(Connection& connection);

    [[nodiscard]] std::optional<Preference> get(std::string_view key);
    [[nodiscard]] std::vector<Preference> list();
    void set(const Preference& preference);
    [[nodiscard]] bool remove(std::string_view key);

private:
    Connection& connection_;
};

}  // namespace oyama::storage
