# Desktop UI Overhaul — Warm Design System, Four Languages, and RTL

- **Status:** Approved (2026-10-30)
- **Date:** 2026-10-30
- **Target repository:** `xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop`
- **Related design:** `docs/superpowers/specs/2026-10-29-linux-desktop-design.md`
- **Scope:** Only the Qt/QML interface under `app/` and the related C++ status copy; the model, storage, backup format, and CLI behavior are unchanged

---

## 0. In One Sentence

Replace the existing Qt interface — a dark top bar with dark cards — with a native desktop interface built on a warm beige base, left-side icon navigation, generous whitespace, and gentle motion. On first launch the user picks a language; Chinese, English, Russian, and Persian are supported, and the entire Persian interface is RTL.

---

## 1. Why Now

The existing interface is functionally complete, but visually it is "an engineering interface that works":

- A dark background `#0e1318`, a top bar spanning the window, and outlined cards leave a clear gap from the gentle feel of the web version.
- Navigation icons are characters such as `◉ ＋ ≡ ⚙`, with no consistent style.
- Page transitions are hard cuts, and buttons and text fields use the Qt Basic defaults.
- All copy is hardcoded English, with no way to choose a language; Persian needs RTL, which the current layout does not consider.

The owner has decided: move the structure toward the web version (left navigation + card content area), default to light, use a warm beige system, and support four languages chosen on first launch.

---

## 2. Goals, Non-Goals, and Anti-Goals

### 2.1 Goals

1. At first glance it should look like the same Oyama product: warm, low contrast, large radii, whitespace, light motion.
2. The desktop structure follows the web version: left navigation, page title, card content area, hover highlight.
3. Light by default, with dark switchable; both palettes meet readable contrast.
4. On first launch the user can choose Chinese / English / Русский / فارسی, and can switch later in Settings without restarting.
5. The Persian interface is RTL throughout: sidebar on the right, mirrored layout, right-aligned text; chart axes stay left-to-right.
6. All existing features, C++ interfaces, storage behavior, and tests are preserved.
7. Linux and FreeBSD share the same QML, and the build outputs are identical.

### 2.2 Non-Goals

- Do not add web-only features such as Labs, Supplies, Plan, accounts, or community voting.
- Do not change the PostgreSQL schema, backup format, or CLI arguments.
- Do not build frameless windows, a system tray, or a global menu.
- Do not introduce Qt Quick 3D, QtCharts, or WebEngine.
- Do not perform full localization review for the four languages; the first version uses the implementer's initial translations, and native-speaker review can follow in later PRs.

### 2.3 Anti-Goals

- Piling on frosted glass, gradients, glows, and heavy shadows for a "premium" feel.
- Giving each component its own radius, font size, and color.
- Requiring a restart to switch languages, or translating Persian text without mirroring the layout.
- Touching model, storage, or backup logic while changing the UI.
- Hardcoding a Chinese or Persian font to a family name that exists only on one Linux distribution.

---

## 3. Settled Decisions

| # | Decision | Notes |
|---|----------|-------|
| U1 | Fixed left sidebar, no floating expansion | Desktop windows are wide enough; fixed at 236px, nav items highlight on hover, the selected item is filled |
| U2 | Light by default, dark switchable | `ui.theme` defaults to `light`; `dark` and `system` are also available |
| U3 | Four languages: `zh`, `en`, `ru`, `fa` | Full-screen choice on first launch; changeable in Settings; Persian RTL |
| U4 | Runtime JSON translations, no compiled `.qm` files | 5 pages, about 120 keys; runtime switching is simple and does not depend on `lrelease` |
| U5 | C++ status messages keep the English `status` and add `statusKey`/`statusArgs` | Does not break existing tests and the CLI; QML displays the translated key |
| U6 | Fonts: bundled Inter + Vazirmatn, system CJK font for Chinese | Inter covers English/Russian; Vazirmatn covers Persian; on FreeBSD install Noto CJK or WenQuanYi for Chinese |
| U7 | Icons drawn in QML with `Shape` + `PathSvg` | Uniform 1.75px stroke; no icon font or SVG image plugin |
| U8 | Almost no shadows; hierarchy through lightness | Stable performance in software-rendered environments, and it fits the "light" look |
| U9 | Three radii: card 18, control 12, pill 999 | Uniform across the app |
| U10 | Five type sizes: 28 / 20 / 15 / 13 / 11 | Weights 400 / 500 / 600 |

---

## 4. Visual System

### 4.1 Color Tokens

Light:

| Token | Value | Use |
|-------|-------|-----|
| `bg` | `#EFE6D0` | Window background |
| `sidebar` | `#F5EDDF` | Sidebar, slightly lighter than the background |
| `surface` | `#FFFDF7` | Cards |
| `surfaceAlt` | `#EFE9D8` | Text fields, secondary sections, list rows |
| `surfaceHover` | `#F2E8D6` | Hover |
| `surfacePressed` | `#E7DCC6` | Selected / pressed |
| `text` | `#33302A` | Body, headings |
| `textSoft` | `#7A7466` | Secondary headings at 20px and above, decorative text |
| `textMuted` | `#6E6759` | 15px secondary body text, default navigation text (same hue as `#7A7466`, slightly darker, meeting 4.5:1) |
| `textFaint` | `#6B6457` | 11–13px captions, placeholders |
| `textDisabled` | `#A69E8E` | Disabled |
| `border` | `#DED3BD` | Very few dividers, popup borders |
| `accent` | `#B5793C` | Primary color, primary buttons, chart lines |
| `accentHover` | `#A66B31` | Primary button hover |
| `accentSubtle` | `#F3E3CC` | Subtle accent background, empty-state icon background |
| `accentInk` | `#8C5A28` | Accent text on light backgrounds |
| `green` | `#4F7A5A` | Success, CPA data |
| `brick` | `#B4573E` | Danger, T data, delete |
| `onAccent` | `#2B241B` | Primary button text (warm dark; light button + dark text, contrast about 5.2:1) |
| `chartLine` | `#B5793C` | Curve primary color (same as `accent`) |
| `chartGrid` | `#E4D8C2` | Chart grid |
| `shadow` | `rgba(60, 45, 20, 0.06)` | Very light shadow (used only on overlays) |

Dark:

| Token | Value | Use |
|-------|-------|-----|
| `bg` | `#1F1D1A` | Window background |
| `sidebar` | `#1B1917` | Sidebar, slightly darker than the background |
| `surface` | `#2B2823` | Cards |
| `surfaceAlt` | `#35312B` | Text fields, overlays, secondary sections |
| `surfaceHover` | `#302C27` | Hover |
| `surfacePressed` | `#3A352E` | Selected / pressed |
| `text` | `#E6E0D4` | Body, headings |
| `textMuted` | `#A39C8D` | Secondary text |
| `textFaint` | `#9A9385` | 11–13px captions (meets 4.5:1 on dark cards) |
| `textDisabled` | `#6F695D` | Placeholders, disabled |
| `border` | `#3D3830` | Dividers |
| `accent` | `#C98F52` | Brightened primary color for dark mode |
| `accentHover` | `#D9A268` | Primary button hover |
| `accentSubtle` | `#3A2F23` | Subtle accent background |
| `accentInk` | `#DFA96F` | Accent text on light backgrounds |
| `green` | `#7FA98A` | Success, CPA |
| `brick` | `#D07A62` | Danger, T, delete |
| `onAccent` | `#1F1D1A` | Dark-mode primary button text |
| `chartGrid` | `#3A352E` | Chart grid |

Notes:

- The primary color is only the ochre `#B5793C`; deep green and brick red carry only the semantics of "success / danger / third data series" and are not used as large decorative areas.
- Dark is not a simple inversion: higher layers are lighter (`bg < surface < surfaceAlt`), following "dark mode progresses by lightness".
- Body `#33302A` on `#EFE6D0` is about 10.6:1; dark body is about 12:1. `textSoft` (`#7A7466`) is used only for secondary headings at 20px and above; 15px secondary body text and default navigation text use `textMuted` (`#6E6759`, about 4.5:1); 11–13px uses `textFaint` (`#6B6457`, about 4.7:1). Dark `textFaint` is raised to `#9A9385`, about 4.5:1 on `surface`. Primary buttons use warm dark text `#2B241B` on ochre (about 5.2:1), matching the web version's "light button + dark text".

### 4.2 Fonts

| Language | Primary font | Fallback |
|----------|--------------|----------|
| `en` / `ru` | Inter | Noto Sans, Liberation Sans, DejaVu Sans |
| `zh` | Noto Sans CJK SC | WenQuanYi Zen Hei, Noto Sans |
| `fa` | Vazirmatn | Noto Naskh Arabic, DejaVu Sans |

- Inter Regular / Medium / SemiBold and Vazirmatn Regular / Medium / SemiBold are bundled as resources (OFL 1.1), loaded at startup with `QFontDatabase::addApplicationFont`.
- The Chinese locale does not bundle a full CJK font (it is large); Linux uses the system Noto CJK, and FreeBSD installs `font-noto-cjk` or `zh-wqy-zenhei` during deployment.
- Headings and body use the same family, layered by size and weight; headings do not use SimSun, avoiding cross-platform font inconsistencies.
- Sizes: page title 28, section title 20, body 15, label 13, caption 11. Body line height 1.55; heading line height 1.2.
- Only weights 400 / 500 / 600; do not bold whole pages.

### 4.3 Radii, Spacing, Shadows

- Radii: cards 18; buttons, text fields, nav items 12; pills / avatars 999. Only these three tiers across the app.
- Spacing: 4 / 8 / 12 / 16 / 20 / 24 / 32 / 40. Page left/right margins 40 (24 in narrow windows); card padding 20; 24–32 between sections.
- Shadows: none by default. Only popups and Toasts use `0 4px 24px rgba(60, 45, 20, 0.06)` (dark `rgba(0,0,0,0.35)`).
- Dividers: use `border` only for settings groups and popup borders; otherwise rely on the lightness difference between `surface` and `bg`.

### 4.4 Motion

| Scenario | Duration | Curve |
|----------|----------|-------|
| Hover color change | 150ms | `Easing.OutCubic` |
| Press scale 0.97 | 90ms | `Easing.OutCubic` |
| Page transition: fade in + move up 8px | 280ms | `Easing.OutCubic` |
| Theme switch: color transition | 240ms | `Easing.InOutCubic` |
| Save success: checkmark scale + copy fade-in | 320ms | `Easing.OutBack` |
| Toast enter / exit | 220 / 160ms | `Easing.OutCubic` |

- No linear animations; no large slides.
- Page transitions only fade in and lift the new page; the old page does not animate, avoiding layout reflow.
- After a dose is saved successfully, the primary button content becomes "checkmark + Recorded" over 320ms, stays for about 700ms, then returns to Overview; the button is disabled during this time.

---

## 5. Window and Page Structure

### 5.1 Shell

```text
┌──────────────┬────────────────────────────────────────────┐
│  Oyama       │  Page title                     Mode / status │
│  Tracker     │  One-line description                       │
│              │                                            │
│  ◉ Overview  │  ┌──────────┐ ┌──────────┐ ┌──────────┐   │
│  ＋ Add dose │  │ Reading  │ │ Reading  │ │ Reading  │   │
│  ☰ History   │  └──────────┘ └──────────┘ └──────────┘   │
│  ⚙ Settings  │  ┌──────────────────────────────────────┐  │
│              │  │ Curve card                            │  │
│              │  └──────────────────────────────────────┘  │
│              │                                            │
│  ● Local     │  ┌──────────────────────────────────────┐  │
│  ☾ Theme     │  │ Recent records                        │  │
└──────────────┴────────────────────────────────────────────┘
```

- Window default 1200×800, minimum 980×680.
- Sidebar fixed at 236px, background `sidebar`, with a very faint divider on the right (`border`).
- Content area left/right margins 40, top 28, bottom 32; overview content max width 960, form/settings max width 680.
- No longer use `ApplicationWindow.header` / `footer`. Backend status moves to the bottom of the sidebar; action feedback uses Toast.
- The page title sits at the top of the content area and is not sticky; when scrolling, the title scrolls with the content, reducing Qt's sticky/glass implementation complexity.

### 5.2 Sidebar

- Top: 36×36 rounded app icon (reuse the existing brand color block or the letter `O`), with `Oyama` (600) + `Tracker` (400, muted) to its right.
- Nav item: height 48, horizontal padding 12, radius 12, icon 20, text 15, gap 14.
  - Default: text `textMuted`, icon `textMuted`, transparent background.
  - Hover: background `surfaceHover`, text `text`, 150ms.
  - Selected: background `surfacePressed`, text `text` (500), icon `accent`.
  - Pressed: overall scale 0.98.
- Navigation: Overview, Add dose, History, Settings. `Add dose` remains a separate entry; clicking it clears the editing state.
- Bottom: mode segmented control (transfem / transmasc, height 32, two equal-width pills, selected item `surfacePressed`); connection status dot + copy (`Local · socket only` / `Not connected`); theme toggle icon button (sun / moon); small version number (if it can be obtained).
- Mode switching is global and available on every page; after switching, data reloads and an unsaved dose form is cleared (same as current behavior).
- Persian: the entire sidebar moves to the right side of the window, icon and text order is mirrored, and text is right-aligned; the mode segmented control is mirrored as well.

### 5.3 Shared Page Patterns

- Header area: title 28/600, description 13/`textMuted`; the right side can hold the mode switch, backend chip, or primary action.
- Card: `Card` component, background `surface`, radius 18, padding 20, no border; in dark mode it is one step lighter than the background.
- Empty state: 64px circular `accentSubtle` background + 40px line icon; title 20/600; description 15/`textMuted`; one primary button.
- List row: height 56–64, radius 12, hover `surfaceHover`; icon on the left, primary text 15, secondary 13, value on the right 15/`accent`.
- All clickable controls have a keyboard focus state: 2px `accent` outer ring + 1px inner ring in the background color.
- **Database not configured** (`app.backend === "none"`): do not block entry into the main interface; show a persistent banner at the top of the content area (`surfaceAlt`, radius 12, info icon + `Database not configured. Run oyama-hrt-cli db setup in a terminal.`), and the sidebar status shows `Not connected`. The empty-state `Add dose` primary button is disabled, with the reason explained below the button. The first-launch language picker still appears before the main interface.
- Database connected (`managed` / `existing`): no banner; the sidebar dot turns green and the copy reads `Local · socket only`.
- Database error at runtime: show `T.t(app.statusKey, app.statusArgs)` in a Toast, not a blocking dialog; `app.backend` keeps its original value.

---

## 6. Page Details

### 6.1 Overview

- Title: `Overview` (Chinese and English), `Обзор` (Russian), `نمای کلی` (Persian); description: `Current exposure and the local model curve.`
- Top right: the connection status chip can be omitted (it is already in the sidebar); the mode switch is at the bottom of the sidebar and available globally.
- Three stat cards: E2, CPA, T. Labels 11 uppercase / letter-spacing 0.06em; values 32/600; units 13, in ochre / deep green / brick red respectively.
- Chart card: title 20/600; description 13; Canvas height 320.
  - **Draw only the E2 curve**; CPA and T appear only in the stat cards, avoiding the complexity of dual Y axes and different units. `cpa`/`tLevel` in `curve` are reserved for later versions.
  - Grid `chartGrid`; axis text 11 `textMuted`; curve 2.5px `chartLine` (same as `accent`); 10% opacity fill under the curve.
  - No data: centered empty state (pill icon + `No doses yet` + `Record your first dose to see the estimated curve.` + primary button `Add dose`).
- Recent doses: title 20/600; at most 5; each row shows route · ester, time, and dose; hover highlight; click opens History.
- Mode switching reloads data and preserves the existing `app.mode` behavior.

### 6.2 Add Dose / Edit Dose

- The title changes with state: `Add dose` or `Edit dose`; one-line description.
- Form card max width 640, centered; padding 24; field spacing 16.
- Two-column grid (automatically one column in narrow windows): Route / Ester, Dose (mg) / Days ago.
- Conditional fields (sublingual, gel, patch) fade in rather than suddenly expanding: `opacity` + `visible`, with height changing naturally through the layout.
- Text fields: height 44, background `surfaceAlt`, radius 12, no border; 1.5px `accent` border on focus; placeholder `textFaint`.
- Primary button: `Add dose` / `Save changes`, ochre background, height 44, radius 12; Cancel is a ghost button.
- Errors: 13px `brick` copy below the field; no dialog.
- Success: the button becomes a checkmark + `Recorded`, 320ms `OutBack`; returns to Overview after 700ms; errors do not navigate.

### 6.3 History

- Title + record count (`N records`).
- List card: each row 64px, pill icon on the left, primary text `route · ester`, secondary time + dose, edit / delete icon buttons on the right.
- Hover the whole row `surfaceHover`; edit/delete buttons are `textMuted` by default and `text` on hover; delete turns `brick` on hover.
- Delete first opens a confirmation dialog (`Delete this dose?` / `This can't be undone.` / `Cancel` / `Delete`), then calls `app.removeDose` after confirmation.
- Empty state: `No doses yet` + `After you record your first dose, it will be listed here in chronological order.` + primary button.

### 6.4 Settings

Grouped cards:

1. **Body**: weight, birth year.
2. **Estimation model**: calibration method, calibration history, PK parameter JSON (monospace, minimum height 140).
3. **Language & appearance**:
   - Language: four pill buttons (Chinese / English / Русский / فارسی), the current item filled with `surfacePressed`, switching immediately on click.
   - Theme: three segmented buttons (Light / Dark / System), with a sliding indicator on the selected item.
4. **About**: app name, version, `Local · socket only`, database status.

- The save button goes below the "Body/Model" cards; language and theme take effect immediately and do not need saving.
- Settings page max width 680; labels 13/500, text fields the same as the dose page.

---

## 7. Internationalization and RTL

### 7.1 Languages and When to Choose

- Language codes: `zh`, `en`, `ru`, `fa`.
- On first launch (`ui.language` empty) show a full-screen language picker:
  - Warm background, centered, max width 720.
  - Brand icon + `Choose your language` (English, because there is no language yet).
  - 2×2 cards: Chinese, English, Русский, فارسی; each card uses the language's native name in large type plus the English name in small type.
  - Hover: `surfaceHover` + `accent` border; pressed scale 0.98; click writes `ui.language`, the picker fades out, and the main interface fades in.
  - Small text at the bottom: `You can change this later in Settings.`
- The Settings page can switch at any time; after switching, all `T.t()` bindings recompute immediately, with no restart needed.
- Tests / screenshots can use the environment variable `OYAMA_UI_LANGUAGE=en|zh|ru|fa` to skip the picker; this variable is not written to configuration.

### 7.2 Runtime Translation

- C++ `UiPreferences` (registered as the QML singleton `Ui`) is responsible for:
  - `language`, `theme`, `systemDark`, `rtl`, `strings`, `fallbackStrings`.
  - Persisting `ui/language` and `ui/theme` with `QSettings`.
  - At startup, reading from `:/i18n/{code}.json`; missing keys fall back to `en`, then to the key itself.
  - System theme: Qt 6.5+ uses `QStyleHints::colorScheme()` and listens to `colorSchemeChanged`; Qt 6.4 uses `QGuiApplication::palette().color(QPalette::Window).lightness() < 128` as a one-time check and cannot detect runtime changes. If neither path is available, fall back to `light`. The minimum Qt stays 6.4; do not raise the bar for "follow system".
- QML singleton `T.qml` provides `t(key, args)`, `rtl`, `language`. `t()` reads `Ui.strings` internally so bindings recompute automatically when the language changes.
- Placeholders use `%1`…`%9`, replaced by `t()`; arguments can be numbers or strings.
- Key naming follows the web version's style: `nav.overview`, `btn.add_dose`, `label.e2`, `status.dose_added`, etc.; existing Chinese from the web version is reused directly to reduce terminology divergence.
- C++ `AppController` additions:
  - `Q_PROPERTY(QString statusKey)`
  - `Q_PROPERTY(QStringList statusArgs)`
  - Every `fail()` / success assignment sets both key and args; `status` keeps the original English so existing tests and the CLI are unaffected.
  - The QML Toast shows `T.t(app.statusKey, app.statusArgs)`; when `statusKey` is empty, fall back to `app.status`.
- About 25 C++ status messages need translation; dynamic database errors pass the original error text as the `%1` argument, without translating technical details.

### 7.3 RTL (Persian)

- `ApplicationWindow.layoutDirection: T.rtl ? Qt.RightToLeft : Qt.LeftToRight`.
- The sidebar automatically moves to the right via `RowLayout`; nav icon and text order is mirrored; text uses `T.textAlign` (`Text.AlignRight` in RTL).
- Numbers, doses, units, and times keep LTR numeric layout; Qt's bidi algorithm handles mixed runs.
- Chart axes stay left-to-right (the time axis is not mirrored); legend and title follow RTL.
- Directional icons (back, chevron) flip horizontally in RTL; neutral icons (gear, sun) do not.
- Form labels are right-aligned and input text is right-aligned; the ComboBox arrow is on the left.

### 7.4 Translation Scope

- Page titles, navigation, buttons, field labels, placeholders, empty states, errors, statuses, settings groups, language/theme names.
- Not translated: drug codes `E2`, `CPA`, `T`, units `pg/mL`, `ng/mL`, `ng/dL`, route internal values such as `oral` (localized names may be shown, but the model still receives the English enum).
- Russian / Persian initial translations are done by the implementer; medical terms are checked against the English and Chinese versions, with native-speaker review handled later.
- Note in `app/i18n/README.md` and at the top of each language's JSON file: native speakers are welcome to submit PRs correcting translations; native-speaker review is not required before release. Corrections to Chinese and English are equally welcome.

---

## 8. Component Inventory

| File | Responsibility |
|------|----------------|
| `Theme.qml` (singleton) | Light/dark tokens, radii, spacing, type sizes, motion durations, font family |
| `T.qml` (singleton) | `t(key, args)`, `rtl`, `language`, `textAlign` |
| `Icon.qml` | Unified line icons (`Shape` + `PathSvg`), supporting `name`, `size`, `color`, `strokeWidth` |
| `Card.qml` | Card container, padding, radius, background, optional hover |
| `NavItem.qml` | Sidebar nav item, hover/selected/pressed states, keyboard focus |
| `AppButton.qml` | Four variants: primary / secondary / ghost / danger |
| `AppTextField.qml` | Rounded text field, focus ring, placeholder color |
| `AppComboBox.qml` | Rounded dropdown, popup, hovered item |
| `PageHeader.qml` | Title, description, right-side action area |
| `StatCard.qml` | Stat card (label, value, unit, accent color) |
| `EmptyState.qml` | Icon, title, description, action button |
| `Toast.qml` | Success/error notice, auto-dismiss |
| `ConfirmDialog.qml` | Delete confirmation, scrim + card |
| `LanguagePicker.qml` | Full-screen first-launch language picker |
| `Sidebar.qml` | Brand, navigation, connection status, theme toggle |

Page files: `Main.qml`, `OverviewPage.qml`, `AddDosePage.qml`, `HistoryPage.qml`, and `SettingsPage.qml` are all rewritten.

---

## 9. Code and Data Changes

- `app/qml/`: add the components and singletons above; rewrite the pages.
- `app/i18n/en.json`, `zh.json`, `ru.json`, `fa.json`: flat key tables.
- `app/fonts/`: Inter and Vazirmatn Regular / Medium / SemiBold, with `OFL.txt` and source notes.
- `app/src/ui_preferences.h/.cpp`: language, theme, translation tables, system dark-mode monitoring.
- `app/src/app_controller.h/.cpp`: `statusKey` / `statusArgs`; every status assignment also sets a key.
- `app/src/main.cpp`: load fonts, register the `Ui` singleton, read `OYAMA_UI_LANGUAGE`.
- `CMakeLists.txt`: QML files, singleton properties, font/i18n resources, test environment variables.
- `app/tests/`: update the QML smoke tests so first launch does not fail because of the language picker; add assertions for the status-key mapping where testable.
- `THIRD_PARTY_NOTICES.md`: Lucide (ISC), Inter (OFL), Vazirmatn (OFL).

---

## 10. Verification

1. **Build**: Linux Release 76/76; FreeBSD Debug/Release builds pass.
   - The FreeBSD acceptance environment is ready: `/mnt/big/freebsd.qcow2`, FreeBSD 15.1, Qt 6.11.2, source `/home/oyama/oyama-src`, Plasma X11, SSH host port 2222, account `oyama`/`oyama12345` (root has the same password; root SSH is disabled, so use `su -`).
2. **Tests**: existing CTest 12/12; the QML smoke tests pass with `OYAMA_UI_LANGUAGE=en`; settings/history/dose interactions do not regress.
3. **Visual**: Xvfb 1200×800 screenshots; at minimum cover:
   - light English Overview;
   - dark Chinese Overview;
   - Persian Overview (sidebar on the right, right-aligned text);
   - the language picker;
   - the dose success state;
   - the history delete confirmation.
4. **Theme**: when switching light/dark, no page retains the old colors; the chart repaints.
5. **Language**: after switching among the four languages, there is no truncation, no leftover `%1`, and no key leaks; the Persian layout mirrors correctly.
6. **Fonts**: no tofu for Chinese and Persian on FreeBSD; if a font is missing, document the package name.
7. **Regression**: model vectors, storage, backup, and CLI tests all pass.

**2026-10-30 FreeBSD acceptance record**

- Branch `desktop-ui-overhaul`, code commit `dcf0fde` (this build and test run is based on that commit; only documentation commits followed). The source was synced with a git bundle to `/home/oyama/oyama-src`; the old non-git source tree was kept as `~/oyama-src.pre-ui-overhaul`.
- Environment: FreeBSD 15.1-RELEASE amd64, Qt 6.11.2, CMake 3.31.12, Ninja 1.13.2, PostgreSQL 18.6.
- Build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DOYAMA_BUILD_QT_SHELL=ON` succeeded; 118/118 targets completed.
- Tests: CTest **19/19** passed in 27.62 s (including the six Qt shell smoke cases, `qml_components`, and `app_controller_test`).
- Fonts: the FreeBSD ports package is named `noto-sc` (there is no `font-noto-cjk` package); after installation `fc-list :lang=zh` lists Noto Sans CJK SC. Persian continues to use the bundled Vazirmatn.
- Screenshots: generated on FreeBSD with `--screenshot` plus `QT_QPA_PLATFORM=offscreen` and `QT_QUICK_BACKEND=software` as `zh-light.png`, `fa-light.png`, and `fa-dark.png` (1200×800), then copied to the host at `/tmp/opencode/freebsd-shots/` for inspection. CJK and Persian glyphs render correctly, the Persian sidebar is mirrored to the right, and the dark theme colors match.
- Post-acceptance fixes (commit `7fcff39`): the combo-box delegate now declares `required property var model` / `required property int index`, fixing blank option text; the sidebar hover color animation was removed to avoid residual hover backgrounds under software rendering. A `qml_components` regression test was added, the FreeBSD rebuild passed **19/19**, and a manual Plasma X11 check confirmed that the route dropdown shows all six options and that no sidebar background remains after the pointer leaves.

---

## 11. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| FreeBSD lacks CJK / Arabic fonts | Chinese or Persian shows tofu | Bundle Vazirmatn; install Noto CJK / WenQuanYi on FreeBSD; detect at startup and warn |
| Inconsistent translation terminology | Misleading medical information | Reuse the web version's Chinese; check Russian/Persian against English; keep English enum values |
| Runtime translation bindings do not refresh | Some text stays unchanged after switching | `T.t()` reads `Ui.strings` explicitly; verify page by page with screenshots |
| RTL layout misalignment | Persian is unusable | Use `RowLayout` + `LayoutMirroring`; screenshot each page; keep charts LTR |
| QML software rendering performance | Animation stutter | No shaders/blur/heavy shadows; only color, opacity, position, and scale |
| Status key changes break tests | CI failure | Keep the English `status`; add keys without changing old fields |
| Scope creep | Delivery delay | Only the pages and components listed here; no Labs/Plan/Supplies |

---

## 12. Implementation Order

1. `UiPreferences` + `T.qml` + the four JSON files + font resources; language picker.
2. `Theme.qml` + base components (Icon, Card, Button, TextField, ComboBox, NavItem).
3. `Main.qml` + `Sidebar.qml` shell; page transition animation; Toast.
4. Overview page (stat cards, chart, empty state, recent doses).
5. Add dose page (form, conditional fields, success state).
6. History page (list, hover, delete confirmation, empty state).
7. Settings page (groups, language, theme, save).
8. Dark and RTL walkthrough of all pages; font and contrast fixes.
9. Linux Release build + CTest + screenshots; FreeBSD build + CTest + screenshots.
10. Install to `~/oyama-hrt-dist`, with delivery paths and run instructions.

---

## 13. Builder Check Resolution Record

Before implementation, a fresh builder with no context read this document and raised six questions that would change the plan; the resolutions are below:

| # | Question | Resolution |
|---|----------|------------|
| 1 | With no database, the old footer hint is removed and the empty-state CTA would fail | **Decided in this document**: no blocking database setup page; keep the main interface with a persistent top banner + sidebar "Not connected" + disabled empty-state CTA. The language picker comes before everything. Do not add a `connecting` connection state; keep `app.backend`'s `none`/`managed`/`existing`. |
| 2 | What is the FreeBSD acceptance environment? | **Ready**: `/mnt/big/freebsd.qcow2`, FreeBSD 15.1, Qt 6.11.2, source `/home/oyama/oyama-src`, Plasma X11; SSH 2222, `oyama`/`oyama12345`. |
| 3 | Should the minimum Qt move from 6.4 to 6.5? | **Decided in this document**: stay on 6.4. Qt 6.5+ uses `QStyleHints::colorScheme`; 6.4 uses palette lightness for a one-time check; fall back to light when unknown. "Follow system" is best-effort on 6.4, and the document says so. |
| 4 | How many curves should be drawn? | **Decided in this document**: only E2; CPA/T appear only in the stat cards. `chartLine` is `accent`. Multiple series are left to a later version. |
| 5 | Is WCAG AA a hard gate, or are the hex values prioritized? | **Decided in this document**: small-text readability comes first, without changing the overall hue. Add `textSoft`, keeping `#7A7466` for 20px+; 15px/nav uses the slightly darker `textMuted` `#6E6759`; 11–13px uses `#6B6457`; dark `textFaint` uses `#9A9385`; primary buttons switch to warm dark text `#2B241B` (matching the web version's light buttons). All other hex values follow the table. |
| 6 | Where does the transfem/transmasc switch go? | **Decided in this document**: at the bottom of the sidebar, globally available; switching reloads data and clears an unsaved dose form. |

Items 1, 3, 4, 5, and 6 are all decisions the implementer can make independently; if the owner prefers otherwise on any of them, it is enough to say so at approval time.

## 14. Owner Confirmation Results (2026-10-30)

1. Persian **fully mirrored interface**: sidebar on the right, RTL layout. Confirmed.
2. Russian / Persian **accept "implementer's initial translation + later native-speaker review"**; state in the translation files and README that native speakers are welcome to submit PRs with corrections. Confirmed.
3. Language selection uses a **full-screen welcome page**, not a dialog after entering the app. Confirmed.

The design is approved and moves to the implementation planning stage.
