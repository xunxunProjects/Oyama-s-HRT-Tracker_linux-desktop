#include <oyama/storage/database.h>

#include <utility>

namespace oyama { namespace storage {
namespace {

constexpr int kMinimumServerVersion = 140000;

}  // namespace

Database::Database(ConnectionConfig config,
                   oyama::fs::path migrations_directory)
    : connection_(config), migrations_(std::move(migrations_directory)) {
    if (connection_.server_version() < kMinimumServerVersion) {
        throw Error("PostgreSQL 14 or newer is required");
    }
}

Connection& Database::connection() noexcept {
    return connection_;
}

const Connection& Database::connection() const noexcept {
    return connection_;
}

MigrationRunner& Database::migrations() noexcept {
    return migrations_;
}

const MigrationRunner& Database::migrations() const noexcept {
    return migrations_;
}

std::vector<Migration> Database::migrate() {
    return migrations_.apply(connection_);
}

std::vector<MigrationStatus> Database::migration_status() {
    return migrations_.status(connection_);
}

int Database::current_version() {
    return migrations_.current_version(connection_);
}

} }  // namespace oyama::storage
