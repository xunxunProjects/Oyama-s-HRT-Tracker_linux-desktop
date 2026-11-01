#include "legacy_migration_worker.h"

#include <QCoreApplication>
#include <QString>

#include <oyama/storage/app_config.h>
#include <oyama/storage/connection.h>
#include <oyama/storage/legacy_promotion.h>
#include <oyama/storage/local_cluster.h>
#include <oyama/storage/vault_store.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

using oyama::storage::AppConfig;
using oyama::storage::Backend;
using oyama::storage::Connection;
using oyama::storage::LocalCluster;
using oyama::storage::VaultState;
using oyama::storage::VaultStore;
using oyama::storage::find_plaintext_dirs;
using oyama::storage::load_app_config;
using oyama::storage::make_legacy_paths;

int failures = 0;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "legacy_migration_worker_test: " << message << '\n';
        ++failures;
        return false;
    }
    return true;
}

#define REQUIRE(condition, message)                       \
    do {                                                  \
        if (!expect((condition), (message))) return 1;    \
    } while (0)

struct Signals {
    bool finished{false};
    bool failed{false};
    QString message;
    QString plaintext_path;
    int progress_count{0};
};

bool use_config(const char* variable) {
    const char* path = std::getenv(variable);
    if (path == nullptr || *path == '\0') {
        std::cerr << "legacy_migration_worker_test: " << variable
                  << " is not set\n";
        return false;
    }
    return ::setenv("OYAMA_CONFIG_FILE", path, 1) == 0;
}

oyama::fs::path target_dir(const AppConfig& config) {
    const oyama::fs::path data_dir(config.data_dir);
    return data_dir.parent_path() /
           (data_dir.filename().string() + "-migrate");
}

std::size_t plaintext_count(const AppConfig& config) {
    return find_plaintext_dirs(
               make_legacy_paths(config.data_dir, config.socket_dir))
        .size();
}

int run_success_case() {
    REQUIRE(use_config("OYAMA_TEST_MIGRATION_SUCCESS_CONFIG"),
            "success config is available");
    const auto config = load_app_config();
    REQUIRE(config.has_value() && config->backend == Backend::Managed,
            "success fixture is a managed config");

    Signals events;
    LegacyMigrationWorker worker;
    QObject::connect(&worker, &LegacyMigrationWorker::progress, &worker,
                     [&events](const QString&, int done, int total) {
                         if (done >= 0 && total >= 0) {
                             ++events.progress_count;
                         }
                     });
    QObject::connect(&worker, &LegacyMigrationWorker::finished, &worker,
                     [&events]() { events.finished = true; });
    QObject::connect(
        &worker, &LegacyMigrationWorker::failed, &worker,
        [&events](const QString& message, const QString& plaintext_path) {
            events.failed = true;
            events.message = message;
            events.plaintext_path = plaintext_path;
        });

    const QString phrase = QStringLiteral("correct horse battery staple");
    worker.run(QString::fromStdString(config->data_dir),
               QString::fromStdString(config->socket_dir), phrase);

    REQUIRE(events.finished, "finished must fire on success");
    REQUIRE(!events.failed, "failed must not fire on success");
    REQUIRE(events.progress_count > 0, "progress must be reported");
    REQUIRE(oyama::fs::exists(target_dir(*config)) == false,
            "the promotion must consume the target directory");
    REQUIRE(plaintext_count(*config) == 1,
            "promotion must leave exactly one plaintext sibling");

    LocalCluster cluster(config->cluster_config());
    cluster.ensure_running();
    VaultStore vault(config->connection_config());
    vault.open();
    REQUIRE(vault.state() == VaultState::Locked,
            "the promoted vault must be locked");
    vault.unlock(phrase.toStdString());
    REQUIRE(vault.is_unlocked(), "the promoted vault must unlock with the phrase");
    REQUIRE(vault.scan("dose_event").size() == 2,
            "the promoted vault must contain the seeded dose events");
    REQUIRE(!vault.scan("settings").empty(),
            "the promoted vault must contain the settings record");
    vault.lock();
    cluster.stop();
    return failures == 0 ? 0 : 1;
}

int run_failure_case() {
    REQUIRE(use_config("OYAMA_TEST_MIGRATION_FAILURE_CONFIG"),
            "failure config is available");
    const auto config = load_app_config();
    REQUIRE(config.has_value() && config->backend == Backend::Managed,
            "failure fixture is a managed config");

    Signals events;
    LegacyMigrationWorker worker;
    QObject::connect(&worker, &LegacyMigrationWorker::progress, &worker,
                     [&events](const QString&, int, int) {
                         ++events.progress_count;
                     });
    QObject::connect(&worker, &LegacyMigrationWorker::finished, &worker,
                     [&events]() { events.finished = true; });
    QObject::connect(
        &worker, &LegacyMigrationWorker::failed, &worker,
        [&events](const QString& message, const QString& plaintext_path) {
            events.failed = true;
            events.message = message;
            events.plaintext_path = plaintext_path;
        });

    const QString phrase = QStringLiteral("correct horse battery staple");
    worker.run(QString::fromStdString(config->data_dir),
               QString::fromStdString(config->socket_dir), phrase);

    REQUIRE(events.failed, "failed must fire for a rejected legacy row");
    REQUIRE(!events.finished, "finished must not fire on failure");
    REQUIRE(!events.message.isEmpty(), "failure must report a message");
    REQUIRE(events.message.contains("dose_event") ||
                events.message.contains("route") ||
                events.message.contains("field"),
            "failure must identify the rejected record");
    REQUIRE(events.plaintext_path.isEmpty(),
            "no plaintext path exists before promotion");
    REQUIRE(oyama::fs::exists(config->data_dir),
            "the old data directory must survive a failed migration");
    REQUIRE(plaintext_count(*config) == 0,
            "a failed migration must not promote anything");
    REQUIRE(oyama::fs::exists(target_dir(*config)),
            "the failed target is kept for resume");
    REQUIRE(events.progress_count > 0,
            "progress must be reported before the failure");

    LocalCluster cluster(config->cluster_config());
    cluster.ensure_running();
    Connection connection(config->connection_config());
    const auto rows = connection.exec("SELECT count(*) FROM dose_events");
    REQUIRE(rows.integer(0, 0) == 3,
            "the old legacy rows must remain untouched");
    cluster.stop();
    return failures == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);

    const char* scenario = std::getenv("OYAMA_WORKER_SCENARIO");
    const std::string selected = scenario != nullptr ? scenario : "success";
    if (selected == "failure") {
        const int status = run_failure_case();
        if (status == 0) {
            std::cout << "legacy_migration_worker_test: ok\n";
        }
        return status;
    }
    const int status = run_success_case();
    if (status == 0) {
        std::cout << "legacy_migration_worker_test: ok\n";
    }
    return status;
}
