# Desktop application shell (M2)

This directory contains the Qt 6 / QML desktop shell for Linux and BSD.  It is
intentionally a native Qt Quick application: no Qt WebEngine, Electron, Node,
or embedded browser.

## Current scope

The M2 shell is implemented and uses the real local database and the C99
pharmacokinetic model.  It currently provides:

- **Overview** — current E2, CPA and testosterone values, the modeled E2 curve,
  and the five most recent doses.
- **Add dose** — route, ester/drug, dose in mg, and “days ago”.  Route-specific
  extras are available for sublingual (tier/theta), gel (site), and patches
  (release rate/wear time).  The dose is written to the vault and the curve is
  recomputed immediately.
- **History** — all doses for the selected mode, with an integrated dose form,
  edit, and delete.
- **Settings** — body weight, birth year, calibration method/history, PK
  parameter overrides as JSON, appearance, language, and mode. These values are
  stored in the encrypted vault and shared by both modes.
- **Single-window shell** — sidebar navigation, a page header, light/dark/system
  themes, and English, Chinese, Russian, and Persian UI text, with right-to-left
  layout for Persian.
- **Vault gate** — first run generates a seven-word recovery phrase, asks you to
  write it down and re-enter three positions, then unlocks the vault. Later
  launches show an unlock screen. A header button locks immediately; the app
  also locks after 15 minutes of inactivity and on quit.
- **Managed cluster lifecycle** — if the saved configuration selects a managed
  cluster, the app starts it at launch through `LocalCluster::ensure_running()`.
  If that cluster still contains legacy plaintext data, setup hands the phrase
  to a background migration worker instead of writing partial vault data.

The app reads the same configuration as the CLI
(`$XDG_CONFIG_HOME/oyama-hrt/config`; `OYAMA_CONFIG_FILE` overrides it).  If no
database is configured, the shell still loads and shows a status message asking
the user to run:

```sh
oyama-hrt-cli db setup
```

## Build and run

Requirements: CMake 3.21+, a C++20 compiler, Qt 6.4+ with Core, Qml, Quick and
Quick Controls 2, libpq, libsodium, and nlohmann/json.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/oyama-hrt-app
```

Qt is optional for the rest of the project.  To build only the core, storage and
CLI:

```sh
cmake -S . -B build -DOYAMA_BUILD_QT_SHELL=OFF
```

### Headless smoke test

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  ./build/oyama-hrt-app --self-test
```

The same check is registered as CTest test `qt_shell_smoke`.

### Controller integration test

`app_controller_test` starts a throwaway PostgreSQL cluster, writes an
`existing` backend config, then verifies vault setup/unlock/lock, settings
round-trip, adding a dose with extras, in-place editing, curve generation, and
deletion. Additional scenarios cover a managed legacy cluster that has not been
migrated, a successful worker migration, and a retry after a bad row is
repaired. It is registered as CTest test `app_controller_test` and skips with
exit code 77 when `initdb`/`pg_ctl` are unavailable.

## Architecture

```text
app/src/main.cpp                    QGuiApplication + QQmlApplicationEngine
app/src/app_controller.*            QObject exposed to QML as `app`
app/src/ui_preferences.*            theme, language, and first-run preferences
app/src/legacy_migration_worker.*   QThread worker for managed-cluster import
app/qml/Main.qml                    window, sidebar shell, page routing
app/qml/Sidebar.qml                 navigation rail and lock action
app/qml/VaultGate.qml               full-window setup/unlock/migrating gate
app/qml/SetupVaultPage.qml          phrase display, challenge, acknowledgement
app/qml/UnlockVaultPage.qml         phrase entry and unlock
app/qml/OverviewPage.qml             stat cards, Canvas curve, recent doses
app/qml/DoseForm.qml                 dose form, route extras, add/edit
app/qml/HistoryPage.qml              dose list, integrated dose form
app/qml/SettingsPage.qml             body, dose, appearance, language settings
app/qml/Theme.qml, T.qml, UiText.qml theme colors and translated UI text
```

`AppController` loads `AppConfig`, starts a managed cluster when needed, checks
for legacy plaintext data before applying migrations, and shows the vault gate.
Only after unlock does it open repositories through `VaultStore`, read the
current mode's doses, convert them to `oyama_pk_dose`, and call
`oyama_pk_simulate`. The curve is exposed as a `QVariantList` of
`{t, e2, cpa, tLevel}` points and drawn with a QML `Canvas`. The display is
downsampled to roughly 700 points.

## Known gaps

- Plan/What-if, Labs, Supplies, Forecast and backup import/export UI are not in
  the M2 shell.
- PK parsing stores the calibration method/history but calibration itself is
  not applied to the curve yet.
- No i18n yet; the shell strings are English.
- No single-instance guard, tray integration, or desktop notifications yet.
- No packaging, install rules beyond the executable, or BSD/X11/Wayland test
  matrix yet.
- Errors are shown in the status bar; there is no modal recovery flow.

## Privacy

The database is local and uses a Unix-domain socket only. The shell performs no
network requests. Health records are encrypted at the application layer before
they reach PostgreSQL; the vault phrase is never logged, placed in argv, or
offered through a clipboard. Backups are encrypted by default.

There is no phrase recovery. The app locks after 15 minutes of inactivity and on
quit, zeroing the in-memory DEK. This does not protect against a malicious
process running as your local account while the vault is unlocked, nor against
memory-freezing or a compromised kernel. Use full-disk encryption and a trusted
operating system. The old plaintext cluster from a migrated install remains a
separate exposure until removed or protected by full-disk encryption.
