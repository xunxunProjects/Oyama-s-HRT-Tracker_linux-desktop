#include "legacy_migration_worker.h"

#include <sodium.h>

#include <climits>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

#include <oyama/storage/app_config.h>
#include <oyama/storage/database.h>
#include <oyama/storage/legacy_migration.h>
#include <oyama/storage/legacy_promotion.h>
#include <oyama/storage/local_cluster.h>
#include <oyama/storage/vault_store.h>

namespace {

using oyama::storage::AppConfig;
using oyama::storage::Backend;
using oyama::storage::Connection;
using oyama::storage::ConnectionConfig;
using oyama::storage::Database;
using oyama::storage::LegacyDetection;
using oyama::storage::LegacyImportResult;
using oyama::storage::LegacyMigrator;
using oyama::storage::LegacyPaths;
using oyama::storage::LocalCluster;
using oyama::storage::LocalClusterConfig;
using oyama::storage::PromotionState;
using oyama::storage::VaultState;
using oyama::storage::VaultStore;

oyama::fs::path migrations_dir() {
    if (const char* value = std::getenv("OYAMA_MIGRATIONS_DIR");
        value != nullptr && *value != '\0') {
        return value;
    }
#ifdef OYAMA_DEFAULT_MIGRATIONS_DIR
    if (oyama::fs::is_directory(OYAMA_DEFAULT_MIGRATIONS_DIR)) {
        return OYAMA_DEFAULT_MIGRATIONS_DIR;
    }
#endif
#ifdef OYAMA_INSTALL_MIGRATIONS_DIR
    if (oyama::fs::is_directory(OYAMA_INSTALL_MIGRATIONS_DIR)) {
        return OYAMA_INSTALL_MIGRATIONS_DIR;
    }
#endif
    return "storage/migrations";
}

ConnectionConfig cluster_connection(const LocalCluster& cluster,
                                    const AppConfig& config) {
    ConnectionConfig connection = cluster.connection_config();
    connection.dbname = config.dbname;
    connection.user = config.user;
    return connection;
}

void ensure_database_exists(const ConnectionConfig& config) {
    ConnectionConfig admin_config = config;
    admin_config.dbname = "postgres";
    Connection admin(admin_config);
    const auto exists = admin.exec_params(
        "SELECT 1 FROM pg_database WHERE datname = $1", {config.dbname});
    if (exists.rows() == 0) {
        (void)admin.exec("CREATE DATABASE " +
                         admin.quote_identifier(config.dbname));
    }
}

int clamp_count(std::size_t value) {
    return value > static_cast<std::size_t>(INT_MAX)
               ? INT_MAX
               : static_cast<int>(value);
}

QString failure_plaintext_path(const LegacyPaths& paths) {
    try {
        const auto plaintext =
            oyama::storage::find_plaintext_dirs(paths);
        if (plaintext.size() == 1) {
            return QString::fromStdString(plaintext.front().string());
        }
    } catch (const std::exception&) {
    }
    return {};
}

}  // namespace

LegacyMigrationWorker::LegacyMigrationWorker(QObject* parent)
    : QObject(parent) {}

void LegacyMigrationWorker::run(const QString& data_dir,
                                const QString& socket_dir,
                                const QString& phrase) {
    std::string secret = phrase.toStdString();
    LegacyPaths paths;
    bool paths_ready = false;

    try {
        const auto config = oyama::storage::load_app_config();
        if (!config.has_value()) {
            throw std::runtime_error(
                "No database configuration found; run `oyama-hrt-cli db "
                "setup --managed`.");
        }
        if (config->backend != Backend::Managed) {
            throw std::runtime_error(
                "Automatic migration only supports an application-managed "
                "cluster. For an existing server, export the old data with "
                "the previous binary and import it into a new vault.");
        }

        paths = oyama::storage::make_legacy_paths(
            oyama::fs::path(data_dir.toStdString()),
            oyama::fs::path(socket_dir.toStdString()));
        paths_ready = true;
        const PromotionState state =
            oyama::storage::inspect_promotion(paths);
        if (state == PromotionState::ManualReview) {
            throw std::runtime_error(
                "The migration directory state requires manual inspection "
                "at " +
                paths.data_dir.parent_path().string());
        }
        if (state == PromotionState::Promoted) {
            throw std::runtime_error(
                "This cluster has already been migrated; restart the "
                "application.");
        }

        LocalClusterConfig old_config;
        old_config.data_dir = paths.data_dir;
        old_config.socket_dir = paths.socket_dir;
        old_config.server_bin_dir = config->server_bin_dir;
        old_config.port = config->port;
        LocalCluster old_cluster(old_config);

        LocalClusterConfig target_config = old_config;
        target_config.data_dir = paths.target_data_dir;
        target_config.socket_dir = paths.target_socket_dir;
        target_config.log_file =
            paths.target_data_dir.parent_path() /
            (paths.target_data_dir.filename().string() + ".log");
        LocalCluster target_cluster(target_config);

        if (state == PromotionState::TargetPending &&
            !oyama::fs::exists(paths.data_dir)) {
            if (!target_cluster.is_initialized()) {
                throw std::runtime_error(
                    "Migration target is not an initialized PostgreSQL "
                    "cluster.");
            }
            target_cluster.start();
            try {
                Connection target_connection(
                    cluster_connection(target_cluster, *config));
                const auto meta = target_connection.exec(
                    "SELECT to_regclass('vault_meta') IS NOT NULL AND "
                    "EXISTS (SELECT 1 FROM vault_meta)");
                if (!meta.boolean(0, 0)) {
                    throw std::runtime_error(
                        "Migration target has no vault metadata; refusing "
                        "promotion.");
                }
            } catch (...) {
                try {
                    target_cluster.stop();
                } catch (const std::exception&) {
                }
                throw;
            }
            oyama::storage::promote_after_verification(
                old_cluster, target_cluster, paths);
            emit progress(QStringLiteral("promote"), 1, 1);
            sodium_memzero(secret.data(), secret.size());
            emit finished();
            return;
        }

        const bool old_was_running =
            old_cluster.is_initialized() && old_cluster.is_running();

        try {
            emit progress(QStringLiteral("prepare"), 0, 1);
            old_cluster.start();
            Connection source(cluster_connection(old_cluster, *config));
            (void)source.exec("SET default_transaction_read_only = on");
            const LegacyDetection detection =
                oyama::storage::detect_legacy(source);
            if (detection.has_meta) {
                throw std::runtime_error(
                    "The old cluster already contains a vault; mixed legacy "
                    "and vault data requires the JSON export/restore path.");
            }
            if (!detection.has_legacy) {
                throw std::runtime_error(
                    "No legacy plaintext data was found in " +
                    paths.data_dir.string());
            }

            target_cluster.initialize();
            target_cluster.start();
            const ConnectionConfig target_connection =
                cluster_connection(target_cluster, *config);
            ensure_database_exists(target_connection);
            {
                Database target_database(target_connection,
                                         migrations_dir());
                (void)target_database.migrate();
                emit progress(QStringLiteral("schema"),
                              target_database.current_version(),
                              target_database.current_version());
            }

            LegacyImportResult result;
            {
                VaultStore vault(target_connection);
                vault.open();
                if (vault.state() == VaultState::Uninitialized) {
                    vault.setup(secret);
                    vault.unlock(secret);
                } else if (vault.state() == VaultState::Locked) {
                    vault.unlock(secret);
                }
                if (!vault.is_unlocked()) {
                    throw std::runtime_error(
                        "The migration target vault could not be unlocked.");
                }

                LegacyMigrator migrator(
                    source, vault,
                    [this](oyama::string_view kind, std::size_t done,
                           std::size_t total) {
                        emit progress(
                            QString::fromUtf8(
                                kind.data(),
                                static_cast<int>(kind.size())),
                            clamp_count(done), clamp_count(total));
                    });
                result = migrator.import_all();
                if (result.source_sha256 != result.target_sha256) {
                    throw std::runtime_error(
                        "Migration verification failed: source and target "
                        "digests differ.");
                }
            }
            emit progress(QStringLiteral("verify"), 1, 1);
        } catch (...) {
            try {
                target_cluster.stop();
            } catch (const std::exception&) {
            }
            if (!old_was_running) {
                try {
                    old_cluster.stop();
                } catch (const std::exception&) {
                }
            }
            throw;
        }

        oyama::storage::promote_after_verification(
            old_cluster, target_cluster, paths);
        emit progress(QStringLiteral("promote"), 1, 1);
        sodium_memzero(secret.data(), secret.size());
        emit finished();
    } catch (const std::exception& error) {
        sodium_memzero(secret.data(), secret.size());
        emit failed(QString::fromUtf8(error.what()),
                    paths_ready ? failure_plaintext_path(paths) : QString());
    } catch (...) {
        sodium_memzero(secret.data(), secret.size());
        emit failed(QStringLiteral("Legacy migration failed."),
                    paths_ready ? failure_plaintext_path(paths) : QString());
    }
}
