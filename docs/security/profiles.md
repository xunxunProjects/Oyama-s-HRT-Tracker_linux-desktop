# Security profiles

`OYAMA_SECURITY_PROFILE` selects how much of the hardening stack is built. The
allowed values are `off`, `standard`, and `full`; the default is `standard`.
The three profiles all keep the encrypted vault; they differ in the optional
hardening modules compiled into the binary.

| Feature | `off` (debug) | `standard` (default) | `full` (secure release) |
| --- | --- | --- | --- |
| Default Qt | 6.4 | 6.4 | 6.4 |
| Qt 5.15.2 compatibility (`OYAMA_QT_MAJOR=5`) | optional | optional | optional |
| C++11 compatibility baseline | yes | yes | yes |
| Vault (Argon2id + AES-GCM/XChaCha20) | yes | yes | yes |
| BIP39 24-word recovery phrase | yes (new vaults) | yes (new vaults) | yes (new vaults) |
| 15-minute idle lock | yes | yes | yes |
| Compiler hardening (`-fstack-protector-strong`, RELRO, etc.) | no | yes | yes |
| Capture guard | no | no | yes |
| Panic lock + memory wipe | no | no | yes |
| `oyama-keyd` / vchan key isolation | no | no | yes |
| Vendored third-party build | optional | optional | optional |

## `off`

`off` is the debug profile. It disables compiler hardening and every **extra**
full-profile security module: capture guard, panic lock, and key isolation. It
still uses the vault because the storage layer has no plaintext mode; the vault
is the data layer, not an optional module. The application's 15-minute idle
lock remains available.

## `standard`

`standard` is the default for ordinary users: Qt 6.4, the vault, the 15-minute
idle lock, compiler hardening, and BIP39 phrase generation. No Xen, no extra
daemons, no capture hooks.

## `full`

`full` is the secure release: all modules compiled, including `oyama-keyd` for
the reference key-guest deployment, the capture guard, and the panic lock. It
may be built with Qt 6.4 (recommended) or Qt 5.15.2 through
`-DOYAMA_QT_MAJOR=5`. Release notes state which Qt was used.

The key-guest deployment is optional at runtime. The `full` binary includes the
daemon and protocol; a desktop installation can run without Xen. See
[`key-isolation.md`](key-isolation.md) and `deploy/xen/README.md`.

## Selecting a profile

```sh
cmake -S . -B build -G Ninja -DOYAMA_SECURITY_PROFILE=standard
cmake -S . -B build-off -G Ninja -DOYAMA_SECURITY_PROFILE=off
cmake -S . -B build-full -G Ninja -DOYAMA_SECURITY_PROFILE=full
```

With the Makefile wrapper, `SECURITY=1` (the default) selects `standard` and
`SECURITY=0` selects `off`:

```sh
make                 # standard
make SECURITY=0      # off
```

`OYAMA_SECURITY_MODE` remains as the compiler-hardening switch for
compatibility. The profile sets it: `off` sets it to `OFF`; `standard` and
`full` set it to `ON`. Do not pass `OYAMA_SECURITY_MODE` directly; a stale
cache entry is ignored.

## Checking the profile

```sh
oyama-hrt-cli version --verbose
# oyama-hrt-cli 0.6.0
# security-profile: standard
```

The profile is recorded in the binary through the
`OYAMA_SECURITY_PROFILE_OFF`, `OYAMA_SECURITY_PROFILE_STANDARD`, and
`OYAMA_SECURITY_PROFILE_FULL` compile definitions. Exactly one is `1`.

## Release status

All modules listed above are part of the 0.6.0 release. `off` and `standard`
omit the full-only modules at build time; they do not provide a plaintext
store. The capture guard is best-effort, the panic lock is a response path
rather than prevention, and the key-guest deployment is a reference design with
the limits documented in [`key-isolation.md`](key-isolation.md).
