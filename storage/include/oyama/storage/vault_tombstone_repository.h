#pragma once

#include "types.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace oyama::storage {

class VaultStore;

class VaultTombstoneRepository {
public:
    explicit VaultTombstoneRepository(VaultStore& vault);

    void record(const Tombstone& tombstone);
    [[nodiscard]] bool remove(std::string_view kind,
                              std::string_view record_id);
    [[nodiscard]] std::vector<Tombstone> list();
    [[nodiscard]] std::size_t prune_before(std::int64_t cutoff);

private:
    VaultStore& vault_;
};

}  // namespace oyama::storage
