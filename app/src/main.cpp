#include "app_controller.h"

#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include <cstdlib>

#ifdef __linux__
#include <sys/prctl.h>
#endif
#include <sys/resource.h>

int main(int argc, char* argv[]) {
#ifdef __linux__
    // Do not let another process attach to the UI process while the vault is
    // unlocked, and never leave a core file with decrypted data behind.
    (void)prctl(PR_SET_DUMPABLE, 0);
#endif
    struct rlimit core_limit {};
    core_limit.rlim_cur = 0;
    core_limit.rlim_max = 0;
    (void)setrlimit(RLIMIT_CORE, &core_limit);

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("Oyama's HRT Tracker");
    QGuiApplication::setOrganizationName("Oyama");
    QQuickStyle::setStyle("Basic");

    AppController controller;
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &controller,
                     &AppController::lockVault, Qt::DirectConnection);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("OyamaHrt", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    if (app.arguments().contains("--self-test")) {
        // The smoke check boots the offscreen shell and verifies that the
        // vault gate covers and disables the application shell whenever the
        // vault is not unlocked.  It never prompts for a phrase.
        QObject* root = engine.rootObjects().constFirst();
        QObject* gate = root->findChild<QObject*>(QStringLiteral("vaultGate"));
        if (gate == nullptr) {
            qCritical("self-test: vault gate was not found");
            return 1;
        }
        QObject* shell = root->findChild<QObject*>(QStringLiteral("appShell"));
        if (shell == nullptr) {
            qCritical("self-test: application shell was not found");
            return 1;
        }
        const bool unlocked = controller.vaultState() == QStringLiteral("unlocked");
        if (gate->property("visible").toBool() == unlocked) {
            qCritical("self-test: vault gate visibility does not match state %s",
                      qPrintable(controller.vaultState()));
            return 1;
        }
        if (controller.vaultState() == QStringLiteral("unavailable") &&
            root->findChild<QObject*>(QStringLiteral("vaultUnavailable")) ==
                nullptr) {
            qCritical("self-test: unavailable vault panel was not loaded");
            return 1;
        }
        if (shell->property("enabled").toBool() != unlocked) {
            qCritical("self-test: application shell enabled state does not "
                      "match state %s",
                      qPrintable(controller.vaultState()));
            return 1;
        }
        return 0;
    }
    return app.exec();
}
