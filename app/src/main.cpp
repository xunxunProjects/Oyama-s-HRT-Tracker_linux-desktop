#include "app_controller.h"
#include "ui_preferences.h"

#include <QDebug>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QtQml/qqml.h>

#include <cstdlib>

namespace {
int pageIndexFor(const QString& name) {
    if (name == "overview") {
        return 0;
    }
    if (name == "add") {
        return 1;
    }
    if (name == "history") {
        return 2;
    }
    if (name == "settings") {
        return 3;
    }
    return -1;
}
}  // namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("Oyama's HRT Tracker");
    QGuiApplication::setOrganizationName("Oyama");
    QGuiApplication::setApplicationVersion(OYAMA_APP_VERSION);

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
        return 0;
    }
    if (!screenshotPath.isEmpty()) {
        auto* window =
            qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
        const int pageIndex = pageIndexFor(pageName);
        QTimer::singleShot(700, &app, [window, pageIndex, screenshotPath, &app]() {
            if (window != nullptr && pageIndex >= 0) {
                window->setProperty("pageIndex", pageIndex);
            }
            QTimer::singleShot(400, &app, [window, screenshotPath]() {
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
