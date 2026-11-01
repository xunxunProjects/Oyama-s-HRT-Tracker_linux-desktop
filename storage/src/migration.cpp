#include <oyama/storage/migration.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

namespace oyama { namespace storage {
namespace {

constexpr std::int64_t kMigrationLockKey = INT64_C(0x4F59414D41485254);

struct AppliedMigration {
    int version{0};
    std::string name;
    std::string checksum;
    std::string applied_at;
    AppliedMigration() = default;
    AppliedMigration(int v, std::string n, std::string c, std::string a)
        : version(v), name(std::move(n)), checksum(std::move(c)),
          applied_at(std::move(a)) {}
};

std::string checksum_of(oyama::string_view data) {
    std::uint64_t hash = UINT64_C(14695981039346656037);
    for (const unsigned char byte : data) {
        hash ^= byte;
        hash *= UINT64_C(1099511628211);
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

bool is_blank(oyama::string_view data) {
    return std::all_of(data.begin(), data.end(), [](unsigned char value) {
        return std::isspace(value) != 0;
    });
}

std::string read_file(const oyama::fs::path& path) {
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input) {
        throw Error("could not open migration: " + path.string());
    }
    std::string contents((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
    if (input.bad()) {
        throw Error("could not read migration: " + path.string());
    }
    return contents;
}

void ensure_migrations_table(Connection& connection) {
    (void)connection.exec(R"(
        CREATE TABLE IF NOT EXISTS schema_migrations (
            version integer PRIMARY KEY,
            name text NOT NULL,
            checksum text NOT NULL,
            applied_at timestamptz NOT NULL DEFAULT now()
        )
    )");
}

bool migrations_table_exists(Connection& connection) {
    const auto result =
        connection.exec("SELECT to_regclass('schema_migrations') IS NOT NULL");
    return result.boolean(0, 0);
}

std::vector<AppliedMigration> read_applied(Connection& connection) {
    const auto result = connection.exec(
        "SELECT version, name, checksum, applied_at::text "
        "FROM schema_migrations ORDER BY version");
    std::vector<AppliedMigration> applied;
    applied.reserve(static_cast<std::size_t>(result.rows()));
    for (int row = 0; row < result.rows(); ++row) {
        applied.push_back(AppliedMigration{
            static_cast<int>(result.integer(row, 0)),
            result.text(row, 1),
            result.text(row, 2),
            result.text(row, 3),
        });
    }
    return applied;
}

const Migration* find_migration(const std::vector<Migration>& migrations,
                                int version) {
    const auto found = std::lower_bound(
        migrations.begin(), migrations.end(), version,
        [](const Migration& migration, int value) {
            return migration.version < value;
        });
    if (found == migrations.end() || found->version != version) {
        return nullptr;
    }
    return &*found;
}

}  // namespace

MigrationRunner::MigrationRunner(oyama::fs::path directory)
    : directory_(std::move(directory)) {}

const oyama::fs::path& MigrationRunner::directory() const noexcept {
    return directory_;
}

std::vector<Migration> MigrationRunner::load() const {
    std::error_code error;
    if (!oyama::fs::exists(directory_, error) || error) {
        throw Error("migration directory does not exist: " + directory_.string());
    }
    if (!oyama::fs::is_directory(directory_, error) || error) {
        throw Error("migration path is not a directory: " + directory_.string());
    }

    static const std::regex filename_pattern(
        R"(^([0-9]{4})_([A-Za-z0-9_]+)\.sql$)");

    std::vector<Migration> migrations;
    oyama::fs::directory_iterator iterator(directory_, error);
    if (error) {
        throw Error("could not list migration directory: " + directory_.string());
    }
    for (const auto& entry : iterator) {
        if (!oyama::fs::is_regular_file(entry.path())) {
            continue;
        }
        const std::string filename = entry.path().filename().string();
        const bool sql_file = filename.size() >= 4 &&
                              filename.compare(filename.size() - 4, 4, ".sql") == 0;
        if (!sql_file) {
            continue;
        }

        std::smatch match;
        if (!std::regex_match(filename, match, filename_pattern)) {
            throw Error("invalid migration filename: " + filename);
        }

        const int version = std::stoi(match[1].str());
        if (version <= 0) {
            throw Error("migration version must be positive: " + filename);
        }

        const std::string sql = read_file(entry.path());
        if (is_blank(sql)) {
            throw Error("migration is empty: " + filename);
        }

        migrations.push_back(Migration{
            version,
            match[2].str(),
            entry.path(),
            sql,
            checksum_of(sql),
        });
    }

    std::sort(migrations.begin(), migrations.end(),
              [](const Migration& left, const Migration& right) {
                  return left.version < right.version;
              });
    for (std::size_t index = 1; index < migrations.size(); ++index) {
        if (migrations[index - 1].version == migrations[index].version) {
            throw Error("duplicate migration version " +
                        std::to_string(migrations[index].version));
        }
    }
    return migrations;
}

std::vector<Migration> MigrationRunner::apply(Connection& connection) const {
    const std::vector<Migration> migrations = load();
    Transaction transaction(connection);

    (void)connection.exec("SELECT pg_advisory_xact_lock(" +
                          std::to_string(kMigrationLockKey) + "::bigint)");
    ensure_migrations_table(connection);

    const std::vector<AppliedMigration> applied = read_applied(connection);
    std::map<int, const AppliedMigration*> applied_by_version;
    for (const auto& record : applied) {
        applied_by_version.emplace(record.version, &record);
    }

    for (const auto& record : applied) {
        const Migration* migration = find_migration(migrations, record.version);
        if (migration == nullptr) {
            throw Error("applied migration " + std::to_string(record.version) +
                        " is missing from " + directory_.string());
        }
        if (migration->checksum != record.checksum) {
            throw Error("checksum mismatch for applied migration " +
                        std::to_string(record.version) + " (" +
                        migration->path.filename().string() + ")");
        }
    }

    std::vector<Migration> newly_applied;
    for (const auto& migration : migrations) {
        if (applied_by_version.find(migration.version) != applied_by_version.end()) {
            continue;
        }
        try {
            (void)connection.exec(migration.sql);
        } catch (const Error& error) {
            throw Error("migration " + migration.path.filename().string() +
                            " failed: " + error.what(),
                        error.sqlstate());
        }
        (void)connection.exec_params(
            "INSERT INTO schema_migrations (version, name, checksum) "
            "VALUES ($1, $2, $3)",
            {std::to_string(migration.version), migration.name,
             migration.checksum});
        newly_applied.push_back(migration);
    }

    transaction.commit();
    return newly_applied;
}

std::vector<MigrationStatus> MigrationRunner::status(Connection& connection) const {
    const std::vector<Migration> migrations = load();
    std::vector<MigrationStatus> statuses;
    statuses.reserve(migrations.size());

    if (!migrations_table_exists(connection)) {
        for (const auto& migration : migrations) {
            statuses.push_back(MigrationStatus{
                migration.version,
                migration.name,
                false,
                true,
                {},
            });
        }
        return statuses;
    }

    const std::vector<AppliedMigration> applied = read_applied(connection);
    std::map<int, const AppliedMigration*> applied_by_version;
    for (const auto& record : applied) {
        applied_by_version.emplace(record.version, &record);
    }

    for (const auto& migration : migrations) {
        const auto found = applied_by_version.find(migration.version);
        if (found == applied_by_version.end()) {
            statuses.push_back(MigrationStatus{
                migration.version,
                migration.name,
                false,
                true,
                {},
            });
            continue;
        }
        statuses.push_back(MigrationStatus{
            migration.version,
            migration.name,
            true,
            migration.checksum == found->second->checksum,
            found->second->applied_at,
        });
    }
    return statuses;
}

int MigrationRunner::current_version(Connection& connection) const {
    if (!migrations_table_exists(connection)) {
        return 0;
    }
    const auto result =
        connection.exec("SELECT COALESCE(MAX(version), 0) FROM schema_migrations");
    return static_cast<int>(result.integer(0, 0));
}

} }  // namespace oyama::storage
