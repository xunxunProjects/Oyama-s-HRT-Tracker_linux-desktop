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

#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
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

oyama::fs::path migrations_dir() {
#ifdef OYAMA_TEST_MIGRATIONS_DIR
    return oyama::fs::path(OYAMA_TEST_MIGRATIONS_DIR);
#else
    return oyama::fs::path("storage/migrations");
#endif
}

oyama::fs::path make_temp_dir() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = oyama::fs::temp_directory_path() /
                ("oyama-backup-test-" + std::to_string(::getpid()) + "-" +
                 std::to_string(stamp));
    oyama::fs::create_directories(path);
    return path;
}

void write_file(const oyama::fs::path& path, oyama::string_view contents) {
    std::ofstream out(path, std::ios::binary);
    out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
}

std::string read_file(const oyama::fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(input)),
                       std::istreambuf_iterator<char>());
}

json read_json(const oyama::fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return json::parse(input);
}

// Runs `probe` in a child process with an alarm.  A crash or a blocking read
// then fails the check instead of taking down or hanging the test harness.
bool run_probe(const std::function<void()>& probe, std::string& message) {
    int fds[2];
    if (::pipe(fds) != 0) {
        message = std::string("pipe failed: ") + std::strerror(errno);
        return false;
    }
    const pid_t pid = ::fork();
    if (pid < 0) {
        ::close(fds[0]);
        ::close(fds[1]);
        message = std::string("fork failed: ") + std::strerror(errno);
        return false;
    }
    if (pid == 0) {
        ::close(fds[0]);
        (void)::alarm(5);
        int code = 1;
        try {
            probe();
        } catch (const Error& error) {
            const char* text = error.what();
            const auto size = std::strlen(text);
            std::size_t offset = 0;
            while (offset < size) {
                const ssize_t written =
                    ::write(fds[1], text + offset, size - offset);
                if (written <= 0) {
                    break;
                }
                offset += static_cast<std::size_t>(written);
            }
            code = 0;
        } catch (...) {
            code = 2;
        }
        ::close(fds[1]);
        ::_exit(code);
    }

    ::close(fds[1]);
    char buffer[1024];
    std::string output;
    for (;;) {
        const ssize_t count = ::read(fds[0], buffer, sizeof(buffer));
        if (count > 0) {
            output.append(buffer, static_cast<std::size_t>(count));
        } else if (count == 0) {
            break;
        } else if (errno != EINTR) {
            break;
        }
    }
    ::close(fds[0]);

    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    }
    if (WIFSIGNALED(status)) {
        message = "probe killed by signal " +
                  std::to_string(WTERMSIG(status));
        return false;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        message = WIFEXITED(status)
                      ? "probe exited " + std::to_string(WEXITSTATUS(status))
                      : "probe did not exit normally";
        return false;
    }
    message = output;
    return true;
}

std::string deep_bundle(std::size_t depth) {
    std::string nested(depth, '[');
    nested.append(depth, ']');
    return std::string(
               R"({"format":"oyama-hrt-complete-backup","formatVersion":1,)"
               R"("createdAt":"2026-11-01T00:00:00Z","mode":"transfem",)"
               R"("modes":{"transfem":{"events":[{"id":)"
               R"("11111111-1111-4111-8111-111111111111","route":"oral",)"
               R"("ester":"E2","timeH":1,"doseMG":1,"extras":)") +
           nested +
           R"(}],"labResults":[],"doseTemplates":[],"quickDoses":[],)"
           R"("supplies":[],"plan":[],"deletions":{}},"transmasc":{)"
           R"("events":[],"labResults":[],"doseTemplates":[],)"
           R"("quickDoses":[],"supplies":[],"plan":[],"deletions":{}}}}})";
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

    const auto symlink_target = dir / "symlink-target.txt";
    write_file(symlink_target, "must not be overwritten");
    const auto symlink_path = dir / "vault-symlink.json";
    std::error_code symlink_error;
    oyama::fs::create_symlink(symlink_target, symlink_path, symlink_error);
    check(!symlink_error, "symlink fixture is created");
    if (!symlink_error) {
        bool refused = false;
        try {
            export_backup(vault, symlink_path, now);
        } catch (const Error&) {
            refused = true;
        }
        check(refused, "vault export refuses a symlink destination");
        check(read_file(symlink_target) == "must not be overwritten",
              "vault export does not follow a symlink destination");
    }

    const auto fifo_path = dir / "vault-fifo.json";
    if (::mkfifo(fifo_path.c_str(), 0600) == 0) {
        std::string message;
        const bool fifo_rejected =
            run_probe([&]() { (void)export_backup(vault, fifo_path, now); },
                      message);
        check(fifo_rejected, "vault export rejects a FIFO without blocking");
    }

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

    // Braces inside strings must not count toward JSON nesting.
    auto extras_bundle = read_json(exported);
    std::string brace_text(2048, '{');
    brace_text += "[[[";
    json extras_value = json::object();
    extras_value["note"] = brace_text;
    extras_value["nested"] = {{"a", 1}, {"b", json::array({1, 2, 3})}};
    extras_bundle["modes"]["transfem"]["events"][0]["extras"] = extras_value;
    extras_bundle["modes"]["transfem"]["events"][0]["updatedAt"] = 4200;
    const auto extras_path = dir / "vault-extras.json";
    write_file(extras_path, extras_bundle.dump());
    const auto extras_import = import_backup(vault, extras_path, now, phrase);
    check(extras_import.applied, "normal nested extras import applies");
    const auto extras_event = events.get(kEventA);
    check(extras_event.has_value() &&
              extras_event->extras_json.find(brace_text) != std::string::npos,
          "normal nested extras round-trip unchanged");

    auto deep_extras_bundle = read_json(exported);
    json deep_extras = json::object();
    for (int i = 0; i < 20; ++i) {
        deep_extras = json{{"level", deep_extras}};
    }
    deep_extras_bundle["modes"]["transfem"]["events"][0]["extras"] =
        deep_extras;
    const auto deep_extras_path = dir / "vault-deep-extras.json";
    write_file(deep_extras_path, deep_extras_bundle.dump());
    const auto deep_extras_preview =
        preview_backup(vault, deep_extras_path, now, phrase);
    check(std::find(deep_extras_preview.blocked.begin(),
                    deep_extras_preview.blocked.end(),
                    "transfem.events") != deep_extras_preview.blocked.end(),
          "deep per-record extras block the category");

    auto large_extras_bundle = read_json(exported);
    large_extras_bundle["modes"]["transfem"]["events"][0]["extras"] = {
        {"note", std::string(20 * 1024, 'x')}};
    const auto large_extras_path = dir / "vault-large-extras.json";
    write_file(large_extras_path, large_extras_bundle.dump());
    const auto large_extras_preview =
        preview_backup(vault, large_extras_path, now, phrase);
    check(std::find(large_extras_preview.blocked.begin(),
                    large_extras_preview.blocked.end(),
                    "transfem.events") != large_extras_preview.blocked.end(),
          "oversize per-record extras block the category");

    auto deep_pk_bundle = read_json(exported);
    json deep_pk = json::object();
    for (int i = 0; i < 20; ++i) {
        deep_pk = json{{"level", deep_pk}};
    }
    deep_pk_bundle["pkParams"] = deep_pk;
    const auto deep_pk_path = dir / "vault-deep-pk.json";
    write_file(deep_pk_path, deep_pk_bundle.dump());
    const auto deep_pk_preview =
        preview_backup(vault, deep_pk_path, now, phrase);
    check(!deep_pk_preview.ok, "deep pkParams is rejected");
    bool pk_error_named = false;
    for (const auto& error : deep_pk_preview.errors) {
        if (error.find("pkParams") != std::string::npos) {
            pk_error_named = true;
        }
    }
    check(pk_error_named, "deep pkParams error names the field");

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

    // A 30,000-tombstone import used to perform one full vault scan per
    // tombstone (quadratic decrypts).  Exercise a meaningful batch and make a
    // regression terminate rather than run for minutes.
    vault.clear();
    VaultTombstoneRepository tombstones(vault);
    const std::string shared_id = "55555555-5555-4555-8555-555555555555";
    tombstones.record(Tombstone{"events", shared_id, 4500});

    constexpr std::size_t kBatchPerKind = 2000;
    json batch{{"format", "oyama-hrt-complete-backup"},
               {"formatVersion", 1},
               {"createdAt", "2026-11-01T00:00:00Z"},
               {"mode", "transfem"},
               {"modes", json::object()}};
    for (const auto* key : {"transfem", "transmasc"}) {
        batch["modes"][key] = {
            {"events", json::array()},       {"labResults", json::array()},
            {"doseTemplates", json::array()}, {"quickDoses", json::array()},
            {"supplies", json::array()},     {"plan", json::array()},
            {"deletions", json::object()}};
    }
    auto& deletions = batch["modes"]["transfem"]["deletions"];
    deletions["events"] = json::object();
    deletions["labResults"] = json::object();
    deletions["doseTemplates"] = json::object();
    deletions["events"][shared_id] = 4600;
    deletions["labResults"][shared_id] = 4700;
    for (std::size_t i = 0; i < kBatchPerKind; ++i) {
        deletions["events"][generate_uuid_v4()] = 4000 + (i % 1000);
        deletions["labResults"][generate_uuid_v4()] = 4000 + (i % 1000);
        deletions["doseTemplates"][generate_uuid_v4()] = 4000 + (i % 1000);
    }
    const auto batch_path = dir / "vault-tombstone-batch.json";
    write_file(batch_path, batch.dump());

    const auto batch_started = std::chrono::steady_clock::now();
    std::signal(SIGALRM, [](int) { ::_exit(124); });
    (void)::alarm(30);
    const auto batch_import = import_backup(vault, batch_path, now, phrase);
    (void)::alarm(0);
    const auto batch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - batch_started)
                              .count();
    check(batch_import.applied, "tombstone batch import applies");
    check(batch_ms < 15000,
          "tombstone batch import stays linear (took " +
              std::to_string(batch_ms) + " ms)");

    const auto listed_tombstones = tombstones.list();
    check_equal(listed_tombstones.size(), kBatchPerKind * 3 + 2,
                "tombstone batch import keeps every kind/id pair");
    bool shared_event_kept = false;
    bool shared_lab_kept = false;
    for (const auto& tombstone : listed_tombstones) {
        if (tombstone.record_id != shared_id) {
            continue;
        }
        if (tombstone.kind == "events") {
            shared_event_kept = tombstone.deleted_at == 4600;
        } else if (tombstone.kind == "labResults") {
            shared_lab_kept = tombstone.deleted_at == 4700;
        }
    }
    check(shared_event_kept,
          "tombstone batch import keeps the newest same-kind stamp");
    check(shared_lab_kept,
          "tombstone batch import keeps equal ids in different kinds");

    oyama::fs::remove_all(dir);
}

void test_input_limits() {
    ConnectionConfig config;
    VaultStore vault(config);
    const auto dir = make_temp_dir();
    const std::int64_t now = 5000;
    std::string message;

    const auto fifo = dir / "probe.fifo";
    check(::mkfifo(fifo.c_str(), 0600) == 0, "FIFO fixture is created");
    const bool fifo_rejected = run_probe(
        [&]() { (void)preview_backup(vault, fifo, now); }, message);
    check(fifo_rejected, "preview rejects a FIFO without blocking");
    check(message.find("regular file") != std::string::npos,
          "FIFO rejection names the regular-file requirement");
    const bool fifo_header_rejected = run_probe(
        [&]() { (void)backup_file_is_encrypted(fifo); }, message);
    check(fifo_header_rejected,
          "encrypted-file pre-check rejects a FIFO without blocking");

    const auto device_link = dir / "device-link.json";
    std::error_code link_error;
    oyama::fs::create_symlink("/dev/zero", device_link, link_error);
    check(!link_error, "device symlink fixture is created");
    if (!link_error) {
        const bool link_rejected = run_probe(
            [&]() { (void)preview_backup(vault, device_link, now); }, message);
        check(link_rejected, "preview rejects a symlink to a device");
    }

    const auto oversize = dir / "oversize.json";
    {
        std::ofstream out(oversize, std::ios::binary);
        out.put('\0');
    }
    constexpr std::uintmax_t kOversize =
        static_cast<std::uintmax_t>(kMaxBackupFileBytes) + 1U;
    std::error_code resize_error;
    oyama::fs::resize_file(oversize, kOversize, resize_error);
    check(!resize_error, "oversize sparse fixture is created");
    if (!resize_error) {
        const bool oversize_rejected = run_probe(
            [&]() { (void)preview_backup(vault, oversize, now); }, message);
        check(oversize_rejected, "preview rejects an oversize backup");
        check(message.find("too large") != std::string::npos,
              "oversize rejection names the size limit");
        const bool oversize_header_rejected = run_probe(
            [&]() { (void)backup_file_is_encrypted(oversize); }, message);
        check(oversize_header_rejected,
              "encrypted-file pre-check rejects an oversize backup");
    }

    const auto deep = dir / "deep.json";
    write_file(deep, deep_bundle(50000));
    const bool deep_rejected = run_probe(
        [&]() { (void)preview_backup(vault, deep, now); }, message);
    check(deep_rejected, "preview rejects deep JSON without crashing");
    check(message.find("nested too deeply") != std::string::npos,
          "deep JSON rejection names the nesting limit");

    oyama::fs::remove_all(dir);
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
        test_input_limits();
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
