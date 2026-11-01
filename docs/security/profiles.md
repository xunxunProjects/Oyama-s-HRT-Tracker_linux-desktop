# Security profiles

`OYAMA_SECURITY_PROFILE` selects how much of the hardening stack is built. The
allowed values are `off`, `standard`, and `full`; the default is `standard`.

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

## `off`

`off` is the debug profile. It disables every **extra** security module: idle
lock, compiler hardening, capture guard, panic hotkey, and key isolation. It
still uses the vault because the storage layer has no plaintext mode; the vault
is the data layer, not an optional module.

## `standard`

`standard` is the default for ordinary users: Qt 6.4, the vault, the 15-minute
idle lock, compiler hardening, and BIP39 phrase generation. No Xen, no extra
daemons, no capture hooks.

## `full`

`full` is the secure release: all modules compiled, `oyama-keyd` in a separate
key guest, capture guard, and panic lock. It may be built with Qt 6.4
(recommended) or Qt 5.15.2 (compatibility). Release notes state which Qt was
used.

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
# oyama-hrt-cli 0.5.0
# security-profile: standard
```

The profile is recorded in the binary through the
`OYAMA_SECURITY_PROFILE_OFF`, `OYAMA_SECURITY_PROFILE_STANDARD`, and
`OYAMA_SECURITY_PROFILE_FULL` compile definitions. Exactly one is `1`.

## Implementation status

Phase 0 wires the profile switch, compiler hardening, and the CLI report. The
idle lock, BIP39 phrase generation, capture guard, panic lock, and `oyama-keyd`
arrive in later phases; until then the table describes the target behavior of
each profile rather than the current 0.5.0 feature set.
