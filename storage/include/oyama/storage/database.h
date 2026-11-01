#pragma once
#include <oyama/compat/all.h>

#include "connection.h"
#include "migration.h"

#include <filesystem>
#include <vector>

namespace oyama { namespace storage {

/// Owns a connection and its migration runner.  Construction checks that the
/// server is PostgreSQL 14 or newer; migrations are explicit so a read-only
/// status command never mutates the database.
class Database {
public:
    Database(ConnectionConfig config, oyama::fs::path migrations_directory);

    OYAMA_NODISCARD Connection& connection() noexcept;
    OYAMA_NODISCARD const Connection& connection() const noexcept;
    OYAMA_NODISCARD MigrationRunner& migrations() noexcept;
    OYAMA_NODISCARD const MigrationRunner& migrations() const noexcept;

    OYAMA_NODISCARD std::vector<Migration> migrate();
    OYAMA_NODISCARD std::vector<MigrationStatus> migration_status();
    OYAMA_NODISCARD int current_version();

private:
    Connection connection_;
    MigrationRunner migrations_;
};

} }  // namespace oyama::storage
