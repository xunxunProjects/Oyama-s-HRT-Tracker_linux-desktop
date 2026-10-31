#include "oyama/storage/vault_codec.h"
#include "oyama/storage/vault_preferences_repository.h"
#include "oyama/storage/vault_settings_repository.h"
#include "oyama/storage/vault_store.h"
#include "oyama/storage/vault_tombstone_repository.h"
#include "oyama/storage/vault_dose_event_repository.h"
#include "oyama/storage/vault_json_record_repository.h"
#include "oyama/storage/vault_lab_result_repository.h"

#include "oyama/vault.h"

#include <libpq-fe.h>
#include <nlohmann/json.hpp>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

using namespace oyama::storage;

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

ConnectionConfig config_from_env() {
    ConnectionConfig config;
    const char* socket_dir = std::getenv("OYAMA_TEST_PG_SOCKET");
    const char* dbname = std::getenv("OYAMA_TEST_PG_DB");
    const char* user = std::getenv("OYAMA_TEST_PG_USER");
    const char* port = std::getenv("OYAMA_TEST_PG_PORT");
    if (socket_dir == nullptr || dbname == nullptr || user == nullptr ||
        port == nullptr) {
        return config;
    }
    config.socket_dir = socket_dir;
    config.dbname = dbname;
    config.user = user;
    config.port = std::atoi(port);
    config.application_name = "vault_repository_test";
    return config;
}

bool env_available() {
    return std::getenv("OYAMA_TEST_PG_SOCKET") != nullptr &&
           std::getenv("OYAMA_TEST_PG_DB") != nullptr &&
           std::getenv("OYAMA_TEST_PG_USER") != nullptr &&
           std::getenv("OYAMA_TEST_PG_PORT") != nullptr;
}

int run_sql(PGconn* conn, const char* sql) {
    PGresult* result = PQexec(conn, sql);
    if (result == nullptr) {
        std::cerr << "SQL failed: " << PQerrorMessage(conn) << '\n';
        return 0;
    }
    const ExecStatusType status = PQresultStatus(result);
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
        std::cerr << "SQL failed: " << sql << '\n'
                  << PQresultErrorMessage(result) << '\n';
        PQclear(result);
        return 0;
    }
    PQclear(result);
    return 1;
}

long query_count(PGconn* conn, const char* sql) {
    PGresult* result = PQexec(conn, sql);
    if (result == nullptr || PQresultStatus(result) != PGRES_TUPLES_OK ||
        PQntuples(result) != 1) {
        if (result != nullptr) {
            PQclear(result);
        }
        return -1;
    }
    const long value = std::strtol(PQgetvalue(result, 0, 0), nullptr, 10);
    PQclear(result);
    return value;
}

}  // namespace

int main() {
    if (!env_available()) {
        std::cout << "SKIP: OYAMA_TEST_PG_* is not set\n";
        return 77;
    }

    const ConnectionConfig config = config_from_env();
    PGconn* admin = PQconnectdb(config.to_conninfo().c_str());
    if (admin == nullptr || PQstatus(admin) != CONNECTION_OK) {
        std::cerr << "admin connection failed\n";
        if (admin != nullptr) {
            PQfinish(admin);
        }
        return 1;
    }
    if (!run_sql(admin, "TRUNCATE vault_meta, vault_records")) {
        PQfinish(admin);
        return 1;
    }

    const std::string phrase = "correct horse battery staple";
    VaultStore vault(config);
    vault.open();
    check(vault.state() == VaultState::Uninitialized, "fresh vault");
    vault.setup(phrase, VaultKdfParams{1, 8 * 1024 * 1024});
    check(vault.state() == VaultState::Locked, "setup leaves locked");
    vault.unlock(phrase);
    check(vault.is_unlocked(), "unlocked");

    VaultSettingsRepository settings(vault);
    const Settings defaults = settings.get();
    check(!defaults.body_weight_kg.has_value(), "default weight absent");
    check(!defaults.birth_year.has_value(), "default birth year absent");
    check(defaults.calibration_method == "mipd", "default calibration");
    check(query_count(admin,
                      "SELECT count(*) FROM vault_records WHERE id = "
                      "'00000000-0000-0000-0000-000000000001'") == 0,
          "default settings are not written");

    Settings saved = defaults;
    saved.body_weight_kg = 60.0;
    saved.updated_at = 123;
    settings.save(saved);
    const Settings loaded = settings.get();
    check(loaded.body_weight_kg.has_value(), "saved weight present");
    check(loaded.body_weight_kg.value_or(0.0) == 60.0, "saved weight value");
    check(loaded.updated_at == 123, "saved settings stamp");

    VaultPreferencesRepository prefs(vault);
    prefs.set(Preference{"theme", R"("dark")", 10});
    prefs.set(Preference{"lang", R"("zh")", 20});
    const auto pref_list = prefs.list();
    check(pref_list.size() == 2, "two preferences");
    check(pref_list[0].key == "lang", "preferences sorted by key");
    const auto theme = prefs.get("theme");
    check(theme.has_value() && theme->value_json == R"("dark")",
          "theme preference");
    check(prefs.remove("theme"), "remove existing preference");
    check(!prefs.get("theme").has_value(), "removed preference absent");
    prefs.set(Preference{"theme", R"("dark")", 10});

    vault.lock();
    vault.unlock(phrase);
    check(prefs.list().size() == 2, "preferences survive lock/unlock");

    VaultTombstoneRepository tombs(vault);
    const std::string tombstone_id = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
    tombs.record(Tombstone{"dose_event", tombstone_id, 99});
    check(tombs.list().size() == 1, "tombstone recorded");
    check(tombs.remove("dose_event", tombstone_id), "tombstone removed");
    check(tombs.list().empty(), "tombstone list empty after remove");

    tombs.record(Tombstone{"dose_event", "bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb", 99});
    tombs.record(Tombstone{"dose_event", "cccccccc-cccc-4ccc-8ccc-cccccccccccc", 100});
    tombs.record(Tombstone{"lab_result", "dddddddd-dddd-4ddd-8ddd-dddddddddddd", 101});
    check(tombs.prune_before(100) == 1, "prune removes one tombstone");
    check(tombs.list().size() == 2, "prune leaves newer tombstones");

    VaultDoseEventRepository doses(vault);
    DoseEvent dose_b{"00000000-0000-0000-0000-000000000020", Mode::Transfem,
                     "oral", "E2", 20.0, 2.0, "{}", 3, 1};
    DoseEvent dose_a{"00000000-0000-0000-0000-000000000021", Mode::Transfem,
                     "oral", "E2", 10.0, 1.0, "{}", 2, 1};
    doses.upsert(dose_b);
    doses.upsert(dose_a);
    const auto dose_list = doses.list(Mode::Transfem);
    check(dose_list.size() == 2 && dose_list[0].id == dose_a.id &&
              dose_list[1].id == dose_b.id,
          "doses sort by time then id");
    check(doses.list(Mode::Transmasc).empty(), "dose mode isolation");
    const auto loaded_dose = doses.get(dose_a.id);
    check(loaded_dose.has_value() && loaded_dose->dose_mg == 1.0,
          "dose get");
    check(doses.remove(dose_a.id), "dose remove");
    check(doses.count(Mode::Transfem) == 1, "dose count");
    DoseEvent no_stamp{"00000000-0000-0000-0000-000000000022",
                       Mode::Transfem, "oral", "E2", 5.0, 1.0, "{}",
                       std::nullopt, 1};
    doses.upsert(no_stamp);
    const auto loaded_no_stamp = doses.get(no_stamp.id);
    check(loaded_no_stamp.has_value() &&
              !loaded_no_stamp->updated_at.has_value(),
          "null updatedAt round-trip");
    (void)doses.remove(no_stamp.id);

    VaultLabResultRepository labs(vault);
    LabResult lab_b{"00000000-0000-0000-0000-000000000030", Mode::Transfem,
                    30.0, 100.0, "pg/ml", "immuno", 5, 1};
    LabResult lab_a{"00000000-0000-0000-0000-000000000031", Mode::Transfem,
                    10.0, 90.0, "pg/ml", "immuno", 4, 1};
    labs.upsert(lab_b);
    labs.upsert(lab_a);
    const auto lab_list = labs.list(Mode::Transfem);
    check(lab_list.size() == 2 && lab_list[0].id == lab_a.id &&
              lab_list[1].id == lab_b.id,
          "labs sort by time then id");
    check(labs.list(Mode::Transmasc).empty(), "lab mode isolation");
    const auto loaded_lab = labs.get(lab_a.id);
    check(loaded_lab.has_value() && loaded_lab->value == 90.0, "lab get");
    check(labs.remove(lab_a.id), "lab remove");
    (void)labs.remove(lab_b.id);

    VaultJsonRecordRepository templates(vault, JsonRecordKind::DoseTemplate);
    JsonRecord template_rec{
        "00000000-0000-0000-0000-000000000040", Mode::Transmasc,
        R"({"id":"00000000-0000-0000-0000-000000000040","mode":"transmasc","name":"weekly","route":"injection","ester":"EN","doseMG":5.0,"extras":{},"createdAt":1})",
        std::nullopt};
    templates.upsert(template_rec);
    const auto template_list = templates.list(Mode::Transmasc);
    check(template_list.size() == 1 && template_list[0].id == template_rec.id,
          "template list");
    const auto loaded_template = templates.get(template_rec.id);
    check(loaded_template.has_value() && loaded_template->id == template_rec.id,
          "template id");
    check(loaded_template.has_value() &&
              nlohmann::json::parse(loaded_template->payload_json) ==
                  nlohmann::json::parse(template_rec.payload_json),
          "template payload");
    VaultJsonRecordRepository quick(vault, JsonRecordKind::QuickDose);
    check(quick.list(Mode::Transmasc).empty(), "kind isolation");
    check(templates.remove(template_rec.id), "template remove");

    JsonRecord quick_rec{
        "00000000-0000-0000-0000-000000000041", Mode::Transfem,
        R"({"id":"00000000-0000-0000-0000-000000000041","route":"oral","ester":"E2","value":1.0,"createdAt":1})",
        std::nullopt};
    quick.upsert(quick_rec);
    check(quick.list(Mode::Transfem).size() == 1, "quick list");
    const auto loaded_quick = quick.get(quick_rec.id);
    check(loaded_quick.has_value() && loaded_quick->id == quick_rec.id,
          "quick id");
    check(quick.remove(quick_rec.id), "quick remove");

    VaultJsonRecordRepository supplies(vault, JsonRecordKind::Supply);
    JsonRecord supply_rec{
        "00000000-0000-0000-0000-000000000042", Mode::Transfem,
        R"({"id":"00000000-0000-0000-0000-000000000042","route":"injection","ester":"EV","pack":{"kind":"vial","ml":10.0,"mgPerMl":40.0},"count":1.0,"startedAt":1})",
        std::nullopt};
    supplies.upsert(supply_rec);
    check(supplies.list(Mode::Transfem).size() == 1, "supply list");
    check(supplies.remove(supply_rec.id), "supply remove");

    VaultJsonRecordRepository plan(vault, JsonRecordKind::PlanItem);
    JsonRecord plan_rec{
        "00000000-0000-0000-0000-000000000043", Mode::Transfem,
        R"({"id":"00000000-0000-0000-0000-000000000043","route":"injection","ester":"EV","doseMG":2.0,"extras":{},"everyDays":7,"timesPerDay":1})",
        std::nullopt};
    plan.upsert(plan_rec);
    check(plan.list(Mode::Transfem).size() == 1, "plan list");
    check(plan.remove(plan_rec.id), "plan remove");

    JsonRecord invalid_quick{
        "00000000-0000-0000-0000-000000000044", Mode::Transfem,
        R"({"id":"00000000-0000-0000-0000-000000000044","route":"teleport","ester":"E2","value":1.0,"createdAt":1})",
        std::nullopt};
    bool invalid_route_threw = false;
    try {
        quick.upsert(invalid_quick);
    } catch (const VaultCodecError&) {
        invalid_route_threw = true;
    }
    check(invalid_route_threw, "invalid JSON record route rejected");

    PGresult* blobs = PQexec(admin, "SELECT blob FROM vault_records");
    check(blobs != nullptr && PQresultStatus(blobs) == PGRES_TUPLES_OK,
          "read raw blobs");
    bool plaintext_found = false;
    if (blobs != nullptr && PQresultStatus(blobs) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(blobs); ++i) {
            const char* value = PQgetvalue(blobs, i, 0);
            if (std::strstr(value, "dark") != nullptr ||
                std::strstr(value, "60.0") != nullptr) {
                plaintext_found = true;
            }
        }
    }
    if (blobs != nullptr) {
        PQclear(blobs);
    }
    check(!plaintext_found, "raw vault blobs contain no plaintext");

    vault.lock();
    bool locked_threw = false;
    try {
        (void)settings.get();
    } catch (const VaultError& error) {
        locked_threw = error.code() == OYAMA_VAULT_ERR_LOCKED;
    }
    check(locked_threw, "locked repository read throws LOCKED");

    PQfinish(admin);

    if (failures != 0) {
        std::cerr << "vault_repository_test: " << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "vault_repository_test: ok\n";
    return 0;
}
