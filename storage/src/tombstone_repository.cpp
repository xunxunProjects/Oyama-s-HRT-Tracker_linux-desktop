#include <oyama/storage/tombstone_repository.h>

#include "repository_util.h"

#include <oyama/storage/uuid.h>

#include <string>
#include <vector>

namespace oyama::storage {
namespace {

void validate(const Tombstone& tombstone) {
    detail::require_text(tombstone.kind, "tombstone kind");
    if (!is_uuid(tombstone.record_id)) {
        throw Error("tombstone record id is not a valid UUID");
    }
}

Tombstone read_tombstone(const Result& result, int row) {
    return Tombstone{
        result.text(row, 0),
        result.text(row, 1),
        result.integer(row, 2),
    };
}

}  // namespace

TombstoneRepository::TombstoneRepository(Connection& connection)
    : connection_(connection) {}

void TombstoneRepository::record(const Tombstone& tombstone) {
    validate(tombstone);
    (void)connection_.exec_params(
        "INSERT INTO tombstones (kind, record_id, deleted_at) "
        "VALUES ($1, $2::uuid, $3) "
        "ON CONFLICT (kind, record_id) DO UPDATE SET "
        "deleted_at = GREATEST(tombstones.deleted_at, EXCLUDED.deleted_at)",
        {tombstone.kind, tombstone.record_id,
         std::to_string(tombstone.deleted_at)});
}

bool TombstoneRepository::remove(std::string_view kind,
                                 std::string_view record_id) {
    detail::require_text(kind, "tombstone kind");
    if (!is_uuid(record_id)) {
        throw Error("tombstone record id is not a valid UUID");
    }
    const auto result = connection_.exec_params(
        "DELETE FROM tombstones WHERE kind = $1 AND record_id = $2::uuid",
        {std::string(kind), std::string(record_id)});
    return result.affected_rows() > 0;
}

std::vector<Tombstone> TombstoneRepository::list() {
    const auto result = connection_.exec(
        "SELECT kind, record_id::text, deleted_at FROM tombstones "
        "ORDER BY deleted_at, kind, record_id");
    std::vector<Tombstone> tombstones;
    tombstones.reserve(static_cast<std::size_t>(result.rows()));
    for (int row = 0; row < result.rows(); ++row) {
        tombstones.push_back(read_tombstone(result, row));
    }
    return tombstones;
}

std::size_t TombstoneRepository::prune_before(std::int64_t cutoff) {
    const auto result = connection_.exec_params(
        "DELETE FROM tombstones WHERE deleted_at < $1",
        {std::to_string(cutoff)});
    return static_cast<std::size_t>(result.affected_rows());
}

}  // namespace oyama::storage
