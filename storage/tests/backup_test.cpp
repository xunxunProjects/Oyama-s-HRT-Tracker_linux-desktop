#include <oyama/storage/storage.h>
#include <oyama/storage/backup.h>
#include <oyama/storage/backup_crypto.h>
#include <oyama/storage/vault_dose_event_repository.h>
#include <oyama/storage/vault_json_record_repository.h>
#include <oyama/storage/vault_lab_result_repository.h>
#include <oyama/storage/vault_preferences_repository.h>
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
constexpr const char* kLab = "33333333-3333-4333-8333-333333333333";

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

    VaultJsonRecordRepository templates(vault, JsonRecordKind::DoseTemplate);
    JsonRecord template_record;
    template_record.id = "44444444-4444-4444-8444-444444444444";
    template_record.mode = Mode::Transfem;
    template_record.payload_json =
        R"({"id":"44444444-4444-4444-8444-444444444444","name":"weekly","route":"injection","ester":"EV","doseMG":2.0,"extras":{},"createdAt":1})";
    template_record.updated_at = 4000;
    templates.upsert(template_record);

    VaultPreferencesRepository preferences(vault);
    preferences.set({"app-theme", R"("dark")", 7000});
    preferences.set({"custom-key", R"({"x":1})", 8000});
    preferences.set({"app-show-prediction", "true", 9000});

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

    const auto exported_root = json::parse(json_text);
    const auto& exported_template =
        exported_root["modes"]["transfem"]["doseTemplates"][0];
    check(!exported_template.contains("mode"),
          "vault template export omits the internal mode");
    const auto self_preview = preview_backup(vault, exported, now);
    check(self_preview.ok, "same-vault preview succeeds");
    check(self_preview.blocked.empty(),
          "same-vault preview is not blocked by equal stamps");
    const auto self_import = import_backup(vault, exported, now, phrase);
    check(self_import.applied, "same-vault import applies");
    check(templates.get(template_record.id).has_value(),
          "same-vault import keeps the template");

    const auto preview = preview_backup(vault, exported, now);
    check(preview.ok, "vault bundle previews");
    check_equal(preview.counts.at("transfem").events, std::size_t{1},
                "vault preview counts dose");
    check_equal(preview.counts.at("transfem").lab_results, std::size_t{1},
                "vault preview counts lab");

    auto preference_bundle = read_json(exported);
    preference_bundle["preferences"] = {{"app-theme", "light"}};
    const auto preference_path = dir / "vault-preference.json";
    write_file(preference_path, preference_bundle.dump());
    const auto preference_import =
        import_backup(vault, preference_path, now, phrase);
    check(preference_import.applied, "preference-only vault import applies");
    const auto theme = preferences.get("app-theme");
    check(theme.has_value() && theme->value_json == R"("light")",
          "imported preference value wins");
    check(theme.has_value() && theme->updated_at > 0,
          "imported preference keeps a real timestamp");
    const auto custom = preferences.get("custom-key");
    check(custom.has_value() && custom->value_json == R"({"x":1})" &&
              custom->updated_at == 8000,
          "non-backup preference key and stamp survive import");
    const auto prediction = preferences.get("app-show-prediction");
    check(prediction.has_value() && prediction->value_json == "true" &&
              prediction->updated_at == 9000,
          "unchanged preference key and stamp survive import");

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
    const std::string backup_phrase = "separate backup phrase";
    export_encrypted_backup(vault, encrypted, now, backup_phrase);
    struct stat encrypted_metadata {};
    check(::stat(encrypted.c_str(), &encrypted_metadata) == 0,
          "encrypted vault export exists");
    check((encrypted_metadata.st_mode & 0777) == 0600,
          "encrypted vault export mode is 0600");
    std::ifstream encrypted_input(encrypted, std::ios::binary);
    const std::string encrypted_text(
        (std::istreambuf_iterator<char>(encrypted_input)),
        std::istreambuf_iterator<char>());
    check(is_encrypted_backup(encrypted_text),
          "encrypted export has the OYAMAEBK magic");
    check(encrypted_text.find("oyama-hrt-complete-backup") ==
              std::string::npos,
          "encrypted export contains no plaintext");

    const auto encrypted_preview =
        preview_backup(vault, encrypted, now, backup_phrase);
    check(encrypted_preview.ok, "encrypted vault bundle previews");
    check_equal(encrypted_preview.counts.at("transfem").events,
                std::size_t{1}, "encrypted preview counts the dose");
    check_equal(encrypted_preview.counts.at("transfem").lab_results,
                std::size_t{0}, "encrypted preview counts the tombstoned lab");

    rejected = false;
    try {
        (void)preview_backup(vault, encrypted, now);
    } catch (const Error& error) {
        rejected = std::string(error.what()).find("phrase") !=
                   std::string::npos;
    }
    check(rejected, "encrypted preview requires a backup phrase");

    rejected = false;
    try {
        (void)preview_backup(vault, encrypted, now, "wrong backup phrase");
    } catch (const Error&) {
        rejected = true;
    }
    check(rejected, "encrypted preview rejects the wrong phrase");

    const auto before_wrong_import = export_backup_json(vault, now);
    rejected = false;
    try {
        (void)import_backup(vault, encrypted, now, "wrong backup phrase");
    } catch (const Error&) {
        rejected = true;
    }
    check(rejected, "encrypted import rejects the wrong phrase");
    check(export_backup_json(vault, now) == before_wrong_import,
          "wrong encrypted import leaves the vault unchanged");

    auto tampered_text = encrypted_text;
    tampered_text.back() = static_cast<char>(
        static_cast<unsigned char>(tampered_text.back()) ^ 0x01U);
    const auto tampered = dir / "vault-tampered.bin";
    write_file(tampered, tampered_text);
    rejected = false;
    try {
        (void)import_backup(vault, tampered, now, backup_phrase);
    } catch (const Error&) {
        rejected = true;
    }
    check(rejected, "tampered encrypted import is rejected");
    check(export_backup_json(vault, now) == before_wrong_import,
          "tampered encrypted import leaves the vault unchanged");

    vault.clear();
    const auto encrypted_import =
        import_backup(vault, encrypted, now, backup_phrase);
    check(encrypted_import.applied, "encrypted vault import applies");
    const auto encrypted_events = events.list(Mode::Transfem);
    check_equal(encrypted_events.size(), std::size_t{1},
                "encrypted import restores the dose");
    if (encrypted_events.size() == 1) {
        check_equal(encrypted_events.front().id, std::string(kEventA),
                    "encrypted import keeps the dose id");
    }

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
    run_vault_backup_checks(config);
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
