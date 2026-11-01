# Recovery phrase

New vaults use a 24-word BIP39 mnemonic as their recovery phrase. The phrase
is 256 bits of entropy plus an 8-bit SHA-256 checksum, encoded as 24 words
from the BIP39 English wordlist.

## What it is

- 24 words = 256 bits of entropy. The phrase is generated from the operating
  system CSPRNG (`getrandom(2)` on Linux, `arc4random_buf` on the BSDs, with a
  `/dev/urandom` fallback). It never falls back to `rand()` or time.
- An 8-bit checksum (the first byte of SHA-256 over the entropy) is appended,
  giving 264 bits total, split into 24 groups of 11 bits, each mapping to one
  word in the 2048-word list.
- The phrase is the only recovery path. The application never stores it, logs
  it, passes it in argv, or offers it through a clipboard without a warning.

## What it is not

- BIP39's PBKDF2 seed derivation is **not** used. The vault derives its key
  encryption key with Argon2id, exactly as before. BIP39 here is mnemonic
  generation and checksum validation only.
- Existing vaults keep their original phrase. `change_phrase` may upgrade to a
  BIP39 phrase, but there is no forced migration and no change to the vault
  KDF, AEAD selection, or backup format.

## Validation

`oyama_bip39_validate` checks:

1. Word count is exactly 24.
2. Every word is in the BIP39 English wordlist.
3. The checksum matches.

A phrase with a bad checksum, a non-wordlist word, 12 words, or 25 words is
rejected. `oyama_bip39_normalize` collapses whitespace and lowercases ASCII
before validation.

## Limits

- Losing the phrase means losing the data. There is no escrow and no back door.
- The phrase protects against a lost or reformatted disk, not against an
  attacker who can read process memory while the vault is unlocked.
- The English wordlist is the only supported list in this release.
