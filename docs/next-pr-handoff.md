# Handoff: finish the merge, then debug the merged desktop UI

Status date: 2026-10-31. This is the handoff for the next session and the next
pull request. It is deliberately separate from the 0.5.0 release notes.

## Current state

- Branch: `feat/encrypted-vault`
- PR: #16, `feat(vault): encrypt the desktop data store for 0.5.0`
- Base: `main`
- `origin/main` moved to `615a3c8` (v0.1.2 desktop UI overhaul).
- The merge of `origin/main` into `feat/encrypted-vault` is **in progress and
  not committed**.
- All textual conflicts are resolved and staged. `git diff --name-only
  --diff-filter=U` is empty.
- `app_controller.cpp` and `main.cpp` compile after the merge, and
  `oyama-hrt-app` builds successfully.
- The headless `--self-test` exits 0 on the merged tree.
- The CLI was rebuilt successfully and prints `oyama-hrt-cli 0.5.0`.
- `docs/dependencies.md` replaces the former Chinese-named dependency file;
  README and Makefile references have been updated.
- The wiki is pushed at commit `5218d22` and already describes 0.5.0 and PR #16.
- Release and ASan suites were 24/24 before the merge and must be rerun on the
  merged tree.

## Unfinished: finish and publish the merge

1. Rebuild the app target with low parallelism: done.
   `cmake --build build-release --target oyama-hrt-app -j1`
2. Run the full merged test suite:
   `PG_BINDIR=/tmp/opencode/pg18/bin ctest --test-dir build-release --output-on-failure`
3. Run the headless QML smoke test: done, exit 0.
   `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software OYAMA_CONFIG_FILE=/tmp/opencode/oyama-no-config ./build-release/oyama-hrt-app --self-test`
4. Run the release checks again if any app/QML code changed after step 2.
5. Inspect the staged diff, then commit the merge without an editor:
   `git commit --no-edit -m "Merge origin/main into feat/encrypted-vault"`
6. Push `feat/encrypted-vault` and confirm PR #16 becomes mergeable.
7. After PR #16 merges, update the wiki links that currently point at
   `blob/main/SECURITY.md` and PR #16 so they resolve against the merge commit.

## Next PR: debug and polish the merged desktop UI

The next PR should be treated as the stabilization pass for the 0.5.0 merge. It
should not add new storage features.

### P0: merge regressions and vault/UI integration

- Verify the app boots with the new single-window shell in every vault state:
  `unavailable`, `uninitialized`, `locked`, `unlocked`, `migrating`.
- Confirm the vault gate covers the sidebar, page header, pages, and the
  first-run intro, and that the underlying shell cannot receive input while
  locked.
- Check the interaction between the new `FirstRun` language/theme intro and the
  vault setup gate. Language selection must not start vault setup, and vault
  setup must not skip the intro.
- Verify the lock action in the new sidebar rail; its `qsTr("Lock")` string is
  not in `app/i18n/*.json` yet.
- Localize the vault pages (`VaultGate.qml`, `SetupVaultPage.qml`,
  `UnlockVaultPage.qml`) through the project `T`/i18n mechanism. They currently
  use English `qsTr` strings, so Chinese, Russian, and Persian stay English.
- Check Persian RTL layout for the vault pages and the new shell.
- Exercise the app controller scenarios against the merged QML: fresh vault,
  managed legacy pending, successful migration, crash-between-renames recovery,
  and retry after a bad row.
- Restore or extend screenshot coverage for the new shell and the vault states.
  Existing CTest smoke coverage reaches only the `unavailable` state.
- Confirm the 15-minute idle lock still resets on keyboard and pointer activity
  in the new shell.
- Confirm `--self-test` still finds `vaultGate` and `appShell` after QML
  restructuring.

### P1: deferred security hardening from the final vault review

These were accepted as Minor findings and intentionally deferred:

- Zero phrase and plaintext temporaries in `VaultStore`, `Passphrase`,
  `backup.cpp`, the CLI, and `AppController`. Some `std::string` copies still
  rely on normal destruction rather than `sodium_memzero`.
- Install `include/oyama/vault.h` with the CLI and verify the public header is
  usable from an installed prefix.
- Extend `vault_crypto_test` byte-flip coverage to the four-byte record header
  (currently starts at offset 4).
- Add a CPU-feature-masked test for the AES-NI-less XChaCha fallback; the
  current host cannot exercise that runtime branch.
- Verify FreeBSD/OpenBSD process-dump hardening (`procctl`) and directory
  `fsync` semantics on real BSD hosts or VMs.
- Consider making the idle-lock duration configurable; the spec left this to
  the builder.
- Add an automated test for the `LegacyMigrationWorker` target-pending branch;
  the current controller crash-recovery scenario covers the reload path, not
  the worker path directly.

### P2: product gaps still outside 0.5.0

- Labs page.
- Plan / What-if page and forecast UI.
- Supplies page.
- Backup import/export UI.
- Apply PK calibration to the curve; the current shell stores the calibration
  method/history but does not use it in the simulation.
- Packaging, installers, and a BSD/X11/Wayland test matrix.
- Single-instance guard, tray integration, and desktop notifications.
- Full upstream UI parity and final visual review on a compositor.

## Verification commands

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DOYAMA_BUILD_QT_SHELL=ON
cmake --build build-release -j1
PG_BINDIR=/tmp/opencode/pg18/bin ctest --test-dir build-release --output-on-failure
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  OYAMA_CONFIG_FILE=/tmp/opencode/oyama-no-config \
  ./build-release/oyama-hrt-app --self-test
```

ASan/UBSan:

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DOYAMA_BUILD_QT_SHELL=ON \
  -DCMAKE_C_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-asan -j1
PG_BINDIR=/tmp/opencode/pg18/bin \
  LSAN_OPTIONS=suppressions=/tmp/opencode/lsan-fontconfig.supp \
  ctest --test-dir build-asan --output-on-failure
```

The fontconfig LeakSanitizer suppression is environment noise, not project code.

## Key files

- Vault C ABI: `include/oyama/vault.h`
- Crypto core: `vault/src/vault_crypto.c`, `vault/src/vault.c`
- C++ wrapper: `storage/include/oyama/storage/vault_store.h`,
  `storage/src/vault_store.cpp`
- Repositories: `storage/src/vault_*_repository.cpp`
- Backup encryption: `storage/src/backup_crypto.cpp`, `storage/src/backup.cpp`
- Legacy migration/promotion: `storage/src/legacy_migration.cpp`,
  `storage/src/legacy_promotion.cpp`
- Desktop controller: `app/src/app_controller.cpp`
- Migration worker: `app/src/legacy_migration_worker.cpp`
- Shell and gate: `app/qml/Main.qml`, `app/qml/VaultGate.qml`
- Design: `docs/superpowers/specs/2026-10-31-encrypted-vault-design.md`
- Plan: `docs/superpowers/plans/2026-10-31-encrypted-vault.md`

## Acceptance criteria for the next PR

- The merged tree builds and all CTests pass in Release and ASan/UBSan.
- The headless self-test exits 0.
- The app can be opened, locked, unlocked, and closed in every vault state.
- Vault UI strings are translated in all four locales or explicitly documented
  as not yet translated.
- No merge regression is left in `app/qml/Main.qml`,
  `app/src/app_controller.cpp`, or `app/src/main.cpp`.
- Deferred security items are either fixed or listed in the PR with a clear
  rationale.
