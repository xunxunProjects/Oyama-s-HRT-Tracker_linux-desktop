#include "app_controller.h"

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
        return 0;
    }
    return app.exec();
}
