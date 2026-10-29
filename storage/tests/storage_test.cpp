#include <oyama/storage/storage.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

namespace {

using oyama::storage::Connection;
using oyama::storage::ConnectionConfig;
using oyama::storage::DoseEvent;
using oyama::storage::JsonRecord;
using oyama::storage::JsonRecordKind;
using oyama::storage::LabResult;
using oyama::storage::DoseEventRepository;
using oyama::storage::JsonRecordRepository;
using oyama::storage::LabResultRepository;
using oyama::storage::Mode;
using oyama::storage::Preference;
using oyama::storage::PreferencesRepository;
using oyama::storage::Settings;
using oyama::storage::SettingsRepository;
using oyama::storage::Tombstone;
using oyama::storage::TombstoneRepository;

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

template <typename T>
void check_equal(const T& actual, const T& expected, const std::string& message) {
    if (!(actual == expected)) {
        std::cerr << "FAIL: " << message << " (actual=" << actual
                  << ", expected=" << expected << ")\n";
        ++failures;
    }
}

std::filesystem::path make_temp_dir() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() /
                ("oyama-storage-test-" + std::to_string(stamp));
    std::filesystem::create_directories(path);
    return path;
}

void write_file(const std::filesystem::path& path, std::string_view contents) {
    std::ofstream out(path, std::ios::binary);
    out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
}

void test_mode_round_trip() {
    check(oyama::storage::to_string(Mode::Transfem) == "transfem",
          "transfem serializes");
    check(oyama::storage::to_string(Mode::Transmasc) == "transmasc",
          "transmasc serializes");
    check(oyama::storage::parse_mode("transfem") == Mode::Transfem,
          "transfem parses");
    check(oyama::storage::parse_mode("transmasc") == Mode::Transmasc,
          "transmasc parses");
    check(!oyama::storage::parse_mode("other").has_value(),
          "unknown mode is rejected");
}

void test_uuid_generation() {
    const std::string id = oyama::storage::generate_uuid_v4();
    check(id.size() == 36, "UUID has 36 characters");
    check(id[8] == '-' && id[13] == '-' && id[18] == '-' && id[23] == '-',
          "UUID has hyphen positions");
    check(id[14] == '4', "UUID version is 4");
    check(id[19] == '8' || id[19] == '9' || id[19] == 'a' || id[19] == 'b',
          "UUID variant bits are RFC 4122");
    check(oyama::storage::is_uuid(id), "generated UUID validates");
    check(!oyama::storage::is_uuid("not-a-uuid"), "garbage is not a UUID");
    check(id != oyama::storage::generate_uuid_v4(), "UUIDs differ");
}

void test_migration_discovery() {
    const auto dir = make_temp_dir();
    write_file(dir / "0002_second.sql", "SELECT 2;\n");
    write_file(dir / "0001_first.sql", "SELECT 1;\n");

    oyama::storage::MigrationRunner runner(dir);
    const auto migrations = runner.load();
    check_equal(migrations.size(), std::size_t{2}, "two migrations load");
    check_equal(migrations[0].version, 1, "migrations sort by version");
    check(migrations[0].name == "first", "migration name is parsed");
    check(migrations[0].checksum.size() == 16, "checksum is 16 hex characters");
    check(migrations[0].checksum != migrations[1].checksum,
          "different SQL has different checksums");

    write_file(dir / "0001_duplicate.sql", "SELECT 3;\n");
    bool duplicate_rejected = false;
    try {
        (void)runner.load();
    } catch (const oyama::storage::Error&) {
        duplicate_rejected = true;
    }
    check(duplicate_rejected, "duplicate migration versions are rejected");

    std::filesystem::remove_all(dir);
}

void test_migration_rejects_bad_files() {
    const auto dir = make_temp_dir();

    write_file(dir / "0001_empty.sql", "   \n");
    bool empty_rejected = false;
    try {
        (void)oyama::storage::MigrationRunner(dir).load();
    } catch (const oyama::storage::Error&) {
        empty_rejected = true;
    }
    check(empty_rejected, "empty migrations are rejected");

    std::filesystem::remove(dir / "0001_empty.sql");
    write_file(dir / "one_bad.sql", "SELECT 1;\n");
    bool name_rejected = false;
    try {
        (void)oyama::storage::MigrationRunner(dir).load();
    } catch (const oyama::storage::Error&) {
        name_rejected = true;
    }
    check(name_rejected, "malformed migration names are rejected");

    std::filesystem::remove_all(dir);
}

void test_migration_missing_directory() {
    bool rejected = false;
    try {
        (void)oyama::storage::MigrationRunner(
            std::filesystem::path("/nonexistent/oyama-migrations"))
            .load();
    } catch (const oyama::storage::Error&) {
        rejected = true;
    }
    check(rejected, "missing migration directory is rejected");
}

std::filesystem::path migrations_dir() {
#ifdef OYAMA_TEST_MIGRATIONS_DIR
    return std::filesystem::path(OYAMA_TEST_MIGRATIONS_DIR);
#else
    return std::filesystem::path("storage/migrations");
#endif
}

struct SchemaGuard {
    Connection& connection;
    std::string schema;
    bool active = true;

    ~SchemaGuard() {
        if (!active) {
            return;
        }
        try {
            (void)connection.exec(
                "DROP SCHEMA " + connection.quote_identifier(schema) + " CASCADE");
            (void)connection.exec("SET search_path TO public");
        } catch (const std::exception& error) {
            std::cerr << "warning: could not drop test schema: "
                      << error.what() << '\n';
        }
    }
};

void test_migration_guard(Connection& connection) {
    const auto dir = make_temp_dir();
    const std::string schema =
        "oyama_mig_" + std::to_string(::getpid()) + "_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    (void)connection.exec("CREATE SCHEMA " +
                          connection.quote_identifier(schema));
    SchemaGuard guard{connection, schema};
    (void)connection.exec("SET search_path TO " +
                          connection.quote_identifier(schema));

    write_file(dir / "0001_probe.sql", "CREATE TABLE probe (id integer);\n");
    oyama::storage::MigrationRunner runner(dir);
    check_equal(runner.apply(connection).size(), std::size_t{1},
                "temporary migration applies");

    write_file(dir / "0001_probe.sql", "CREATE TABLE probe (id bigint);\n");
    bool checksum_rejected = false;
    try {
        (void)runner.apply(connection);
    } catch (const oyama::storage::Error&) {
        checksum_rejected = true;
    }
    check(checksum_rejected, "changed applied migration is rejected");
    const auto status = runner.status(connection);
    check_equal(status.size(), std::size_t{1}, "temporary status has one row");
    check(status[0].applied, "temporary migration is applied");
    check(!status[0].checksum_matches,
          "temporary status reports the checksum mismatch");

    std::filesystem::remove(dir / "0001_probe.sql");
    bool missing_rejected = false;
    try {
        (void)runner.apply(connection);
    } catch (const oyama::storage::Error&) {
        missing_rejected = true;
    }
    check(missing_rejected, "missing applied migration is rejected");

    std::filesystem::remove_all(dir);
}

void run_integration_checks(Connection& connection) {
    const std::string schema =
        "oyama_test_" + std::to_string(::getpid()) + "_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    (void)connection.exec("CREATE SCHEMA " +
                          connection.quote_identifier(schema));
    SchemaGuard guard{connection, schema};
    (void)connection.exec("SET search_path TO " +
                          connection.quote_identifier(schema));

    oyama::storage::MigrationRunner runner(migrations_dir());
    const auto first = runner.apply(connection);
    check_equal(first.size(), std::size_t{2}, "initial migrations apply");
    check_equal(runner.current_version(connection), 2,
                "schema version is 2");
    check(runner.apply(connection).empty(), "second migrate is a no-op");

    const auto status = runner.status(connection);
    check_equal(status.size(), std::size_t{2}, "two migrations are reported");
    check(status[0].applied && status[1].applied,
          "both migrations are applied");
    check(status[0].checksum_matches && status[1].checksum_matches,
          "applied checksums match");

    SettingsRepository settings(connection);
    Settings current = settings.get();
    check_equal(current.calibration_method, std::string("mipd"),
                "default calibration is mipd");
    check_equal(current.calibration_history_mode, std::string("retrospective"),
                "default history mode is retrospective");
    check_equal(current.updated_at, std::int64_t{0}, "default row updated_at");
    check(!current.body_weight_kg.has_value(), "default weight is unset");

    current.body_weight_kg = 70.5;
    current.birth_year = 1990;
    current.pk_params_json = R"({"e2_kClear":0.4})";
    current.pk_params_updated_at = 123;
    current.updated_at = 456;
    settings.save(current);
    const Settings stored = settings.get();
    check(stored.body_weight_kg.has_value() &&
              std::abs(*stored.body_weight_kg - 70.5) < 1e-12,
          "weight round-trips");
    check_equal(*stored.birth_year, 1990, "birth year round-trips");
    check_equal(*stored.pk_params_json, std::string(R"({"e2_kClear": 0.4})"),
                "PK params round-trip through jsonb");
    check_equal(*stored.pk_params_updated_at, std::int64_t{123},
                "PK params timestamp round-trips");
    check_equal(stored.updated_at, std::int64_t{456},
                "settings timestamp round-trips");

    DoseEventRepository doses(connection);
    DoseEvent dose;
    dose.id = oyama::storage::generate_uuid_v4();
    dose.mode = Mode::Transfem;
    dose.route = "oral";
    dose.ester = "E2";
    dose.time_h = 1000.25;
    dose.dose_mg = 2.0;
    dose.extras_json = R"({"note":"test"})";
    dose.updated_at = 10;
    dose.created_at = 5;
    doses.upsert(dose);
    const auto found = doses.get(dose.id);
    check(found.has_value(), "dose can be fetched by id");
    check_equal(found->route, std::string("oral"), "dose route round-trips");
    check(std::abs(found->time_h - 1000.25) < 1e-12, "dose time round-trips");

    dose.dose_mg = 4.0;
    dose.updated_at = 11;
    doses.upsert(dose);
    check(std::abs(doses.get(dose.id)->dose_mg - 4.0) < 1e-12,
          "dose upsert updates the row");

    DoseEvent other;
    other.id = oyama::storage::generate_uuid_v4();
    other.mode = Mode::Transmasc;
    other.route = "gel";
    other.ester = "T";
    other.time_h = 2000.0;
    other.dose_mg = 50.0;
    other.updated_at = 12;
    other.created_at = 6;
    doses.upsert(other);

    check_equal(doses.list(Mode::Transfem).size(), std::size_t{1},
                "transfem list is filtered");
    check_equal(doses.list(Mode::Transmasc).size(), std::size_t{1},
                "transmasc list is filtered");
    check_equal(doses.count(Mode::Transfem), std::size_t{1}, "dose count");
    check(doses.remove(dose.id), "existing dose can be removed");
    check(!doses.remove(dose.id), "removing a missing dose returns false");
    check(doses.list(Mode::Transfem).empty(), "removed dose is gone");

    LabResultRepository labs(connection);
    LabResult lab;
    lab.id = oyama::storage::generate_uuid_v4();
    lab.mode = Mode::Transfem;
    lab.time_h = 3000.0;
    lab.value = 180.0;
    lab.unit = "pg/mL";
    lab.assay = "lcms";
    lab.updated_at = 20;
    lab.created_at = 19;
    labs.upsert(lab);
    check_equal(labs.list(Mode::Transfem).size(), std::size_t{1},
                "lab list");
    check(labs.remove(lab.id), "lab can be removed");

    JsonRecordRepository templates(connection, JsonRecordKind::DoseTemplate);
    JsonRecord record;
    record.id = oyama::storage::generate_uuid_v4();
    record.mode = Mode::Transfem;
    record.payload_json = R"({"route":"oral","doseMG":2})";
    record.updated_at = 30;
    templates.upsert(record);
    check_equal(templates.list(Mode::Transfem).size(), std::size_t{1},
                "json record list");
    check(templates.get(record.id).has_value(), "json record get");
    check(templates.remove(record.id), "json record remove");

    PreferencesRepository preferences(connection);
    preferences.set(Preference{"theme", R"("dark")", 40});
    const auto theme = preferences.get("theme");
    check(theme.has_value() && theme->value_json == R"("dark")",
          "preference round-trips");
    check_equal(preferences.list().size(), std::size_t{1}, "preference list");
    check(preferences.remove("theme"), "preference remove");

    TombstoneRepository tombstones(connection);
    tombstones.record(Tombstone{"dose", oyama::storage::generate_uuid_v4(), 50});
    check_equal(tombstones.list().size(), std::size_t{1}, "tombstone list");

    test_migration_guard(connection);
}

void test_integration() {
    const char* socket = std::getenv("OYAMA_TEST_PG_SOCKET");
    const char* dbname = std::getenv("OYAMA_TEST_PG_DB");
    if (socket == nullptr || *socket == '\0' || dbname == nullptr ||
        *dbname == '\0') {
        std::cout << "SKIP: set OYAMA_TEST_PG_SOCKET and OYAMA_TEST_PG_DB "
                     "to run PostgreSQL integration checks\n";
        return;
    }

    ConnectionConfig config;
    config.socket_dir = socket;
    config.dbname = dbname;
    if (const char* user = std::getenv("OYAMA_TEST_PG_USER");
        user != nullptr && *user != '\0') {
        config.user = user;
    }
    if (const char* port = std::getenv("OYAMA_TEST_PG_PORT");
        port != nullptr && *port != '\0') {
        config.port = std::stoi(port);
    }

    Connection connection(config);
    run_integration_checks(connection);
}

}  // namespace

int main() {
    try {
        test_mode_round_trip();
        test_uuid_generation();
        test_migration_discovery();
        test_migration_rejects_bad_files();
        test_migration_missing_directory();
        test_integration();
    } catch (const std::exception& error) {
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
        return 1;
    }

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "storage_test: ok\n";
    return 0;
}
