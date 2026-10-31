#include "oyama/storage/vault_tombstone_repository.h"

#include "oyama/storage/uuid.h"
#include "oyama/storage/vault_codec.h"
#include "oyama/storage/vault_store.h"

#include <algorithm>

namespace oyama::storage {
namespace {

bool matches(const Tombstone& tombstone, std::string_view kind,
             std::string_view record_id) {
    return tombstone.kind == kind && tombstone.record_id == record_id;
}

}  // namespace

VaultTombstoneRepository::VaultTombstoneRepository(VaultStore& vault)
    : vault_(vault) {}

void VaultTombstoneRepository::record(const Tombstone& tombstone) {
    vault_.put(VaultRecord{generate_uuid_v4(), "tombstone",
                           static_cast<std::uint64_t>(tombstone.deleted_at),
                           encode_tombstone(tombstone)});
}

bool VaultTombstoneRepository::remove(std::string_view kind,
                                      std::string_view record_id) {
    bool removed = false;
    const auto records = vault_.scan("tombstone");
    for (const auto& record : records) {
        const auto tombstone = decode_tombstone(record.payload);
        if (!matches(tombstone, kind, record_id)) {
            continue;
        }
        (void)vault_.remove(record.id);
        removed = true;
    }
    return removed;
}

std::vector<Tombstone> VaultTombstoneRepository::list() {
    std::vector<Tombstone> tombstones;
    for (const auto& record : vault_.scan("tombstone")) {
        tombstones.push_back(decode_tombstone(record.payload));
    }
    std::sort(tombstones.begin(), tombstones.end(),
              [](const Tombstone& left, const Tombstone& right) {
                  if (left.kind != right.kind) {
                      return left.kind < right.kind;
                  }
                  return left.record_id < right.record_id;
              });
    return tombstones;
}

std::size_t VaultTombstoneRepository::prune_before(std::int64_t cutoff) {
    std::size_t removed = 0;
    const auto records = vault_.scan("tombstone");
    for (const auto& record : records) {
        const auto tombstone = decode_tombstone(record.payload);
        if (tombstone.deleted_at >= cutoff) {
            continue;
        }
        (void)vault_.remove(record.id);
        ++removed;
    }
    return removed;
}

}  // namespace oyama::storage
