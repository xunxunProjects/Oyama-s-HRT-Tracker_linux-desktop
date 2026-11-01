#pragma once
#include <oyama/compat/all.h>

#include "connection.h"

#include <filesystem>
#include <string>
#include <vector>

namespace oyama { namespace storage {

struct Migration {
    int version{0};
    std::string name;
    oyama::fs::path path;
    std::string sql;
    std::string checksum;
    Migration() = default;
    Migration(int v, std::string n, oyama::fs::path p, std::string s, std::string c)
        : version(v), name(std::move(n)), path(std::move(p)),
          sql(std::move(s)), checksum(std::move(c)) {}
};

struct MigrationStatus {
    int version{0};
    std::string name;
    bool applied{false};
    bool checksum_matches{true};
    std::string applied_at;
    MigrationStatus() = default;
    MigrationStatus(int v, std::string n, bool a, bool c, std::string t)
        : version(v), name(std::move(n)), applied(a),
          checksum_matches(c), applied_at(std::move(t)) {}
};

/// Forward-only migration runner for storage/migrations/NNNN_name.sql.
/// Migrations are applied in one transaction under a PostgreSQL advisory lock;
/// applied checksums are verified on every run so silently edited history is
/// rejected.
class MigrationRunner {
public:
    explicit MigrationRunner(oyama::fs::path directory);

    OYAMA_NODISCARD const oyama::fs::path& directory() const noexcept;

    OYAMA_NODISCARD std::vector<Migration> load() const;
    OYAMA_NODISCARD std::vector<Migration> apply(Connection& connection) const;
    OYAMA_NODISCARD std::vector<MigrationStatus> status(Connection& connection) const;
    OYAMA_NODISCARD int current_version(Connection& connection) const;

private:
    oyama::fs::path directory_;
};

} }  // namespace oyama::storage
