#include "app_controller.h"
#include "ui_preferences.h"

#include <QDebug>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QMouseEvent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QTimer>
#include <QtQml/qqml.h>

#include <cstdlib>

#ifdef __linux__
#include <sys/prctl.h>
#endif
#include <sys/resource.h>

int main(int argc, char* argv[]) {
    // The interface is drawn lines (Qt Quick Shapes): multisampling keeps
    // their edges smooth on the default renderer.
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

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
    QGuiApplication::setApplicationVersion(OYAMA_APP_VERSION);
    QGuiApplication::setWindowIcon(
        QIcon(QStringLiteral(":/qt/qml/OyamaHrt/app/assets/app-icon.png")));

    QString screenshotPath;
    QString pageName = "overview";
    const QStringList arguments = app.arguments();
    for (int i = 1; i < arguments.size(); ++i) {
        if (arguments[i] == "--screenshot" && i + 1 < arguments.size()) {
            screenshotPath = arguments[++i];
        } else if (arguments[i] == "--page" && i + 1 < arguments.size()) {
            pageName = arguments[++i];
        }
    }
    for (const QString& path : {QStringLiteral(":/fonts/InterVariable.ttf"),
                                QStringLiteral(":/fonts/Vazirmatn-Regular.ttf"),
                                QStringLiteral(":/fonts/Vazirmatn-Medium.ttf"),
                                QStringLiteral(":/fonts/Vazirmatn-SemiBold.ttf")}) {
        if (QFontDatabase::addApplicationFont(path) < 0) {
            qWarning() << "Could not load font" << path;
        }
    }
    QQuickStyle::setStyle("Basic");

    AppController controller;
    UiPreferences ui;
    qmlRegisterSingletonInstance("OyamaHrt", 1, 0, "Ui", &ui);
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
    if (!screenshotPath.isEmpty()) {
        auto* window =
            qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        QTimer::singleShot(700, &app, [window, pageName, screenshotPath, &app]() {
            if (window != nullptr) {
                QMetaObject::invokeMethod(window, "openRoute",
                                          Q_ARG(QVariant, QVariant(pageName)));
            }
            // The offscreen pointer rests at the window's corner, over the
            // sidebar, which opens while it is pointed at. Move it away.
            if (window != nullptr) {
                const QPointF away(window->width() / 2.0, window->height() - 4);
                QMouseEvent move(QEvent::MouseMove, away, window->mapToGlobal(away),
                                 Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QCoreApplication::sendEvent(window, &move);
            }
            QTimer::singleShot(1400, &app, [window, screenshotPath]() {
                if (window != nullptr) {
                    const QImage image = window->grabWindow();
                    if (image.isNull() || !image.save(screenshotPath)) {
                        qWarning() << "Could not save screenshot"
                                   << screenshotPath;
                    }
                }
                QCoreApplication::quit();
            });
        });
        return app.exec();
    }
    return app.exec();
}
