#include "oyama/storage/connection.h"
#include "oyama/storage/legacy_migration.h"
#include "oyama/storage/uuid.h"
#include "oyama/storage/vault_codec.h"
#include "oyama/storage/vault_store.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

using namespace oyama::storage;
using json = nlohmann::json;

namespace {

int failures = 0;

void check(bool condition, std::string_view what) {
    if (!condition) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

std::string env_value(std::string_view name) {
    const std::string key(name);
    const char* value = std::getenv(key.c_str());
    return value != nullptr ? value : "";
}

ConnectionConfig config_from_env(std::string_view prefix) {
    const std::string base(prefix);
    ConnectionConfig config;
    config.socket_dir = env_value(base + "_SOCKET");
    config.dbname = env_value(base + "_DB");
    config.user = env_value(base + "_USER");
    const std::string port = env_value(base + "_PORT");
    config.port = port.empty() ? 0 : std::stoi(port);
    config.application_name = "legacy_migration_test";
    return config;
}

bool env_available(std::string_view prefix) {
    const std::string base(prefix);
    return !env_value(base + "_SOCKET").empty() &&
           !env_value(base + "_DB").empty() &&
           !env_value(base + "_USER").empty() &&
           !env_value(base + "_PORT").empty();
}

std::int64_t count_rows(Connection& connection, const std::string& sql) {
    const auto result = connection.exec(sql);
    if (result.rows() != 1) {
        throw std::runtime_error("count query returned " +
                                 std::to_string(result.rows()) + " rows");
    }
    return result.integer(0, 0);
}

constexpr const char* kSettingsId =
    "00000000-0000-0000-0000-000000000001";
constexpr const char* kPreferencesId =
    "00000000-0000-0000-0000-000000000002";
constexpr const char* kInvalidDoseId =
    "00000000-0000-0000-0000-0000000000aa";
constexpr const char* kDeletedRecordId =
    "00000000-0000-0000-0000-000000000199";

}  // namespace

int main() {
    if (!env_available("OYAMA_TEST_LEGACY_PG") ||
        !env_available("OYAMA_TEST_VAULT_PG") ||
        !env_available("OYAMA_TEST_FRESH_PG")) {
        std::cout << "SKIP: OYAMA_TEST_*_PG_* is not set\n";
        return 77;
    }

    try {
        const ConnectionConfig legacy_config =
            config_from_env("OYAMA_TEST_LEGACY_PG");
        const ConnectionConfig vault_config =
            config_from_env("OYAMA_TEST_VAULT_PG");
        const ConnectionConfig fresh_config =
            config_from_env("OYAMA_TEST_FRESH_PG");

        Connection source(legacy_config);
        Connection fresh(fresh_config);

        const LegacyDetection detection = detect_legacy(source);
        check(detection.has_legacy && !detection.has_meta,
              "legacy detected before vault meta exists");

        const LegacyDetection fresh_detection = detect_legacy(fresh);
        check(!fresh_detection.has_legacy,
              "fresh default settings are not legacy data");
        check(!fresh_detection.has_meta,
              "fresh database has no vault meta");

        const std::string phrase = "correct horse battery staple";
        VaultStore vault(vault_config);
        vault.open();
        check(vault.state() == VaultState::Uninitialized, "fresh vault");
        vault.setup(phrase, VaultKdfParams{1, 8 * 1024 * 1024});
        check(vault.state() == VaultState::Locked, "setup leaves locked");
        vault.unlock(phrase);
        check(vault.is_unlocked(), "vault unlocked");

        Connection meta_check(vault_config);
        const LegacyDetection meta_detection = detect_legacy(meta_check);
        check(meta_detection.has_meta, "has_meta after vault setup");
        check(!meta_detection.has_legacy,
              "vault-only database has no legacy data");

        LegacyMigrator migrator(source, vault);
        const LegacyImportResult result = migrator.import_all();

        check(result.counts.settings == 1, "settings count");
        check(result.counts.preferences == 2, "preferences count");
        check(result.counts.dose_events == 2, "dose event count");
        check(result.counts.lab_results == 1, "lab result count");
        check(result.counts.dose_templates == 1, "dose template count");
        check(result.counts.quick_doses == 1, "quick dose count");
        check(result.counts.supplies == 1, "supply count");
        check(result.counts.plan_items == 1, "plan item count");
        check(result.counts.tombstones == 1, "tombstone count");

        check(result.source_sha256.size() == 64,
              "source hash is a SHA-256 hex digest");
        check(result.target_sha256.size() == 64,
              "target hash is a SHA-256 hex digest");
        check(result.source_sha256 == result.target_sha256,
              "canonical source and target hashes match");
        check(detect_legacy(source).has_legacy, "source unchanged");
        check(count_rows(source, "SELECT count(*) FROM dose_events") == 2,
              "source dose rows unchanged");

        const auto settings_record = vault.get(kSettingsId);
        check(settings_record.has_value() &&
                  settings_record->kind == "settings",
              "settings singleton imported");
        if (settings_record.has_value()) {
            const Settings settings = decode_settings(settings_record->payload);
            check(settings.body_weight_kg.has_value() &&
                      *settings.body_weight_kg == 60.0,
                  "settings weight projection");
            check(settings.weight_updated_at.has_value() &&
                      *settings.weight_updated_at == 100,
                  "settings weight stamp projection");
            check(settings.birth_year.has_value() &&
                      *settings.birth_year == 1990,
                  "settings birth year projection");
            check(settings.birth_year_updated_at.has_value() &&
                      *settings.birth_year_updated_at == 101,
                  "settings birth year stamp projection");
            check(settings.pk_params_updated_at.has_value() &&
                      *settings.pk_params_updated_at == 102,
                  "settings pk params stamp projection");
            check(settings.calibration_method == "mipd" &&
                      settings.calibration_history_mode == "retrospective",
                  "settings calibration projection");
            check(settings.updated_at == 103, "settings stamp projection");
            check(settings_record->updated_at_ms == 103,
                  "settings envelope stamp");
            const json expected = {
                {"weight", 60.0},
                {"weightUpdatedAt", 100},
                {"birthYear", 1990},
                {"birthYearUpdatedAt", 101},
                {"pkParams", {{"e2_kClear", 0.4}}},
                {"pkParamsUpdatedAt", 102},
                {"calibration",
                 {{"method", "mipd"}, {"historyMode", "retrospective"}}},
                {"updatedAt", 103},
            };
            check(settings_record->payload == expected.dump(2, ' ', false),
                  "settings canonical payload");
        }

        const auto preferences_record = vault.get(kPreferencesId);
        check(preferences_record.has_value() &&
                  preferences_record->kind == "preferences",
              "preferences aggregate imported");
        if (preferences_record.has_value()) {
            const auto preferences =
                decode_preferences(preferences_record->payload);
            check(preferences.size() == 2, "preference projection count");
            const json expected = {
                {"items",
                 {{"app-theme",
                   {{"value", "dark"}, {"updatedAt", 140}}},
                  {"hrt-lang", {{"value", "zh"}, {"updatedAt", 141}}}}},
            };
            check(preferences_record->payload == expected.dump(2, ' ', false),
                  "preferences canonical payload");
            check(preferences_record->updated_at_ms == 141,
                  "preferences aggregate envelope stamp");
        }

        const auto tombstones = vault.scan("tombstone");
        check(tombstones.size() == 1, "one tombstone imported");
        if (tombstones.size() == 1) {
            const Tombstone decoded = decode_tombstone(tombstones[0].payload);
            check(decoded.kind == "dose_events" &&
                      decoded.record_id == kDeletedRecordId &&
                      decoded.deleted_at == 150,
                  "tombstone projection");
            check(tombstones[0].updated_at_ms == 150,
                  "tombstone envelope stamp");
            check(is_uuid(tombstones[0].id), "tombstone vault id is a UUID");
            const json expected = {
                {"kind", "dose_events"},
                {"recordId", kDeletedRecordId},
                {"deletedAt", 150},
            };
            check(tombstones[0].payload == expected.dump(2, ' ', false),
                  "tombstone canonical payload");
        }

        vault.clear();
        const LegacyImportResult repeated = migrator.import_all();
        check(repeated.source_sha256 == result.source_sha256,
              "repeat import source hash stable");
        check(repeated.target_sha256 == result.target_sha256,
              "repeat import target hash stable");
        check(repeated.target_sha256 == repeated.source_sha256,
              "repeat import hashes match");
        check(repeated.counts.preferences == 2, "repeat preference count");
        check(repeated.counts.tombstones == 1, "repeat tombstone count");

        vault.clear();
        check(vault.scan().empty(), "target empty before invalid import");
        (void)source.exec(
            "INSERT INTO dose_events "
            "(id, mode, route, ester, time_h, dose_mg, extras, updated_at, "
            "created_at) VALUES "
            "('00000000-0000-0000-0000-0000000000aa', 'transfem', 'oral', "
            "'E9', 1.0, 1.0, '{}'::jsonb, 1, 1)");
        bool invalid_threw = false;
        try {
            (void)migrator.import_all();
        } catch (const LegacyMigrationError& error) {
            invalid_threw = true;
            check(error.kind() == "dose_event", "invalid row error kind");
            check(error.id() == kInvalidDoseId, "invalid row error id");
            check(error.field() == "ester", "invalid row error field");
        }
        check(invalid_threw, "strict invalid legacy row is rejected");
        check(vault.scan().empty(),
              "failed strict import rolls back to an empty vault");
        check(count_rows(meta_check, "SELECT count(*) FROM vault_records") == 0,
              "no raw vault rows after rollback");
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }

    if (failures != 0) {
        std::cerr << "legacy_migration_test: " << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "legacy_migration_test: ok\n";
    return 0;
}
