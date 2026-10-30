#include "ui_preferences.h"

#include <QGuiApplication>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QVariantMap>

#include <iostream>

namespace {
int fail(const char* message) {
    std::cerr << "ui_preferences_test: " << message << '\n';
    return 1;
}
}  // namespace

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("OyamaUiTest");
    QCoreApplication::setApplicationName("OyamaUiTest");
    QSettings().clear();

    UiPreferences ui;
    if (!ui.needsLanguage() || !ui.language().isEmpty()) {
        return fail("fresh install should ask for a language");
    }
    if (ui.rtl() || ui.fontFamily() != "Inter") {
        return fail("empty language should default to LTR Inter");
    }
    if (ui.strings().value("nav.overview").toString() != "Overview") {
        return fail("empty language should preview English");
    }

    ui.setLanguage("zh");
    if (ui.needsLanguage() || ui.rtl() ||
        ui.fontFamily() != "Noto Sans CJK SC") {
        return fail("zh locale properties are wrong");
    }
    if (ui.translate("nav.overview") != QString::fromUtf8("概览")) {
        return fail("zh nav.overview translation is wrong");
    }
    if (ui.translate("status.connected", {"oyama_hrt", "managed"})
            .contains("%1") ||
        !ui.translate("status.connected", {"oyama_hrt", "managed"})
             .contains("oyama_hrt")) {
        return fail("status.connected arguments were not substituted");
    }

    ui.setLanguage("fa");
    if (!ui.rtl() || ui.fontFamily() != "Vazirmatn") {
        return fail("fa should be RTL with Vazirmatn");
    }
    const QString faOverview = ui.translate("nav.overview");
    if (faOverview.isEmpty() || faOverview == "nav.overview") {
        return fail("fa nav.overview is missing");
    }

    ui.setLanguage("ru");
    if (ui.rtl() || ui.fontFamily() != "Inter") {
        return fail("ru should be LTR Inter");
    }
    if (ui.translate("nav.overview") == "nav.overview") {
        return fail("ru nav.overview is missing");
    }

    ui.setLanguage("xx");
    if (ui.language() != "ru") {
        return fail("invalid language must be ignored");
    }
    ui.setLanguage("en");
    if (ui.translate("does.not.exist") != "does.not.exist") {
        return fail("missing keys must fall back to the key");
    }

    const QVariantMap en = ui.stringsFor("en");
    if (en.isEmpty() || en.contains("_meta")) {
        return fail("en strings are missing or _meta leaked");
    }
    for (const QString& code : {QString("zh"), QString("ru"), QString("fa")}) {
        const QVariantMap table = ui.stringsFor(code);
        if (table.keys() != en.keys()) {
            return fail("translation key sets do not match en");
        }
    }

    ui.setTheme("dark");
    if (ui.theme() != "dark") {
        return fail("dark theme was not set");
    }
    ui.setTheme("not-a-theme");
    if (ui.theme() != "dark") {
        return fail("invalid theme must be ignored");
    }

    UiPreferences persisted;
    if (persisted.language() != "en" || persisted.theme() != "dark") {
        return fail("language/theme did not persist");
    }
    if (persisted.availableLanguages().size() != 4) {
        return fail("expected four available languages");
    }

    for (const QString& path : {QStringLiteral(":/fonts/InterVariable.ttf"),
                               QStringLiteral(":/fonts/Vazirmatn-Regular.ttf"),
                               QStringLiteral(":/fonts/Vazirmatn-Medium.ttf"),
                               QStringLiteral(":/fonts/Vazirmatn-SemiBold.ttf")}) {
        if (!QFile::exists(path)) {
            std::cerr << "ui_preferences_test: missing " << path.toStdString()
                      << '\n';
            return 1;
        }
    }

    std::cout << "ui_preferences_test: ok\n";
    return 0;
}
