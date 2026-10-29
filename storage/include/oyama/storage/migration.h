#pragma once

#include "connection.h"

#include <filesystem>
#include <string>
#include <vector>

namespace oyama::storage {

struct Migration {
    int version{0};
    std::string name;
    std::filesystem::path path;
    std::string sql;
    std::string checksum;
};

struct MigrationStatus {
    int version{0};
    std::string name;
    bool applied{false};
    bool checksum_matches{true};
    std::string applied_at;
};

/// Forward-only migration runner for storage/migrations/NNNN_name.sql.
/// Migrations are applied in one transaction under a PostgreSQL advisory lock;
/// applied checksums are verified on every run so silently edited history is
/// rejected.
class MigrationRunner {
public:
    explicit MigrationRunner(std::filesystem::path directory);

    [[nodiscard]] const std::filesystem::path& directory() const noexcept;

    [[nodiscard]] std::vector<Migration> load() const;
    [[nodiscard]] std::vector<Migration> apply(Connection& connection) const;
    [[nodiscard]] std::vector<MigrationStatus> status(Connection& connection) const;
    [[nodiscard]] int current_version(Connection& connection) const;

private:
    std::filesystem::path directory_;
};

}  // namespace oyama::storage
