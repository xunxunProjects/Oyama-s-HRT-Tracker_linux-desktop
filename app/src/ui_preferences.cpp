#include "ui_preferences.h"

#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QSettings>
#include <QStyleHints>

namespace {
const QStringList kLanguages{"en", "zh", "ru", "fa"};
}

UiPreferences::UiPreferences(QObject* parent) : QObject(parent) {
    fallbackStrings_ = loadStrings("en");

    QString language = qEnvironmentVariable("OYAMA_UI_LANGUAGE");
    if (!validLanguage(language)) {
        language = QSettings().value("ui/language").toString();
    }
    if (validLanguage(language)) {
        loadLanguage(language);
    } else {
        language_.clear();
        strings_ = fallbackStrings_;
    }

    QString theme = qEnvironmentVariable("OYAMA_UI_THEME");
    if (!validTheme(theme)) {
        theme = QSettings().value("ui/theme", "light").toString();
    }
    theme_ = validTheme(theme) ? theme : QStringLiteral("light");

    if (auto* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        app->setFont(QFont(fontFamily(), 11));
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        connect(app->styleHints(), &QStyleHints::colorSchemeChanged, this,
                &UiPreferences::systemDarkChanged);
#endif
    }
}

QString UiPreferences::language() const { return language_; }

void UiPreferences::setLanguage(const QString& code) {
    if (!validLanguage(code) || code == language_) {
        return;
    }
    loadLanguage(code);
    QSettings().setValue("ui/language", code);
    if (auto* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        app->setFont(QFont(fontFamily(), 11));
    }
    emit languageChanged();
}

QString UiPreferences::theme() const { return theme_; }

void UiPreferences::setTheme(const QString& mode) {
    if (!validTheme(mode) || mode == theme_) {
        return;
    }
    theme_ = mode;
    QSettings().setValue("ui/theme", mode);
    emit themeChanged();
}

bool UiPreferences::systemDark() const {
    if (qobject_cast<QGuiApplication*>(QCoreApplication::instance()) != nullptr) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
        return QGuiApplication::palette().color(QPalette::Window).lightness() < 128;
#endif
    }
    return false;
}

bool UiPreferences::rtl() const { return language_ == "fa"; }

QString UiPreferences::fontFamily() const {
    if (language_ == "fa") {
        return QStringLiteral("Vazirmatn");
    }
    if (language_ == "zh") {
        return QStringLiteral("Noto Sans CJK SC");
    }
    return QStringLiteral("Inter");
}

QVariantMap UiPreferences::strings() const { return strings_; }

QVariantList UiPreferences::availableLanguages() const {
    return {
        QVariantMap{{"code", "en"}, {"nativeName", "English"},
                    {"englishName", "English"}, {"sample", "Aa"}, {"rtl", false}},
        QVariantMap{{"code", "zh"}, {"nativeName", "中文"},
                    {"englishName", "Chinese"}, {"sample", "永"}, {"rtl", false}},
        QVariantMap{{"code", "ru"}, {"nativeName", "Русский"},
                    {"englishName", "Russian"}, {"sample", "Дд"}, {"rtl", false}},
        QVariantMap{{"code", "fa"}, {"nativeName", "فارسی"},
                    {"englishName", "Persian"}, {"sample", "فا"}, {"rtl", true}},
    };
}

bool UiPreferences::needsLanguage() const { return language_.isEmpty(); }

QString UiPreferences::translate(const QString& key, const QVariantList& args) const {
    QString text = strings_.value(key).toString();
    if (text.isEmpty()) {
        text = fallbackStrings_.value(key).toString();
    }
    if (text.isEmpty()) {
        return key;
    }
    for (int i = 0; i < args.size() && i < 9; ++i) {
        text.replace(QStringLiteral("%") + QString::number(i + 1), args[i].toString());
    }
    return text;
}

QVariantMap UiPreferences::stringsFor(const QString& code) const {
    return loadStrings(code);
}

bool UiPreferences::validLanguage(const QString& code) {
    return kLanguages.contains(code);
}

bool UiPreferences::validTheme(const QString& mode) {
    return mode == "light" || mode == "dark" || mode == "system";
}

QVariantMap UiPreferences::loadStrings(const QString& code) {
    QVariantMap result;
    if (!validLanguage(code)) {
        return result;
    }
    QFile file(QStringLiteral(":/i18n/") + code + QStringLiteral(".json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return result;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return result;
    }
    const QJsonObject object = document.object();
    for (auto it = object.begin(); it != object.end(); ++it) {
        if (!it.key().startsWith('_')) {
            result.insert(it.key(), it.value().toString());
        }
    }
    return result;
}

void UiPreferences::loadLanguage(const QString& code) {
    language_ = code;
    strings_ = loadStrings(code);
    if (strings_.isEmpty()) {
        strings_ = fallbackStrings_;
    }
}
