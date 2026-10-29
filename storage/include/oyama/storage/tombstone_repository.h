#pragma once

#include "connection.h"
#include "types.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace oyama::storage {

class TombstoneRepository {
public:
    explicit TombstoneRepository(Connection& connection);

    void record(const Tombstone& tombstone);
    [[nodiscard]] bool remove(std::string_view kind, std::string_view record_id);
    [[nodiscard]] std::vector<Tombstone> list();
    [[nodiscard]] std::size_t prune_before(std::int64_t cutoff);

private:
    Connection& connection_;
};

}  // namespace oyama::storage
