#pragma once

#include "types.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string_view>
#include <vector>

namespace oyama::storage {

class VaultStore;

class VaultTombstoneRepository {
public:
    explicit VaultTombstoneRepository(VaultStore& vault);

    void record(const Tombstone& tombstone);
    /// Merges every tombstone in one vault scan and one transaction.  Duplicate
    /// (kind, record_id) entries keep the newest stamp and the existing record
    /// id; new pairs get a fresh UUID.  The input map is already unique for
    /// backup imports, but duplicates are accepted and collapsed.
    void record_many(const std::map<std::string,
                                    std::map<std::string, std::int64_t>>&
                         tombstones);
    [[nodiscard]] bool remove(std::string_view kind,
                              std::string_view record_id);
    [[nodiscard]] std::vector<Tombstone> list();
    [[nodiscard]] std::size_t prune_before(std::int64_t cutoff);

private:
    VaultStore& vault_;
};

}  // namespace oyama::storage
