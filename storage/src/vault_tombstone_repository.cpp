#include "oyama/storage/vault_tombstone_repository.h"

#include "oyama/storage/uuid.h"
#include "oyama/storage/vault_codec.h"
#include "oyama/storage/vault_store.h"

#include <algorithm>
#include <map>
#include <utility>

namespace oyama { namespace storage {
namespace {

bool matches(const Tombstone& tombstone, oyama::string_view kind,
             oyama::string_view record_id) {
    return tombstone.kind == kind && tombstone.record_id == record_id;
}

}  // namespace

VaultTombstoneRepository::VaultTombstoneRepository(VaultStore& vault)
    : vault_(vault) {}

void VaultTombstoneRepository::record(const Tombstone& tombstone) {
    record_many({{tombstone.kind, {{tombstone.record_id,
                                    tombstone.deleted_at}}}});
}

void VaultTombstoneRepository::record_many(
    const std::map<std::string, std::map<std::string, std::int64_t>>&
        tombstones) {
    using Key = std::pair<std::string, std::string>;
    struct Existing {
        std::string record_id;
        std::int64_t deleted_at;
    };

    std::map<Key, Existing> existing;
    for (const auto& record : vault_.scan("tombstone")) {
        const auto tombstone = decode_tombstone(record.payload);
        const Key key{tombstone.kind, tombstone.record_id};
        const auto found = existing.find(key);
        if (found == existing.end()) {
            existing.emplace(key, Existing{record.id, tombstone.deleted_at});
        } else {
            found->second.deleted_at =
                std::max(found->second.deleted_at, tombstone.deleted_at);
        }
    }

    std::map<Key, std::int64_t> merged;
    for (const auto& _entry : tombstones) { const auto& kind = _entry.first; const auto& records = _entry.second;
        for (const auto& _entry : records) { const auto& record_id = _entry.first; const auto& deleted_at = _entry.second;
            auto& stamp = merged[Key{kind, record_id}];
            stamp = std::max(stamp, deleted_at);
        }
    }

    for (const auto& _entry : merged) { const auto& key = _entry.first; const auto& deleted_at = _entry.second;
        std::string record_id;
        std::int64_t stamp = deleted_at;
        const auto found = existing.find(key);
        if (found != existing.end()) {
            record_id = found->second.record_id;
            stamp = std::max(stamp, found->second.deleted_at);
        } else {
            record_id = generate_uuid_v4();
        }
        const Tombstone tombstone{key.first, key.second, stamp};
        vault_.put(VaultRecord{record_id, "tombstone",
                               static_cast<std::uint64_t>(tombstone.deleted_at),
                               encode_tombstone(tombstone)});
    }
}

bool VaultTombstoneRepository::remove(oyama::string_view kind,
                                      oyama::string_view record_id) {
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
    std::map<std::pair<std::string, std::string>, Tombstone> unique;
    for (const auto& record : vault_.scan("tombstone")) {
        const auto tombstone = decode_tombstone(record.payload);
        const auto key = std::make_pair(tombstone.kind, tombstone.record_id);
        const auto found = unique.find(key);
        if (found == unique.end() ||
            tombstone.deleted_at > found->second.deleted_at) {
            unique[key] = tombstone;
        }
    }
    std::vector<Tombstone> tombstones;
    tombstones.reserve(unique.size());
    for (const auto& _entry : unique) { const auto& key = _entry.first; const auto& tombstone = _entry.second;
        (void)key;
        tombstones.push_back(tombstone);
    }
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

} }  // namespace oyama::storage
