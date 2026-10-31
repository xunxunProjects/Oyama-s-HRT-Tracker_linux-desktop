#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <oyama/storage/storage.h>
#include <oyama/storage/vault_store.h>

class QThread;
class QTimer;

class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(QString backend READ backend NOTIFY changed)
    Q_PROPERTY(QString vaultState READ vaultState NOTIFY changed)
    Q_PROPERTY(bool vaultBusy READ vaultBusy NOTIFY changed)
    Q_PROPERTY(bool migrationRetryAvailable READ migrationRetryAvailable
                   NOTIFY changed)
    Q_PROPERTY(QString setupPhrase READ setupPhrase NOTIFY changed)
    Q_PROPERTY(QVariantList setupChallenge READ setupChallenge NOTIFY changed)
    Q_PROPERTY(QVariantList doses READ doses NOTIFY changed)
    Q_PROPERTY(QVariantList curve READ curve NOTIFY changed)
    Q_PROPERTY(double currentE2 READ currentE2 NOTIFY changed)
    Q_PROPERTY(double currentCPA READ currentCPA NOTIFY changed)
    Q_PROPERTY(double currentT READ currentT NOTIFY changed)
    Q_PROPERTY(double weight READ weight NOTIFY changed)
    Q_PROPERTY(int birthYear READ birthYear NOTIFY changed)
    Q_PROPERTY(QString calibrationMethod READ calibrationMethod NOTIFY changed)
    Q_PROPERTY(QString calibrationHistory READ calibrationHistory NOTIFY changed)
    Q_PROPERTY(QString pkParamsJson READ pkParamsJson NOTIFY changed)

public:
    explicit AppController(QObject* parent = nullptr);
    ~AppController() override;

    [[nodiscard]] QString status() const { return status_; }
    [[nodiscard]] QString mode() const { return mode_; }
    [[nodiscard]] QString backend() const { return backend_; }
    [[nodiscard]] QString vaultState() const { return vault_state_; }
    [[nodiscard]] bool vaultBusy() const { return vault_busy_; }
    [[nodiscard]] bool migrationRetryAvailable() const;
    [[nodiscard]] QString setupPhrase() const { return setup_phrase_; }
    [[nodiscard]] QVariantList setupChallenge() const;
    [[nodiscard]] QVariantList doses() const { return doses_; }
    [[nodiscard]] QVariantList curve() const { return curve_; }
    [[nodiscard]] double currentE2() const { return current_e2_; }
    [[nodiscard]] double currentCPA() const { return current_cpa_; }
    [[nodiscard]] double currentT() const { return current_t_; }
    [[nodiscard]] double weight() const;
    [[nodiscard]] int birthYear() const;
    [[nodiscard]] QString calibrationMethod() const;
    [[nodiscard]] QString calibrationHistory() const;
    [[nodiscard]] QString pkParamsJson() const;

    void setMode(const QString& mode);

    Q_INVOKABLE void reload();
    Q_INVOKABLE bool beginVaultSetup();
    Q_INVOKABLE bool confirmVaultSetup(const QVariantList& answers,
                                      bool acknowledged);
    Q_INVOKABLE void cancelVaultSetup();
    Q_INVOKABLE bool unlockVault(const QString& phrase);
    Q_INVOKABLE void lockVault();
    Q_INVOKABLE bool retryLegacyMigration();
    Q_INVOKABLE void cancelLegacyMigration();
    Q_INVOKABLE bool addDose(const QString& route, const QString& ester,
                             double dose_mg, double time_h,
                             const QVariantMap& extras = {});
    Q_INVOKABLE bool updateDose(const QString& id, const QString& route,
                                const QString& ester, double dose_mg,
                                double time_h, const QVariantMap& extras = {});
    Q_INVOKABLE bool saveSettings(double weight, int birth_year,
                                  const QString& calibration_method,
                                  const QString& calibration_history,
                                  const QString& pk_params_json);
    Q_INVOKABLE bool removeDose(const QString& id);
    [[nodiscard]] Q_INVOKABLE double nowHours() const;
    [[nodiscard]] Q_INVOKABLE QString formatTime(double time_h) const;

signals:
    void changed();
    void modeChanged();

private slots:
    void on_migration_progress(const QString& kind, int done, int total);
    void on_migration_finished();
    void on_migration_failed(const QString& message,
                             const QString& plaintext_path);

private:
    void fail(const QString& message);
    void load_data();
    void compute_curve(const std::vector<oyama::storage::DoseEvent>& events);
    void set_vault_state(const QString& state);
    void set_vault_busy(bool busy);
    void clear_setup();
    void clear_models();
    void start_idle_timer();
    [[nodiscard]] bool require_unlocked();
    [[nodiscard]] bool start_migration_worker();
    void join_migration_thread();
    void clear_pending_migration_phrase();
    [[nodiscard]] QString migration_plaintext_path() const;

    std::unique_ptr<oyama::storage::Database> database_;
    std::unique_ptr<oyama::storage::LocalCluster> cluster_;
    std::unique_ptr<oyama::storage::VaultStore> vault_;
    oyama::storage::AppConfig app_config_;
    oyama::storage::Settings settings_;

    QString status_{"Starting..."};
    QString mode_{"transfem"};
    QString backend_{"none"};
    QString vault_state_{"unavailable"};
    bool vault_busy_{false};
    QString setup_phrase_;
    std::vector<std::size_t> setup_challenge_;
    bool legacy_pending_{false};
    std::string pending_migration_phrase_;
    QThread* migration_thread_{nullptr};
    QTimer* idle_timer_{nullptr};
    QVariantList doses_;
    QVariantList curve_;
    double current_e2_{0.0};
    double current_cpa_{0.0};
    double current_t_{0.0};
};
