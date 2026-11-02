# Changelog

## Unreleased

- Pin Qt, PostgreSQL, libsodium, nlohmann/json, and Xen to checksum-verified
  source archives, with an offline bootstrap script and a CycloneDX SBOM.
- Add `off`, `standard`, and `full` security profiles. `standard` is the
  default; `oyama-hrt-cli version --verbose` reports the active profile.

## 0.5.1 — 2026-11-01

- Align vault pages with the PR #10 design system.
- Add a Makefile security-mode switch (`SECURITY=0/1`).
- Add cross-platform man pages.
- Add XFCE/KDE desktop integration.
- Add NetBSD build and packaging support.
- Harden backup handling and CLI secret exposure.
- Complete OpenBSD build and test support.

## 0.5.0 — 2026-11-01

- First release with the application-layer encrypted vault.
- Argon2id KEK, AES-256-GCM/XChaCha20-Poly1305 records, UUID-bound AAD.
- Seven-word EFF paper recovery phrase.
- Legacy plaintext migration with SHA-256 verification and atomic promotion.
- Encrypted `OYAMAEBK` backups by default.
- Vault-gated Qt/QML shell.

## 0.1.2 — 2026-10-30

- Support Qt 6.4 and clear settings hover state.

## 0.1.1 — 2026-10-30

- Desktop UI overhaul with i18n, warm theme, and E2 chart.

## 0.1.0 — 2026-10-30

- First desktop-interface milestone.