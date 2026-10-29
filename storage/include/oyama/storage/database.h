#pragma once

#include "connection.h"
#include "migration.h"

#include <filesystem>
#include <vector>

namespace oyama::storage {

/// Owns a connection and its migration runner.  Construction checks that the
/// server is PostgreSQL 14 or newer; migrations are explicit so a read-only
/// status command never mutates the database.
class Database {
public:
    Database(ConnectionConfig config, std::filesystem::path migrations_directory);

    [[nodiscard]] Connection& connection() noexcept;
    [[nodiscard]] const Connection& connection() const noexcept;
    [[nodiscard]] MigrationRunner& migrations() noexcept;
    [[nodiscard]] const MigrationRunner& migrations() const noexcept;

    [[nodiscard]] std::vector<Migration> migrate();
    [[nodiscard]] std::vector<MigrationStatus> migration_status();
    [[nodiscard]] int current_version();

private:
    Connection connection_;
    MigrationRunner migrations_;
};

}  // namespace oyama::storage
