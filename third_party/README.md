# Third-party sources

This directory pins and builds the large dependencies from source. The goal is
reproducible provenance and an offline-capable build, not a claim that old code
is automatically safe.

## Layout

```text
third_party/
  versions.cmake   # version, URL, SHA-256, and patches for every dependency
  cache/           # downloaded archives (gitignored)
  build/           # extracted source trees and build output (gitignored)
  prefix/          # installed headers, libraries, and tools (gitignored)
  patches/         # pinned patches, each with its own README
  sbom.json        # generated CycloneDX SBOM (Task 0.5)
```

`cache/`, `build/`, and `prefix/` are never committed. The release artifacts
carry the SBOM and the notices, not the source trees.

## Filling hashes

Every `OYAMA_<DEP>_SHA256` must be a real SHA-256. The placeholder `<fill>` is
rejected by `scripts/verify-third-party.sh` with `UNPINNED`.

1. Download the archive from the pinned URL.
2. Run `sha256sum <archive>` (Linux) or `shasum -a 256 <archive>` (BSD).
3. Put the archive in `third_party/cache/` and the hash in
   `third_party/versions.cmake`.
4. Run `scripts/verify-third-party.sh` to confirm.

Never edit a hash without changing the version, and never edit a version
without re-checking the hash.

## Offline build

```sh
scripts/bootstrap-third-party.sh --offline
```

The offline mode uses only `third_party/cache/`. If an archive is missing it
fails with the file name and the cache path. It never downloads, never runs
`sudo`, and never installs outside `third_party/prefix`.

## Default and compatibility Qt

The default desktop build uses Qt 6.4.3. Qt 5.15.2 is an optional compatibility
build (`OYAMA_QT_MAJOR=5`) and needs the pinned KDE Qt5 patch collection. The
open-source Qt 5.15.2 release is end-of-life upstream; track the patch
collection and do not describe it as receiving upstream security fixes.
