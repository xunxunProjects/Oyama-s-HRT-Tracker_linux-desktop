# Contributing

Thank you for contributing to Oyama's HRT Tracker. This project stores health
data and has an encrypted vault; security-sensitive changes require care.

## Before you start

1. Read `README.md`, `SECURITY.md`, and the relevant design note under
   `docs/superpowers/specs/`.
2. Build and run the baseline tests before editing.
3. Work on a feature branch and open a pull request against `main`.

## Rules

- One logical change per branch.
- Include tests with the change; a bug fix needs a regression test.
- Run the full suite, not only the touched test:
  `PG_BINDIR=/path/to/postgres/bin ctest --test-dir build --output-on-failure`.
- Never commit real health data, recovery phrases, tokens, or private keys.
- Never add a code path that writes plaintext health data to PostgreSQL, a log,
  argv, shell history, or the clipboard.
- Do not weaken the vault AAD, nonce handling, KDF parameters, strict codecs,
  or backup authentication.
- Update `README.md` and the matching wiki page when behaviour changes.
- If you change the threat model, update `SECURITY.md` in the same pull
  request.

## Pull requests

Describe the problem, the change, the security impact, the verification
commands and results, and any migration or compatibility concern. State whether
the work was assisted by an AI.

## Reporting vulnerabilities

Do not open a public issue or pull request with exploit details. Use the
private advisory form linked from `SECURITY.md`.

## Wiki

The project wiki is the canonical user and developer documentation:
https://github.com/xunxunProjects/Oyama-s-HRT-Tracker_linux-desktop/wiki

For AI-specific standards, see `AI-CONTRIBUTORS.md`.