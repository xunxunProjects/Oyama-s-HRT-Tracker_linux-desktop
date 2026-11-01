#pragma once
#include <oyama/compat/all.h>

#include <string>
#include <string_view>

namespace oyama { namespace storage {

/// Returns true when `bytes` starts with the encrypted-backup magic
/// (`OYAMAEBK`).  The magic alone does not prove that the envelope is valid;
/// decrypt_backup_json performs the full validation.
OYAMA_NODISCARD bool is_encrypted_backup(oyama::string_view bytes);

/// Seals the upstream backup JSON with an Argon2id-derived key and an AEAD.
///
/// The envelope is `OYAMAEBK` v1: magic[8], version u8, algorithm u8,
/// reserved u16, opslimit u32, memlimit u32, salt[16], nonce[12 or 24],
/// ciphertext, tag[16].  All integers are big-endian.  The AAD is the full
/// header through the nonce.  AES-256-GCM is used when the CPU supports it;
/// otherwise XChaCha20-Poly1305 is used.
///
/// The phrase is normalized with Passphrase::normalize before derivation.
/// Throws Error when the phrase is empty or libsodium fails.
OYAMA_NODISCARD std::string encrypt_backup_json(oyama::string_view json,
                                              oyama::string_view phrase);

/// Validates and opens an encrypted backup envelope.
///
/// The magic, version, algorithm, reserved bytes, nonce length, and stored
/// KDF bounds (opslimit 1..10, memlimit 8 MiB..256 MiB) are checked before
/// any Argon2id work.  Throws Error for malformed input, an unsupported
/// algorithm, a wrong phrase, or a tampered/truncated file.
OYAMA_NODISCARD std::string decrypt_backup_json(oyama::string_view bytes,
                                              oyama::string_view phrase);

} }  // namespace oyama::storage
