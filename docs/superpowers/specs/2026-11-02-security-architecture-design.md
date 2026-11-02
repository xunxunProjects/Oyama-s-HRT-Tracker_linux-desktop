# Security Architecture and Build-Profile Design

**Status:** Proposed — awaiting maintainer review
**Date:** 2026-11-02
**Scope:** C++11 baseline with Qt 6.4 default and Qt 5.15.2 compatibility,
BIP39 recovery phrase, panic lock, screenshot blackout, Xen key isolation,
vendored dependencies, and three build profiles.
**Related:** `docs/superpowers/specs/2026-10-29-linux-desktop-design.md`,
`docs/superpowers/specs/2026-10-31-encrypted-vault-design.md`,
`SECURITY.md`.

---

## 1. Intent

The application already protects health records at rest: PostgreSQL rows are
encrypted with a vault data-encryption key (DEK) that is wrapped by an
Argon2id-derived key-encryption key (KEK), and the vault locks after 15 minutes
of inactivity and on quit.

This design adds a second, optional layer for users who face stronger
adversaries:

1. **Capture guard** — detect the desktop environment's own screenshot action
   and immediately cover the application window with an opaque black surface.
2. **BIP39 recovery phrase** — new vaults get a 24-word, 256-bit mnemonic with a
   checksum instead of an ad-hoc phrase.
3. **Panic lock** — a global hotkey that locks the vault, wipes plaintext and
   key material, replaces the UI with a solid gray surface, and requires the
   vault phrase before anything is shown again.
4. **Key isolation** — move the DEK and all record encryption into a separate
   Xen guest (`oyama-keyd`) so the desktop UI guest never holds the key.
5. **Supply-chain control** — build the large dependencies from pinned,
   checksum-verified sources inside the project rather than trusting whatever
   the host package manager installed.
6. **Compatibility** — compile the whole project as C++11; keep Qt 6.4 as the
   default shell and offer a Qt 5.15.2 compatibility build.

The capture guard, panic lock, and key isolation are delivered as
project-internal C99 headers and C sources under `include/oyama/security/` and
`security/src/`, with thin C++ wrappers in the Qt shell. They are not
third-party runtime plugins. BIP39 lives in the vault layer
(`include/oyama/bip39.h`, `vault/src/bip39.c`).

### 1.1 Feasibility verdict

| Project | Verdict | What is actually achievable |
| --- | --- | --- |
| **A. C++11 baseline + dual Qt** | Green, large | The C++11 port is mechanical but touches every C++ translation unit. Qt 6.4 remains the default; Qt 5.15.2 is an optional compatibility build. This is the prerequisite for B, C, and D. |
| **V. BIP39 recovery phrase** | Green | Replace the ad-hoc phrase with a 24-word BIP39 mnemonic (256 bits of entropy). The vault KDF stays Argon2id; BIP39 is only mnemonic generation and checksum validation. |
| **B. Panic lock + memory hygiene** | Green | A global hotkey, vault lock, `sodium_memzero`, `mlock`, core-dump suppression, and a solid-color re-unlock screen are all implementable in-process. |
| **C. Screenshot blackout** | Yellow, best-effort | Linux has no portable “deny capture” API. Process and D-Bus detection plus an opaque overlay reduce the exposure window but cannot guarantee that a fast or privileged capture tool never gets a frame. A real guarantee needs a cooperating compositor. |
| **D. Xen key isolation** | Yellow, architectural | A separate key guest is implementable and testable. It protects the DEK from a compromised UI guest. It does **not** protect against a dom0 administrator reading guest memory unless AMD SEV-SNP or Intel TDX is in use; that is a hardware/deployment property, not a header library. |

The design is deliberately honest about C and D. Shipping them as “extreme
security” without these caveats would be misleading.

---

## 2. Goals, non-goals, anti-goals

### Goals

- Keep the existing vault format and backup format readable and writable.
- Let a distribution or ordinary user choose one of three build profiles.
- Make every security module independently testable without a real desktop or
  a real Xen host.
- Make supply-chain provenance explicit: every third-party source archive has a
  pinned version and SHA-256, and the build can run offline from a local cache.
- Keep Qt 6.4 as the default build for ordinary users; keep a Qt 5.15.2
  compatibility build for conservative distributions and older systems.
- Keep the extra security modules off by default for the standard release.
- Generate new recovery phrases with BIP39 (24 words, 256 bits of entropy) while
  keeping existing vaults unlockable with their original phrases.

### Non-goals

- Defeating a compromised kernel, firmware, hypervisor, or physical attacker
  with unlimited time and hardware.
- Preventing a photograph of the screen. The panic lock is the response to that
  threat, not prevention.
- Hiding the fact that the application is running, or hiding its window title.
- Protecting plaintext after the user has unlocked the vault and an attacker
  already controls the same user account.
- Replacing full-disk encryption, a locked screen, or a trusted OS.

### Anti-goals

- A build that silently claims “screenshot-proof” or “dom0-proof.”
- A build that weakens the existing vault to make the new modules simpler.
- A default profile that requires Xen, a special kernel, or a compositor patch.
- Vendoring code without recording its origin, version, license, and hash.

---

## 3. Threat model

### 3.1 Assets

| Asset | Where it lives | Consequence of disclosure |
| --- | --- | --- |
| Vault DEK | `oyama_vault` process memory (standard), `oyama-keyd` guest memory (full) | Decrypts every record and backup. |
| Plaintext records | UI process memory, Qt models, PostgreSQL page cache | Health and identity disclosure. |
| Recovery phrase | User's paper, transiently in the unlock input | Permanent vault compromise. |
| Screen pixels | Compositor framebuffer | Disclosure to a screenshot tool or observer. |

### 3.2 Adversaries

| ID | Adversary | Capability | Mitigation |
| --- | --- | --- | --- |
| T1 | Desktop screenshot tool | Same user session, X11 or Wayland, captures the screen or a window | Capture guard + black overlay. Best-effort. |
| T2 | Physical observer / coercion | Can see the screen and demand access | Panic hotkey, gray lock screen, phrase re-entry, memory wipe. |
| T3 | Local process as the same user | Can read `/proc/<pid>/mem`, ptrace, inspect the PostgreSQL socket | `PR_SET_DUMPABLE=0`, `mlock`, `memfd_secret`, key isolation in full profile. Partial. |
| T4 | Compromised UI guest | Controls the desktop application and its guest memory | `oyama-keyd` in a separate guest; the UI guest never holds the DEK. |
| T5 | dom0 administrator | Can inspect guest memory through Xen | Not solvable by this project on ordinary hardware. Requires SEV-SNP/TDX or a separate physical machine. Documented as a limit. |
| T6 | Supply-chain attacker | Can tamper with a downloaded dependency | Pinned versions, SHA-256 verification, offline cache, SBOM, license review. |

### 3.3 Trust boundaries

```
+--------------------------------------------------------------+
| dom0: Xen toolstack, display server, optional management      |
|   - can read guest memory unless SEV/TDX is active (T5)       |
+---------------------------+----------------------------------+
                            | vchan (full profile only)
+---------------------------v----------------------------------+
| UI guest (domU): oyama-hrt-app                                |
|   - Qt/QML, PostgreSQL client, plaintext models               |
|   - never holds the DEK in full profile                       |
|   - capture guard, panic overlay                              |
+---------------------------+----------------------------------+
                            | vchan / Unix socket
+---------------------------v----------------------------------+
| Key guest (domU): oyama-keyd                                  |
|   - Argon2id, DEK, AES-256-GCM / XChaCha20-Poly1305           |
|   - no network, no display, minimal root filesystem           |
|   - mlock + memfd_secret + no core dumps                      |
+--------------------------------------------------------------+
```

In the **standard** profile there is no key guest; the UI process holds the DEK
inside `oyama_vault`, protected by `sodium_malloc` and the existing lock path.

In the **full** profile the UI guest may still see plaintext records while the
vault is unlocked. Key isolation protects the DEK, not the plaintext. This is
stated in the user-facing documentation.

---

## 4. Build profiles

A new cache variable selects the profile:

```cmake
set(OYAMA_SECURITY_PROFILE "standard" CACHE STRING
    "Security profile: off, standard, or full")
```

| Feature | `off` (debug) | `standard` (default) | `full` (secure release) |
| --- | --- | --- | --- |
| Default Qt | 6.4 | 6.4 | 6.4 or 5.15.2 |
| C++ standard | C++11 | C++11 | C++11 |
| Vault (Argon2id + AES-GCM/XChaCha20) | yes | yes | yes |
| BIP39 24-word recovery phrase | yes (new vaults) | yes (new vaults) | yes (new vaults) |
| 15-minute idle lock | no | yes | yes |
| Compiler hardening (`-fstack-protector-strong`, RELRO, etc.) | no | yes | yes |
| Capture guard | no | no | yes |
| Panic hotkey + memory wipe | no | no | yes |
| `oyama-keyd` / vchan key isolation | no | no | yes |
| Vendored third-party build | optional | yes (release) | yes |
| Xen deployment required | no | no | yes |

`off` turns off every **extra** security module: idle lock, compiler hardening,
capture guard, panic hotkey, and key isolation. It still uses the vault because
the storage layer has no plaintext mode; the vault is the data layer, not an
optional module. A plaintext no-vault debug store is a separate, larger change
and is not part of this roadmap. `off` builds with the system Qt 6.4 for a
comfortable development loop.

`standard` is the default for ordinary users: Qt 6.4, C++11, the vault, the
15-minute idle lock, compiler hardening, and BIP39 phrase generation. No Xen,
no extra daemons, no capture hooks.

`full` is the secure release: all modules compiled, `oyama-keyd` in a separate
key guest, capture guard, panic lock. It may be built with Qt 6.4 (recommended)
or Qt 5.15.2 (compatibility). The release notes state which Qt was used.

`OYAMA_QT_MAJOR` selects the Qt major version: `6` (default) or `5`. The Qt 5
build requires 5.15.2 exactly and uses the KDE Qt5 patch collection. The Qt 6
build requires 6.4 or newer.

`OYAMA_SECURITY_MODE` remains as the compiler-hardening switch for
compatibility. The profile sets it:

- `off` → `OYAMA_SECURITY_MODE=OFF`
- `standard` / `full` → `OYAMA_SECURITY_MODE=ON`

The profile is recorded in the binary (`OYAMA_SECURITY_PROFILE_*` compile
definition) and printed by `oyama-hrt-cli version --verbose` and by the app's
About panel.

---

## 5. Component design

### 5.1 Project-internal security headers

All public security APIs are C99 and live under `include/oyama/security/`.
Implementations live in `security/src/`. The Qt shell includes only the public
headers and wraps them in QObjects.

```text
include/oyama/security/
  export.h        visibility, OYAMA_NODISCARD, OYAMA_WARN_UNUSED
  mem.h           secure zero, secure alloc, mlock, core-dump control
  panic.h         panic hotkey registration and callback
  capture.h       screenshot-tool detection and callback
  vchan.h         Xen vchan transport (full profile only)
  keyproto.h      wire protocol between the UI guest and oyama-keyd
security/src/
  mem.c
  panic_x11.c
  panic_wayland.c
  panic_null.c
  capture_proc.c
  capture_dbus.c
  capture_null.c
  vchan.c
security/tests/
  mem_test.c
  panic_test.c
  capture_test.c
  vchan_test.c
```

The headers are not header-only in the “single-header” sense: complex backends
need a `.c` file and platform libraries. The *public surface* is a small set of
headers, and every module has a null backend so builds do not require X11,
D-Bus, or Xen.

#### `mem.h`

```c
void oyama_secure_zero(void *ptr, size_t len);
void *oyama_secure_alloc(size_t len);      /* mmap + mlock + guard page */
void oyama_secure_free(void *ptr, size_t len);
int  oyama_mlock(const void *ptr, size_t len);
int  oyama_munlock(const void *ptr, size_t len);
int  oyama_disable_core_dumps(void);
int  oyama_mark_dontdump(void *ptr, size_t len);
int  oyama_release_free_pages(void);        /* malloc_trim / madvise */
```

`oyama_secure_zero` uses `sodium_memzero` when libsodium is available and
`explicit_bzero` otherwise. `oyama_secure_alloc` fails closed: if `mlock`
fails, it zeroes and frees the mapping and returns `NULL`. `oyama_disable_core_dumps`
sets `RLIMIT_CORE` to zero and calls `prctl(PR_SET_DUMPABLE, 0)` on Linux.

#### `panic.h`

```c
typedef void (*oyama_panic_cb)(void *user_data);

typedef struct oyama_panic oyama_panic;

int  oyama_panic_create(oyama_panic **out, oyama_error *err);
int  oyama_panic_set_hotkey(oyama_panic *panic, const char *accelerator,
                            oyama_error *err);
int  oyama_panic_start(oyama_panic *panic, oyama_panic_cb cb,
                       void *user_data, oyama_error *err);
void oyama_panic_stop(oyama_panic *panic);
void oyama_panic_destroy(oyama_panic *panic);
```

Default accelerator: `Ctrl+Alt+Escape`. The accelerator is parsed by a small
internal parser; the accepted syntax is `Modifier+Modifier+Key`, where modifiers
are `Ctrl`, `Alt`, `Shift`, and `Super`, and the key is a single character or
one of `Escape`, `F1`–`F12`, `Space`, `Pause`.

Backends:

- **X11:** a dedicated thread opens its own `Display*`, calls `XGrabKey` with
  `AnyModifier`, and reads `KeyPress` events. It does not depend on Qt's X11
  extras. On a key press, it invokes the callback from a queued Qt invocation.
- **Wayland:** try `org.freedesktop.portal.GlobalShortcuts` through
  `xdg-desktop-portal` (version 1.17+). If the portal is absent or refuses,
  fall back to an application-local `QShortcut` and report `best_effort=1`.
- **Null:** used on builds without X11/Wayland support and in tests.

The panic callback is always invoked on the Qt main thread. The C library owns
the hotkey thread; the C++ wrapper marshals with `QMetaObject::invokeMethod`.

#### `capture.h`

```c
typedef void (*oyama_capture_cb)(void *user_data, const char *tool, int pid);

typedef struct oyama_capture_guard oyama_capture_guard;

int  oyama_capture_guard_create(oyama_capture_guard **out, oyama_error *err);
int  oyama_capture_guard_start(oyama_capture_guard *guard,
                               oyama_capture_cb cb, void *user_data,
                               oyama_error *err);
void oyama_capture_guard_stop(oyama_capture_guard *guard);
void oyama_capture_guard_destroy(oyama_capture_guard *guard);
int  oyama_capture_guard_is_best_effort(const oyama_capture_guard *guard);
```

Detection layers, in priority order:

1. **Process watch (all desktops):** poll `/proc` every 100 ms (configurable
   with `OYAMA_CAPTURE_POLL_MS`). Match `comm` against a list:
   `gnome-screenshot`, `spectacle`, `xfce4-screenshooter`, `mate-screenshot`,
   `deepin-screenshot`, `grim`, `flameshot`, `scrot`, `maim`, `import`,
   `ksnip`, `shutter`, `screengrab`. The list is compiled in and can be
   extended with `OYAMA_CAPTURE_TOOLS` (colon-separated).
2. **D-Bus name watch (optional):** subscribe to `NameOwnerChanged` on the
   session bus for screenshot services that are activated on demand
   (`org.kde.KWin.ScreenShot2`, `org.gnome.Shell.Screenshot` when it is not
   already owned). This catches service activation earlier than process
   polling.
3. **X11 RECORD (optional, best-effort):** use the X RECORD extension to watch
   `GetImage` requests. This is the closest thing to real detection on X11.
   It is optional because it requires `libXtst` and because a capture tool can
   still complete the request before the guard reacts.
4. **Wayland:** there is no client-side hook. The guard reports
   `best_effort=1` and relies on process polling only.

Blackout behavior:

- On detection, the Qt wrapper emits `captureDetected(QString tool)`.
- QML sets `captureOverlay.visible = true` and `captureOverlay.color = "#000000"`.
  The overlay covers the whole window, has `z: 10000`, and accepts no input.
- The window title is temporarily replaced with an empty string while the
  overlay is visible.
- The overlay is cleared after `OYAMA_CAPTURE_BLACKOUT_MS` (default 2000 ms) or
  when the detected process exits, whichever is later.
- If the vault is locked, the overlay is still shown; it never reveals data.

The guard is disabled when `OYAMA_SECURITY_PROFILE` is `off` or `standard`.

#### `vchan.h` and `keyproto.h`

See §5.4.

### 5.2 Project B — panic lock and memory hygiene

The panic path is a single function in `AppController`:

```cpp
void AppController::panicLock();
```

It performs these steps in order, and every step is idempotent:

1. Stop the idle timer and reject new data operations (`require_unlocked()`
   returns false).
2. Call `oyama_vault_lock(vault_)`; the C ABI zeroes the DEK and frees it with
   `sodium_free`.
3. Zero the setup phrase, challenge answers, pending migration phrase, and any
   transient `std::string` that held a phrase.
4. Clear `doses_`, `curve_`, and `settings_`; call `oyama_release_free_pages()`.
5. Call `oyama_secure_zero` on the Qt model storage where the container type
   allows it. Qt's implicit sharing means a full wipe is best-effort; the
   models are replaced with empty containers, and the old buffers are released.
6. Set `vault_state_ = "panic"`, emit `changed()`, and show
   `PanicLockPage.qml`.
7. `PanicLockPage` is a solid `#808080` background with a single phrase field
   and an unlock button. It has no navigation, no sidebar, and no data.
8. Unlocking calls the normal `unlockVault()` path. On success the normal shell
   is restored; on failure the gray screen remains.

The global hotkey is registered only in the `full` profile. The standard
profile keeps the existing 15-minute idle lock.

Memory hygiene is also applied at startup, regardless of profile:

- `prctl(PR_SET_DUMPABLE, 0)` on Linux (already present).
- `setrlimit(RLIMIT_CORE, 0)` (already present).
- `oyama_disable_core_dumps()` for the BSDs.
- `sodium_mlock`/`sodium_munlock` around the DEK (already via `sodium_malloc`).
- `MADV_DONTDUMP` on secure allocations.
- A best-effort `mlock` of the Qt input buffer is **not** attempted; Qt owns
  that memory and may reallocate it.

### 5.3 Project C — screenshot blackout

The Qt wrapper is a `CaptureGuard` QObject:

```cpp
class CaptureGuard : public QObject {
    Q_OBJECT
public:
    explicit CaptureGuard(QObject *parent = nullptr);
    ~CaptureGuard() override;
    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool bestEffort READ bestEffort NOTIFY activeChanged)
signals:
    void captureDetected(const QString &tool);
    void activeChanged();
};
```

`Main.qml` owns the overlay:

```qml
Rectangle {
    id: captureOverlay
    objectName: "captureOverlay"
    anchors.fill: parent
    color: "#000000"
    visible: false
    z: 10000
}
```

`AppController` connects `CaptureGuard::captureDetected` to a slot that sets
`captureOverlay.visible` through a property, starts a single-shot timer, and
restores the previous title.

The guard is **not** a guarantee. The documentation must say:

> The capture guard detects common screenshot tools and blacks the window.
> A tool that captures faster than the 100 ms poll, a compositor that ignores
> client hints, or a privileged process can still capture the screen. Use a
> compositor that denies screencopy for a hard guarantee.

### 5.4 Project D — Xen key isolation

#### Topology

- **UI guest (`oyama-ui`)**: runs `oyama-hrt-app`, Qt, the PostgreSQL client,
  and the capture guard. No DEK.
- **Key guest (`oyama-key`)**: runs `oyama-keyd`, a C99 daemon with a minimal
  root filesystem. It owns the DEK and the `vault_meta` row. No network, no
  display, no shell.
- **dom0**: Xen toolstack only. Provides vchan grant references through
  `xenstore`. The display server may run in dom0 or in the UI guest depending
  on the deployment; the reference deployment passes through a GPU to the UI
  guest.

#### Wire protocol

`keyproto.h` defines a fixed-size, versioned, little-endian protocol:

```c
#define OYAMA_KEY_PROTO_VERSION 1
#define OYAMA_KEY_MAX_FRAME 1048576

enum oyama_key_op {
    OYAMA_KEY_OP_HELLO   = 1,
    OYAMA_KEY_OP_UNLOCK  = 2,
    OYAMA_KEY_OP_LOCK    = 3,
    OYAMA_KEY_OP_PUT     = 4,
    OYAMA_KEY_OP_GET     = 5,
    OYAMA_KEY_OP_REMOVE  = 6,
    OYAMA_KEY_OP_SCAN    = 7,
    OYAMA_KEY_OP_STATUS  = 8,
    OYAMA_KEY_OP_PANIC   = 9
};

typedef struct oyama_key_header {
    uint32_t magic;      /* 'O','K','E','Y' */
    uint16_t version;
    uint16_t op;
    uint32_t request_id;
    uint32_t payload_len;
} oyama_key_header;
```

Every request is framed as `header || payload`. Every response is framed as
`header || status || payload`. The protocol is request/response with one
outstanding request per connection. `HELLO` carries the protocol version and a
random 32-byte nonce; the key guest checks the peer domain ID from the vchan
transport and rejects unknown domains.

`UNLOCK` carries the UTF-8 phrase. The key guest derives the KEK with Argon2id,
unwraps the DEK, zeroes the phrase and KEK, and replies with the vault state.
The DEK never crosses the vchan.

`PUT` carries `id`, `kind`, `updated_at_ms`, and plaintext payload; the key
guest encrypts and writes to PostgreSQL. `GET` carries an `id`; the key guest
reads and decrypts. `SCAN` streams records in frames terminated by a zero-length
frame. The UI guest's `oyama_vault` implementation delegates to these calls, so
`VaultStore` and the repositories keep their current interfaces.

#### Staging

- **D1 — local key daemon:** `oyama-keyd` listens on a Unix socket with
  `SO_PEERCRED` and the same protocol. This proves the split, the protocol, and
  the tests without Xen.
- **D2 — vchan transport:** replace the Unix socket with `libxenvchan`. Add
  `xenstore` grant setup, a reference `xl` configuration, and a smoke test
  using two local processes connected by a socketpair (the vchan backend is
  compiled but not required for CI).
- **D3 — confidential computing (optional):** detect AMD SEV-SNP / Intel TDX
  from `/dev/sev-guest` or CPUID, report it in `oyama-keyd status`, and document
  that only a confidential guest protects the DEK from dom0. No code pretends
  to provide this on ordinary hardware.

#### Key guest hardening

- `oyama_secure_alloc` for the DEK; fail closed if `mlock` fails.
- `memfd_secret` (Linux 5.14+) for the DEK page when available; fall back to
  `mlock`.
- `PR_SET_DUMPABLE=0`, `RLIMIT_CORE=0`, `MADV_DONTDUMP`.
- `seccomp` allowlist: `read`, `write`, `recvmsg`, `sendmsg`, `mmap`, `munmap`,
  `mprotect`, `futex`, `clock_gettime`, `exit_group`. PostgreSQL client calls
  are proxied through a separate worker process if the allowlist becomes
  impractical.
- No `fork`/`exec` after startup; no shell; no dynamic loading after init.
- Read-only root filesystem; the only writable path is the PostgreSQL socket
  and the vault metadata file.

### 5.5 Project A — C++11 baseline and dual Qt build

#### Language compatibility layer

Add `include/oyama/compat/` with C++11 implementations of the C++14/17/20
facilities the codebase uses. The aliases switch to `std::` when the compiler
is newer, so the same source builds on C++11 and C++20:

```cpp
// include/oyama/compat/optional.h
namespace oyama {
#if __cplusplus >= 201703L
using std::optional;
using std::nullopt;
#else
template <typename T> class optional { /* has_value, value, value_or, reset, emplace, operator*, operator-> */ };
struct nullopt_t {};
constexpr nullopt_t nullopt{};
#endif
}
```

The same pattern applies to `string_view`, `filesystem`, `make_unique`,
`clamp`, `size`, `data`, `as_const`, and the attribute macros.

`std::filesystem` is replaced by a POSIX-backed `oyama::fs` namespace that
implements only the operations the project uses: `path`, `exists`,
`is_directory`, `create_directories`, `remove`, `remove_all`, `rename`,
`directory_iterator`, `temp_directory_path`, and `current_path`. The
implementation uses `stat`, `mkdir`, `opendir`/`readdir`, `rename`, `unlink`,
and `rmdir`. It is Linux/BSD-only; Windows is not a target.

Language constructs that need rewriting:

| C++14/17/20 construct | C++11 replacement |
| --- | --- |
| `auto` return type deduction | explicit return type |
| generic lambdas | explicit parameter types |
| `std::make_unique` | `oyama::make_unique` |
| `if constexpr` | tag dispatch or overloads |
| structured bindings | `std::tie` or explicit members |
| `std::scoped_lock` | `std::lock_guard` / `std::lock` |
| `std::shared_mutex` | `pthread_rwlock_t` wrapper |
| `std::string_view` | `oyama::string_view` |
| `std::optional` | `oyama::optional` |
| `std::filesystem` | `oyama::fs` |
| `std::clamp`, `std::size`, `std::data`, `std::as_const` | `oyama::` helpers |
| `std::byte` | `unsigned char` |
| `[[nodiscard]]`, `[[maybe_unused]]` | `OYAMA_NODISCARD`, `OYAMA_MAYBE_UNUSED` |
| binary literals, digit separators | hex / plain decimal |
| `std::chrono_literals` | explicit `std::chrono::milliseconds(n)` |

The migration is verified by compiling with
`-std=c++11 -pedantic-errors` and by a CI job that also compiles with
`-std=c++20` to catch accidental use of C++14+ features.

#### Qt 6.4 default and Qt 5.15.2 compatibility

The default build uses Qt 6.4 (or newer) and keeps the current Qt 6 CMake path,
with a few changes so the same C++ sources work on both majors:

| Concern | Qt 6.4 default | Qt 5.15.2 compatibility |
| --- | --- | --- |
| CMake package | `find_package(Qt6 6.4 ...)` | `find_package(Qt5 5.15 ...)` |
| Target | `qt_add_executable` / `qt_add_qml_module` | `add_executable` / `qt5_add_resources` |
| QML loading | `engine.loadFromModule("OyamaHrt", "Main")` | `engine.load(QUrl("qrc:/qml/Main.qml"))` |
| QML imports | versionless (`import QtQuick`) | versioned, generated by `scripts/qml5ify.py` |
| Singletons | `QT_QML_SINGLETON_TYPE` | `pragma Singleton` + `qmldir` |
| Failure check | `objectCreationFailed` | `rootObjects().isEmpty()` |
| `QMouseEvent` | Qt 6 constructor | `#if QT_VERSION < QT_VERSION_CHECK(6,0,0)` branch |
| `QQuickWindow::mapToGlobal` | `QPointF` | `QPoint` |
| Qt policies | `qt_policy(QTP0001/QTP0004)` | none |

Qt 6 and Qt 5 disagree on QML imports: Qt 6 accepts versionless imports, while
Qt 5.15 requires explicit versions. Maintaining two QML trees would drift.
Instead, `scripts/qml5ify.py` copies `app/qml` into
`${CMAKE_BINARY_DIR}/qml5` and rewrites:

```text
import QtQuick          -> import QtQuick 2.15
import QtQuick.Controls -> import QtQuick.Controls 2.15
import QtQuick.Layouts  -> import QtQuick.Layouts 1.15
import QtQuick.Shapes   -> import QtQuick.Shapes 1.15
```

The script fails on an unknown `import Qt...` line, is idempotent, and writes a
`qmldir` with:

```text
singleton T 1.0 T.qml
singleton Theme 1.0 Theme.qml
```

The Qt 5 build compiles the generated tree; the Qt 6 build compiles the
original tree. A unit test (`qml5ify_test.py`) pins the exact rewrite.

The QML module URI `OyamaHrt` is not used by the QML files today (they use
relative imports), so the Qt 5 qrc prefix is `/qml` without a module URI. The
C++ `UiPreferences` singleton continues to use
`qmlRegisterSingletonInstance("OyamaHrt", 1, 0, "Ui", &ui)`, which exists in
Qt 5.14+.

`main.cpp` gets a small `#if QT_VERSION` block for the load call and the
failure check. The rest of the C++ code is Qt-version-neutral.

#### Build and test matrix

- **Default:** Linux X11 + Wayland, Qt 6.4+ (system), C++11, `standard`
  profile.
- **Compatibility:** Linux and FreeBSD, Qt 5.15.2 with the KDE Qt5 patch
  collection, C++11, `standard` or `full`.
- OpenBSD and NetBSD, Qt 6.4 from packages; if a module is missing, the shell
  is skipped exactly as the current Qt 6 build does.
- C++11 compile job (`-pedantic-errors`) and a C++20 compatibility compile job.
- The existing CTest suite must pass unchanged, plus the new security tests.
- The Qt 5.15.2 build is optional and is not the default. The release notes
  must state that open-source Qt 5.15.2 is EOL and that the KDE patch
  collection is applied.

### 5.6 Project V — BIP39 recovery phrase

New vaults use a 24-word BIP39 mnemonic instead of an ad-hoc phrase. This raises
the phrase entropy to 256 bits and gives the user a standard, checksummed
backup that can be written on paper and verified.

#### Scope

- **Generation:** 256 bits from the OS CSPRNG, 8-bit SHA-256 checksum, 24 words
  from the BIP39 English wordlist.
- **Validation:** word count, membership in the 2048-word list, and checksum.
- **No BIP39 seed derivation.** The vault already derives its KEK with
  Argon2id; applying BIP39's PBKDF2 on top would add cost without adding
  security. The mnemonic is passed to `oyama_vault_setup` as the phrase string.
- **Backward compatibility:** existing vaults keep their original phrase and
  unlock exactly as before. `change_phrase` may upgrade to BIP39, but no forced
  migration.

#### API

```c
/* include/oyama/bip39.h */
#define OYAMA_BIP39_WORDS 24
#define OYAMA_BIP39_MAX_LEN 512

int oyama_bip39_generate(char *out, size_t out_len, oyama_error *err);
int oyama_bip39_validate(const char *phrase, oyama_error *err);
int oyama_bip39_word_count(const char *phrase);
int oyama_bip39_normalize(const char *in, char *out, size_t out_len,
                          oyama_error *err);
```

`oyama_bip39_generate` writes a NUL-terminated 24-word phrase separated by
single spaces. It fails closed if the CSPRNG is unavailable. The wordlist lives
in `vault/src/bip39_wordlist.c` as a `static const char *const[2048]` and is
compiled into `oyama_vault`. `vault/src/vault.c` also exposes
`oyama_vault_setup_generated`, which calls `oyama_bip39_generate` and then
`oyama_vault_setup`, so callers that do not already have a phrase get a valid
one.

#### Entropy source

- Linux: `getrandom(2)` with a `/dev/urandom` fallback.
- FreeBSD/OpenBSD/NetBSD: `arc4random_buf`.
- The function returns `OYAMA_STATUS_CRYPTO` if no source succeeds; it never
  falls back to `rand()` or time.

#### UI flow

`SetupVaultPage.qml` displays the 24 words in a 4×6 grid, warns that they are
the only recovery path, and asks the user to confirm three random positions
(the existing challenge mechanism). A "copy" action is allowed but shows a
warning and clears the clipboard after 60 seconds. The phrase is never logged.

#### Tests

- BIP39/Trezor test vectors for 256-bit entropy (at least two).
- Generate → validate round-trip 100 times.
- Reject a phrase with a bad checksum, a non-wordlist word, 12 words, and 25
  words.
- A legacy vault created with an old phrase still unlocks.
- `oyama_bip39_generate` fails cleanly when the entropy source is stubbed to
  fail.

#### License

The BIP39 wordlist is distributed under the MIT license in the
`bitcoin/bips` repository. Record it in `THIRD_PARTY_NOTICES.md`.

### 5.7 Supply chain — vendored third-party build

The “heavy” profile builds these from source where the table says “vendored”:

| Dependency | Version policy | Why |
| --- | --- | --- |
| Qt 6 | 6.4.x or newer (system for `off`/`standard`; vendored for release) | Default user experience; receives upstream security fixes. |
| Qt 5 | 5.15.2 + pinned KDE Qt5 patch collection (optional) | Compatibility build for conservative distributions. Open-source 5.15.2 is EOL. |
| PostgreSQL | 14.x latest patch (or 16.x, decided in the plan) | The app needs both `libpq` and the server binaries (`initdb`, `pg_ctl`, `postgres`). |
| libsodium | 1.0.20 (verify at plan time) | Argon2id and AEAD primitives. |
| nlohmann/json | 3.11.x | Header-only JSON; vendored as a single header. |
| Xen tools (`libxenvchan`, `libxenstore`) | Matching the target Xen version | Full profile only. |

Layout:

```text
third_party/
  README.md
  versions.cmake          # version + SHA-256 for every archive
  cache/                  # optional offline tarballs (gitignored)
  prefix/                 # build output (gitignored)
  patches/                # pinned patches, each with a README
scripts/
  bootstrap-third-party.sh
  verify-third-party.sh
cmake/
  OyamaThirdParty.cmake
```

`bootstrap-third-party.sh`:

1. Reads `third_party/versions.cmake`.
2. For each archive: use `third_party/cache/<file>` if present; otherwise
   download from the pinned URL. In `--offline` mode, a missing archive is a
   hard error.
3. Verifies SHA-256 before extraction.
4. Applies the pinned patches with `patch -p1`.
5. Configures and builds into `third_party/prefix`.
6. Writes a stamp file with the version and hash.
7. Never runs `sudo`, never installs into `/usr`, never contacts the network
   after the download step.

`cmake/OyamaThirdParty.cmake`:

```cmake
set(OYAMA_THIRD_PARTY "vendored" CACHE STRING "vendored or system")
if(OYAMA_THIRD_PARTY STREQUAL "vendored")
    list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/third_party/prefix")
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/prefix/.stamp")
        message(FATAL_ERROR "Run scripts/bootstrap-third-party.sh first")
    endif()
endif()
```

`verify-third-party.sh` re-hashes every archive and every installed library and
compares against `versions.cmake`. It is a CTest test (`supply_chain_hashes`)
that skips when the cache is absent.

An SBOM (`third_party/sbom.json`, CycloneDX format) is generated by the
bootstrap script from `versions.cmake` and committed on release branches.

**Honest limitation:** vendoring controls provenance and build flags; it does
not make old code safe. The default Qt 6.4 build receives upstream fixes and is
the recommended choice. Qt 5.15.2 is from 2020 and receives no upstream
open-source security patches; the KDE patch collection helps, but the project
must track it. There is no commercial Qt 5.15 LTS license for this project, so
the compatibility build uses 5.15.2 plus the pinned patch collection. The
release notes must say so.

---

## 6. Branch and PR strategy

Each project is a separate branch based on the current `main`, with its own
PR. Merge in this order:

| Order | Branch | PR contents | Depends on |
| --- | --- | --- | --- |
| 0 | `project/0-supply-chain` | `third_party/`, bootstrap/verify scripts, profile skeleton, docs | — |
| 1 | `project/a-cpp11-qt` | compatibility layer, C++11 port, Qt 6.4 default, Qt 5.15.2 compatibility, test matrix | 0 |
| 2 | `project/v-bip39-phrase` | `bip39.h`, wordlist, generator/validator, vault integration, setup UI | 1 |
| 3 | `project/b-panic-lock` | `mem.h`, `panic.h`, panic UI, memory wipe tests | 1, 2 |
| 4 | `project/c-capture-guard` | `capture.h`, detection backends, black overlay, tests | 1 |
| 5 | `project/d-xen-keyd` | `vchan.h`, `keyproto.h`, `oyama-keyd`, Xen deployment docs | 1, 2, 3 |

B and C are independent of each other; they can be developed in parallel after
V, but their PRs land one at a time to keep `main` green. D depends on B for
the panic path and on V for the phrase format.

Every PR:

- rebases on `main` after the previous PR merges;
- keeps `main` green (`ctest` on Linux and at least one BSD);
- adds tests for the new behavior;
- updates `README.md`, `SECURITY.md`, `docs/dependencies.md`, and
  `CHANGELOG.md`;
- does not mix formatting-only changes with behavior changes.

---

## 7. Risks and open decisions

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Qt 5.15.2 is EOL for open source | Security regressions over time | Pin and track the KDE patch collection; document that no commercial license is available; add a patch-tracking task. |
| Screenshot detection is reactive | A fast tool captures a frame | State best-effort; recommend a cooperating compositor; keep the overlay immediate. |
| dom0 can read guest memory | Key isolation is weaker than it sounds | Document; offer SEV/TDX detection in D3; do not claim dom0 resistance. |
| C++11 polyfills add code | New bugs and audit surface | Small, tested, header-only; compile the same code as C++20 to catch divergence. |
| Vendored Qt/PostgreSQL builds are large | Build time and disk | Cache tarballs, build once, stamp outputs; keep a `system` fallback for distro packaging. |
| QML versioned imports change rendering | UI regressions | Run the existing screenshot matrix (`capture_ui.sh`) and the QML tests on Qt 6.4 (default) and Qt 5.15.2 (compatibility). |
| Panic hotkey conflicts with the desktop | Panic may not fire | Configurable accelerator; detect grab failure and warn; fall back to an app-local shortcut. |
| `oyama-keyd` becomes a crypto oracle | A compromised UI guest can decrypt while unlocked | Document; add per-session nonce, peer domain check, and optional user-presence token in D3. |

### Decisions made in this design

1. **Profile names** are `off`, `standard`, and `full`; the default is
   `standard`.
2. **Qt default is 6.4 for every profile.** The `off`, `standard`, and `full`
   builds use the system Qt 6.4 by default for a comfortable user and developer
   experience. A Qt 5.15.2 compatibility build is available with
   `OYAMA_QT_MAJOR=5`.
3. **No commercial Qt license.** The compatibility build uses open-source Qt
   5.15.2 plus the pinned KDE Qt5 patch collection. The release notes state
   that open-source 5.15.2 is EOL.
4. **Panic hotkey default** is `Ctrl+Alt+Escape`. The literal “EAC” key does not
   exist on standard keyboards; if a different chord is intended, change the
   default in `panic.h`.
5. **Panic unlock uses the recovery phrase.** New vaults use a 24-word BIP39
   mnemonic; old vaults keep their original phrase. There is no separate app
   password.
6. **Key guest topology is a separate domU.** The maintainer confirmed this.
   Running the key service in dom0 would give it no isolation from the dom0
   administrator.
7. **D is staged** as D1 (Unix socket) → D2 (vchan) → D3 (confidential
   computing). D1 is testable on an ordinary Linux machine.
8. **BIP39 is for mnemonic generation only.** The vault KEK stays Argon2id;
   BIP39's PBKDF2 seed derivation is not used.
9. **PostgreSQL** stays at 14.x for the vendored build to match the documented
   minimum; the system build continues to accept 14+.
10. **`off` disables the extra security modules, not the vault.** The vault is
    the data layer; a plaintext no-vault debug store is out of scope.

### Question for the maintainer

1. Does `off` need a plaintext (no-vault) debug mode in addition to “vault
   without extra modules”? The current design assumes no. A plaintext store is
   a separate change to the storage layer, not a profile switch.
