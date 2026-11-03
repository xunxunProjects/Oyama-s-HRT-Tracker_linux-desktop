# Documentation index

> **Do not contact the developer for support. Read the man pages, the
> documentation in `docs/`, and the GitHub wiki first; only open a GitHub issue
> if those do not answer your question.**

This page is the entry point for repository documentation for version
**0.6.0**. It covers the installed man pages, every security note in
`docs/security/`, dependency and build notes, project policies, the GitHub wiki,
and the release verification documents.

## Start here

| Need | Read |
| --- | --- |
| Install a release archive | [Installation](https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/wiki/Installation) and `man oyama-hrt-cli` |
| Run the desktop application | `man oyama-hrt-app` and [User Guide](https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/wiki/User-Guide) |
| Use the CLI | `man oyama-hrt-cli` and [CLI Reference](https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/wiki/CLI-Reference) |
| Build from source | [`dependencies.md`](dependencies.md) and [Building](https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/wiki/Building) |
| Understand the security model | [`SECURITY.md`](../SECURITY.md), [`security/profiles.md`](security/profiles.md), and [Security](https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/wiki/Security) |
| Report a vulnerability | [`SECURITY.md`](../SECURITY.md) and [`security/vulnerability-report-template.md`](security/vulnerability-report-template.md) |
| Ask for help | [`SUPPORT.md`](SUPPORT.md) first |

## Man pages

The release archives install two manual pages:

- `man/oyama-hrt-cli.1` — commands, global options, phrase handling, backups,
  and the `SUPPORT` section.
- `man/oyama-hrt-app.1` — desktop application, self-test options, files, and
  the `SUPPORT` section.

Read them with `man oyama-hrt-cli` and `man oyama-hrt-app`, or with
`man ./man/oyama-hrt-cli.1` from a source checkout.

## Security documentation

| Page | Contents |
| --- | --- |
| [`security/profiles.md`](security/profiles.md) | `off`, `standard`, and `full` profiles; compiler hardening; `version --verbose` |
| [`security/recovery-phrase.md`](security/recovery-phrase.md) | 24-word BIP39 phrase generation, checksum, validation, and limits |
| [`security/panic.md`](security/panic.md) | Full-profile panic lock, memory wiping, and gray lock screen |
| [`security/capture-guard.md`](security/capture-guard.md) | Best-effort screenshot detection, overlay, and limits |
| [`security/key-isolation.md`](security/key-isolation.md) | `oyama-keyd`, the Xen key guest, and the versioned wire protocol |
| [`security/vulnerability-report-template.md`](security/vulnerability-report-template.md) | Structured private vulnerability report template |

The canonical policy, supported versions, scope, threat model, and safe-harbour
terms are in [`SECURITY.md`](../SECURITY.md). Ordinary support questions are
not vulnerability reports; see [`SUPPORT.md`](SUPPORT.md).

## Build, dependencies, and design

- [`dependencies.md`](dependencies.md) — build and runtime dependencies by
  platform, package names, vendored builds, and migration lookup.
- [`superpowers/specs/2026-10-29-linux-desktop-design.md`](superpowers/specs/2026-10-29-linux-desktop-design.md)
  — desktop design.
- [`superpowers/specs/2026-10-31-encrypted-vault-design.md`](superpowers/specs/2026-10-31-encrypted-vault-design.md)
  — vault format, key hierarchy, and threat model.
- [`superpowers/specs/2026-11-02-security-architecture-design.md`](superpowers/specs/2026-11-02-security-architecture-design.md)
  — 0.6.0 security profiles, BIP39, panic lock, capture guard, key isolation,
  and supply-chain design.
- [`superpowers/plans/`](superpowers/plans/) — implementation plans (historical).

## Project policies

- [`SUPPORT.md`](SUPPORT.md) — self-service support policy, issue contents, and
  the boundary between support and security reports.
- [`../CONTRIBUTING.md`](../CONTRIBUTING.md) — contribution rules, tests, pull
  requests, and the "before asking for help" pointer.
- [`../SECURITY.md`](../SECURITY.md) — vulnerability reporting, supported
  versions, scope, threat model, and hardening checklist.
- [`../AI-CONTRIBUTORS.md`](../AI-CONTRIBUTORS.md) — rules for AI-assisted
  contributions.
- [`../THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md) — bundled fonts,
  icons, Qt, PostgreSQL, libsodium, nlohmann/json, Xen, and the BIP39 wordlist.
- [`../CHANGELOG.md`](../CHANGELOG.md) — release history, including the 0.6.0
  entry.

## Release and verification

- [Release 0.6.0](https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/releases/tag/v0.6.0)
  — nine platform archives and the source tarball.
- [`VERIFYING.md`](https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/releases/download/v0.6.0/VERIFYING.md)
  — the signed release verification steps.
- [`SHA512SUMS`](https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/releases/download/v0.6.0/SHA512SUMS)
  — ten SHA-512 entries: nine archives and the source tarball.
- [`RELEASE-NOTES-0.6.0.md`](https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/releases/download/v0.6.0/RELEASE-NOTES-0.6.0.md)
  — the release announcement.
- [`NOTICE-NetBSD.md`](https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/releases/download/v0.6.0/NOTICE-NetBSD.md)
  — why NetBSD is not built for 0.6.0.
- [Verification](https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/wiki/Verification)
  — wiki version of the checksum, signature, fingerprint, and release-bundle
  instructions.
- [Release Notes](https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/wiki/Release-Notes)
  — 0.6.0 changes, the six merged security pull requests, and the NetBSD
  notice.

## GitHub wiki

The wiki is the canonical user-facing documentation:

<https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/wiki>

It covers installation, building, CLI reference, usage, user guide, security,
security reporting, security research, verification, troubleshooting, FAQ,
support, release notes, recent updates, architecture, storage schema, backup
format, dependencies, testing, development, AI contributions, and NetBSD.

## Documentation rules for contributors

- Every command, option, path, table, and version in a document must exist in
  the code or in a release artifact.
- Update `README.md`, the matching `docs/` page, and the wiki page in the same
  change when behaviour changes.
- Do not rewrite historical changelog entries. Add a new entry instead.
- Do not put a recovery phrase, real health data, a private key, or a live
  database in an example, screenshot, issue, or pull request.
