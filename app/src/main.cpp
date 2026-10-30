#include "app_controller.h"
#include "ui_preferences.h"

#include <QDebug>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QtQml/qqml.h>

#include <cstdlib>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("Oyama's HRT Tracker");
    QGuiApplication::setOrganizationName("Oyama");
    QGuiApplication::setApplicationVersion(OYAMA_APP_VERSION);
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
    return app.exec();
}
