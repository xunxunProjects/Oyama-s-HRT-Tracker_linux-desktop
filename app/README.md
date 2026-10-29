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
  (release rate/wear time).  The dose is written to PostgreSQL and the curve is
  recomputed immediately.
- **History** — all doses for the selected mode, with edit and delete.
- **Settings** — body weight, birth year, calibration method/history, and PK
  parameter overrides as JSON.  These values are shared by both modes.
- **Mode switch** — `transfem` / `transmasc` in the header.
- **Managed cluster lifecycle** — if the saved configuration selects a managed
  cluster, the app starts it at launch through `LocalCluster::ensure_running()`.

The app reads the same configuration as the CLI
(`$XDG_CONFIG_HOME/oyama-hrt/config`; `OYAMA_CONFIG_FILE` overrides it).  If no
database is configured, the shell still loads and shows a status message asking
the user to run:

```sh
oyama-hrt-cli db setup
```

## Build and run

Requirements: CMake 3.21+, a C++20 compiler, Qt 6.4+ with Core, Qml, Quick and
Quick Controls 2, libpq, and nlohmann/json.

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
`existing` backend config, then verifies settings round-trip, adding a dose
with extras, in-place editing, curve generation, and deletion. It is
registered as CTest test `app_controller_test` and skips with exit code 77
when `initdb`/`pg_ctl` are unavailable.

## Architecture

```text
app/src/main.cpp             QGuiApplication + QQmlApplicationEngine
app/src/app_controller.*     QObject exposed to QML as `app`
app/qml/Main.qml             window, sidebar, mode switch, status bar
app/qml/OverviewPage.qml      stat cards, Canvas curve, recent doses
app/qml/AddDosePage.qml       dose form, route extras, add/edit
app/qml/HistoryPage.qml       dose list, edit and delete
app/qml/SettingsPage.qml      shared body/model settings
```

`AppController` loads `AppConfig`, starts a managed cluster when needed, applies
migrations, opens repositories, reads the current mode's doses, converts them to
`oyama_pk_dose`, and calls `oyama_pk_simulate`.  The curve is exposed as a
`QVariantList` of `{t, e2, cpa, tLevel}` points and drawn with a QML `Canvas`.
The display is downsampled to roughly 700 points.

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

The database is local and uses a Unix-domain socket only.  The shell performs no
network requests.  The database and backups are plaintext health data; the
existing CLI export warning applies.
