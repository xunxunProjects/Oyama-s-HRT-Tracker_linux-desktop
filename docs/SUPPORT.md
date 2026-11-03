# Support policy

> **Do not contact the developer for support. Read the man pages, the
> documentation in `docs/`, and the GitHub wiki first; only open a GitHub issue
> if those do not answer your question.**

This is a small, local-first project. The developer does **not** provide
individual support, private troubleshooting, account help, data recovery, or
one-to-one medical or technical advice. Support is self-service and
community-visible.

This policy applies to every channel: email, chat, social media, direct
messages, code-review comments, and questions attached to unrelated issues or
pull requests. A direct message is not a support channel.

## The support order

Use these resources in order. Stop at the first one that answers the question.

1. **The man pages.** These are installed with the release archives:
   - `man oyama-hrt-cli` for commands, options, phrase input, and backups.
   - `man oyama-hrt-app` for the desktop application.
2. **The repository documentation.** Start at
   [`docs/README.md`](README.md). It indexes the security pages, dependency
   notes, policies, and release/verification documents.
3. **The GitHub wiki.**
   <https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/wiki>
   The wiki has installation, building, CLI, usage, security, storage,
   backup-format, testing, troubleshooting, FAQ, and NetBSD pages.
4. **The GitHub issue tracker.** Only after the first three steps do not answer
   the question:
   <https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/issues>
   Search existing issues before opening a new one.

Please do not skip to step 4. Most questions are already answered by the man
pages, `docs/`, or the wiki, and a question that repeats documented material
will usually be closed with a link.

## What the developer does not provide

The developer does not provide:

- individual setup, installation, build, or debugging help;
- recovery of a lost, forgotten, or damaged recovery phrase;
- recovery of a deleted database, a corrupted backup, or a failed disk;
- medical advice, dose recommendations, interpretation of lab results, or a
  treatment plan;
- private consultation by email, chat, or direct message;
- guaranteed response times for ordinary questions;
- support for old releases after a newer release is available.

There is no phrase escrow and no back door. If the recovery phrase is lost, the
data is lost. No support request can change that.

## Before opening a GitHub issue

Do all of the following first:

1. Reproduce on the latest release, **0.6.0**, or on the latest commit of
   `main`. State which one you used.
2. Read the matching man page and the relevant `docs/` or wiki page.
3. Search open and closed issues for the same symptom.
4. Remove real health data, recovery phrases, database files, keys, and
   personal identifiers from every log, screenshot, and command output.
5. Reduce the problem to the shortest sequence of synthetic steps that still
   shows it.

If the problem is a security vulnerability, do **not** open a public issue.
Follow [`SECURITY.md`](../SECURITY.md) and use the private advisory form.

## What to include in an issue

A useful issue includes:

- **Version output.** Paste the exact output of:

  ```sh
  oyama-hrt-cli version --verbose
  ```

  It must report `oyama-hrt-cli 0.6.0` and the active
  `security-profile: <name>`.
- **Platform.** Operating system and version, CPU architecture, and desktop
  session (X11 or Wayland). For a source build, include the compiler, Qt
  version, and CMake generator.
- **Profile.** `off`, `standard`, or `full`, as reported by the command above.
  For a release archive, include the archive name.
- **Backend.** Whether PostgreSQL is an application-managed cluster or an
  existing local server.
- **Steps to reproduce.** Numbered steps using synthetic records only.
- **Expected and actual behaviour.** Include the exact error text.
- **Logs.** Redacted terminal output, `oyama-hrt-cli db status`,
  `oyama-hrt-cli db cluster-status`, or a screenshot of the failing screen.
  Never attach a recovery phrase, a real database, a private key, or real
  health records.
- **What you already tried.** Link the man page, docs, or wiki section you
  checked.

An issue that omits the version, platform, profile, and reproduction steps may
be closed without investigation.

## Security vulnerabilities

Security reports are handled privately and are **not** ordinary support
requests. Do not open a public issue, discussion, or pull request with exploit
details.

Use the private advisory form linked from [`SECURITY.md`](../SECURITY.md):

<https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/security/advisories/new>

Include the application version or commit, the operating system and backend,
whether the vault was locked or unlocked, reproduction steps with synthetic
data, the impact, and redacted logs. See `SECURITY.md` for scope, response
targets, and safe-harbour terms.

## Documentation fixes

Documentation bugs are welcome as GitHub issues. If a man page, `docs/` page,
or wiki page is wrong, say which page, which version, and what the correct
behaviour is. Do not include secrets or real health data in the report.

## Medical disclaimer

This application shows estimates from a pharmacokinetic model. They are not
medical advice, a diagnosis, or a prescription. Do not start, stop, or change a
medication based on this application alone. A support issue is not a medical
consultation.
