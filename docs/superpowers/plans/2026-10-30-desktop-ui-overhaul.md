# Desktop UI Overhaul Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the dark engineering UI with the approved warm token system, left icon sidebar, full-screen language picker, and four locales (`en`, `zh`, `ru`, `fa`), including Persian RTL, without changing storage, model, backup, or CLI behavior.

**Architecture:** A C++ `UiPreferences` singleton owns language, theme, translations, and system-dark detection. QML singletons `Theme` and `T` expose design tokens and translated strings. All five pages are rewritten on a small set of styled components (`Card`, `AppButton`, `AppTextField`, `AppComboBox`, `Icon`, etc.). Translations are flat JSON files in Qt resources, loaded at runtime; the existing English `AppController::status` stays for tests, while `statusKey`/`statusArgs` drive localized UI messages.

**Tech Stack:** C++17, Qt 6.4+ (Qt Quick, Qt Quick Controls 2, QML), JSON resources, bundled Inter/Vazirmatn fonts, CMake/Ninja, existing PostgreSQL storage and C99 PK model.

**Spec:** `docs/superpowers/specs/2026-10-30-desktop-ui-overhaul-design.md`

## Global Constraints

- Qt minimum stays **6.4**. Do not require Qt 6.5 APIs; `system` theme is best-effort on 6.4.
- Exactly four locales: **`en`, `zh`, `ru`, `fa`**. Every locale file must have the same keys; `_meta` is ignored by the loader.
- All user-visible text in QML goes through `T.t(...)`. No hardcoded interface strings. Brand name `Oyama`, hormone abbreviations `E2`/`CPA`/`T`, and units (`mg`, `pg/mL`, `ng/mL`, `ng/dL`, `µg/day`) may stay literal.
- Default theme is **light**. Theme values are `light`, `dark`, `system`; persist under `ui/theme`.
- Light: bg `#EFE6D0`, sidebar `#F5EDDF`, surface `#FFFDF7`, surfaceAlt `#EFE9D8`, surfaceHover `#F2E8D6`, surfacePressed `#E7DCC6`, text `#33302A`, textSoft `#7A7466`, textMuted `#6E6759`, textFaint `#6B6457`, textDisabled `#A69E8E`, border `#DED3BD`, accent `#B5793C`, accentHover `#A66B31`, accentSubtle `#F3E3CC`, accentInk `#8C5A28`, green `#4F7A5A`, brick `#B4573E`, onAccent `#2B241B`, chartLine `#B5793C`, chartGrid `#E4D8C2`.
- Dark: bg `#1F1D1A`, sidebar `#1B1917`, surface `#2B2823`, surfaceAlt `#35312B`, surfaceHover `#302C27`, surfacePressed `#3A352E`, text `#E6E0D4`, textMuted `#A39C8D`, textFaint `#9A9385`, textDisabled `#6F695D`, border `#3D3830`, accent `#C98F52`, accentHover `#D9A268`, accentSubtle `#3A2F23`, accentInk `#DFA96F`, green `#7FA98A`, brick `#D07A62`, onAccent `#1F1D1A`, chartGrid `#3A352E`.
- Only three radii: **18** card, **12** control, **999** pill. Only five type sizes: **28, 20, 15, 13, 11**. Weights: **400, 500, 600**.
- Window **1200×800**, minimum **980×680**; sidebar **236 px**; page margins **40 px**; card padding **20 px**; chart **320 px**.
- Only the **E2** curve is drawn (current behavior). Do not add CPA/T series or a second axis.
- Persian (`fa`) is fully RTL: sidebar on the right, mirrored layout, right-aligned text; chart time axis stays left-to-right.
- Keep `AppController::status` in English for existing tests and CLI semantics. Add `statusKey` and `statusArgs`; never remove or rename existing properties/invokables.
- Do not change PostgreSQL schema, migrations, backup format, route/ester enum strings, extras keys, or time math.
- No QtCharts, WebEngine, Qt Quick 3D, SVG image plugin, or shader effects.
- Linux test command: `PG_BINDIR=/tmp/opencode/pg18/bin ctest --test-dir build-release --output-on-failure`.
- FreeBSD acceptance target: VM `15.1-RELEASE`, Qt 6.11.2, `~/oyama-src`, credentials `oyama`/`oyama12345`, SSH host port `2222`, Plasma X11. Root SSH is disabled; use `printf "oyama12345\n" | su -m root -c "<cmd>"` from an `oyama` session.

## Review Focus

1. **First run with no database**: choosing a language must reveal the shell with a persistent DB banner; the shell must not crash or silently show an actionable “Add dose” empty state. Tested by `qt_shell_smoke_first_run`.
2. **Live language switching**: every visible label, placeholder, ComboBox item, toast, and validation message changes immediately; no `%1` or translation key leaks. Tested by `ui_preferences_test` key parity and four `qt_shell_smoke` locale cases.
3. **Persian RTL**: sidebar moves to the right, labels right-align, form order mirrors, numbers remain readable, directional icons flip. Verified by the `fa` screenshot at 1200×800.
4. **Data integrity after the rewrite**: add/edit/delete still send the same route/ester strings, extras keys, and `timeH` math. Tested by the existing `app_controller_test`, extended with status-key assertions.
5. **Theme switch**: the chart repaints, all surfaces recolor, and no stale hardcoded hex values remain outside `Theme.qml`. Verified by the hex scan and light/dark screenshots.

---

### Task 1: Translation core, `UiPreferences`, and `T` singleton

**Files:**
- Create: `app/i18n/en.json`, `app/i18n/zh.json`, `app/i18n/ru.json`, `app/i18n/fa.json`
- Create: `app/i18n/README.md`
- Create: `app/src/ui_preferences.h`, `app/src/ui_preferences.cpp`
- Create: `app/qml/T.qml`
- Create: `app/tests/ui_preferences_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `app/src/main.cpp`

**Interfaces:**
- Consumes: Qt Core/Gui, `QSettings`, `QJsonDocument`, `QFile`, `QStyleHints`.
- Produces:
  - `class UiPreferences : public QObject` with:
    - `Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)`
    - `Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)`
    - `Q_PROPERTY(bool systemDark READ systemDark NOTIFY systemDarkChanged)`
    - `Q_PROPERTY(bool rtl READ rtl NOTIFY languageChanged)`
    - `Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY languageChanged)`
    - `Q_PROPERTY(QVariantMap strings READ strings NOTIFY languageChanged)`
    - `Q_PROPERTY(QVariantList availableLanguages READ availableLanguages CONSTANT)`
    - `Q_PROPERTY(bool needsLanguage READ needsLanguage NOTIFY languageChanged)`
    - `Q_INVOKABLE QString translate(const QString& key, const QVariantList& args = {}) const`
    - `Q_INVOKABLE QVariantMap stringsFor(const QString& code) const`
  - QML singleton `T` with `t(key, args)`, `rtl`, `language`, `textAlign`.
  - Translation files at `:/i18n/<code>.json`; font resources at `:/fonts/...`.

- [ ] **Step 1: Write the failing `UiPreferences` test**

Create `app/tests/ui_preferences_test.cpp`:

```cpp
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
    if (ui.translate("nav.overview") != QString::fromUtf8("\xE6\xA6\x82\xE8\xA7\x88")) {
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

    std::cout << "ui_preferences_test: ok\n";
    return 0;
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DOYAMA_BUILD_QT_SHELL=ON -DCMAKE_INSTALL_PREFIX="$HOME/oyama-hrt-dist"` then `cmake --build build-release --target ui_preferences_test`.
Expected: configure/build fails because `ui_preferences.h`, the test target, and the resource files do not exist yet.

- [ ] **Step 3: Create the canonical English translation file**

Create `app/i18n/en.json` with exactly these entries (keep `_meta`; the loader filters keys starting with `_`):

```json
{
  "_meta": "Translations welcome via pull request. See README.md.",
  "app.name": "Oyama's HRT Tracker",
  "app.tagline": "HRT Tracker",
  "nav.overview": "Overview",
  "nav.add_dose": "Add dose",
  "nav.history": "History",
  "nav.settings": "Settings",
  "status.connected": "Connected to %1 via %2.",
  "status.local_socket": "Local · socket only",
  "status.not_connected": "Not connected",
  "status.managed": "Managed",
  "status.existing": "Existing database",
  "mode.transfem": "Transfem",
  "mode.transmasc": "Transmasc",
  "common.cancel": "Cancel",
  "common.save": "Save",
  "common.saved": "Saved",
  "common.delete": "Delete",
  "common.edit": "Edit",
  "common.close": "Close",
  "common.view_all": "View all",
  "theme.label": "Theme",
  "theme.light": "Light",
  "theme.dark": "Dark",
  "theme.system": "System",
  "language.label": "Language",
  "language.choose_title": "Choose your language",
  "language.choose_subtitle": "You can change this later in Settings.",
  "overview.title": "Overview",
  "overview.subtitle": "Current exposure and the local model curve.",
  "overview.e2": "ESTRADIOL",
  "overview.cpa": "CYPROTERONE",
  "overview.t": "TESTOSTERONE",
  "overview.chart_title": "Modeled concentration",
  "overview.chart_subtitle": "E2 curve from the local C99 model. Times are local.",
  "overview.chart_empty": "No doses yet. Add one to see the curve.",
  "overview.recent": "Recent doses",
  "overview.recent_empty": "No doses recorded for this mode.",
  "overview.add_cta": "Add dose",
  "dose.add_title": "Add dose",
  "dose.edit_title": "Edit dose",
  "dose.subtitle": "Record a dose; the local model will update the curve.",
  "dose.route": "Route",
  "dose.ester": "Ester",
  "dose.amount": "Dose (mg)",
  "dose.days_ago": "Days ago",
  "dose.sublingual_tier": "Sublingual tier",
  "dose.theta": "Custom hold theta (optional)",
  "dose.theta_placeholder": "e.g. 0.08",
  "dose.gel_site": "Application site",
  "dose.release_rate": "Release rate (µg/day)",
  "dose.release_placeholder": "e.g. 100",
  "dose.wear": "Planned wear (hours, optional)",
  "dose.wear_placeholder": "e.g. 84",
  "dose.save": "Save changes",
  "dose.add": "Add dose",
  "dose.error_days": "Days ago must be a number.",
  "route.oral": "Oral",
  "route.sublingual": "Sublingual",
  "route.injection": "Injection",
  "route.gel": "Gel",
  "route.patchApply": "Patch (apply)",
  "route.patchRemove": "Patch (remove)",
  "ester.E2": "E2",
  "ester.EV": "EV",
  "ester.EB": "EB",
  "ester.EC": "EC",
  "ester.EN": "EN",
  "ester.EU": "EU",
  "ester.CPA": "CPA",
  "ester.T": "T",
  "ester.TC": "TC",
  "ester.TE": "TE",
  "ester.TU": "TU",
  "tier.quick": "Quick",
  "tier.casual": "Casual",
  "tier.standard": "Standard",
  "tier.strict": "Strict",
  "site.arm": "Arm",
  "site.thigh": "Thigh",
  "site.scrotal": "Scrotal",
  "history.title": "History",
  "history.subtitle": "Doses recorded for %1 mode. Edit or delete an entry.",
  "history.count": "%1 records",
  "history.empty_title": "No doses yet",
  "history.empty_body": "Record your first dose and it will appear here.",
  "history.delete_title": "Delete this dose?",
  "history.delete_body": "This can't be undone.",
  "settings.title": "Settings",
  "settings.subtitle": "Shared body and model settings. Changes apply to both modes.",
  "settings.body_section": "Body",
  "settings.weight": "Body weight (kg)",
  "settings.weight_placeholder": "e.g. 70",
  "settings.birth_year": "Birth year",
  "settings.birth_placeholder": "e.g. 1990; empty to unset",
  "settings.model_section": "Estimation model",
  "settings.calibration_method": "Calibration method",
  "settings.calibration_history": "Calibration history",
  "settings.pk_overrides": "PK parameter overrides (JSON)",
  "settings.save": "Save settings",
  "settings.appearance_section": "Language & appearance",
  "settings.about_section": "About",
  "settings.version": "Version",
  "settings.database": "Database",
  "settings.database_none": "Not configured",
  "settings.database_ok": "Connected",
  "calibration.mipd": "MIPD",
  "calibration.off": "Off",
  "calibration.ekf": "EKF",
  "calibration.ou_kalman": "OU Kalman",
  "calibration.retrospective": "Retrospective",
  "calibration.forward": "Forward",
  "db.banner": "Database not configured. Run `oyama-hrt-cli db setup` in a terminal.",
  "db.cta_disabled": "Connect the database before adding a dose.",
  "error.amount": "Dose must be greater than zero.",
  "error.days": "Days ago must be a number.",
  "status.no_database": "No database configured. Run `oyama-hrt-cli db setup`.",
  "status.database_error": "Database error: %1",
  "status.invalid_mode": "Invalid mode: %1",
  "status.invalid_mode_plain": "Invalid mode.",
  "status.db_not_connected": "Database is not connected.",
  "status.invalid_dose_time": "Enter a valid dose and time.",
  "status.unsupported_route": "Unsupported route or ester.",
  "status.dose_added": "Dose added.",
  "status.add_failed": "Could not add dose: %1",
  "status.no_db_open": "No database is open.",
  "status.dose_positive": "Dose must be greater than zero.",
  "status.invalid_time": "Dose time is invalid.",
  "status.invalid_combination": "Invalid mode, route, or ester.",
  "status.dose_not_found": "Dose not found.",
  "status.dose_updated": "Dose updated.",
  "status.update_failed": "Could not update dose: %1",
  "status.weight_positive": "Body weight must be greater than zero.",
  "status.birth_year_range": "Birth year must be between 1900 and 2200, or 0 to unset.",
  "status.invalid_calibration": "Invalid calibration method.",
  "status.invalid_history": "Invalid calibration history mode.",
  "status.settings_saved": "Settings saved.",
  "status.save_failed": "Could not save settings: %1",
  "status.dose_removed": "Dose removed.",
  "status.remove_failed": "Could not remove dose: %1"
}
```

- [ ] **Step 4: Create `zh.json`, `ru.json`, and `fa.json` with the same key set**

Create `app/i18n/README.md`:

```markdown
# Translations

Translations live in `en.json`, `zh.json`, `ru.json`, and `fa.json`.
Every file must contain exactly the same keys; `_meta` is ignored by the
loader. Chinese is maintained by the author. Russian and Persian are a
first pass by the implementer.

Native speakers: corrections are very welcome. Please open a pull request
that changes only the affected JSON file and keeps the key set identical.
```

Use the existing `zh.json` localisation values in the repository as the source
of truth. Preserve the key set and do not duplicate Chinese strings in this
plan; the translation file is the test fixture and the UI contract.


For Russian and Persian, translate the English values; keep hormone names in their standard medical forms (Эстрадиол / تستوسترون, etc.), keep `E2`, `CPA`, `T`, and units unchanged. Do not translate the values of `_meta`.

- [ ] **Step 5: Implement `UiPreferences`**

Create `app/src/ui_preferences.h`:

```cpp
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

    Q_INVOKABLE QString translate(const QString& key,
                                  const QVariantList& args = {}) const;
    Q_INVOKABLE QVariantMap stringsFor(const QString& code) const;

signals:
    void languageChanged();
    void themeChanged();
    void systemDarkChanged();

private:
    static bool validLanguage(const QString& code);
    static bool validTheme(const QString& mode);
    static QVariantMap loadStrings(const QString& code);
    void loadLanguage(const QString& code);

    QString language_;
    QString theme_{"light"};
    QVariantMap strings_;
    QVariantMap fallbackStrings_;
};
```

Implement in `app/src/ui_preferences.cpp`:

- `validLanguage` accepts only `zh`, `en`, `ru`, `fa`.
- Constructor:
  - `fallbackStrings_ = loadStrings("en")`.
  - Read `ui/language` from `QSettings`. If `OYAMA_UI_LANGUAGE` is one of the four codes, use it and do not persist it. Else use the saved value if valid.
  - If no valid language, `language_` stays empty and `strings_ = fallbackStrings_`; `needsLanguage()` is true.
  - Read `ui/theme` defaulting to `light`; invalid values become `light`. If `OYAMA_UI_THEME` is `light`, `dark`, or `system`, use it without persisting (used by screenshot capture).
  - If `QGuiApplication` exists, connect `QStyleHints::colorSchemeChanged` to `systemDarkChanged` under `#if QT_VERSION >= QT_VERSION_CHECK(6,5,0)`.
- `setLanguage`: ignore invalid or unchanged codes; set `language_`, persist to `QSettings` under `ui/language`, reload strings, emit `languageChanged`.
- `setTheme`: ignore invalid or unchanged modes; set, persist under `ui/theme`, emit `themeChanged`.
- `systemDark`: if a `QGuiApplication` exists and Qt >= 6.5, return `styleHints()->colorScheme() == Qt::ColorScheme::Dark`; otherwise use `QPalette::Window` lightness < 128; if no GUI app, return false.
- `rtl` is `language_ == "fa"`.
- `fontFamily`: `fa` → `Vazirmatn`; `zh` → `Noto Sans CJK SC`; otherwise `Inter`.
- When the language changes, call `QGuiApplication::setFont(QFont(fontFamily(), 11))` if a GUI application exists, so plain `Text`/controls that do not set an explicit family use the language font. Components may still set `font.family: Theme.fontFamily` explicitly.
- `stringsFor`: `loadStrings(code)`, filtering out keys starting with `_`.
- `translate`: look up `strings_`, then `fallbackStrings_`, then return `key`; replace `%1`…`%9` with `args[i].toString()`. Do not use `QString::arg` because positional replacement must support repeated placeholders.
- `loadStrings`: `QFile(":/i18n/" + code + ".json")`, parse with `QJsonDocument::fromJson`, require an object, filter `_meta`.

- [ ] **Step 6: Add `T.qml`**

Create `app/qml/T.qml`:

```qml
pragma Singleton
import QtQuick

QtObject {
    function t(key, args) {
        var _language = Ui.language
        return Ui.translate(key, args || [])
    }

    readonly property bool rtl: Ui.rtl
    readonly property string language: Ui.language
    readonly property int textAlign: Ui.rtl ? Text.AlignRight : Text.AlignLeft
}
```

- [ ] **Step 7: Register `Ui` and wire resources in CMake**

In `CMakeLists.txt`:

```cmake
set(OYAMA_QML_FILES
    app/qml/Main.qml
    app/qml/OverviewPage.qml
    app/qml/AddDosePage.qml
    app/qml/HistoryPage.qml
    app/qml/SettingsPage.qml
    app/qml/T.qml)
set_source_files_properties(app/qml/T.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(oyama-hrt-app URI OyamaHrt VERSION 1.0
    QML_FILES ${OYAMA_QML_FILES})
```

Add each new QML file to `OYAMA_QML_FILES` in the task that creates it. Do not list files before they exist; `qt_add_qml_module` fails at configure time on a missing QML file.

Add `app/src/ui_preferences.cpp` and `.h` to `qt_add_executable(oyama-hrt-app ...)`.

Add a resource helper and call it for the app and the test:

```cmake
function(oyama_add_app_assets target)
    qt_add_resources(${target} "app_i18n"
        PREFIX "/i18n"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/app/i18n"
        FILES
          "${CMAKE_CURRENT_SOURCE_DIR}/app/i18n/en.json"
          "${CMAKE_CURRENT_SOURCE_DIR}/app/i18n/zh.json"
          "${CMAKE_CURRENT_SOURCE_DIR}/app/i18n/ru.json"
          "${CMAKE_CURRENT_SOURCE_DIR}/app/i18n/fa.json")
endfunction()

oyama_add_app_assets(oyama-hrt-app)
```

Font resources are added to this same function in Task 3, after the font files exist.

Add the test target:

```cmake
qt_add_executable(ui_preferences_test
    app/tests/ui_preferences_test.cpp
    app/src/ui_preferences.cpp
    app/src/ui_preferences.h)
target_include_directories(ui_preferences_test PRIVATE app/src)
target_link_libraries(ui_preferences_test PRIVATE Qt6::Core Qt6::Gui)
oyama_add_app_assets(ui_preferences_test)
add_test(NAME ui_preferences_test COMMAND ui_preferences_test)
set_tests_properties(ui_preferences_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

The font files are created in Task 3. Do not add placeholder fonts; the Task 1 test and build must pass using only the i18n resources.

- [ ] **Step 8: Register the singleton in `main.cpp`**

Add:

```cpp
#include "ui_preferences.h"
#include <QtQml/qqml.h>

UiPreferences ui;
qmlRegisterSingletonInstance("OyamaHrt", 1, 0, "Ui", &ui);
```

Place `UiPreferences ui;` before `QQmlApplicationEngine engine;` and register before `engine.loadFromModule(...)`.

- [ ] **Step 9: Build and run the test**

Run:
```sh
cmake --build build-release --target ui_preferences_test
ctest --test-dir build-release -R ui_preferences_test --output-on-failure
```
Expected: `ui_preferences_test: ok`, 1/1 passed. If it fails only because fonts are missing, complete Task 3 Step 3 before re-running.

- [ ] **Step 10: Commit**

```sh
git add app/i18n app/src/ui_preferences.h app/src/ui_preferences.cpp \
  app/qml/T.qml app/tests/ui_preferences_test.cpp app/src/main.cpp CMakeLists.txt
git commit -m "feat(ui): add runtime locale preferences and translations"
```

---

### Task 2: Localized `AppController` status keys

**Files:**
- Modify: `app/src/app_controller.h`
- Modify: `app/src/app_controller.cpp`
- Modify: `app/tests/app_controller_test.cpp`

**Interfaces:**
- Consumes: existing `status`, `fail`, all action methods.
- Produces:
  - `Q_PROPERTY(QString statusKey READ statusKey NOTIFY changed)`
  - `Q_PROPERTY(QStringList statusArgs READ statusArgs NOTIFY changed)`
  - private `void setStatus(const QString& key, const QString& message, const QStringList& args = {});`
  - private `void fail(const QString& key, const QString& message, const QStringList& args = {});`

- [ ] **Step 1: Extend the controller test with failing key assertions**

In `app/tests/app_controller_test.cpp`, after the initial status check:

```cpp
if (controller.statusKey() != "status.connected" ||
    controller.statusArgs().size() != 2) {
    return fail("connected status key/args are wrong");
}
```

After `saveSettings` succeeds:

```cpp
if (controller.statusKey() != "status.settings_saved") {
    return fail("settings status key is wrong");
}
```

Before the valid `addDose`, add:

```cpp
if (controller.addDose("oral", "E2", 0.0, now - 24.0, {}) ||
    controller.statusKey() != "status.invalid_dose_time") {
    return fail("invalid dose did not set the expected status key");
}
```

After the valid add:

```cpp
if (controller.statusKey() != "status.dose_added") {
    return fail("add status key is wrong");
}
```

Before the valid `updateDose`, add:

```cpp
if (controller.updateDose(id, "sublingual", "EV", 0.0, now - 24.0, {}) ||
    controller.statusKey() != "status.dose_positive") {
    return fail("invalid update did not set the expected status key");
}
```

After the valid update:

```cpp
if (controller.statusKey() != "status.dose_updated") {
    return fail("update status key is wrong");
}
```

After `removeDose`:

```cpp
if (controller.statusKey() != "status.dose_removed") {
    return fail("remove status key is wrong");
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `PG_BINDIR=/tmp/opencode/pg18/bin ctest --test-dir build-release -R app_controller_test --output-on-failure`.
Expected: compile fails because `statusKey`/`statusArgs` do not exist.

- [ ] **Step 3: Add the properties and helpers**

In `app/src/app_controller.h`:

```cpp
Q_PROPERTY(QString statusKey READ statusKey NOTIFY changed)
Q_PROPERTY(QStringList statusArgs READ statusArgs NOTIFY changed)
QString statusKey() const { return status_key_; }
QStringList statusArgs() const { return status_args_; }
```

Add private:

```cpp
void setStatus(const QString& key, const QString& message,
               const QStringList& args = {});
void fail(const QString& key, const QString& message,
          const QStringList& args = {});
QString status_key_;
QStringList status_args_;
```

Replace the existing `void fail(const QString& message);` declaration. Add `#include <QStringList>` if needed.

- [ ] **Step 4: Implement `setStatus` and update every call**

In `app/src/app_controller.cpp`:

```cpp
void AppController::setStatus(const QString& key, const QString& message,
                              const QStringList& args) {
    status_key_ = key;
    status_args_ = args;
    status_ = message;
}

void AppController::fail(const QString& key, const QString& message,
                         const QStringList& args) {
    setStatus(key, message, args);
    doses_.clear();
    curve_.clear();
    current_e2_ = 0.0;
    current_cpa_ = 0.0;
    current_t_ = 0.0;
    emit changed();
}
```

Map every message exactly:

| Existing message | New call |
|---|---|
| `No database configured...` | `fail("status.no_database", "No database configured. Run `oyama-hrt-cli db setup`.")` |
| `Connected to <db> via <backend>` | `setStatus("status.connected", "Connected to " + db + " via " + backend, {db, backend})` |
| `Invalid mode: <mode>` | `fail("status.invalid_mode", "Invalid mode: " + mode_, {mode_})` |
| `Database error: <what>` | `fail("status.database_error", "Database error: " + what, {what})` |
| `Database is not connected.` | `fail("status.db_not_connected", "Database is not connected.")` |
| `Enter a valid dose and time.` | `fail("status.invalid_dose_time", "Enter a valid dose and time.")` |
| `Invalid mode.` | `fail("status.invalid_mode_plain", "Invalid mode.")` |
| `Unsupported route or ester.` | `fail("status.unsupported_route", "Unsupported route or ester.")` |
| `Dose added.` | `setStatus("status.dose_added", "Dose added.")` |
| `Could not add dose: <what>` | `fail("status.add_failed", "Could not add dose: " + what, {what})` |
| `No database is open.` | `fail("status.no_db_open", "No database is open.")` |
| `Dose must be greater than zero.` | `fail("status.dose_positive", "Dose must be greater than zero.")` |
| `Dose time is invalid.` | `fail("status.invalid_time", "Dose time is invalid.")` |
| `Invalid mode, route, or ester.` | `fail("status.invalid_combination", "Invalid mode, route, or ester.")` |
| `Dose not found.` | `fail("status.dose_not_found", "Dose not found.")` |
| `Dose updated.` | `setStatus("status.dose_updated", "Dose updated.")` |
| `Could not update dose: <what>` | `fail("status.update_failed", "Could not update dose: " + what, {what})` |
| `Body weight must be greater than zero.` | `fail("status.weight_positive", "Body weight must be greater than zero.")` |
| `Birth year must be between...` | `fail("status.birth_year_range", "Birth year must be between 1900 and 2200, or 0 to unset.")` |
| `Invalid calibration method.` | `fail("status.invalid_calibration", "Invalid calibration method.")` |
| `Invalid calibration history mode.` | `fail("status.invalid_history", "Invalid calibration history mode.")` |
| `Settings saved.` | `setStatus("status.settings_saved", "Settings saved.")` |
| `Could not save settings: <what>` | `fail("status.save_failed", "Could not save settings: " + what, {what})` |
| `Dose removed.` | `setStatus("status.dose_removed", "Dose removed.")` |
| `Could not remove dose: <what>` | `fail("status.remove_failed", "Could not remove dose: " + what, {what})` |

Keep `status_` English exactly as before. In `reload()`, the success assignment becomes `setStatus("status.connected", ...)`. In `addDose`/`updateDose`/`saveSettings`/`removeDose`, replace `status_ = "...";` with the `setStatus(...)` call; keep `reload()` before it.

- [ ] **Step 5: Run the controller test**

Run: `PG_BINDIR=/tmp/opencode/pg18/bin ctest --test-dir build-release -R app_controller_test --output-on-failure`.
Expected: 1/1 passed; existing English status assertions still pass.

- [ ] **Step 6: Commit**

```sh
git add app/src/app_controller.h app/src/app_controller.cpp app/tests/app_controller_test.cpp
git commit -m "feat(app): expose localized status keys"
```

---

### Task 3: Theme, fonts, and icon system

**Files:**
- Create: `app/qml/Theme.qml`
- Create: `app/qml/Icon.qml`
- Create: `app/fonts/InterVariable.ttf`
- Create: `app/fonts/Vazirmatn-Regular.ttf`, `Vazirmatn-Medium.ttf`, `Vazirmatn-SemiBold.ttf`
- Create: `app/fonts/OFL.txt`
- Create: `THIRD_PARTY_NOTICES.md`
- Modify: `CMakeLists.txt`
- Modify: `app/src/main.cpp`
- Modify: `app/tests/ui_preferences_test.cpp` (add font resource assertions here)

**Interfaces:**
- Consumes: `Ui.theme`, `Ui.systemDark`, `Ui.fontFamily`.
- Produces: `Theme` singleton properties listed below; `Icon` with `name`, `size`, `color`, `strokeWidth`.

- [ ] **Step 1: Add failing font resource assertions and CMake entries**

In `app/tests/ui_preferences_test.cpp`, before the final `std::cout`, add:

```cpp
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
```

Extend `oyama_add_app_assets` in `CMakeLists.txt` with:

```cmake
    qt_add_resources(${target} "app_fonts"
        PREFIX "/fonts"
        BASE "${CMAKE_CURRENT_SOURCE_DIR}/app/fonts"
        FILES
          "${CMAKE_CURRENT_SOURCE_DIR}/app/fonts/InterVariable.ttf"
          "${CMAKE_CURRENT_SOURCE_DIR}/app/fonts/Vazirmatn-Regular.ttf"
          "${CMAKE_CURRENT_SOURCE_DIR}/app/fonts/Vazirmatn-Medium.ttf"
          "${CMAKE_CURRENT_SOURCE_DIR}/app/fonts/Vazirmatn-SemiBold.ttf")
```

Run: `cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DOYAMA_BUILD_QT_SHELL=ON -DCMAKE_INSTALL_PREFIX="$HOME/oyama-hrt-dist"`.
Expected: configure fails because the font files do not exist, proving the resource wiring is active.

- [ ] **Step 2: Add the font files and re-run the test**

Download the fonts (the URLs return 302 to the current release):

```sh
mkdir -p app/fonts
curl -L --fail -o app/fonts/InterVariable.ttf \
  'https://github.com/google/fonts/raw/main/ofl/inter/Inter%5Bopsz,wght%5D.ttf'
for w in Regular Medium SemiBold; do
  curl -L --fail -o "app/fonts/Vazirmatn-$w.ttf" \
    "https://github.com/rastikerdar/vazirmatn/raw/master/fonts/ttf/Vazirmatn-$w.ttf"
done
curl -L --fail -o app/fonts/OFL.txt \
  'https://raw.githubusercontent.com/rastikerdar/vazirmatn/master/OFL.txt'
```

If the Inter URL fails, copy `/usr/share/fonts/inter/InterVariable.ttf` on Linux as a fallback. Verify with `file app/fonts/*.ttf`; each must be a TrueType font. Then:

```sh
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DOYAMA_BUILD_QT_SHELL=ON -DCMAKE_INSTALL_PREFIX="$HOME/oyama-hrt-dist"
cmake --build build-release --target ui_preferences_test
ctest --test-dir build-release -R ui_preferences_test --output-on-failure
```

Expected: configure succeeds; test 1/1 passed.

- [ ] **Step 3: Create `Theme.qml`**

```qml
pragma Singleton
import QtQuick

QtObject {
    readonly property bool dark: Ui.theme === "dark" ||
        (Ui.theme === "system" && Ui.systemDark)

    readonly property color bg: dark ? "#1F1D1A" : "#EFE6D0"
    readonly property color sidebar: dark ? "#1B1917" : "#F5EDDF"
    readonly property color surface: dark ? "#2B2823" : "#FFFDF7"
    readonly property color surfaceAlt: dark ? "#35312B" : "#EFE9D8"
    readonly property color surfaceHover: dark ? "#302C27" : "#F2E8D6"
    readonly property color surfacePressed: dark ? "#3A352E" : "#E7DCC6"
    readonly property color text: dark ? "#E6E0D4" : "#33302A"
    readonly property color textSoft: dark ? "#A39C8D" : "#7A7466"
    readonly property color textMuted: dark ? "#A39C8D" : "#6E6759"
    readonly property color textFaint: dark ? "#9A9385" : "#6B6457"
    readonly property color textDisabled: dark ? "#6F695D" : "#A69E8E"
    readonly property color border: dark ? "#3D3830" : "#DED3BD"
    readonly property color accent: dark ? "#C98F52" : "#B5793C"
    readonly property color accentHover: dark ? "#D9A268" : "#A66B31"
    readonly property color accentSubtle: dark ? "#3A2F23" : "#F3E3CC"
    readonly property color accentInk: dark ? "#DFA96F" : "#8C5A28"
    readonly property color green: dark ? "#7FA98A" : "#4F7A5A"
    readonly property color brick: dark ? "#D07A62" : "#B4573E"
    readonly property color onAccent: dark ? "#1F1D1A" : "#2B241B"
    readonly property color chartLine: accent
    readonly property color chartGrid: dark ? "#3A352E" : "#E4D8C2"
    readonly property color shadow: dark ? "#59000000" : "#0F3C2D14"

    readonly property int radiusCard: 18
    readonly property int radiusControl: 12
    readonly property int radiusPill: 999

    readonly property int fontTitle: 28
    readonly property int fontSection: 20
    readonly property int fontBody: 15
    readonly property int fontLabel: 13
    readonly property int fontCaption: 11

    readonly property int durationHover: 150
    readonly property int durationPage: 280
    readonly property int durationTheme: 240
    readonly property int durationSuccess: 320

    readonly property string fontFamily: Ui.fontFamily
    readonly property int pageMargin: 40
    readonly property int cardPadding: 20
}
```

- [ ] **Step 4: Create `Icon.qml`**

Use a 24×24 viewBox and `Shape` + `PathSvg`; scale the shape to `size`, compensate stroke width so the visible stroke stays at `strokeWidth`:

```qml
import QtQuick
import QtQuick.Shapes

Item {
    id: root
    property string name: ""
    property real size: 20
    property color color: Theme.textMuted
    property real strokeWidth: 1.75
    implicitWidth: size
    implicitHeight: size

    readonly property var paths: ({
        "home": "M3 11 12 3l9 8v9a1 1 0 0 1-1 1h-5v-6H9v6H4a1 1 0 0 1-1-1z",
        "plus-circle": "M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18zM12 8v8M8 12h8",
        "list": "M8 6h13M8 12h13M8 18h13M3 6h.01M3 12h.01M3 18h.01",
        "sliders": "M4 7h10M18 7h2M4 17h2M10 17h10M14 4v6M6 14v6",
        "sun": "M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8zM12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4",
        "moon": "M20 14.5A8.5 8.5 0 0 1 9.5 4a8.5 8.5 0 1 0 10.5 10.5z",
        "globe": "M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18zM3 12h18M12 3c2.5 2.5 3.5 5.5 3.5 9s-1 6.5-3.5 9c-2.5-2.5-3.5-5.5-3.5-9S9.5 5.5 12 3z",
        "check": "M4 12l5 5L20 6",
        "chevron-right": "M9 6l6 6-6 6",
        "chevron-down": "M6 9l6 6 6-6",
        "pill": "M10.5 20.5a6 6 0 0 1-8.5-8.5l6-6a6 6 0 0 1 8.5 8.5zM8 8l8 8",
        "pencil": "M4 20h4L19 9l-4-4L4 16zM14 6l4 4",
        "trash": "M4 7h16M9 7V4h6v3M6 7l1 14h10l1-14M10 11v6M14 11v6",
        "x": "M6 6l12 12M18 6L6 18",
        "alert": "M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18zM12 8v5M12 16h.01",
        "info": "M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18zM12 11v5M12 8h.01",
        "database": "M4 6c0-1.7 3.6-3 8-3s8 1.3 8 3-3.6 3-8 3-8-1.3-8-3zM4 6v6c0 1.7 3.6 3 8 3s8-1.3 8-3V6M4 12v6c0 1.7 3.6 3 8 3s8-1.3 8-3v-6",
        "syringe": "M18 2l4 4M17 3l4 4M19 7l-9 9M14 6l4 4M13 9l-7 7v4h4l7-7",
        "droplet": "M12 3s6 6.5 6 11a6 6 0 0 1-12 0c0-4.5 6-11 6-11z"
    })

    Shape {
        width: 24
        height: 24
        transform: Scale {
            origin.x: 0
            origin.y: 0
            xScale: root.size / 24
            yScale: root.size / 24
        }
        ShapePath {
            strokeColor: root.color
            strokeWidth: root.strokeWidth * 24 / root.size
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.paths[root.name] || "" }
        }
    }
}
```

Add `rotation` support by wrapping the Shape in an `Item` if needed for RTL chevrons; pages can set `rotation: T.rtl ? 180 : 0`.

Add `app/qml/Theme.qml` and `app/qml/Icon.qml` to `OYAMA_QML_FILES`, and add:

```cmake
set_source_files_properties(app/qml/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
```

- [ ] **Step 5: Load fonts in `main.cpp`**

After `QGuiApplication` is created and before creating the engine:

```cpp
#include <QFontDatabase>
...
for (const QString& path : {QStringLiteral(":/fonts/InterVariable.ttf"),
                            QStringLiteral(":/fonts/Vazirmatn-Regular.ttf"),
                            QStringLiteral(":/fonts/Vazirmatn-Medium.ttf"),
                            QStringLiteral(":/fonts/Vazirmatn-SemiBold.ttf")}) {
    if (QFontDatabase::addApplicationFont(path) < 0) {
        qWarning() << "Could not load font" << path;
    }
}
```

Also set `QGuiApplication::setApplicationVersion("0.5.0");` (or a compile definition `OYAMA_APP_VERSION` from `PROJECT_VERSION`).

- [ ] **Step 6: Add third-party notices**

Create `THIRD_PARTY_NOTICES.md` with sections for Inter (SIL Open Font License 1.1), Vazirmatn (SIL OFL 1.1), and Lucide-style icon paths (ISC). Include the license text or a link plus the copyright line. Add `app/fonts/OFL.txt`.

- [ ] **Step 7: Build, run the test, and lint**

Run:
```sh
cmake --build build-release --target ui_preferences_test oyama-hrt-app
ctest --test-dir build-release -R ui_preferences_test --output-on-failure
/usr/lib64/qt6/bin/qmllint -I app/qml app/qml/Theme.qml app/qml/Icon.qml
```
Expected: test 1/1 passed; `qmllint` reports no errors. Warnings about `Ui` being a C++ singleton and `app` being a context property are acceptable.

- [ ] **Step 8: Commit**

```sh
git add app/qml/Theme.qml app/qml/Icon.qml app/fonts THIRD_PARTY_NOTICES.md \
  app/src/main.cpp CMakeLists.txt
git commit -m "feat(ui): add warm theme, bundled fonts, and line icons"
```

---

### Task 4: Base components

**Files:**
- Create: `app/qml/Card.qml`
- Create: `app/qml/NavItem.qml`
- Create: `app/qml/AppButton.qml`
- Create: `app/qml/AppTextField.qml`
- Create: `app/qml/AppComboBox.qml`
- Create: `app/qml/PageHeader.qml`
- Create: `app/qml/StatCard.qml`
- Create: `app/qml/EmptyState.qml`
- Create: `app/qml/Toast.qml`
- Create: `app/qml/ConfirmDialog.qml`
- Modify: `CMakeLists.txt` (append these files to `OYAMA_QML_FILES`)

**Interfaces:**
- `Card`: `default property alias content`, `padding`, `spacing`, `fill`, `hoverable`.
- `NavItem`: `text`, `iconName`, `selected`, `signal clicked()`.
- `AppButton`: `variant` (`primary|secondary|ghost|danger`), `iconName`, `busy`; inherits `Button` (`text`, `enabled`, `clicked`).
- `AppTextField`: inherits `TextField`; 44px height, token colors, focus ring.
- `AppComboBox`: inherits `ComboBox`; `textRole`, `valueRole`, `currentValue`, `indexOfValue(value)`.
- `PageHeader`: `title`, `subtitle`, `default property alias extra` (right-side actions).
- `StatCard`: `label`, `value`, `unit`, `accentColor`.
- `EmptyState`: `iconName`, `title`, `body`, `actionText`, `actionEnabled`, `signal actionClicked()`.
- `Toast`: `show(message, isError)`, `hide()`; anchored by the caller.
- `ConfirmDialog`: `title`, `body`, `confirmText`; `open()`, `close()`; signals `confirmed()`, `cancelled()`.

- [ ] **Step 1: Append the component files to CMake and create `Card.qml`**

Append all ten new component paths to `OYAMA_QML_FILES` before building.

Use a `Rectangle` with `color: Theme.surface`, `radius: Theme.radiusCard`, and a `ColumnLayout` filling the card with `padding`/`spacing`:

```qml
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    default property alias content: layout.data
    property int padding: Theme.cardPadding
    property int spacing: 12
    property color fill: Theme.surface
    property bool hoverable: false
    implicitWidth: 320
    color: fill
    radius: Theme.radiusCard
    Behavior on color { ColorAnimation { duration: Theme.durationTheme } }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: root.padding
        spacing: root.spacing
    }
}
```

- [ ] **Step 2: Create `NavItem.qml`**

Use `Button` with `hoverEnabled: true`, `focusPolicy: Qt.StrongFocus`, height 48, radius 12, and a `RowLayout` content:

- icon 20px, color `selected ? Theme.accent : Theme.textMuted`;
- text 15px, weight `selected ? Font.Medium : Font.Normal`, color `selected || hovered ? Theme.text : Theme.textMuted`;
- background `selected ? Theme.surfacePressed : hovered ? Theme.surfaceHover : "transparent"`;
- `scale: down ? 0.98 : 1` with 90ms `Easing.OutCubic`;
- `Accessible.name: text`.

- [ ] **Step 3: Create `AppButton.qml`**

Use `Button` with `implicitHeight: 44`, `padding: 0`, `hoverEnabled: true`. Background radius `Theme.radiusControl`; primary uses `accent`/`accentHover`, secondary `surfaceAlt`/`surfaceHover`, ghost transparent/`surfaceHover`, danger transparent with `brick` text. Content is a `Row` with optional `Icon` and `Text` at 15px/500. Press scale 0.97 with 90ms. Focus ring 2px `Theme.accent` via `visualFocus`.

- [ ] **Step 4: Create `AppTextField.qml`**

```qml
import QtQuick
import QtQuick.Controls

TextField {
    id: root
    implicitHeight: 44
    leftPadding: 14
    rightPadding: 14
    color: Theme.text
    placeholderTextColor: Theme.textFaint
    selectionColor: Theme.accent
    selectedTextColor: Theme.onAccent
    font.pixelSize: Theme.fontBody
    font.family: Theme.fontFamily
    background: Rectangle {
        radius: Theme.radiusControl
        color: Theme.surfaceAlt
        border.width: root.activeFocus ? 1.5 : 0
        border.color: Theme.accent
        Behavior on color { ColorAnimation { duration: Theme.durationTheme } }
    }
}
```

- [ ] **Step 5: Create `AppComboBox.qml`**

Use `ComboBox` with a 44px background, `AppTextField`-style colors, a `chevron-down` indicator rotating 180° when the popup is visible, and a custom `Popup`:

- popup width = combo width, y = combo height + 4, padding 4, background `surfaceAlt`, radius 12, 1px `border`;
- `ListView` with `delegate: ItemDelegate` 36px high, text 15px, radius 8, hovered background `surfaceHover`, selected text weight 500;
- `ScrollIndicator.vertical`;
- expose:

```qml
function indexOfValue(value) {
    for (var i = 0; i < count; ++i) {
        var v = valueRole ? model[i][valueRole] : model[i]
        if (v === value) return i
    }
    return 0
}
```

The page code uses `currentValue`, which `ComboBox` provides when `valueRole` is set.

- [ ] **Step 6: Create `PageHeader.qml`**

A `ColumnLayout`/`RowLayout` with title 28/600, subtitle 13/`textMuted`, and a right-side `RowLayout` exposed as `default property alias extra: extraRow.data`. Set `Layout.fillWidth: true` in pages. Keep title/subtitle baseline aligned; no bottom border.

- [ ] **Step 7: Create `StatCard.qml`, `EmptyState.qml`**

`StatCard` is a `Card`-like `Rectangle`: label 11 uppercase with `letterSpacing: 0.6`, value 32/600, unit 13 with `accentColor`. Height 112, padding 20, radius 18.

`EmptyState` is a centered `ColumnLayout` with a 64px `accentSubtle` circle containing a 40px `Icon`, title 20/600, body 15/`textMuted` (max width 360, wrap), and an `AppButton` when `actionText` is non-empty. Disable the button when `actionEnabled` is false.

- [ ] **Step 8: Create `Toast.qml`**

An `Item` anchored by the caller. A `Rectangle` with `color: Theme.surfaceAlt`, radius 12, 1px `border`, padding 12/16, a 18px icon (`check` green or `alert` brick), and 13px text. Enter: opacity 0→1 and y 8→0 over 220ms `OutCubic`; exit 160ms. Auto-hide after 4s. `show(message, isError)` sets text, visible, restarts the timer. `Accessible.name: text`.

- [ ] **Step 9: Create `ConfirmDialog.qml`**

An `Item` filling the window, `visible: false`, `z: 100`. Scrim `#66000000`; card 360px wide, `surface`, radius 18, padding 24; title 20/600, body 15/`textMuted`, buttons `Cancel` and `Delete` (danger). `open()` sets visible and focuses the cancel button; `close()` hides. Emit `confirmed()` then close; `cancelled()` then close. Close on scrim click and Escape.

- [ ] **Step 10: Lint and smoke-test**

Run:
```sh
/usr/lib64/qt6/bin/qmllint -I app/qml app/qml/Card.qml app/qml/NavItem.qml \
  app/qml/AppButton.qml app/qml/AppTextField.qml app/qml/AppComboBox.qml \
  app/qml/PageHeader.qml app/qml/StatCard.qml app/qml/EmptyState.qml \
  app/qml/Toast.qml app/qml/ConfirmDialog.qml
cmake --build build-release --target oyama-hrt-app
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  OYAMA_UI_LANGUAGE=en OYAMA_CONFIG_FILE=/tmp/oyama-no-config \
  ./build-release/oyama-hrt-app --self-test
```
Expected: lint no errors; app exits 0. Warnings about `app`/`Ui` are acceptable.

- [ ] **Step 11: Commit**

```sh
git add app/qml/Card.qml app/qml/NavItem.qml app/qml/AppButton.qml \
  app/qml/AppTextField.qml app/qml/AppComboBox.qml app/qml/PageHeader.qml \
  app/qml/StatCard.qml app/qml/EmptyState.qml app/qml/Toast.qml \
  app/qml/ConfirmDialog.qml
git commit -m "feat(ui): add warm component library"
```

---

### Task 5: Window shell, sidebar, language picker

**Files:**
- Create: `app/qml/Sidebar.qml`
- Create: `app/qml/LanguagePicker.qml`
- Rewrite: `app/qml/Main.qml`
- Modify: `CMakeLists.txt` (append `Sidebar.qml` and `LanguagePicker.qml` to `OYAMA_QML_FILES`; add test cases)

**Interfaces:**
- Consumes: `app.mode`, `app.backend`, `Ui`, `T`, `Theme`, `Sidebar`, `LanguagePicker`.
- Produces: `Main` properties `pageIndex` (int) and `editingDose` (var); function `goTo(index)`.
- `Sidebar` exposes `property int currentIndex` and `signal navigate(int index)`.
- `LanguagePicker` exposes no public API; it writes `Ui.language`.

- [ ] **Step 1: Write `Sidebar.qml`**

`Rectangle` with `Layout.preferredWidth: 236`, `color: Theme.sidebar`, and a right border `1px Theme.border`. Column:

- Brand row: 36×36 rounded tile (`Theme.accentSubtle` with `Theme.accentInk` letter `O`, 18/600) + `Oyama` (15/600) + `Tracker` (11/`textMuted`); 24px top margin.
- Nav `Repeater` over `[{key:"overview", icon:"home"}, {key:"add_dose", icon:"plus-circle"}, {key:"history", icon:"list"}, {key:"settings", icon:"sliders"}]`; each `NavItem` 48px high, margins 12, text `T.t("nav." + modelData.key)`; `selected: currentIndex === index`; `onClicked: navigate(index)`.
- Spacer.
- Mode segmented control: two `AppButton`s with `text: T.t("mode.transfem")` / `T.t("mode.transmasc")`; active variant primary, inactive ghost; clicking sets `app.mode`.
- Connection row: 8px dot (`app.backend === "none" ? Theme.textFaint : Theme.green`) + 11px `T.t(app.backend === "none" ? "status.not_connected" : "status.local_socket")`.
- Theme toggle: `AppButton { variant: "ghost"; iconName: Theme.dark ? "sun" : "moon"; onClicked: Ui.theme = Theme.dark ? "light" : "dark" }` with `Accessible.name: T.t("theme.label")`.
- Bottom padding 16.

Use `LayoutMirroring.enabled: T.rtl` and `LayoutMirroring.childrenInherit: true` on the sidebar; the parent `RowLayout` will place it on the right.

- [ ] **Step 2: Write `LanguagePicker.qml`**

`Rectangle` filling the window, `color: Theme.bg`, `z: 50`, `opacity: Ui.needsLanguage ? 1 : 0`, `visible: opacity > 0`, `Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }`.

Center a 720px-wide `ColumnLayout`:

- Brand tile 56px, title `T.t("language.choose_title")` 28/600, subtitle `T.t("language.choose_subtitle")` 15/`textMuted`.
- `GridLayout` 2 columns, spacing 12; `Repeater` over `Ui.availableLanguages`; each `AppButton` variant secondary, `implicitHeight: 88`, and an overridden `contentItem` (`AppButton` defines a Row by default, so replace it with a `ColumnLayout`): native name 20/600, English name 13/`textMuted`, sample glyph 28/`accentInk`; on click `Ui.language = modelData.code`.
- `Accessible.name: modelData.nativeName`.

The picker has no close button; the user must choose. `OYAMA_UI_LANGUAGE` bypasses it for tests.

- [ ] **Step 3: Rewrite `Main.qml`**

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1200
    height: 800
    minimumWidth: 980
    minimumHeight: 680
    visible: true
    title: T.t("app.name")
    color: Theme.bg
    layoutDirection: T.rtl ? Qt.RightToLeft : Qt.LeftToRight

    property int pageIndex: 0
    property var editingDose: null
    property real pageOpacity: 1
    property real pageOffset: 0

    function goTo(index) {
        if (index === pageIndex) return
        pageOpacity = 0
        pageOffset = 8
        pageIndex = index
        pageAnim.restart()
    }

    ParallelAnimation {
        id: pageAnim
        NumberAnimation { target: window; property: "pageOpacity"; from: 0; to: 1; duration: Theme.durationPage; easing.type: Easing.OutCubic }
        NumberAnimation { target: window; property: "pageOffset"; from: 8; to: 0; duration: Theme.durationPage; easing.type: Easing.OutCubic }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        Sidebar {
            Layout.fillHeight: true
            currentIndex: window.pageIndex
            onNavigate: function(index) {
                if (index === 1) addPage.resetForm()
                window.goTo(index)
            }
        }
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                Rectangle {
                    visible: app.backend === "none"
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.pageMargin
                    Layout.rightMargin: Theme.pageMargin
                    Layout.topMargin: 20
                    height: 48
                    radius: Theme.radiusControl
                    color: Theme.accentSubtle
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        Icon { name: "database"; size: 18; color: Theme.accentInk }
                        Text {
                            Layout.fillWidth: true
                            text: T.t("db.banner")
                            color: Theme.accentInk
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontLabel
                            wrapMode: Text.Wrap
                        }
                    }
                }
                StackLayout {
                    currentIndex: window.pageIndex
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    opacity: window.pageOpacity
                    transform: Translate { y: window.pageOffset }
                    OverviewPage { id: overviewPage }
                    AddDosePage {
                        id: addPage
                        onSaved: window.goTo(0)
                    }
                    HistoryPage {
                        onEditRequested: function(dose) {
                            window.editingDose = dose
                            addPage.loadDose(dose)
                            window.goTo(1)
                        }
                        onAddRequested: window.goTo(1)
                    }
                    SettingsPage {}
                }
            }
        }
    }

    LanguagePicker { anchors.fill: parent }
    Toast { id: toast; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 24 }
}
```

`OverviewPage`, `HistoryPage`, and `SettingsPage` need `Layout.fillWidth/Height` inside `StackLayout`; the pages set these themselves.

- [ ] **Step 4: Add locale smoke tests**

In `CMakeLists.txt`, after the existing `qt_shell_smoke` test:

```cmake
foreach(locale en zh ru fa)
    add_test(NAME qt_shell_smoke_${locale}
        COMMAND oyama-hrt-app --self-test)
    set_tests_properties(qt_shell_smoke_${locale} PROPERTIES
        ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;OYAMA_UI_LANGUAGE=${locale};OYAMA_CONFIG_FILE=${CMAKE_CURRENT_BINARY_DIR}/qt-shell-smoke-no-config")
endforeach()
add_test(NAME qt_shell_smoke_first_run
    COMMAND oyama-hrt-app --self-test)
set_tests_properties(qt_shell_smoke_first_run PROPERTIES
    ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/qt-first-run;OYAMA_CONFIG_FILE=${CMAKE_CURRENT_BINARY_DIR}/qt-shell-smoke-no-config")
```

`${CMAKE_CURRENT_BINARY_DIR}/qt-first-run` may be reused; remove it at configure time with `file(REMOVE_RECURSE ...)` if you want a guaranteed clean first run.

- [ ] **Step 5: Run smoke tests**

Run:
```sh
cmake --build build-release --target oyama-hrt-app
ctest --test-dir build-release -R 'qt_shell_smoke' --output-on-failure
```
Expected: `qt_shell_smoke`, `qt_shell_smoke_en/zh/ru/fa`, and `qt_shell_smoke_first_run` all pass. If `Main.qml` references pages that are not rewritten yet, keep the old page files temporarily; the new shell must still load.

- [ ] **Step 6: Commit**

```sh
git add app/qml/Main.qml app/qml/Sidebar.qml app/qml/LanguagePicker.qml CMakeLists.txt
git commit -m "feat(ui): add warm shell, sidebar, and language picker"
```

---

### Task 6: Overview page

**Files:**
- Rewrite: `app/qml/OverviewPage.qml`

**Interfaces:**
- Consumes: `app.currentE2`, `app.currentCPA`, `app.currentT`, `app.curve`, `app.doses`, `app.backend`, `app.mode`, `app.formatTime`.
- Produces: no new C++ interface.

- [ ] **Step 1: Rewrite the page structure**

`Item` with `Layout.fillWidth/Height: true`. `ColumnLayout` margins `Theme.pageMargin`, spacing 24:

1. `PageHeader { title: T.t("overview.title"); subtitle: T.t("overview.subtitle"); Layout.fillWidth: true }`.
2. `RowLayout` of three `StatCard`s, each `Layout.fillWidth: true`:
   - E2: `label: T.t("overview.e2")`, `value: app.currentE2.toFixed(1)`, `unit: "pg/mL"`, `accentColor: Theme.accent`.
   - CPA: `label: T.t("overview.cpa")`, `value: app.currentCPA.toFixed(2)`, `unit: "ng/mL"`, `accentColor: Theme.green`.
   - T: `label: T.t("overview.t")`, `value: app.currentT.toFixed(1)`, `unit: "ng/dL"`, `accentColor: Theme.brick`.
3. Chart `Card` with `Layout.fillWidth: true`, height 420:
   - Header row: title 20/600 `T.t("overview.chart_title")`; subtitle 13/`textMuted` `T.t("overview.chart_subtitle")`.
   - `Canvas` height 320, `Layout.fillWidth: true`, `onPaint` draws the E2 curve.
   - If `app.curve.length < 2`, show a centered `ColumnLayout` with `T.t("overview.chart_empty")` instead of the Canvas.
4. Recent `Card`:
   - Header row: title 20/600 `T.t("overview.recent")`; `AppButton` ghost `T.t("common.view_all")` → navigate to History. To navigate, add `signal viewAllRequested()` and have `Main.qml` connect it to `goTo(2)`.
   - If `app.doses.length === 0`, show `EmptyState` with `iconName: "pill"`, `title: T.t("overview.recent_empty")`, `body: ""`, `actionText: T.t("overview.add_cta")`, `actionEnabled: app.backend !== "none"`, `onActionClicked: addRequested()`; add `signal addRequested()` and connect in `Main.qml` to `goTo(1)`.
   - Else `Repeater model: Math.min(app.doses.length, 5)` with a 56px row: pill icon in `accentSubtle` 36px circle, route · ester 15, time 13/`textMuted`, dose `Number(...).toFixed(2) + " mg"` 15/`accent`.

- [ ] **Step 2: Implement the Canvas paint**

Use `app.curve` points; each has `t`, `e2`. Compute `minT`, `maxT` from first/last; `maxE2 = max(10, ceil(max e2 / 10) * 10)`. Draw:

- clear with `Theme.surface`;
- 4 horizontal grid lines `Theme.chartGrid`, 1px;
- y labels at 0, 25%, 50%, 75%, 100% with `Theme.textMuted`, 11px, right-aligned;
- x labels at start and end using `app.formatTime(points[0].t)` and `app.formatTime(points[last].t)`, 11px;
- curve path `Theme.chartLine`, width 2.5, round caps/joins;
- fill under curve with `Theme.chartLine` at alpha `0.10`, closing to the bottom.

Keep the plot area left-to-right even in RTL. Add:

```qml
Connections { target: app; function onChanged() { chart.requestPaint() } }
Connections { target: Theme; function onDarkChanged() { chart.requestPaint() } }
```

- [ ] **Step 3: Connect navigation signals in `Main.qml`**

Add:

```qml
OverviewPage {
    id: overviewPage
    onViewAllRequested: window.goTo(2)
    onAddRequested: window.goTo(1)
}
```

- [ ] **Step 4: Lint and smoke-test**

Run:
```sh
/usr/lib64/qt6/bin/qmllint -I app/qml app/qml/OverviewPage.qml app/qml/Main.qml
cmake --build build-release --target oyama-hrt-app
ctest --test-dir build-release -R 'qt_shell_smoke' --output-on-failure
```
Expected: lint no errors; all smoke tests pass.

- [ ] **Step 5: Commit**

```sh
git add app/qml/OverviewPage.qml app/qml/Main.qml
git commit -m "feat(ui): rebuild overview with warm cards and E2 chart"
```

---

### Task 7: Add dose page

**Files:**
- Rewrite: `app/qml/AddDosePage.qml`

**Interfaces:**
- Consumes: `app.addDose`, `app.updateDose`, `app.nowHours`, `app.status`, `app.statusKey`, `app.statusArgs`, `app.backend`.
- Produces: `loadDose(dose)`, `resetForm()`, `selectedExtras()`, `save()`, signal `saved()`.

- [ ] **Step 1: Preserve the data functions, change only presentation**

Keep the existing logic for:

- route values: `["oral","sublingual","injection","gel","patchApply","patchRemove"]`;
- ester values: `["E2","EV","EB","EC","EN","EU","CPA","T","TC","TE","TU"]`;
- tier values `[0,1,2,3]`, site values `[0,1,2]`;
- extras keys: `sublingualTier`, `sublingualTheta`, `gelSite`, `releaseRateUGPerDay`, `patchWearH`;
- `timeH = app.nowHours() - days * 24`;
- `app.updateDose(editId, ...)` or `app.addDose(...)`.

Use `AppComboBox` with object models:

```qml
model: [
    { value: "oral", label: T.t("route.oral") },
    ...
]
textRole: "label"
valueRole: "value"
```

`loadDose` sets `routeBox.currentIndex = routeBox.indexOfValue(dose.route)`, etc. `selectedExtras` reads `routeBox.currentValue`, `tierBox.currentValue`, `gelBox.currentValue`.

- [ ] **Step 2: Build the visual form**

`Item` with `Layout.fillWidth/Height: true`, a centered `ColumnLayout` max width 640, margins `Theme.pageMargin`:

- `PageHeader { title: editId ? T.t("dose.edit_title") : T.t("dose.add_title"); subtitle: T.t("dose.subtitle") }`.
- `Card` with:
  - `GridLayout` 2 columns: route, ester, amount, days.
  - Conditional sublingual block: tier ComboBox + theta field (placeholder `dose.theta_placeholder`), fade in with `opacity` + `visible`.
  - Conditional gel block: site ComboBox.
  - Conditional patch-apply block: release rate + wear fields.
  - Error `Text` 13/`brick`, `visible: message !== ""`.
  - Button row: `AppButton` ghost `T.t("common.cancel")` → `resetForm(); saved()`; `AppButton` primary with `text: saveSucceeded ? T.t("common.saved") : (editId ? T.t("dose.save") : T.t("dose.add"))`, `iconName: saveSucceeded ? "check" : ""`, `enabled: !saveSucceeded`, `onClicked: save()`.
- Disable the save button when `app.backend === "none"` and show `T.t("db.cta_disabled")` in the error line.

- [ ] **Step 3: Add the success animation**

Add:

```qml
property bool saveSucceeded: false
Timer {
    id: successTimer
    interval: 850
    onTriggered: {
        page.saveSucceeded = false
        page.saved()
        page.resetForm()
    }
}
```

In `save()`, after a successful `addDose`/`updateDose`, set `saveSucceeded = true`, clear `message`, and `successTimer.restart()`. On failure, set `message = app.statusKey ? T.t(app.statusKey, app.statusArgs) : app.status` and keep the form. During success, the button shows a check and is disabled; the card fades slightly (opacity 0.98→1 not required).

- [ ] **Step 4: Lint and smoke-test**

Run:
```sh
/usr/lib64/qt6/bin/qmllint -I app/qml app/qml/AddDosePage.qml
cmake --build build-release --target oyama-hrt-app
ctest --test-dir build-release -R 'qt_shell_smoke' --output-on-failure
```
Expected: lint no errors; all smoke tests pass.

- [ ] **Step 5: Manual data check**

With the test database configured, add an oral E2 dose, edit it to sublingual EV, and delete it. Verify the history row and curve update. (The existing `app_controller_test` already covers the same calls at the C++ level.)

- [ ] **Step 6: Commit**

```sh
git add app/qml/AddDosePage.qml
git commit -m "feat(ui): rebuild dose form with success feedback"
```

---

### Task 8: History page

**Files:**
- Rewrite: `app/qml/HistoryPage.qml`

**Interfaces:**
- Consumes: `app.doses`, `app.removeDose`, `app.statusKey`, `app.statusArgs`, `app.mode`.
- Produces: signals `editRequested(var dose)` and `addRequested()`.

- [ ] **Step 1: Rewrite the list**

`Item` with `Layout.fillWidth/Height: true`, `ColumnLayout` margins 40, spacing 24:

- `PageHeader { title: T.t("history.title"); subtitle: T.t("history.subtitle", [T.t("mode." + app.mode)]); Layout.fillWidth: true }`.
- Count text `T.t("history.count", [app.doses.length])` 13/`textMuted`.
- `Card` with `ListView`:
  - `model: app.doses`, `clip: true`, `spacing: 4`.
  - Delegate: 64px row, radius 12; hover `surfaceHover`; left 36px `accentSubtle` circle with `pill` icon; route · ester 15; `timeLabel · doseMG mg` 13/`textMuted`; right `AppButton` ghost `iconName: "pencil"` and `iconName: "trash"` 36×36; delete icon color `brick` on hover.
  - Edit → `editRequested(modelData)`.
  - Delete → set `pendingDelete = modelData`, `confirm.open()`.
- Empty state when `app.doses.length === 0`: `EmptyState` with `iconName: "pill"`, `title: T.t("history.empty_title")`, `body: T.t("history.empty_body")`, `actionText: T.t("dose.add")`, `onActionClicked: addRequested()` (the `Main.qml` Task 5 snippet already connects `onAddRequested` to `goTo(1)`).

- [ ] **Step 2: Add the delete confirmation**

Add `ConfirmDialog { id: confirm; title: T.t("history.delete_title"); body: T.t("history.delete_body"); confirmText: T.t("common.delete"); onConfirmed: { app.removeDose(pendingDelete.id); toast.show(...) } }`.

`app.removeDose` sets `statusKey`; show `T.t(app.statusKey, app.statusArgs)` in a page-local `Toast`, or add a `showToast` signal to Main. Simpler: use a page-local `Toast { id: toast; anchors... }` and call `toast.show(...)`.

- [ ] **Step 3: Lint and smoke-test**

Run:
```sh
/usr/lib64/qt6/bin/qmllint -I app/qml app/qml/HistoryPage.qml
cmake --build build-release --target oyama-hrt-app
ctest --test-dir build-release -R 'qt_shell_smoke' --output-on-failure
```
Expected: lint no errors; smoke tests pass.

- [ ] **Step 4: Commit**

```sh
git add app/qml/HistoryPage.qml app/qml/Main.qml
git commit -m "feat(ui): rebuild history with delete confirmation"
```

---

### Task 9: Settings page

**Files:**
- Rewrite: `app/qml/SettingsPage.qml`

**Interfaces:**
- Consumes: `app.weight`, `app.birthYear`, `app.calibrationMethod`, `app.calibrationHistory`, `app.pkParamsJson`, `app.saveSettings`, `app.statusKey`, `app.statusArgs`, `app.backend`, `Ui.availableLanguages`, `Ui.language`, `Ui.theme`.
- Produces: no new C++ interface.

- [ ] **Step 1: Build the grouped cards**

`Item` with `Layout.fillWidth/Height: true`; `Flickable` or `ScrollView` with a centered `ColumnLayout` max width 680, margins 40, spacing 20:

1. `PageHeader { title: T.t("settings.title"); subtitle: T.t("settings.subtitle") }`.
2. `Card` "Body": weight `AppTextField`, birth year `AppTextField`; labels 13/500 above each field; placeholder colors.
3. `Card` "Estimation model": calibration method `AppComboBox` (`mipd`/`off`/`ekf`/`ou_kalman`), history `AppComboBox` (`retrospective`/`forward`), PK JSON `TextArea` in a rounded `surfaceAlt` container, monospace, 150px min height.
4. `Card` "Language & appearance":
   - Language: `RowLayout` of four `AppButton`s (secondary when inactive, primary when active) from `Ui.availableLanguages`; `onClicked: Ui.language = modelData.code`.
   - Theme: three `AppButton`s (`light`/`dark`/`system`), active primary; `onClicked: Ui.theme = "light"` etc.
5. `Card` "About": app name, `T.t("settings.version") + " " + Qt.application.version`, database status `app.backend === "none" ? T.t("settings.database_none") : T.t("settings.database_ok")`.

- [ ] **Step 2: Preserve save behavior**

`loadSettings()` copies the five app properties into the fields. `Connections { target: app; function onChanged() { loadSettings() } }` (guard against overwriting while the user is editing: only reload when `!settingsDirty`; set `settingsDirty` in field `onTextEdited` and clear on save). Validate weight > 0 and JSON object before calling `app.saveSettings(...)`. On failure, show `T.t(app.statusKey, app.statusArgs)`; on success show a `Toast` with `T.t("status.settings_saved")`.

- [ ] **Step 3: Add language/theme persistence check**

Switch to `fa`, `ru`, `zh`, `en`, then toggle light/dark/system. Confirm the layout and colors update immediately and survive a restart. The `UiPreferences` test covers persistence at the data level; this step verifies the QML controls write to it.

- [ ] **Step 4: Lint and smoke-test**

Run:
```sh
/usr/lib64/qt6/bin/qmllint -I app/qml app/qml/SettingsPage.qml
cmake --build build-release --target oyama-hrt-app
ctest --test-dir build-release -R 'qt_shell_smoke' --output-on-failure
```
Expected: lint no errors; all smoke tests pass.

- [ ] **Step 5: Commit**

```sh
git add app/qml/SettingsPage.qml
git commit -m "feat(ui): rebuild settings with language and theme controls"
```

---

### Task 10: Screenshot mode and Linux visual verification

**Files:**
- Modify: `app/src/main.cpp`
- Create: `app/tests/capture_ui.sh`
- Modify: `CMakeLists.txt` (optional screenshot test)

**Interfaces:**
- Produces: CLI flags `--screenshot <path>` and `--page <overview|add|history|settings>`; `capture_ui.sh` writes PNGs.

- [ ] **Step 1: Add screenshot flags**

In `main.cpp`, parse `--screenshot` and `--page` from `app.arguments()`. After the engine loads:

```cpp
if (!screenshotPath.isEmpty()) {
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    const int page = pageIndexFor(pageName);
    QTimer::singleShot(700, &app, [window, page, screenshotPath]() {
        if (window && page >= 0) window->setProperty("pageIndex", page);
        QTimer::singleShot(400, &app, [window, screenshotPath]() {
            if (window) window->grabWindow().save(screenshotPath);
            QCoreApplication::quit();
        });
    });
    return app.exec();
}
```

Include `<QQuickWindow>` and `<QTimer>`. Use `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software` for deterministic output.

- [ ] **Step 2: Create `capture_ui.sh`**

```sh
#!/bin/sh
set -eu
BIN="${1:-./build-release/oyama-hrt-app}"
OUT="${2:-./build-release/screenshots}"
mkdir -p "$OUT"
for lang in en zh ru fa; do
  for theme in light dark; do
    XDG_CONFIG_HOME="$OUT/config-$lang" \
    OYAMA_UI_LANGUAGE="$lang" \
    OYAMA_UI_THEME="$theme" \
    QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
      "$BIN" --screenshot "$OUT/${lang}-${theme}-overview.png" --page overview
  done
done
```

`UiPreferences` already reads `OYAMA_UI_THEME` from Task 1; if the variable is set it overrides the saved theme without persisting.

- [ ] **Step 3: Run screenshots and inspect**

Run:
```sh
chmod +x app/tests/capture_ui.sh
cmake --build build-release --target oyama-hrt-app
app/tests/capture_ui.sh
file build-release/screenshots/*.png
```

Open `en-light-overview.png`, `zh-dark-overview.png`, `ru-light-overview.png`, and `fa-dark-overview.png` with the `read` tool. Check: no black window, no overlapping text, no missing glyphs, sidebar on the right for `fa`, chart repainted.

- [ ] **Step 4: Scan for hardcoded colors**

Run:
```sh
rg -n '#[0-9A-Fa-f]{6}' app/qml --glob '!Theme.qml'
```
Expected: no interface color literals. Acceptable exceptions: `#66000000` scrim in `ConfirmDialog.qml`; move it into `Theme` (`Theme.scrim`) if you prefer zero exceptions.

- [ ] **Step 5: Run the full Linux suite**

Run:
```sh
cmake --build build-release -j"$(nproc)"
PG_BINDIR=/tmp/opencode/pg18/bin ctest --test-dir build-release --output-on-failure
```
Expected: 12 original tests + `ui_preferences_test` + 5 new smoke cases (`en`, `zh`, `ru`, `fa`, `first_run`) = 18 tests, all pass.

- [ ] **Step 6: Install and smoke the installed app**

Run:
```sh
cmake --install build-release
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  timeout 8 "$HOME/oyama-hrt-dist/bin/oyama-hrt-app"
"$HOME/oyama-hrt-dist/bin/oyama-hrt-cli" version
```
Expected: app runs until the 8s timeout (exit 124), no crash; CLI prints `oyama-hrt-cli 0.5.0`.

- [ ] **Step 7: Commit**

```sh
git add app/src/main.cpp app/tests/capture_ui.sh CMakeLists.txt
git commit -m "test(ui): add offscreen screenshot capture"
```

---

### Task 11: FreeBSD build and acceptance

**Files:** none (environment verification only).

**Interfaces:**
- Consumes: the committed branch, FreeBSD VM, Qt 6.11.2, existing PostgreSQL.
- Produces: verified FreeBSD build, CTest result, screenshots.

- [ ] **Step 1: Start the VM and sync the branch**

The VM disk is `/mnt/big/freebsd.qcow2`; the source is `/home/oyama/oyama-src`. Start QEMU with the existing command (headless or GTK), wait for SSH on port 2222, then:

```sh
git bundle create /tmp/opencode/ui-overhaul.bundle main
SSH_ASKPASS=/tmp/opencode/freebsd-askpass.sh SSH_ASKPASS_REQUIRE=force DISPLAY=:0 \
  setsid -w scp -P 2222 -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/tmp/opencode/freebsd_known_hosts \
  -o PreferredAuthentications=keyboard-interactive -o PubkeyAuthentication=no \
  /tmp/opencode/ui-overhaul.bundle oyama@127.0.0.1:/home/oyama/ui-overhaul.bundle
```

Then over SSH:

```sh
cd ~/oyama-src
git fetch ~/ui-overhaul.bundle main
git merge --ff-only FETCH_HEAD
```

If `--ff-only` fails, stop and report the conflict instead of forcing.

- [ ] **Step 2: Ensure a CJK font is present**

Run:

```sh
printf "oyama12345\n" | su -m root -c "pkg install -y font-noto-cjk" || true
fc-list :lang=zh | head -3
fc-list :lang=fa | head -3
```

Expected: at least one Chinese font. Persian uses bundled Vazirmatn, so `fc-list :lang=fa` may be empty before the app starts; that is acceptable.

- [ ] **Step 3: Build and run CTest**

```sh
cd ~/oyama-src
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DOYAMA_BUILD_QT_SHELL=ON
cmake --build build -j2
PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin \
  ctest --test-dir build --output-on-failure
```

Expected: build succeeds; 18/18 tests pass (12 original + `ui_preferences_test` + 5 new smoke cases). If the test target is not built on FreeBSD because of a missing dependency, fix the CMake guard rather than disabling the test.

- [ ] **Step 4: Capture FreeBSD screenshots**

With the Plasma X11 session running, launch the app on the VM display:

```sh
DISPLAY=:0 OYAMA_UI_LANGUAGE=zh ~/oyama-src/build/oyama-hrt-app --screenshot /tmp/zh.png --page overview
DISPLAY=:0 OYAMA_UI_LANGUAGE=fa ~/oyama-src/build/oyama-hrt-app --screenshot /tmp/fa.png --page overview
DISPLAY=:0 OYAMA_UI_LANGUAGE=fa ~/oyama-src/build/oyama-hrt-app --screenshot /tmp/fa-dark.png --page overview
```

If `QQuickWindow::grabWindow()` is unsupported on the FreeBSD X11/software stack, use the QEMU monitor `screendump` while the app is visible instead. Copy the PNGs back with the SSH/askpass pattern and inspect with `read`.

Check: Chinese glyphs render; Persian glyphs render and the sidebar is on the right; panel/card colors match the tokens.

- [ ] **Step 5: Record results**

Append a short verification note to `docs/superpowers/specs/2026-10-30-desktop-ui-overhaul-design.md` §10 or to the task's final report: FreeBSD Qt version, build result, CTest count, screenshot paths. Do not commit build artifacts.

- [ ] **Step 6: Commit any fixes and hand over**

If FreeBSD exposed a fix, commit it with a descriptive message. Final deliverables:

- Linux: `~/oyama-hrt-dist/bin/oyama-hrt-app`, `~/oyama-hrt-dist/bin/oyama-hrt-cli`, `~/oyama-hrt-dist/share/oyama-hrt/migrations`.
- FreeBSD: `~/oyama-src/build/oyama-hrt-app` (or Release build), source synced at the same commit.
- User instructions: `chmod +x`, `./oyama-hrt-app`, first-run language choice, `oyama-hrt-cli db setup` if the banner appears.

---

## Appendix A: Translation key checklist

The loader filters keys starting with `_`. The following keys must exist in all four files; the list is also the canonical `en.json` content in Task 1 Step 3.

`app.name`, `app.tagline`, `nav.overview`, `nav.add_dose`, `nav.history`, `nav.settings`, `status.connected`, `status.local_socket`, `status.not_connected`, `status.managed`, `status.existing`, `mode.transfem`, `mode.transmasc`, `common.cancel`, `common.save`, `common.saved`, `common.delete`, `common.edit`, `common.close`, `common.view_all`, `theme.label`, `theme.light`, `theme.dark`, `theme.system`, `language.label`, `language.choose_title`, `language.choose_subtitle`, `overview.title`, `overview.subtitle`, `overview.e2`, `overview.cpa`, `overview.t`, `overview.chart_title`, `overview.chart_subtitle`, `overview.chart_empty`, `overview.recent`, `overview.recent_empty`, `overview.add_cta`, `dose.add_title`, `dose.edit_title`, `dose.subtitle`, `dose.route`, `dose.ester`, `dose.amount`, `dose.days_ago`, `dose.sublingual_tier`, `dose.theta`, `dose.theta_placeholder`, `dose.gel_site`, `dose.release_rate`, `dose.release_placeholder`, `dose.wear`, `dose.wear_placeholder`, `dose.save`, `dose.add`, `dose.error_days`, `route.oral`, `route.sublingual`, `route.injection`, `route.gel`, `route.patchApply`, `route.patchRemove`, `ester.E2`, `ester.EV`, `ester.EB`, `ester.EC`, `ester.EN`, `ester.EU`, `ester.CPA`, `ester.T`, `ester.TC`, `ester.TE`, `ester.TU`, `tier.quick`, `tier.casual`, `tier.standard`, `tier.strict`, `site.arm`, `site.thigh`, `site.scrotal`, `history.title`, `history.subtitle`, `history.count`, `history.empty_title`, `history.empty_body`, `history.delete_title`, `history.delete_body`, `settings.title`, `settings.subtitle`, `settings.body_section`, `settings.weight`, `settings.weight_placeholder`, `settings.birth_year`, `settings.birth_placeholder`, `settings.model_section`, `settings.calibration_method`, `settings.calibration_history`, `settings.pk_overrides`, `settings.save`, `settings.appearance_section`, `settings.about_section`, `settings.version`, `settings.database`, `settings.database_none`, `settings.database_ok`, `calibration.mipd`, `calibration.off`, `calibration.ekf`, `calibration.ou_kalman`, `calibration.retrospective`, `calibration.forward`, `db.banner`, `db.cta_disabled`, `error.amount`, `error.days`, `status.no_database`, `status.database_error`, `status.invalid_mode`, `status.invalid_mode_plain`, `status.db_not_connected`, `status.invalid_dose_time`, `status.unsupported_route`, `status.dose_added`, `status.add_failed`, `status.no_db_open`, `status.dose_positive`, `status.invalid_time`, `status.invalid_combination`, `status.dose_not_found`, `status.dose_updated`, `status.update_failed`, `status.weight_positive`, `status.birth_year_range`, `status.invalid_calibration`, `status.invalid_history`, `status.settings_saved`, `status.save_failed`, `status.dose_removed`, `status.remove_failed`.
