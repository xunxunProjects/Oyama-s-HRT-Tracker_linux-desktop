#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QString>

class UiPreferences : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool systemDark READ systemDark NOTIFY systemDarkChanged)
    Q_PROPERTY(bool rtl READ rtl NOTIFY languageChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY languageChanged)
    Q_PROPERTY(QVariantMap strings READ strings NOTIFY languageChanged)
    Q_PROPERTY(QVariantList availableLanguages READ availableLanguages CONSTANT)
    Q_PROPERTY(bool needsLanguage READ needsLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)

public:
    explicit UiPreferences(QObject* parent = nullptr);

    QString language() const;
    void setLanguage(const QString& code);
    QString theme() const;
    void setTheme(const QString& mode);
    bool systemDark() const;
    bool rtl() const;
    QString fontFamily() const;
    QVariantMap strings() const;
    QVariantList availableLanguages() const;
    bool needsLanguage() const;
    QString mode() const;
    void setMode(const QString& mode);

    Q_INVOKABLE QString translate(const QString& key,
                                  const QVariantList& args = {}) const;
    Q_INVOKABLE QVariantMap stringsFor(const QString& code) const;

signals:
    void languageChanged();
    void themeChanged();
    void systemDarkChanged();
    void modeChanged();

private:
    static bool validLanguage(const QString& code);
    static bool validTheme(const QString& mode);
    static QVariantMap loadStrings(const QString& code);
    void loadLanguage(const QString& code);

    QString language_;
    QString theme_{"light"};
    QString mode_;
    QVariantMap strings_;
    QVariantMap fallbackStrings_;
};
