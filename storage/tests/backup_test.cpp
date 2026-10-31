#include <oyama/storage/storage.h>
#include <oyama/storage/backup.h>
#include <oyama/storage/vault_dose_event_repository.h>
#include <oyama/storage/vault_lab_result_repository.h>
#include <oyama/storage/vault_settings_repository.h>
#include <oyama/storage/vault_store.h>

#include <nlohmann/json.hpp>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

namespace {

using json = nlohmann::json;
using namespace oyama::storage;

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

std::filesystem::path migrations_dir() {
#ifdef OYAMA_TEST_MIGRATIONS_DIR
    return std::filesystem::path(OYAMA_TEST_MIGRATIONS_DIR);
#else
    return std::filesystem::path("storage/migrations");
#endif
}

std::filesystem::path make_temp_dir() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() /
                ("oyama-backup-test-" + std::to_string(::getpid()) + "-" +
                 std::to_string(stamp));
    std::filesystem::create_directories(path);
    return path;
}

void write_file(const std::filesystem::path& path, std::string_view contents) {
    std::ofstream out(path, std::ios::binary);
    out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
}

json read_json(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return json::parse(input);
}

constexpr const char* kEventA = "11111111-1111-4111-8111-111111111111";
constexpr const char* kEventB = "22222222-2222-4222-8222-222222222222";
constexpr const char* kEventC = "66666666-6666-4666-8666-666666666666";
constexpr const char* kDeletedD = "77777777-7777-4777-8777-777777777777";
constexpr const char* kLab = "33333333-3333-4333-8333-333333333333";
constexpr const char* kTemplate = "44444444-4444-4444-8444-444444444444";
constexpr const char* kQuick = "55555555-5555-4555-8555-555555555555";
constexpr const char* kSupply = "88888888-8888-4888-8888-888888888888";
constexpr const char* kPlan = "99999999-9999-4999-8999-999999999999";

json make_event(const char* id, double time_h, double dose, int updated_at) {
    return json{{"id", id},        {"route", "oral"}, {"ester", "E2"},
                {"timeH", time_h}, {"doseMG", dose}, {"updatedAt", updated_at}};
}

json make_bundle() {
    json transfem;
    transfem["events"] = json::array(
        {make_event(kEventA, 100.0, 2.0, 2000),
         make_event(kEventB, 200.0, 4.0, 2000),
         make_event(kEventC, 300.0, 6.0, 2000)});
    transfem["labResults"] = json::array(
        {{{"id", kLab},
          {"timeH", 150.0},
          {"concValue", 180.0},
          {"unit", "pg/ml"},
          {"updatedAt", 2000}}});
    transfem["doseTemplates"] = json::array(
        {{{"id", kTemplate},
          {"name", "morning"},
          {"route", "oral"},
          {"ester", "E2"},
          {"doseMG", 2.0},
          {"extras", json::object()},
          {"createdAt", 900},
          {"updatedAt", 2000}}});
    transfem["quickDoses"] = json::array(
        {{{"id", kQuick},
          {"route", "oral"},
          {"ester", "E2"},
          {"value", 4.0},
          {"createdAt", 900}},
         {{"id", "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"},
          {"route", "oral"},
          {"ester", "E2"},
          {"value", 8.0},
          {"createdAt", 1100}}});
    transfem["supplies"] = json::array(
        {{{"id", kSupply},
          {"route", "injection"},
          {"ester", "EV"},
          {"pack", {{"kind", "vial"}, {"ml", 10.0}, {"mgPerMl", 20.0}}},
          {"count", 2.0},
          {"startedAt", 1000}}});
    transfem["plan"] = json::array(
        {{{"id", kPlan},
          {"route", "oral"},
          {"ester", "E2"},
          {"doseMG", 2.0},
          {"extras", json::object()},
          {"everyDays", 1.0},
          {"timesPerDay", 1.0}}});
    transfem["deletions"] = json{
        {"events", {{kEventC, 2500}, {kDeletedD, 2500}}},
        {"labResults", json::object()},
        {"doseTemplates", json::object()}};

    json transmasc;
    transmasc["events"] = json::array();
    transmasc["labResults"] = json::array();
    transmasc["doseTemplates"] = json::array();
    transmasc["quickDoses"] = json::array();
    transmasc["supplies"] = json::array();
    transmasc["plan"] = json::array();
    transmasc["deletions"] = json{{"events", json::object()},
                                  {"labResults", json::object()},
                                  {"doseTemplates", json::object()}};

    return json{{"format", "oyama-hrt-complete-backup"},
                {"formatVersion", 1},
                {"createdAt", "2026-10-29T00:00:00Z"},
                {"mode", "transfem"},
                {"modes", {{"transfem", transfem}, {"transmasc", transmasc}}},
                {"weight", 75.0},
                {"weightUpdatedAt", 2000},
                {"birthYear", 1991},
                {"birthYearUpdatedAt", 2000},
                {"pkParams", {{"e2_kClear", 0.4}}},
                {"pkParamsUpdatedAt", 2000},
                {"calibration", {{"method", "ekf"}, {"historyMode", "forward"}}},
                {"preferences", {{"app-theme", "dark"}, {"unknown-key", "x"}}}};
}

void seed_local(Connection& connection) {
    SettingsRepository settings(connection);
    Settings current = settings.get();
    current.body_weight_kg = 70.0;
    current.weight_updated_at = 1000;
    current.birth_year = 1990;
    current.birth_year_updated_at = 1000;
    current.pk_params_json = R"({"e2_kClear":0.3})";
    current.pk_params_updated_at = 1000;
    current.updated_at = 1000;
    settings.save(current);

    DoseEventRepository events(connection);
    DoseEvent a;
    a.id = kEventA;
    a.mode = Mode::Transfem;
    a.route = "oral";
    a.ester = "E2";
    a.time_h = 100.0;
    a.dose_mg = 1.0;
    a.updated_at = 1000;
    events.upsert(a);

    DoseEvent b = a;
    b.id = kEventB;
    b.time_h = 200.0;
    b.dose_mg = 3.0;
    b.updated_at = 3000;
    events.upsert(b);

    LabResultRepository labs(connection);
    LabResult lab;
    lab.id = kLab;
    lab.mode = Mode::Transfem;
    lab.time_h = 150.0;
    lab.value = 100.0;
    lab.unit = "pg/ml";
    lab.assay = "lcms";
    lab.updated_at = 1000;
    labs.upsert(lab);

    JsonRecordRepository templates(connection, JsonRecordKind::DoseTemplate);
    JsonRecord templ;
    templ.id = kTemplate;
    templ.mode = Mode::Transfem;
    templ.payload_json =
        R"({"id":"44444444-4444-4444-8444-444444444444","name":"old","route":"oral","ester":"E2","doseMG":1,"extras":{},"createdAt":900})";
    templ.updated_at = 1000;
    templates.upsert(templ);

    JsonRecordRepository quick(connection, JsonRecordKind::QuickDose);
    JsonRecord q;
    q.id = kQuick;
    q.mode = Mode::Transfem;
    q.payload_json =
        R"({"id":"55555555-5555-4555-8555-555555555555","route":"oral","ester":"E2","value":2,"createdAt":900})";
    quick.upsert(q);
}

void run_backup_checks(Connection& connection) {
    seed_local(connection);
    const auto dir = make_temp_dir();
    const auto incoming = dir / "incoming.json";
    write_file(incoming, make_bundle().dump(2));
    const auto exported = dir / "exported.json";
    const std::int64_t now = 5000;

    const auto preview = preview_backup(connection, incoming, now);
    check(preview.ok, "valid bundle previews");
    check_equal(preview.counts.at("transfem").events, std::size_t{3},
                "preview counts events");
    check_equal(preview.counts.at("transfem").deletions, std::size_t{2},
                "preview counts deletions");
    check_equal(preview.counts.at("transfem").quick_doses, std::size_t{2},
                "preview counts quick doses");
    check(preview.blocked.size() == 1 &&
              preview.blocked[0] == "transfem.quickDoses",
          "duplicate unstamped ids block only that category");
    check_equal(connection.exec("SELECT dose_mg FROM dose_events WHERE id = '" +
                                std::string(kEventA) + "'::uuid")
                    .number(0, 0),
                1.0,
                "preview does not mutate the database");

    const auto imported = import_backup(connection, incoming, now);
    check(imported.applied, "import applies");
    check_equal(connection.exec("SELECT dose_mg FROM dose_events WHERE id = '" +
                                std::string(kEventA) + "'::uuid")
                    .number(0, 0),
                2.0,
                "newer incoming event updates");
    check_equal(connection.exec("SELECT dose_mg FROM dose_events WHERE id = '" +
                                std::string(kEventB) + "'::uuid")
                    .number(0, 0),
                3.0,
                "newer local event is preserved");
    check_equal(connection.exec("SELECT count(*) FROM dose_events WHERE id = '" +
                                std::string(kEventC) + "'::uuid")
                    .integer(0, 0),
                std::int64_t{0},
                "same-bundle tombstone removes incoming event");
    check_equal(connection.exec("SELECT count(*) FROM dose_events WHERE id = '" +
                                std::string(kDeletedD) + "'::uuid")
                    .integer(0, 0),
                std::int64_t{0},
                "deletion for absent event is stored as tombstone");
    check_equal(connection.exec("SELECT value FROM lab_results WHERE id = '" +
                                std::string(kLab) + "'::uuid")
                    .number(0, 0),
                180.0,
                "newer incoming lab updates");
    check_equal(connection.exec("SELECT count(*) FROM quick_doses")
                    .integer(0, 0),
                std::int64_t{1},
                "blocked quick-dose category is unchanged");
    check_equal(connection.exec("SELECT count(*) FROM supplies")
                    .integer(0, 0),
                std::int64_t{1},
                "new supply is added");
    check_equal(connection.exec("SELECT count(*) FROM plan_items")
                    .integer(0, 0),
                std::int64_t{1},
                "new plan item is added");

    const Settings stored = SettingsRepository(connection).get();
    check(stored.body_weight_kg.has_value() &&
              std::abs(*stored.body_weight_kg - 75.0) < 1e-12,
          "newer weight wins");
    check_equal(*stored.birth_year, 1991, "newer birth year wins");
    check_equal(stored.calibration_method, std::string("ekf"),
                "calibration method imports");
    check_equal(stored.calibration_history_mode, std::string("forward"),
                "calibration history mode imports");
    check_equal(*stored.pk_params_json, std::string(R"({"e2_kClear": 0.4})"),
                "newer PK params win");

    const auto theme = PreferencesRepository(connection).get("app-theme");
    check(theme.has_value() && theme->value_json == R"("dark")",
          "allowlisted preference imports");
    check(!PreferencesRepository(connection).get("unknown-key").has_value(),
          "unknown preference is ignored");

    export_backup(connection, exported, now);
    const auto out = read_json(exported);
    check_equal(out.at("format").get<std::string>(),
                std::string("oyama-hrt-complete-backup"),
                "export format");
    check(std::abs(out.at("weight").get<double>() - 75.0) < 1e-12,
          "export weight");
    check_equal(out.at("birthYear").get<int>(), 1991, "export birth year");
    check_equal(out.at("modes").at("transfem").at("events").size(),
                std::size_t{2}, "export keeps two events");
    check_equal(
        out.at("modes").at("transfem").at("deletions").at("events").size(),
        std::size_t{2}, "export keeps tombstones");
    check_equal(out.at("modes").at("transfem").at("quickDoses").size(),
                std::size_t{1}, "export keeps one quick dose");
    check_equal(out.at("preferences").at("app-theme").get<std::string>(),
                std::string("dark"), "export preference");

    const auto bad = dir / "bad.json";
    write_file(bad, R"({"format":"wrong"})");
    const auto bad_preview = preview_backup(connection, bad, now);
    check(!bad_preview.ok, "invalid envelope is reported");
    check_equal(SettingsRepository(connection).get().body_weight_kg.value(),
                75.0, "invalid preview does not mutate");

    std::filesystem::remove_all(dir);
}

void run_vault_backup_checks(const ConnectionConfig& config) {
    const std::string phrase = "correct horse battery staple";
    VaultStore vault(config);
    vault.open();
    vault.setup(phrase, VaultKdfParams{1, 8 * 1024 * 1024});
    vault.unlock(phrase);

    const std::int64_t now = 5000;
    const auto dir = make_temp_dir();

    VaultDoseEventRepository events(vault);
    DoseEvent dose;
    dose.id = kEventA;
    dose.mode = Mode::Transfem;
    dose.route = "oral";
    dose.ester = "E2";
    dose.time_h = 100.0;
    dose.dose_mg = 2.0;
    dose.updated_at = 4000;
    events.upsert(dose);

    VaultLabResultRepository labs(vault);
    LabResult lab;
    lab.id = kLab;
    lab.mode = Mode::Transfem;
    lab.time_h = 150.0;
    lab.value = 180.0;
    lab.unit = "pg/ml";
    lab.assay = "lcms";
    lab.updated_at = 4000;
    labs.upsert(lab);

    VaultSettingsRepository settings(vault);
    Settings current = settings.get();
    current.body_weight_kg = 70.0;
    current.weight_updated_at = 3000;
    current.updated_at = 3000;
    settings.save(current);

    const auto json_text = export_backup_json(vault, now);
    check(json_text.find("\"format\":\"oyama-hrt-complete-backup\"") !=
              std::string::npos,
          "vault export carries the upstream format marker");
    check(json_text.find(phrase) == std::string::npos,
          "vault export does not contain the phrase");

    const auto exported = dir / "vault-exported.json";
    export_backup(vault, exported, now);
    struct stat metadata {};
    check(::stat(exported.c_str(), &metadata) == 0, "vault export exists");
    check((metadata.st_mode & 0777) == 0600, "vault export mode is 0600");
    std::ifstream exported_input(exported, std::ios::binary);
    const std::string exported_text(
        (std::istreambuf_iterator<char>(exported_input)),
        std::istreambuf_iterator<char>());
    check(exported_text == json_text,
          "vault export file matches export_backup_json");

    const auto preview = preview_backup(vault, exported, now);
    check(preview.ok, "vault bundle previews");
    check_equal(preview.counts.at("transfem").events, std::size_t{1},
                "vault preview counts dose");
    check_equal(preview.counts.at("transfem").lab_results, std::size_t{1},
                "vault preview counts lab");

    vault.clear();
    check(vault.scan().empty(), "vault is empty before import");
    const auto imported = import_backup(vault, exported, now, phrase);
    check(imported.applied, "vault import applies");

    const auto restored_events = events.list(Mode::Transfem);
    check_equal(restored_events.size(), std::size_t{1},
                "vault dose returns");
    if (restored_events.size() == 1) {
        check_equal(restored_events.front().id, std::string(kEventA),
                    "vault dose keeps its id");
        check_equal(restored_events.front().route, std::string("oral"),
                    "vault dose keeps its payload");
        check(std::abs(restored_events.front().dose_mg - 2.0) < 1e-12,
              "vault dose keeps its amount");
    }
    const auto restored_lab = labs.get(kLab);
    check(restored_lab.has_value(), "vault lab returns");
    if (restored_lab.has_value()) {
        check_equal(restored_lab->unit, std::string("pg/ml"),
                    "vault lab keeps its unit");
        check(std::abs(restored_lab->value - 180.0) < 1e-12,
              "vault lab keeps its value");
    }
    check(settings.get().body_weight_kg.has_value() &&
              std::abs(*settings.get().body_weight_kg - 70.0) < 1e-12,
          "vault settings return");
    check(export_backup_json(vault, now) == json_text,
          "vault export round-trips identically");

    const auto before_invalid = export_backup_json(vault, now);
    const auto bad = dir / "vault-bad.json";
    write_file(bad, "{not json");
    bool rejected = false;
    try {
        (void)import_backup(vault, bad, now, phrase);
    } catch (const Error&) {
        rejected = true;
    }
    check(rejected, "invalid vault backup throws");
    check(export_backup_json(vault, now) == before_invalid,
          "invalid vault backup leaves the vault unchanged");

    auto older = read_json(exported);
    older["modes"]["transfem"]["events"][0]["updatedAt"] = 1000;
    older["modes"]["transfem"]["events"][0]["doseMG"] = 1.0;
    const auto older_path = dir / "vault-older.json";
    write_file(older_path, older.dump());
    const auto older_result = import_backup(vault, older_path, now, phrase);
    check(older_result.applied, "older vault bundle applies");
    const auto after_lww = events.get(kEventA);
    check(after_lww.has_value() &&
              std::abs(after_lww->dose_mg - 2.0) < 1e-12,
          "older incoming vault record loses");

    auto tombstoned = read_json(exported);
    tombstoned["modes"]["transfem"]["labResults"] = json::array();
    tombstoned["modes"]["transfem"]["deletions"]["labResults"] = {{kLab, 4500}};
    const auto tombstone_path = dir / "vault-tombstone.json";
    write_file(tombstone_path, tombstoned.dump());
    const auto removed = import_backup(vault, tombstone_path, now, phrase);
    check(removed.applied, "tombstone vault bundle applies");
    check(!labs.get(kLab).has_value(), "tombstone removes the vault lab");

    const auto encrypted = dir / "vault-encrypted.bin";
    write_file(encrypted, "OYAMAEBK-not-built-yet");
    rejected = false;
    try {
        (void)preview_backup(vault, encrypted, now, phrase);
    } catch (const Error& error) {
        rejected = std::string(error.what()).find(
                       "encrypted backup support not built") !=
                   std::string::npos;
    }
    check(rejected, "encrypted vault backup reports unsupported");

    std::filesystem::remove_all(dir);
}

void test_integration() {
    const char* socket = std::getenv("OYAMA_TEST_PG_SOCKET");
    const char* dbname = std::getenv("OYAMA_TEST_PG_DB");
    if (socket == nullptr || *socket == '\0' || dbname == nullptr ||
        *dbname == '\0') {
        std::cout << "SKIP: set OYAMA_TEST_PG_SOCKET and OYAMA_TEST_PG_DB\n";
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
    (void)connection.exec("SET search_path TO public");
    (void)MigrationRunner(migrations_dir()).apply(connection);
    const std::string schema =
        "oyama_backup_" + std::to_string(::getpid()) + "_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    (void)connection.exec("CREATE SCHEMA " +
                          connection.quote_identifier(schema));
    (void)connection.exec("SET search_path TO " +
                          connection.quote_identifier(schema));
    (void)MigrationRunner(migrations_dir()).apply(connection);
    run_backup_checks(connection);
    (void)connection.exec("SET search_path TO public");
    run_vault_backup_checks(config);
    (void)connection.exec("DROP SCHEMA " + connection.quote_identifier(schema) +
                          " CASCADE");
}

}  // namespace

int main() {
    try {
        test_integration();
    } catch (const std::exception& error) {
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
        return 1;
    }
    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "backup_test: ok\n";
    return 0;
}
