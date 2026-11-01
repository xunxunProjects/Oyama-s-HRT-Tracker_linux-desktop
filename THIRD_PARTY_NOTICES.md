# Third-party notices

Oyama's HRT Tracker bundles the following third-party assets.

## Inter

- Files: `app/fonts/InterVariable.ttf`
- Copyright: 2016 The Inter Project Authors (https://github.com/rsms/inter)
- License: SIL Open Font License 1.1. The full license text is in
  `app/fonts/OFL.txt`.

## Vazirmatn

- Files: `app/fonts/Vazirmatn-Regular.ttf`,
  `app/fonts/Vazirmatn-Medium.ttf`, `app/fonts/Vazirmatn-SemiBold.ttf`
- Copyright: 2015-2023 The Vazirmatn Project Authors
  (https://github.com/rastikerdar/vazirmatn)
- License: SIL Open Font License 1.1. The full license text is in
  `app/fonts/OFL.txt`.

## Lucide-style icon paths

- Files: `app/qml/Icon.qml`
- Copyright: Lucide Contributors (https://lucide.dev); portions derived
  from Feather, copyright Cole Bemis 2013-2022.
- License: ISC.

```text
ISC License

Copyright (c) for portions of Lucide are held by Cole Bemis 2013-2022 as
part of Feather (MIT). All other copyright (c) for Lucide are held by
Lucide Contributors 2022.

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
```

## Qt 6.4.3

- Files: Qt 6 runtime and development libraries linked by the desktop shell.
- Copyright: The Qt Company and contributors.
- License: LGPL-3.0 or GPL-3.0 for open-source builds; a commercial license is
  available from The Qt Company. This project uses the open-source terms.
- Source: pinned in `third_party/versions.cmake` and built by
  `scripts/bootstrap-third-party.sh` for vendored builds.

## Qt 5.15.2 (compatibility, end-of-life)

- Files: optional Qt 5 compatibility build (`OYAMA_QT_MAJOR=5`).
- Copyright: The Qt Company and contributors.
- License: LGPL-3.0 or GPL-3.0 for open-source builds.
- Status: the open-source Qt 5.15.2 release is end-of-life upstream. The build
  uses the pinned KDE Qt5 patch collection; this is downstream maintenance, not
  an upstream security-fix stream.

## PostgreSQL

- Files: libpq and, for the vendored build, the PostgreSQL server and client
  tools.
- Copyright: PostgreSQL Global Development Group.
- License: PostgreSQL License (OSI-approved, MIT-like).

## libsodium

- Files: the `libsodium` library linked by the vault and CLI.
- Copyright: Frank Denis and contributors.
- License: ISC.

## nlohmann/json

- Files: the `nlohmann/json` headers used by the storage and CLI layers.
- Copyright: Niels Lohmann and contributors.
- License: MIT.

## Xen

- Files: optional hypervisor and vchan headers used by the `full` profile key
  isolation build.
- Copyright: The Xen Project and contributors.
- License: GPL-2.0.

## BIP39 wordlist

- Files: the BIP39 English wordlist used for 24-word recovery phrases (added in
  Phase V).
- Copyright: the Bitcoin BIPs contributors.
- License: MIT.

## Vendored build license texts

The vendored build compiles from the archives pinned in
`third_party/versions.cmake`; it does not download code at build time unless the
bootstrap script is run online. The upstream install rules place the full
license texts under `third_party/prefix/share/doc` after bootstrap. The
generated CycloneDX SBOM is `third_party/sbom.json`.
