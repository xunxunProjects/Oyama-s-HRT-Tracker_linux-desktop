# Security Policy

Oyama's HRT Tracker for Linux and BSD stores health information. We take
security reports seriously and are grateful for every report, including
low-risk hardening findings and documentation problems.

## Supported versions

Security fixes land in the latest release. Please reproduce on the latest
release or on the latest commit of `main` before reporting.

| Version | Supported |
| ------- | --------- |
| 0.5.x   | Yes       |
| 0.1.x and older | No |

## Reporting a vulnerability

**Do not open a public issue, discussion, or pull request for a security
problem.** Use GitHub's private advisory form:

<https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/security/advisories/new>

Reports may be written in English. Include:

- the application version and commit, or the release archive name;
- the operating system, desktop session (X11 or Wayland), and whether the
  backend is an application-managed cluster or an existing PostgreSQL server;
- whether the vault was locked, unlocked, or migrating when the problem
  occurred;
- the shortest reproduction sequence, using synthetic records only;
- the impact: what an attacker gains, which data or capability is exposed,
  and whether it survives a restart or a lock;
- redacted logs, screenshots, or command output. Never attach real health
  data, a recovery phrase, a database file, or a live key.

If you are unsure whether something is in scope, report it privately and ask.
We would rather triage a false positive than miss a real issue.

## What to expect

- We aim to acknowledge a report within **7 days**.
- We aim to share an initial assessment within **30 days**.
- Confirmed problems are fixed with a regression test and released as soon as
  users can update.
- We publish a GitHub security advisory after a fixed release is available.
  Please allow a reasonable disclosure window; we will coordinate a date with
  you.
- We credit reporters in the advisory unless you ask to stay anonymous.

If the issue is being exploited in the wild, say **urgent** in the subject and
we will prioritise containment.

## Scope

In scope:

- the C99 vault ABI and cryptography in `include/oyama/vault.h`, `vault/src/`,
  and the libsodium integration;
- the C++ `VaultStore`, strict codecs, vault-backed repositories, backup
  encryption, and legacy migration/promotion code;
- the CLI, including phrase input, backup handling, and `db migrate-vault`;
- the Qt/QML application, vault gate, setup/unlock/lock flow, idle lock, and
  managed PostgreSQL lifecycle;
- local PostgreSQL configuration written by the application, file modes, Unix
  socket handling, and migration files;
- documentation that could lead a user into an unsafe setup.

Out of scope for the application security policy:

- a malicious process or user that can act as the same local account while the
  vault is unlocked (it can read process memory, the PostgreSQL socket, or
  input);
- cold-boot, memory-freezing, DMA, or physical-access attacks against a
  running machine;
- a compromised operating system, kernel, firmware, or PostgreSQL server;
- recovery of a lost phrase, a forgotten phrase, or a deleted database;
- medical accuracy, a concentration estimate that differs from a lab result,
  or a treatment decision;
- denial of service, load testing, or brute force against a system you do not
  own;
- third-party dependencies, distributions, or deployment environments, except
  where this repository's code makes the issue reachable.

## Threat model

The vault protects health records at rest. Encrypted record blobs, `vault_meta`,
WAL, backups, and a powered-off disk contain ciphertext rather than plaintext
records. The recovery phrase is never written by the application, logged,
passed in argv, or offered through a clipboard.

The vault does **not** protect against a local attacker who can already act as
your user while the vault is unlocked. It also cannot promise that Qt, libpq,
PostgreSQL, the kernel page cache, swap, a core dump, or a hibernation image
never held a copy of plaintext or a key. Use full-disk encryption (for example
LUKS2), a locked screen, a trusted OS, and suspend/hibernate settings you
understand.

The old plaintext cluster created by a pre-vault release remains a separate
exposure. Deleting it is not guaranteed erasure on SSDs, copy-on-write
filesystems, snapshots, RAID, or backups. Full-disk encryption and destruction
of the old medium are the only reliable remedies.

There is no phrase escrow and no back door. Losing the phrase means losing the
data.

## Hardening checklist

- [ ] Enable full-disk encryption before storing health records.
- [ ] Keep the operating system and Qt, libpq, PostgreSQL, and libsodium
      packages up to date.
- [ ] Lock the vault when you leave the machine; the application also locks
      after 15 minutes of inactivity and on quit.
- [ ] Keep the recovery phrase on paper in a safe place. Do not store it in a
      password manager, a shell history, an environment variable, or a
      screenshot.
- [ ] Treat encrypted backups as sensitive. Store them on encrypted media and
      test that the phrase opens them before deleting the only copy.
- [ ] After migrating an old plaintext cluster, remove the
      `<data_dir>-plaintext-<timestamp>` directory only after verifying the new
      vault, and understand that deletion is not secure erasure on every
      filesystem.
- [ ] Do not run the application as root and do not expose the PostgreSQL
      socket to other users.

## Report templates

Use `docs/security/vulnerability-report-template.md` for a structured report.
The template covers Critical/RCE, High, Medium, Low/hardening, information
disclosure, access control, privacy/metadata, cryptographic, dependency, and
denial-of-service findings. It is a reporting aid, not a scoring decision; the
maintainers may re-rate after reproducing the issue.

## Safe harbour

We will not pursue legal action against researchers who:

- test only systems they own or have explicit permission to test;
- use synthetic data and stop at the first proof;
- do not access, modify, or retain another person's data;
- do not run destructive commands, install persistence, or move laterally;
- report privately and allow a reasonable fix window before disclosure.

## Medical disclaimer

The application shows estimates from a pharmacokinetic model. They are not
medical advice, a diagnosis, or a prescription. Do not start, stop, or change a
medication based on this application alone.
