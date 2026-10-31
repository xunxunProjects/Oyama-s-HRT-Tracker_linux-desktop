#include "app_controller.h"
#include "legacy_migration_worker.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QEvent>
#include <QMetaObject>
#include <QThread>
#include <QTimeZone>
#include <QTimer>

#include <nlohmann/json.hpp>
#include <sodium.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <oyama/pk.h>
#include <oyama/storage/legacy_migration.h>
#include <oyama/storage/legacy_promotion.h>
#include <oyama/storage/passphrase.h>
#include <oyama/storage/vault_dose_event_repository.h>
#include <oyama/storage/vault_settings_repository.h>

namespace {

using oyama::storage::AppConfig;
using oyama::storage::Backend;
using oyama::storage::Database;
using oyama::storage::Connection;
using oyama::storage::ConnectionConfig;
using oyama::storage::DoseEvent;
using oyama::storage::LegacyPaths;
using oyama::storage::LocalCluster;
using oyama::storage::LocalClusterConfig;
using oyama::storage::Mode;
using oyama::storage::PromotionState;
using oyama::storage::Passphrase;
using oyama::storage::VaultDoseEventRepository;
using oyama::storage::VaultSettingsRepository;
using oyama::storage::VaultState;
using oyama::storage::VaultStore;

constexpr int kIdleLockMilliseconds = 15 * 60 * 1000;
constexpr const char* kLegacyMigrationSetupStatus =
    "Set up a recovery phrase to migrate";
constexpr const char* kPlaintextWarning =
    "Deleting the old database directory removes the files this application "
    "knows about. It does not guarantee erasure on SSDs, copy-on-write "
    "filesystems, snapshots, RAID, or backups. Full-disk encryption and "
    "destruction of the old medium are the only reliable remedies.";

std::filesystem::path migrations_dir() {
    if (const char* value = std::getenv("OYAMA_MIGRATIONS_DIR");
        value != nullptr && *value != '\0') {
        return value;
    }
#ifdef OYAMA_DEFAULT_MIGRATIONS_DIR
    if (std::filesystem::is_directory(OYAMA_DEFAULT_MIGRATIONS_DIR)) {
        return OYAMA_DEFAULT_MIGRATIONS_DIR;
    }
#endif
#ifdef OYAMA_INSTALL_MIGRATIONS_DIR
    if (std::filesystem::is_directory(OYAMA_INSTALL_MIGRATIONS_DIR)) {
        return OYAMA_INSTALL_MIGRATIONS_DIR;
    }
#endif
    return "storage/migrations";
}

std::optional<oyama_pk_route> route_from(const QString& value) {
    const QString route = value.trimmed();
    if (route.compare("oral", Qt::CaseInsensitive) == 0) {
        return OYAMA_PK_ROUTE_ORAL;
    }
    if (route.compare("sublingual", Qt::CaseInsensitive) == 0) {
        return OYAMA_PK_ROUTE_SUBLINGUAL;
    }
    if (route.compare("injection", Qt::CaseInsensitive) == 0) {
        return OYAMA_PK_ROUTE_INJECTION;
    }
    if (route.compare("gel", Qt::CaseInsensitive) == 0) {
        return OYAMA_PK_ROUTE_GEL;
    }
    if (route.compare("patchApply", Qt::CaseInsensitive) == 0 ||
        route.compare("patch", Qt::CaseInsensitive) == 0) {
        return OYAMA_PK_ROUTE_PATCH_APPLY;
    }
    if (route.compare("patchRemove", Qt::CaseInsensitive) == 0) {
        return OYAMA_PK_ROUTE_PATCH_REMOVE;
    }
    return std::nullopt;
}

std::optional<oyama_pk_ester> ester_from(const QString& value) {
    const QString ester = value.trimmed().toUpper();
    if (ester == "E2") return OYAMA_PK_ESTER_E2;
    if (ester == "EB") return OYAMA_PK_ESTER_EB;
    if (ester == "EV") return OYAMA_PK_ESTER_EV;
    if (ester == "EC") return OYAMA_PK_ESTER_EC;
    if (ester == "EN") return OYAMA_PK_ESTER_EN;
    if (ester == "EU") return OYAMA_PK_ESTER_EU;
    if (ester == "CPA") return OYAMA_PK_ESTER_CPA;
    if (ester == "SPIRO") return OYAMA_PK_ESTER_SPIRO;
    if (ester == "BICA") return OYAMA_PK_ESTER_BICA;
    if (ester == "MPA") return OYAMA_PK_ESTER_MPA;
    if (ester == "LEUP") return OYAMA_PK_ESTER_LEUP;
    if (ester == "TRIP") return OYAMA_PK_ESTER_TRIP;
    if (ester == "RLX") return OYAMA_PK_ESTER_RLX;
    if (ester == "T") return OYAMA_PK_ESTER_T;
    if (ester == "TC") return OYAMA_PK_ESTER_TC;
    if (ester == "TE") return OYAMA_PK_ESTER_TE;
    if (ester == "TU") return OYAMA_PK_ESTER_TU;
    return std::nullopt;
}

oyama_pk_extras extras_from_json(const std::string& text) {
    oyama_pk_extras extras{};
    const auto parsed = nlohmann::json::parse(text, nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) {
        return extras;
    }
    const auto number = [&](const char* key) -> std::optional<double> {
        const auto it = parsed.find(key);
        if (it == parsed.end() || !it->is_number()) {
            return std::nullopt;
        }
        return it->get<double>();
    };
    if (const auto value = number("sublingualTheta")) {
        extras.has_sublingual_theta = 1;
        extras.sublingual_theta = *value;
    }
    if (const auto value = number("sublingualTier")) {
        extras.has_sublingual_tier = 1;
        extras.sublingual_tier = static_cast<int>(std::llround(*value));
    }
    if (const auto value = number("gelSite")) {
        extras.gel_site = static_cast<int>(std::llround(*value));
    }
    if (const auto value = number("releaseRateUGPerDay")) {
        extras.has_release_rate_ug_per_day = 1;
        extras.release_rate_ug_per_day = *value;
    } else if (const auto value = number("releaseRate")) {
        extras.has_release_rate_ug_per_day = 1;
        extras.release_rate_ug_per_day = *value;
    }
    if (const auto value = number("patchWearH")) {
        extras.has_patch_wear_h = 1;
        extras.patch_wear_h = *value;
    }
    return extras;
}

std::string extras_to_json(const QVariantMap& values) {
    nlohmann::json out = nlohmann::json::object();
    const auto put_number = [&](const char* key) {
        const auto it = values.find(QString::fromLatin1(key));
        if (it == values.end()) {
            return;
        }
        bool ok = false;
        const double value = it->toDouble(&ok);
        if (ok && std::isfinite(value)) {
            out[key] = value;
        }
    };
    put_number("sublingualTheta");
    put_number("sublingualTier");
    put_number("gelSite");
    put_number("releaseRateUGPerDay");
    put_number("patchWearH");
    return out.dump();
}

oyama_pk_params pk_params_from_json(const QString& text) {
    oyama_pk_params params{};
    oyama_pk_params_defaults(&params);
    const auto parsed =
        nlohmann::json::parse(text.toStdString(), nullptr, false);
    if (parsed.is_discarded() || !parsed.is_object()) {
        return params;
    }
    const auto set = [&](const char* key, double& field) {
        const auto it = parsed.find(key);
        if (it != parsed.end() && it->is_number()) {
            field = it->get<double>();
        }
    };
    set("e2_kClear", params.e2_kClear);
    set("e2_kClearInj", params.e2_kClearInj);
    set("e2_ff_EB", params.e2_ff_EB);
    set("e2_ff_EV", params.e2_ff_EV);
    set("e2_ff_EC", params.e2_ff_EC);
    set("e2_ff_EN", params.e2_ff_EN);
    set("e2_ff_EU", params.e2_ff_EU);
    set("e2_oral_bio", params.e2_oral_bio);
    set("e2_sl_quick", params.e2_sl_quick);
    set("e2_sl_casual", params.e2_sl_casual);
    set("e2_sl_standard", params.e2_sl_standard);
    set("e2_sl_strict", params.e2_sl_strict);
    set("e2_gel_arm", params.e2_gel_arm);
    set("e2_gel_thigh", params.e2_gel_thigh);
    set("e2_gel_scrotal", params.e2_gel_scrotal);
    set("t_kClear", params.t_kClear);
    set("t_kClearInj", params.t_kClearInj);
    set("t_ff_TC", params.t_ff_TC);
    set("t_ff_TE", params.t_ff_TE);
    set("t_ff_TU", params.t_ff_TU);
    set("t_gel_arm", params.t_gel_arm);
    set("t_gel_thigh", params.t_gel_thigh);
    set("t_gel_scrotal", params.t_gel_scrotal);
    return params;
}

std::int64_t now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

LocalClusterConfig cluster_config_for(const AppConfig& config,
                                      const std::filesystem::path& data_dir,
                                      const std::filesystem::path& socket_dir) {
    LocalClusterConfig cluster = config.cluster_config();
    cluster.data_dir = data_dir;
    cluster.socket_dir = socket_dir;
    cluster.log_file =
        data_dir.parent_path() / (data_dir.filename().string() + ".log");
    return cluster;
}

void verify_target_vault_meta(const LocalCluster& target,
                              const AppConfig& config) {
    ConnectionConfig connection = target.connection_config();
    connection.dbname = config.dbname;
    connection.user = config.user;
    Connection database(connection);
    const auto meta = database.exec(
        "SELECT to_regclass('vault_meta') IS NOT NULL AND "
        "EXISTS (SELECT 1 FROM vault_meta)");
    if (!meta.boolean(0, 0)) {
        throw std::runtime_error(
            "Migration target has no vault metadata; refusing promotion.");
    }
}

void finish_interrupted_promotion(const AppConfig& config,
                                  const LegacyPaths& paths) {
    LocalCluster old_cluster(
        cluster_config_for(config, paths.data_dir, paths.socket_dir));
    LocalCluster target_cluster(cluster_config_for(
        config, paths.target_data_dir, paths.target_socket_dir));
    if (!target_cluster.is_initialized()) {
        throw std::runtime_error(
            "Migration target is not an initialized PostgreSQL cluster.");
    }
    target_cluster.start();
    try {
        verify_target_vault_meta(target_cluster, config);
    } catch (...) {
        try {
            target_cluster.stop();
        } catch (const std::exception&) {
        }
        throw;
    }
    oyama::storage::promote_after_verification(old_cluster, target_cluster,
                                               paths);
}

void discard_failed_target(const AppConfig& config,
                           const LegacyPaths& paths) {
    LocalCluster target_cluster(cluster_config_for(
        config, paths.target_data_dir, paths.target_socket_dir));
    if (target_cluster.is_initialized() && target_cluster.is_running()) {
        target_cluster.stop();
    }
    std::error_code error;
    std::filesystem::remove_all(paths.target_data_dir, error);
    if (error) {
        throw std::runtime_error(
            "Could not remove the failed migration target: " +
            error.message());
    }
    error.clear();
    std::filesystem::remove_all(paths.target_socket_dir, error);
    if (error) {
        throw std::runtime_error(
            "Could not remove the failed migration socket directory: " +
            error.message());
    }
}

void prepare_managed_directories(const AppConfig& config) {
    const LegacyPaths paths = oyama::storage::make_legacy_paths(
        config.data_dir, config.socket_dir);
    const PromotionState state = oyama::storage::inspect_promotion(paths);
    if (state == PromotionState::ManualReview) {
        throw std::runtime_error(
            "The migration directory state requires manual inspection at " +
            paths.data_dir.parent_path().string());
    }
    if (state != PromotionState::TargetPending) {
        return;
    }
    if (!std::filesystem::exists(paths.data_dir)) {
        finish_interrupted_promotion(config, paths);
        return;
    }
    discard_failed_target(config, paths);
}

}  // namespace

AppController::AppController(QObject* parent) : QObject(parent) {
    idle_timer_ = new QTimer(this);
    idle_timer_->setObjectName("vaultIdleTimer");
    idle_timer_->setSingleShot(true);
    idle_timer_->setInterval(kIdleLockMilliseconds);
    connect(idle_timer_, &QTimer::timeout, this, &AppController::lockVault);
    if (auto* application = QCoreApplication::instance();
        application != nullptr) {
        application->installEventFilter(this);
        connect(application, &QCoreApplication::aboutToQuit, this,
                &AppController::lockVault, Qt::DirectConnection);
    }
    reload();
}

AppController::~AppController() {
    if (auto* application = QCoreApplication::instance();
        application != nullptr) {
        application->removeEventFilter(this);
    }
    // A migration owns the old and target clusters.  Wait for it to finish so
    // the process never exits with a half-written import, then zero the phrase.
    join_migration_thread();
    clear_pending_migration_phrase();
}

bool AppController::eventFilter(QObject* watched, QEvent* event) {
    if (vault_state_ == "unlocked" && idle_timer_ != nullptr) {
        switch (event->type()) {
            case QEvent::KeyPress:
            case QEvent::MouseButtonPress:
            case QEvent::MouseMove:
            case QEvent::Wheel:
            case QEvent::TouchBegin:
                idle_timer_->start(kIdleLockMilliseconds);
                break;
            default:
                break;
        }
    }
    return QObject::eventFilter(watched, event);
}

void AppController::setMode(const QString& mode) {
    if (mode != "transfem" && mode != "transmasc") {
        return;
    }
    if (mode_ == mode) {
        return;
    }
    mode_ = mode;
    emit modeChanged();
    if (vault_state_ == "unlocked") {
        try {
            load_data();
        } catch (const std::exception& error) {
            fail(QString("Database error: ") + QString::fromUtf8(error.what()));
        }
    } else {
        reload();
    }
}

void AppController::fail(const QString& message) {
    status_ = message;
    doses_.clear();
    curve_.clear();
    current_e2_ = 0.0;
    current_cpa_ = 0.0;
    current_t_ = 0.0;
    emit changed();
}

void AppController::set_vault_state(const QString& state) {
    if (vault_state_ == state) {
        return;
    }
    vault_state_ = state;
    emit changed();
}

void AppController::set_vault_busy(bool busy) {
    if (vault_busy_ == busy) {
        return;
    }
    vault_busy_ = busy;
    emit changed();
}

void AppController::clear_setup() {
    if (!setup_phrase_.isEmpty()) {
        setup_phrase_.fill(QChar(0));
    }
    setup_phrase_.clear();
    setup_challenge_.clear();
}

void AppController::clear_models() {
    doses_.clear();
    curve_.clear();
    current_e2_ = 0.0;
    current_cpa_ = 0.0;
    current_t_ = 0.0;
    settings_ = oyama::storage::Settings{};
}

void AppController::start_idle_timer() {
    if (idle_timer_ != nullptr && vault_state_ == "unlocked") {
        idle_timer_->start(kIdleLockMilliseconds);
    }
}

QVariantList AppController::setupChallenge() const {
    QVariantList challenge;
    challenge.reserve(static_cast<qsizetype>(setup_challenge_.size()));
    for (const auto position : setup_challenge_) {
        challenge.append(static_cast<int>(position));
    }
    return challenge;
}

void AppController::reload() {
    if (idle_timer_ != nullptr) {
        idle_timer_->stop();
    }
    join_migration_thread();
    clear_pending_migration_phrase();
    clear_setup();
    clear_models();
    vault_.reset();
    database_.reset();
    cluster_.reset();
    legacy_pending_ = false;
    set_vault_state("unavailable");

    try {
        const auto config = oyama::storage::load_app_config();
        if (!config.has_value()) {
            backend_ = "none";
            fail("No database configured. Run `oyama-hrt-cli db setup`.");
            return;
        }
        app_config_ = *config;
        backend_ = app_config_.backend == Backend::Managed ? "managed"
                                                           : "existing";
        if (app_config_.backend == Backend::Managed) {
            prepare_managed_directories(app_config_);
            cluster_ = std::make_unique<LocalCluster>(
                app_config_.cluster_config());
            cluster_->ensure_running();
        }

        database_ = std::make_unique<Database>(
            app_config_.connection_config(), migrations_dir());

        // Detect legacy data before 0003.  A legacy database is never
        // migrated in place: managed clusters go through the dedicated
        // migration command, and external databases use the JSON path.
        const auto detection =
            oyama::storage::detect_legacy(database_->connection());
        if (detection.has_legacy) {
            if (detection.has_meta) {
                fail("Vault and legacy plaintext data are both present. "
                     "Export the legacy rows with the old binary or restore "
                     "a pre-migration backup before continuing.");
                return;
            }
            if (app_config_.backend == Backend::Managed) {
                legacy_pending_ = true;
                set_vault_state("uninitialized");
                status_ = QString::fromUtf8(kLegacyMigrationSetupStatus);
                emit changed();
                return;
            }
            fail("Legacy plaintext data requires the JSON export/import "
                 "path; export it with the old binary, then run "
                 "`oyama-hrt-cli backup import --plaintext`.");
            return;
        }

        (void)database_->migrate();
        vault_ = std::make_unique<VaultStore>(app_config_.connection_config());
        vault_->open();
        switch (vault_->state()) {
        case VaultState::Uninitialized:
            set_vault_state("uninitialized");
            status_ = "Vault is not initialized. Create a paper recovery "
                      "phrase to protect your data.";
            emit changed();
            break;
        case VaultState::Locked:
            set_vault_state("locked");
            status_ = "Vault is locked.";
            emit changed();
            break;
        case VaultState::Unlocked:
            set_vault_state("unlocked");
            load_data();
            break;
        }
    } catch (const std::exception& error) {
        fail(QString("Database error: ") + QString::fromUtf8(error.what()));
    }
}

bool AppController::beginVaultSetup() {
    if (vault_state_ != "uninitialized" ||
        (!legacy_pending_ && vault_ == nullptr)) {
        fail(QStringLiteral("Vault setup is not available."));
        return false;
    }
    try {
        auto words = Passphrase::generate(7);
        std::string joined = Passphrase::join(words);
        for (auto& word : words) {
            sodium_memzero(word.data(), word.size());
        }
        const auto positions = Passphrase::challenge(words.size(), 3);
        clear_setup();
        setup_phrase_ = QString::fromStdString(joined);
        sodium_memzero(joined.data(), joined.size());
        for (const auto position : positions) {
            setup_challenge_.push_back(position);
        }
        status_ = legacy_pending_
                      ? "Write down the recovery phrase.  The old plaintext "
                        "data is migrated into a new encrypted vault when you "
                        "confirm."
                      : "Write down the recovery phrase, then answer the "
                        "challenge.";
        emit changed();
        return true;
    } catch (const std::exception& error) {
        clear_setup();
        fail(QString("Could not generate a recovery phrase: ") +
             QString::fromUtf8(error.what()));
        return false;
    }
}

bool AppController::confirmVaultSetup(const QVariantList& answers,
                                      bool acknowledged) {
    if (vault_state_ != "uninitialized" ||
        (!legacy_pending_ && vault_ == nullptr) ||
        setup_phrase_.isEmpty() || setup_challenge_.empty()) {
        fail("Vault setup is not in progress.");
        return false;
    }
    if (!acknowledged) {
        status_ = "Acknowledge that there is no recovery before continuing.";
        emit changed();
        return false;
    }

    std::vector<std::string> normalized_answers;
    normalized_answers.reserve(static_cast<std::size_t>(answers.size()));
    for (const auto& answer : answers) {
        normalized_answers.push_back(answer.toString().toStdString());
    }
    std::string secret = setup_phrase_.toStdString();
    const bool matches = Passphrase::verify_challenge(
        secret, setup_challenge_, normalized_answers);
    if (!matches) {
        sodium_memzero(secret.data(), secret.size());
        status_ = "The challenge words do not match. Try again.";
        emit changed();
        return false;
    }

    if (legacy_pending_) {
        pending_migration_phrase_.assign(secret.data(), secret.size());
        sodium_memzero(secret.data(), secret.size());
        secret.clear();
        clear_setup();
        if (!start_migration_worker()) {
            clear_pending_migration_phrase();
            set_vault_busy(false);
            set_vault_state("uninitialized");
            fail("Could not start the legacy migration.");
            return false;
        }
        return true;
    }

    set_vault_busy(true);
    try {
        vault_->setup(secret);
        vault_->unlock(secret);
        sodium_memzero(secret.data(), secret.size());
        clear_setup();
        set_vault_state("unlocked");
        load_data();
        set_vault_busy(false);
        return true;
    } catch (const std::exception& error) {
        sodium_memzero(secret.data(), secret.size());
        vault_->lock();
        bool initialized = false;
        try {
            initialized = vault_->state() != VaultState::Uninitialized;
        } catch (const std::exception&) {
            initialized = true;
        }
        clear_setup();
        set_vault_state(initialized ? "locked" : "uninitialized");
        set_vault_busy(false);
        fail(QString("Could not create the vault: ") +
             QString::fromUtf8(error.what()));
        return false;
    }
}

void AppController::cancelVaultSetup() {
    if (vault_state_ != "uninitialized") {
        return;
    }
    clear_setup();
    status_ = "Vault setup cancelled.";
    emit changed();
}

bool AppController::unlockVault(const QString& phrase) {
    if (vault_ == nullptr) {
        fail("Database is not connected.");
        return false;
    }
    if (vault_state_ == "uninitialized") {
        fail("Vault is not initialized.");
        return false;
    }
    if (vault_state_ != "locked") {
        fail("Vault is not locked.");
        return false;
    }
    std::string secret = phrase.toStdString();
    std::string normalized = Passphrase::normalize(secret);
    const bool empty = normalized.empty();
    sodium_memzero(normalized.data(), normalized.size());
    if (empty) {
        sodium_memzero(secret.data(), secret.size());
        fail("Enter the recovery phrase.");
        return false;
    }

    set_vault_busy(true);
    try {
        vault_->unlock(secret);
        sodium_memzero(secret.data(), secret.size());
        set_vault_state("unlocked");
        load_data();
        set_vault_busy(false);
        return true;
    } catch (const std::exception&) {
        sodium_memzero(secret.data(), secret.size());
        vault_->lock();
        set_vault_state("locked");
        set_vault_busy(false);
        fail("Could not unlock the vault. Check the recovery phrase and try "
             "again.");
        return false;
    }
}

void AppController::lockVault() {
    if (vault_state_ == "migrating" || migration_thread_ != nullptr) {
        // The worker owns the clusters and the pending phrase.  The destructor
        // joins the thread and zeroes the phrase before the process exits.
        return;
    }
    if (idle_timer_ != nullptr) {
        idle_timer_->stop();
    }
    if (vault_ != nullptr) {
        vault_->lock();
    }
    clear_pending_migration_phrase();
    clear_setup();
    clear_models();
    if (vault_state_ == "unlocked") {
        set_vault_state("locked");
        status_ = "Vault is locked.";
    }
    emit changed();
}

bool AppController::require_unlocked() {
    if (!vault_ || vault_state_ != "unlocked" || !vault_->is_unlocked()) {
        fail("Vault is locked");
        return false;
    }
    return true;
}

bool AppController::migrationRetryAvailable() const {
    return legacy_pending_ && vault_state_ == "migrating" &&
           !vault_busy_ && migration_thread_ == nullptr &&
           !pending_migration_phrase_.empty();
}

void AppController::clear_pending_migration_phrase() {
    if (!pending_migration_phrase_.empty()) {
        sodium_memzero(pending_migration_phrase_.data(),
                       pending_migration_phrase_.size());
        pending_migration_phrase_.clear();
    }
}

bool AppController::start_migration_worker() {
    if (migration_thread_ != nullptr || !legacy_pending_ ||
        pending_migration_phrase_.empty() ||
        app_config_.backend != Backend::Managed ||
        app_config_.data_dir.empty() || app_config_.socket_dir.empty()) {
        return false;
    }
    if (idle_timer_ != nullptr) {
        idle_timer_->stop();
    }

    auto* thread = new QThread(this);
    auto* worker = new LegacyMigrationWorker;
    worker->moveToThread(thread);
    connect(worker, &LegacyMigrationWorker::progress, this,
            &AppController::on_migration_progress, Qt::QueuedConnection);
    connect(worker, &LegacyMigrationWorker::finished, this,
            &AppController::on_migration_finished, Qt::QueuedConnection);
    connect(worker, &LegacyMigrationWorker::failed, this,
            &AppController::on_migration_failed, Qt::QueuedConnection);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    migration_thread_ = thread;

    set_vault_busy(true);
    set_vault_state("migrating");
    status_ = "Migrating legacy data...";
    emit changed();

    thread->start();
    QString data_dir = QString::fromStdString(app_config_.data_dir);
    QString socket_dir = QString::fromStdString(app_config_.socket_dir);
    QString phrase = QString::fromStdString(pending_migration_phrase_);
    QMetaObject::invokeMethod(
        worker,
        [worker, data_dir, socket_dir, phrase]() mutable {
            worker->run(data_dir, socket_dir, phrase);
            phrase.fill(QChar(0));
            phrase.clear();
        },
        Qt::QueuedConnection);
    phrase.fill(QChar(0));
    phrase.clear();
    return true;
}

void AppController::join_migration_thread() {
    QThread* thread = migration_thread_;
    if (thread == nullptr) {
        return;
    }
    migration_thread_ = nullptr;
    thread->quit();
    thread->wait();
    delete thread;
}

bool AppController::retryLegacyMigration() {
    if (!migrationRetryAvailable()) {
        return false;
    }
    if (!start_migration_worker()) {
        fail("Could not restart the legacy migration.");
        return false;
    }
    return true;
}

void AppController::cancelLegacyMigration() {
    if (vault_state_ != "migrating" || vault_busy_ ||
        migration_thread_ != nullptr) {
        return;
    }
    clear_pending_migration_phrase();
    clear_setup();
    legacy_pending_ = true;
    set_vault_state("uninitialized");
    status_ = "Migration cancelled. Set up a recovery phrase to migrate.";
    emit changed();
}

QString AppController::migration_plaintext_path() const {
    try {
        if (app_config_.data_dir.empty() || app_config_.socket_dir.empty()) {
            return {};
        }
        const auto paths = oyama::storage::make_legacy_paths(
            app_config_.data_dir, app_config_.socket_dir);
        const auto plaintext =
            oyama::storage::find_plaintext_dirs(paths);
        if (plaintext.size() == 1) {
            return QString::fromStdString(plaintext.front().string());
        }
    } catch (const std::exception&) {
    }
    return {};
}

void AppController::on_migration_progress(const QString& kind, int done,
                                          int total) {
    if (vault_state_ != "migrating") {
        return;
    }
    if (total > 0) {
        status_ = QStringLiteral("Migrating %1 (%2/%3)...")
                      .arg(kind)
                      .arg(done)
                      .arg(total);
    } else {
        status_ = QStringLiteral("Migrating %1...").arg(kind);
    }
    emit changed();
}

void AppController::on_migration_finished() {
    join_migration_thread();
    if (vault_state_ != "migrating" || !legacy_pending_ ||
        pending_migration_phrase_.empty()) {
        return;
    }

    std::string phrase = pending_migration_phrase_;
    clear_pending_migration_phrase();
    set_vault_busy(false);
    legacy_pending_ = false;
    reload();

    if (vault_state_ == "locked") {
        QString qphrase = QString::fromStdString(phrase);
        sodium_memzero(phrase.data(), phrase.size());
        phrase.clear();
        const bool unlocked = unlockVault(qphrase);
        qphrase.fill(QChar(0));
        qphrase.clear();
        if (unlocked) {
            const QString plaintext = migration_plaintext_path();
            status_ = plaintext.isEmpty()
                          ? QStringLiteral("Legacy data migrated to the new "
                                           "encrypted vault.")
                          : QStringLiteral(
                                "Legacy data migrated. Old plaintext data is "
                                "kept at %1. %2")
                                .arg(plaintext,
                                     QString::fromUtf8(kPlaintextWarning));
        } else {
            status_ = QStringLiteral(
                "The migration finished, but the new vault could not be "
                "unlocked. Use the recovery phrase you wrote down.");
        }
    } else {
        sodium_memzero(phrase.data(), phrase.size());
        phrase.clear();
        status_ = QStringLiteral(
                      "The migration finished, but the new vault is not "
                      "available: ") +
                  status_;
    }
    emit changed();
}

void AppController::on_migration_failed(const QString& message,
                                        const QString& plaintext_path) {
    join_migration_thread();
    if (vault_state_ != "migrating" || !legacy_pending_) {
        return;
    }
    set_vault_busy(false);
    QString text = QStringLiteral("Migration failed: ") + message;
    if (!plaintext_path.isEmpty()) {
        text += QStringLiteral(
                    " A plaintext copy of the old cluster is at %1; inspect "
                    "the migration directories before retrying.")
                    .arg(plaintext_path);
    } else if (!app_config_.data_dir.empty()) {
        text += QStringLiteral(" Your old data is unchanged at %1.")
                    .arg(QString::fromStdString(app_config_.data_dir));
    }
    text += QStringLiteral(" Retry or cancel.");
    status_ = text;
    emit changed();
}

void AppController::load_data() {
    if (!vault_ || vault_state_ != "unlocked" || !vault_->is_unlocked()) {
        return;
    }
    settings_ = VaultSettingsRepository(*vault_).get();
    const auto parsed_mode = oyama::storage::parse_mode(mode_.toStdString());
    if (!parsed_mode.has_value()) {
        fail("Invalid mode: " + mode_);
        return;
    }
    auto events = VaultDoseEventRepository(*vault_).list(*parsed_mode);
    doses_.clear();
    for (const auto& event : events) {
        QVariantMap item;
        item["id"] = QString::fromStdString(event.id);
        item["route"] = QString::fromStdString(event.route);
        item["ester"] = QString::fromStdString(event.ester);
        item["doseMG"] = event.dose_mg;
        item["timeH"] = event.time_h;
        item["timeLabel"] = formatTime(event.time_h);
        item["extras"] = QString::fromStdString(event.extras_json);
        doses_.append(item);
    }
    compute_curve(events);
    status_ = "Connected to " + QString::fromStdString(app_config_.dbname) +
              " via " + backend_;
    start_idle_timer();
    emit changed();
}

void AppController::compute_curve(const std::vector<DoseEvent>& events) {
    curve_.clear();
    current_e2_ = 0.0;
    current_cpa_ = 0.0;
    current_t_ = 0.0;

    std::vector<std::string> ids;
    ids.reserve(events.size());
    std::vector<oyama_pk_dose> doses;
    doses.reserve(events.size());
    for (const auto& event : events) {
        const auto route = route_from(QString::fromStdString(event.route));
        const auto ester = ester_from(QString::fromStdString(event.ester));
        if (!route.has_value() || !ester.has_value()) {
            continue;
        }
        ids.push_back(event.id);
        oyama_pk_dose dose{};
        dose.id = ids.back().c_str();
        dose.route = *route;
        dose.ester = *ester;
        dose.time_h = event.time_h;
        dose.dose_mg = event.dose_mg;
        dose.extras = extras_from_json(event.extras_json);
        doses.push_back(dose);
    }
    if (doses.empty()) {
        return;
    }

    const auto params = pk_params_from_json(pkParamsJson());
    oyama_pk_request request{};
    request.events = doses.data();
    request.event_count = doses.size();
    request.body_weight_kg = settings_.body_weight_kg.value_or(70.0);
    if (request.body_weight_kg <= 0.0) {
        request.body_weight_kg = 70.0;
    }
    request.birth_year = settings_.birth_year.value_or(0);
    request.now_h = nowHours();
    request.params = &params;

    oyama_pk_result result{};
    oyama_pk_result_init(&result, nullptr, nullptr, nullptr, nullptr, nullptr,
                         0);
    const auto status = oyama_pk_simulate(&request, &result);
    if (status == OYAMA_PK_NO_RESULT) {
        return;
    }
    if (status == OYAMA_PK_ERR_BUFFER_TOO_SMALL) {
        const std::size_t n = result.length;
        std::vector<double> time(n);
        std::vector<double> total(n);
        std::vector<double> e2(n);
        std::vector<double> cpa(n);
        std::vector<double> t(n);
        oyama_pk_result_init(&result, time.data(), total.data(), e2.data(),
                             cpa.data(), t.data(), n);
        if (oyama_pk_simulate(&request, &result) != OYAMA_PK_OK) {
            return;
        }
        const std::size_t stride = std::max<std::size_t>(1, n / 700);
        for (std::size_t i = 0; i < n; i += stride) {
            QVariantMap point;
            point["t"] = time[i];
            point["e2"] = e2[i];
            point["cpa"] = cpa[i];
            point["tLevel"] = t[i];
            curve_.append(point);
        }
        const double now = nowHours();
        const double e2_now = oyama_pk_interpolate_e2(&result, now);
        const double cpa_now = oyama_pk_interpolate_cpa(&result, now);
        const double t_now = oyama_pk_interpolate_t(&result, now);
        if (std::isfinite(e2_now)) current_e2_ = e2_now;
        if (std::isfinite(cpa_now)) current_cpa_ = cpa_now;
        if (std::isfinite(t_now)) current_t_ = t_now;
    }
}

bool AppController::addDose(const QString& route, const QString& ester,
                            double dose_mg, double time_h,
                            const QVariantMap& extras) {
    if (!require_unlocked()) {
        return false;
    }
    if (!std::isfinite(dose_mg) || dose_mg <= 0.0 || !std::isfinite(time_h)) {
        fail("Enter a valid dose and time.");
        return false;
    }
    const auto parsed_mode = oyama::storage::parse_mode(mode_.toStdString());
    if (!parsed_mode.has_value()) {
        fail("Invalid mode.");
        return false;
    }
    if (!route_from(route).has_value() || !ester_from(ester).has_value()) {
        fail("Unsupported route or ester.");
        return false;
    }
    try {
        DoseEvent event;
        event.id = oyama::storage::generate_uuid_v4();
        event.mode = *parsed_mode;
        event.route = route.toStdString();
        event.ester = ester.toStdString();
        event.time_h = time_h > 0.0 ? time_h : nowHours();
        event.dose_mg = dose_mg;
        event.extras_json = extras_to_json(extras);
        event.created_at = now_ms();
        event.updated_at = event.created_at;
        VaultDoseEventRepository(*vault_).upsert(event);
        load_data();
        status_ = "Dose added.";
        emit changed();
        return true;
    } catch (const std::exception& error) {
        fail(QString("Could not add dose: ") + QString::fromUtf8(error.what()));
        return false;
    }
}

bool AppController::updateDose(const QString& id, const QString& route,
                               const QString& ester, double dose_mg,
                               double time_h, const QVariantMap& extras) {
    if (!require_unlocked()) {
        return false;
    }
    if (!std::isfinite(dose_mg) || dose_mg <= 0.0) {
        fail("Dose must be greater than zero.");
        return false;
    }
    if (!std::isfinite(time_h) || time_h <= 0.0) {
        fail("Dose time is invalid.");
        return false;
    }
    const auto mode = oyama::storage::parse_mode(mode_.toStdString());
    const auto route_value = route_from(route);
    const auto ester_value = ester_from(ester);
    if (!mode || !route_value || !ester_value) {
        fail("Invalid mode, route, or ester.");
        return false;
    }

    try {
        VaultDoseEventRepository repository(*vault_);
        auto existing = repository.get(id.toStdString());
        if (!existing.has_value()) {
            fail("Dose not found.");
            return false;
        }
        existing->mode = *mode;
        existing->route = route.toStdString();
        existing->ester = ester.toStdString();
        existing->time_h = time_h;
        existing->dose_mg = dose_mg;
        existing->extras_json = extras_to_json(extras);
        existing->updated_at = now_ms();
        repository.upsert(*existing);
        load_data();
        status_ = "Dose updated.";
        emit changed();
        return true;
    } catch (const std::exception& error) {
        fail(QString("Could not update dose: %1").arg(error.what()));
        return false;
    }
}

bool AppController::saveSettings(double weight, int birth_year,
                                 const QString& calibration_method,
                                 const QString& calibration_history,
                                 const QString& pk_params_json) {
    if (!require_unlocked()) {
        return false;
    }
    if (!std::isfinite(weight) || weight <= 0.0) {
        fail("Body weight must be greater than zero.");
        return false;
    }
    if (birth_year != 0 && (birth_year < 1900 || birth_year > 2200)) {
        fail("Birth year must be between 1900 and 2200, or 0 to unset.");
        return false;
    }
    const std::string method = calibration_method.toStdString();
    if (method != "off" && method != "ekf" && method != "ou_kalman" &&
        method != "mipd") {
        fail("Invalid calibration method.");
        return false;
    }
    const std::string history = calibration_history.toStdString();
    if (history != "forward" && history != "retrospective") {
        fail("Invalid calibration history mode.");
        return false;
    }

    try {
        VaultSettingsRepository repository(*vault_);
        oyama::storage::Settings settings = repository.get();
        settings.body_weight_kg = weight;
        settings.weight_updated_at = now_ms();
        settings.birth_year =
            birth_year > 0 ? std::optional<int>(birth_year) : std::nullopt;
        settings.birth_year_updated_at =
            birth_year > 0 ? std::optional<std::int64_t>(now_ms())
                           : std::nullopt;
        settings.calibration_method = method;
        settings.calibration_history_mode = history;
        if (!pk_params_json.trimmed().isEmpty()) {
            settings.pk_params_json = pk_params_json.toStdString();
            settings.pk_params_updated_at = now_ms();
        }
        settings.updated_at = now_ms();
        repository.save(settings);
        load_data();
        status_ = "Settings saved.";
        emit changed();
        return true;
    } catch (const std::exception& error) {
        fail(QString("Could not save settings: %1").arg(error.what()));
        return false;
    }
}

double AppController::weight() const {
    return settings_.body_weight_kg.value_or(70.0);
}

int AppController::birthYear() const {
    return settings_.birth_year.value_or(0);
}

QString AppController::calibrationMethod() const {
    return QString::fromStdString(settings_.calibration_method);
}

QString AppController::calibrationHistory() const {
    return QString::fromStdString(settings_.calibration_history_mode);
}

QString AppController::pkParamsJson() const {
    return QString::fromStdString(settings_.pk_params_json.value_or("{}"));
}

bool AppController::removeDose(const QString& id) {
    if (!require_unlocked()) {
        return false;
    }
    try {
        (void)VaultDoseEventRepository(*vault_).remove(id.toStdString());
        load_data();
        status_ = "Dose removed.";
        emit changed();
        return true;
    } catch (const std::exception& error) {
        fail(QString("Could not remove dose: ") +
             QString::fromUtf8(error.what()));
        return false;
    }
}

double AppController::nowHours() const {
    return static_cast<double>(QDateTime::currentMSecsSinceEpoch()) /
           3600000.0;
}

QString AppController::formatTime(double time_h) const {
    if (!std::isfinite(time_h)) {
        return "-";
    }
    return QDateTime::fromSecsSinceEpoch(
               static_cast<qint64>(time_h * 3600.0), QTimeZone::UTC)
        .toLocalTime()
        .toString("yyyy-MM-dd HH:mm");
}
