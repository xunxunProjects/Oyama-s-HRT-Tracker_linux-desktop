#include <oyama/storage/backup_crypto.h>

#include <oyama/storage/error.h>
#include <oyama/storage/passphrase.h>

#include <sodium.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace oyama { namespace storage {
namespace {

constexpr std::array<char, 8> kMagic = {'O', 'Y', 'A', 'M', 'A',
                                        'E', 'B', 'K'};
constexpr std::uint8_t kFormatVersion = 1;
constexpr std::uint8_t kAlgorithmAes256Gcm = 1;
constexpr std::uint8_t kAlgorithmXChaCha20Poly1305 = 2;
constexpr std::size_t kSaltBytes = 16;
constexpr std::size_t kTagBytes = 16;
constexpr std::size_t kFixedHeaderBytes = kMagic.size() + 1 + 1 + 2 + 4 + 4 +
                                          kSaltBytes;
constexpr std::size_t kAesNonceBytes = 12;
constexpr std::size_t kXChaChaNonceBytes = 24;
constexpr std::size_t kKeyBytes = 32;

constexpr std::uint32_t kMinOpsLimit = 1;
constexpr std::uint32_t kMaxOpsLimit = 10;
constexpr std::uint32_t kMinMemLimit = 8U * 1024U * 1024U;
constexpr std::uint32_t kMaxMemLimit = 256U * 1024U * 1024U;
constexpr std::uint32_t kDefaultOpsLimit = 3;
constexpr std::uint32_t kDefaultMemLimit = 64U * 1024U * 1024U;

constexpr std::size_t kVersionOffset = 8;
constexpr std::size_t kAlgorithmOffset = 9;
constexpr std::size_t kReservedOffset = 10;
constexpr std::size_t kOpsLimitOffset = 12;
constexpr std::size_t kMemLimitOffset = 16;
constexpr std::size_t kSaltOffset = 20;
constexpr std::size_t kNonceOffset = kSaltOffset + kSaltBytes;

struct SensitiveString {
    std::string value;
    ~SensitiveString() {
        if (!value.empty()) {
            sodium_memzero(value.empty() ? nullptr : &value[0], value.size());
        }
    }
};

struct SensitiveKey {
    std::array<unsigned char, kKeyBytes> bytes{};
    ~SensitiveKey() {
        sodium_memzero(bytes.data(), bytes.size());
    }
};

struct SensitiveBytes {
    std::vector<unsigned char> value;
    ~SensitiveBytes() {
        if (!value.empty()) {
            sodium_memzero(value.empty() ? nullptr : &value[0], value.size());
        }
    }
};

std::size_t nonce_length(std::uint8_t algorithm) {
    switch (algorithm) {
        case kAlgorithmAes256Gcm:
            return kAesNonceBytes;
        case kAlgorithmXChaCha20Poly1305:
            return kXChaChaNonceBytes;
        default:
            return 0;
    }
}

void write_u32(std::string& out, std::size_t offset, std::uint32_t value) {
    out[offset] = static_cast<char>((value >> 24U) & 0xffU);
    out[offset + 1] = static_cast<char>((value >> 16U) & 0xffU);
    out[offset + 2] = static_cast<char>((value >> 8U) & 0xffU);
    out[offset + 3] = static_cast<char>(value & 0xffU);
}

std::uint32_t read_u32(oyama::string_view bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(
                static_cast<unsigned char>(bytes[offset]))
            << 24U) |
           (static_cast<std::uint32_t>(
                static_cast<unsigned char>(bytes[offset + 1]))
            << 16U) |
           (static_cast<std::uint32_t>(
                static_cast<unsigned char>(bytes[offset + 2]))
            << 8U) |
           static_cast<std::uint32_t>(
               static_cast<unsigned char>(bytes[offset + 3]));
}

void ensure_sodium() {
    if (sodium_init() < 0) {
        throw Error("libsodium initialization failed");
    }
}

const unsigned char* as_bytes(const char* data) {
    return reinterpret_cast<const unsigned char*>(data);
}

unsigned char* as_bytes(char* data) {
    return reinterpret_cast<unsigned char*>(data);
}

}  // namespace

bool is_encrypted_backup(oyama::string_view bytes) {
    return bytes.size() >= kMagic.size() &&
           std::memcmp(bytes.data(), kMagic.data(), kMagic.size()) == 0;
}

std::string encrypt_backup_json(oyama::string_view json,
                                oyama::string_view phrase) {
    ensure_sodium();
    SensitiveString normalized{Passphrase::normalize(phrase)};
    if (normalized.value.empty()) {
        throw Error("backup phrase must not be empty");
    }

    const auto algorithm = crypto_aead_aes256gcm_is_available() == 1
                               ? kAlgorithmAes256Gcm
                               : kAlgorithmXChaCha20Poly1305;
    const auto nonce_bytes = nonce_length(algorithm);
    const auto header_bytes = kFixedHeaderBytes + nonce_bytes;

    std::string sealed(header_bytes + json.size() + kTagBytes, '\0');
    std::memcpy(&sealed[0], kMagic.data(), kMagic.size());
    sealed[kVersionOffset] = static_cast<char>(kFormatVersion);
    sealed[kAlgorithmOffset] = static_cast<char>(algorithm);
    sealed[kReservedOffset] = '\0';
    sealed[kReservedOffset + 1] = '\0';
    write_u32(sealed, kOpsLimitOffset, kDefaultOpsLimit);
    write_u32(sealed, kMemLimitOffset, kDefaultMemLimit);
    randombytes_buf(as_bytes(&sealed[kSaltOffset]), kSaltBytes);
    randombytes_buf(as_bytes(&sealed[kNonceOffset]), nonce_bytes);

    SensitiveKey key;
    if (crypto_pwhash(
            key.bytes.data(), key.bytes.size(), normalized.value.data(),
            normalized.value.size(), as_bytes(&sealed[kSaltOffset]),
            kDefaultOpsLimit, kDefaultMemLimit,
            crypto_pwhash_ALG_ARGON2ID13) != 0) {
        throw Error("could not derive the backup key");
    }

    unsigned long long cipher_length = 0;
    int status = 0;
    if (algorithm == kAlgorithmAes256Gcm) {
        status = crypto_aead_aes256gcm_encrypt(
            as_bytes(&sealed[header_bytes]), &cipher_length,
            as_bytes(json.data()), json.size(), as_bytes(&sealed[0]),
            header_bytes, nullptr,
            as_bytes(&sealed[kNonceOffset]), key.bytes.data());
    } else {
        status = crypto_aead_xchacha20poly1305_ietf_encrypt(
            as_bytes(&sealed[header_bytes]), &cipher_length,
            as_bytes(json.data()), json.size(), as_bytes(&sealed[0]),
            header_bytes, nullptr,
            as_bytes(&sealed[kNonceOffset]), key.bytes.data());
    }
    if (status != 0 || cipher_length != json.size() + kTagBytes) {
        throw Error("could not encrypt the backup");
    }
    return sealed;
}

std::string decrypt_backup_json(oyama::string_view bytes,
                                oyama::string_view phrase) {
    if (!is_encrypted_backup(bytes)) {
        throw Error("not an encrypted backup");
    }
    if (bytes.size() < kVersionOffset + 1) {
        throw Error("encrypted backup header is truncated");
    }
    if (static_cast<std::uint8_t>(
            static_cast<unsigned char>(bytes[kVersionOffset])) !=
        kFormatVersion) {
        throw Error("unsupported encrypted backup version");
    }
    ensure_sodium();
    if (bytes.size() < kAlgorithmOffset + 1) {
        throw Error("encrypted backup header is truncated");
    }
    const auto algorithm =
        static_cast<std::uint8_t>(static_cast<unsigned char>(
            bytes[kAlgorithmOffset]));
    const auto nonce_bytes = nonce_length(algorithm);
    if (nonce_bytes == 0) {
        throw Error("unsupported encrypted backup algorithm");
    }
    if (algorithm == kAlgorithmAes256Gcm &&
        crypto_aead_aes256gcm_is_available() != 1) {
        throw Error(
            "encrypted backup uses AES-256-GCM, which is not available on "
            "this machine");
    }
    if (bytes.size() < kReservedOffset + 2) {
        throw Error("encrypted backup header is truncated");
    }
    if (bytes[kReservedOffset] != '\0' || bytes[kReservedOffset + 1] != '\0') {
        throw Error("encrypted backup reserved bytes are invalid");
    }
    if (bytes.size() < kOpsLimitOffset + 4) {
        throw Error("encrypted backup header is truncated");
    }
    const auto ops_limit = read_u32(bytes, kOpsLimitOffset);
    if (ops_limit < kMinOpsLimit || ops_limit > kMaxOpsLimit) {
        throw Error("encrypted backup opslimit is out of range");
    }
    if (bytes.size() < kMemLimitOffset + 4) {
        throw Error("encrypted backup header is truncated");
    }
    const auto mem_limit = read_u32(bytes, kMemLimitOffset);
    if (mem_limit < kMinMemLimit || mem_limit > kMaxMemLimit) {
        throw Error("encrypted backup memlimit is out of range");
    }
    if (bytes.size() < kFixedHeaderBytes) {
        throw Error("encrypted backup header is truncated");
    }
    const auto header_bytes = kFixedHeaderBytes + nonce_bytes;
    if (bytes.size() < header_bytes + kTagBytes) {
        throw Error("encrypted backup is truncated");
    }

    SensitiveString normalized{Passphrase::normalize(phrase)};
    if (normalized.value.empty()) {
        throw Error("backup phrase must not be empty");
    }

    SensitiveKey key;
    if (crypto_pwhash(
            key.bytes.data(), key.bytes.size(), normalized.value.data(),
            normalized.value.size(), as_bytes(bytes.data() + kSaltOffset),
            ops_limit, mem_limit, crypto_pwhash_ALG_ARGON2ID13) != 0) {
        throw Error("could not derive the backup key");
    }

    const auto cipher_bytes = bytes.size() - header_bytes;
    const auto expected_plain_bytes = cipher_bytes - kTagBytes;
    SensitiveBytes plaintext;
    plaintext.value.assign(expected_plain_bytes == 0 ? 1 : expected_plain_bytes,
                           '\0');
    unsigned long long plain_length = 0;
    int status = 0;
    if (algorithm == kAlgorithmAes256Gcm) {
        status = crypto_aead_aes256gcm_decrypt(
            plaintext.value.data(), &plain_length, nullptr,
            as_bytes(bytes.data() + header_bytes),
            static_cast<unsigned long long>(cipher_bytes),
            as_bytes(bytes.data()), header_bytes,
            as_bytes(bytes.data() + kNonceOffset), key.bytes.data());
    } else {
        status = crypto_aead_xchacha20poly1305_ietf_decrypt(
            plaintext.value.data(), &plain_length, nullptr,
            as_bytes(bytes.data() + header_bytes),
            static_cast<unsigned long long>(cipher_bytes),
            as_bytes(bytes.data()), header_bytes,
            as_bytes(bytes.data() + kNonceOffset), key.bytes.data());
    }
    if (status != 0 || plain_length != expected_plain_bytes) {
        throw Error(
            "backup phrase is wrong or the encrypted backup is damaged");
    }
    return std::string(reinterpret_cast<const char*>(plaintext.value.data()),
                       expected_plain_bytes);
}

} }  // namespace oyama::storage
