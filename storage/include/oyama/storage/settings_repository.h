#pragma once

#include "connection.h"
#include "types.h"

#include <optional>

namespace oyama::storage {

/// The single global settings row.  get() creates the default row if it is
/// missing, so callers always receive usable values.
class SettingsRepository {
public:
    explicit SettingsRepository(Connection& connection);

    [[nodiscard]] Settings get();
    void save(const Settings& settings);

private:
    Connection& connection_;
};

}  // namespace oyama::storage
