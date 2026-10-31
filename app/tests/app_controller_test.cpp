#include "app_controller.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <oyama/storage/app_config.h>
#include <oyama/storage/connection.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

using oyama::storage::Connection;
using oyama::storage::load_app_config;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "app_controller_test: " << message << '\n';
        return false;
    }
    return true;
}

#define REQUIRE(condition, message)                       \
    do {                                                  \
        if (!expect((condition), (message))) return 1;    \
    } while (0)

bool vault_meta_relation_exists() {
    const auto config = load_app_config();
    if (!config.has_value()) {
        return false;
    }
    Connection connection(config->connection_config());
    const auto result =
        connection.exec("SELECT to_regclass('vault_meta') IS NOT NULL");
    return result.boolean(0, 0);
}

bool vault_meta_row_exists() {
    const auto config = load_app_config();
    if (!config.has_value()) {
        return false;
    }
    Connection connection(config->connection_config());
    const auto result =
        connection.exec("SELECT EXISTS (SELECT 1 FROM vault_meta)");
    return result.boolean(0, 0);
}

QVariantList answers_for(const QStringList& words,
                         const QVariantList& challenge) {
    QVariantList answers;
    for (const auto& position : challenge) {
        answers << words.at(position.toInt());
    }
    return answers;
}

int run_existing_database_scenario(QCoreApplication& application,
                                   AppController& controller) {
    REQUIRE(controller.backend() == "existing",
            "expected the existing-backend fixture");
    REQUIRE(controller.vaultState() == "uninitialized",
            "a fresh vault must report uninitialized");
    REQUIRE(!controller.vaultBusy(), "controller must start not busy");
    REQUIRE(controller.setupPhrase().isEmpty(),
            "setup phrase must be empty before setup begins");
    REQUIRE(controller.setupChallenge().isEmpty(),
            "challenge must be empty before setup begins");
    REQUIRE(!vault_meta_row_exists(),
            "a fresh migrated schema must not have a vault_meta row");

    REQUIRE(!controller.addDose("oral", "E2", 2.0,
                                controller.nowHours() - 24.0, {}),
            "data commands must fail before the vault is set up");
    REQUIRE(controller.status() == "Vault is locked",
            "locked data access must report a clear vault lock status");
    REQUIRE(controller.doses().isEmpty(), "no data may be loaded early");
    REQUIRE(std::abs(controller.weight() - 70.0) < 1e-9,
            "locked settings must use the safe default");

    REQUIRE(controller.beginVaultSetup(), "begin vault setup");
    REQUIRE(controller.setupPhrase().split(' ', Qt::SkipEmptyParts).size() ==
                7,
            "setup must generate seven words");
    REQUIRE(controller.setupChallenge().size() == 3,
            "setup must choose three challenge positions");
    controller.cancelVaultSetup();
    REQUIRE(controller.vaultState() == "uninitialized",
            "cancel must leave the vault uninitialized");
    REQUIRE(controller.setupPhrase().isEmpty(),
            "cancel must clear the setup phrase");
    REQUIRE(controller.setupChallenge().isEmpty(),
            "cancel must clear the setup challenge");
    REQUIRE(!vault_meta_row_exists(),
            "cancel must not write vault metadata");

    REQUIRE(controller.beginVaultSetup(), "restart vault setup");
    const QString phrase = controller.setupPhrase();
    const QStringList words = phrase.split(' ', Qt::SkipEmptyParts);
    REQUIRE(words.size() == 7, "setup phrase must have seven words");
    const QVariantList challenge = controller.setupChallenge();
    REQUIRE(challenge.size() == 3, "challenge must have three positions");
    for (const auto& position : challenge) {
        const int index = position.toInt();
        REQUIRE(index >= 0 && index < words.size(),
                "challenge position must be a valid word index");
    }
    const QVariantList answers = answers_for(words, challenge);

    QVariantList wrong_answers = answers;
    wrong_answers[0] = QStringLiteral("definitely-not-a-phrase-word");
    REQUIRE(!controller.confirmVaultSetup(wrong_answers, true),
            "a wrong challenge must not complete setup");
    REQUIRE(controller.vaultState() == "uninitialized",
            "a wrong challenge must leave the vault uninitialized");
    REQUIRE(!vault_meta_row_exists(),
            "a wrong challenge must not write vault metadata");

    REQUIRE(!controller.confirmVaultSetup(answers, false),
            "setup must require the acknowledgement");
    REQUIRE(controller.vaultState() == "uninitialized",
            "missing acknowledgement must leave the vault uninitialized");
    REQUIRE(!vault_meta_row_exists(),
            "missing acknowledgement must not write vault metadata");
    REQUIRE(controller.setupPhrase() == phrase,
            "failed confirmation must keep the phrase for retry");

    REQUIRE(controller.confirmVaultSetup(answers, true),
            "setup must complete with the right challenge");
    REQUIRE(controller.vaultState() == "unlocked",
            "vault must be unlocked after setup");
    REQUIRE(vault_meta_row_exists(),
            "successful setup must write vault metadata");
    REQUIRE(controller.setupPhrase().isEmpty(),
            "successful setup must clear the phrase");
    REQUIRE(controller.setupChallenge().isEmpty(),
            "successful setup must clear the challenge");
    REQUIRE(!controller.vaultBusy(), "setup must clear the busy flag");
    REQUIRE(controller.status().startsWith("Connected"),
            "unlocked status must report the connection");

    QTimer* idle_timer =
        controller.findChild<QTimer*>("vaultIdleTimer");
    REQUIRE(idle_timer != nullptr, "controller must own an idle timer");
    REQUIRE(idle_timer->interval() == 15 * 60 * 1000,
            "idle timer must use a fifteen minute interval");
    REQUIRE(idle_timer->isSingleShot(),
            "idle timer must fire only once");
    REQUIRE(idle_timer->isActive(),
            "idle timer must run while the vault is unlocked");

    REQUIRE(controller.saveSettings(68.5, 1991, "mipd", "retrospective",
                                    R"({"e2_kClear":0.4})"),
            "save settings while unlocked");
    REQUIRE(std::abs(controller.weight() - 68.5) < 1e-9 &&
                controller.birthYear() == 1991 &&
                controller.calibrationMethod() == "mipd" &&
                controller.calibrationHistory() == "retrospective" &&
                controller.pkParamsJson().contains("e2_kClear"),
            "settings must round-trip through the vault");

    const double now = controller.nowHours();
    REQUIRE(controller.addDose("oral", "E2", 2.0, now - 24.0, {}),
            "add a dose while unlocked");
    REQUIRE(controller.doses().size() == 1,
            "expected exactly one dose");
    const QVariantMap added = controller.doses().front().toMap();
    const QString id = added.value("id").toString();
    REQUIRE(!id.isEmpty() && added.value("route").toString() == "oral" &&
                added.value("ester").toString() == "E2",
            "added dose has unexpected fields");
    REQUIRE(!controller.curve().isEmpty(),
            "curve must not be empty after adding a dose");

    QVariantMap extras;
    extras.insert("sublingualTier", 2);
    REQUIRE(controller.updateDose(id, "sublingual", "EV", 4.0, now - 24.0,
                                  extras),
            "update a dose while unlocked");
    REQUIRE(controller.doses().size() == 1,
            "editing must not create a second dose");
    const QVariantMap updated = controller.doses().front().toMap();
    REQUIRE(updated.value("route").toString() == "sublingual" &&
                updated.value("ester").toString() == "EV" &&
                std::abs(updated.value("doseMG").toDouble() - 4.0) < 1e-9 &&
                updated.value("extras").toString().contains("sublingualTier"),
            "updated dose did not round-trip");

    controller.lockVault();
    REQUIRE(controller.vaultState() == "locked",
            "lock must report the locked state");
    REQUIRE(controller.doses().isEmpty() && controller.curve().isEmpty(),
            "lock must clear decrypted models");
    REQUIRE(std::abs(controller.weight() - 70.0) < 1e-9 &&
                controller.birthYear() == 0,
            "lock must clear decrypted settings");
    REQUIRE(!idle_timer->isActive(),
            "idle timer must stop while the vault is locked");

    REQUIRE(!controller.addDose("oral", "E2", 2.0, now, {}),
            "add must fail while locked");
    REQUIRE(controller.status() == "Vault is locked",
            "locked add must report the vault lock");
    REQUIRE(!controller.updateDose(id, "oral", "E2", 2.0, now, {}),
            "update must fail while locked");
    REQUIRE(controller.status() == "Vault is locked",
            "locked update must report the vault lock");
    REQUIRE(!controller.removeDose(id), "remove must fail while locked");
    REQUIRE(controller.status() == "Vault is locked",
            "locked remove must report the vault lock");
    REQUIRE(!controller.saveSettings(70.0, 0, "mipd", "retrospective", "{}"),
            "save settings must fail while locked");
    REQUIRE(controller.status() == "Vault is locked",
            "locked settings must report the vault lock");

    REQUIRE(!controller.unlockVault("wrong phrase"),
            "a wrong phrase must be rejected");
    REQUIRE(controller.vaultState() == "locked",
            "a wrong phrase must leave the vault locked");
    REQUIRE(!controller.vaultBusy(), "failed unlock must clear the busy flag");

    REQUIRE(controller.unlockVault(phrase), "the right phrase must unlock");
    REQUIRE(controller.vaultState() == "unlocked",
            "vault must be unlocked after the right phrase");
    REQUIRE(controller.doses().size() == 1,
            "unlocking must load the stored dose");
    REQUIRE(std::abs(controller.weight() - 68.5) < 1e-9,
            "unlocking must load the stored settings");
    REQUIRE(idle_timer->isActive(),
            "idle timer must restart after unlock");

    QMetaObject::invokeMethod(idle_timer, "timeout", Qt::DirectConnection);
    REQUIRE(controller.vaultState() == "locked",
            "the idle timeout must lock the vault");
    REQUIRE(controller.doses().isEmpty(),
            "the idle timeout must clear the models");
    REQUIRE(!idle_timer->isActive(),
            "the idle timer must stop after auto-lock");

    REQUIRE(controller.unlockVault(phrase), "unlock after auto-lock");
    REQUIRE(controller.removeDose(id), "remove the dose while unlocked");
    REQUIRE(controller.doses().isEmpty(), "dose was not removed");

    controller.reload();
    REQUIRE(controller.vaultState() == "locked",
            "reload of an initialized vault must return locked");
    REQUIRE(controller.doses().isEmpty(),
            "reload must not load vault data while locked");
    REQUIRE(std::abs(controller.weight() - 70.0) < 1e-9,
            "reload must not load settings while locked");
    REQUIRE(!idle_timer->isActive(),
            "reload into the locked state must stop the idle timer");
    REQUIRE(vault_meta_row_exists(),
            "reload must keep the existing vault metadata");

    REQUIRE(controller.unlockVault(phrase), "unlock after reload");
    REQUIRE(controller.doses().isEmpty(),
            "the removed dose must stay removed");

    REQUIRE(QMetaObject::invokeMethod(&application, "aboutToQuit",
                                      Qt::DirectConnection),
            "aboutToQuit could not be invoked");
    REQUIRE(controller.vaultState() == "locked",
            "aboutToQuit must lock the vault");
    REQUIRE(controller.doses().isEmpty() &&
                controller.setupPhrase().isEmpty(),
            "aboutToQuit must clear decrypted state and the setup phrase");

    return 0;
}

int run_managed_legacy_scenario(AppController& controller) {
    REQUIRE(controller.backend() == "managed",
            "expected the managed-backend fixture");
    REQUIRE(controller.vaultState() == "unavailable",
            "managed legacy data must report the vault as unavailable");
    REQUIRE(controller.status().contains("migrate-vault"),
            "managed legacy data must point at the migration command");
    REQUIRE(controller.setupPhrase().isEmpty(),
            "managed legacy data must not generate a phrase");
    REQUIRE(controller.setupChallenge().isEmpty(),
            "managed legacy data must not generate a challenge");
    REQUIRE(!controller.beginVaultSetup(),
            "setup must be refused while legacy migration is pending");
    REQUIRE(controller.vaultState() == "unavailable",
            "refused setup must keep the legacy state unavailable");
    REQUIRE(!controller.confirmVaultSetup({}, true),
            "setup confirmation must be refused while migration is pending");
    REQUIRE(controller.status().contains("migrate-vault"),
            "refused confirmation must point at the migration command");
    REQUIRE(!vault_meta_relation_exists(),
            "legacy detection must not create vault_meta");

    controller.reload();
    REQUIRE(controller.vaultState() == "unavailable",
            "reload must keep managed legacy data unavailable");
    REQUIRE(controller.status().contains("migrate-vault"),
            "reload must repeat the migration instructions");
    REQUIRE(!vault_meta_relation_exists(),
            "reload must not migrate or create vault_meta");
    REQUIRE(controller.doses().isEmpty(),
            "legacy data must never be loaded in plaintext");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);

    AppController controller;
    const char* scenario = std::getenv("OYAMA_APP_CONTROLLER_SCENARIO");
    if (scenario != nullptr &&
        std::string(scenario) == "managed-legacy") {
        const int status = run_managed_legacy_scenario(controller);
        if (status != 0) {
            return status;
        }
        std::cout << "app_controller_test: ok\n";
        return 0;
    }

    const int status = run_existing_database_scenario(application, controller);
    if (status != 0) {
        return status;
    }
    std::cout << "app_controller_test: ok\n";
    return 0;
}
