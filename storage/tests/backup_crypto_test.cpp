#include <oyama/storage/backup_crypto.h>
#include <oyama/storage/error.h>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace oyama::storage;

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

template <typename T>
void check_equal(const T& actual, const T& expected, const std::string& message) {
    if (!(actual == expected)) {
        std::cerr << "FAIL: " << message << " (actual=" << actual
                  << ", expected=" << expected << ")\n";
        ++failures;
    }
}

template <typename Fn>
void check_throws(Fn&& fn, const std::string& message) {
    try {
        fn();
    } catch (const Error&) {
        return;
    }
    std::cerr << "FAIL: " << message << " (did not throw Error)\n";
    ++failures;
}

std::uint32_t read_u32(std::string_view bytes, std::size_t offset) {
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

void write_u32(std::string& bytes, std::size_t offset, std::uint32_t value) {
    bytes[offset] = static_cast<char>((value >> 24U) & 0xffU);
    bytes[offset + 1] = static_cast<char>((value >> 16U) & 0xffU);
    bytes[offset + 2] = static_cast<char>((value >> 8U) & 0xffU);
    bytes[offset + 3] = static_cast<char>(value & 0xffU);
}

std::size_t nonce_length(std::uint8_t algorithm) {
    if (algorithm == 1) {
        return 12;
    }
    if (algorithm == 2) {
        return 24;
    }
    return 0;
}

std::size_t header_length(std::string_view sealed) {
    constexpr std::size_t fixed = 8 + 1 + 1 + 2 + 4 + 4 + 16;
    return fixed + nonce_length(
                       static_cast<std::uint8_t>(
                           static_cast<unsigned char>(sealed[9])));
}

void test_round_trip() {
    const std::string json =
        R"({"format":"oyama-hrt-complete-backup","version":1})";
    const std::string phrase = "correct horse battery staple";
    const auto sealed = encrypt_backup_json(json, phrase);

    check(is_encrypted_backup(sealed), "magic detected");
    check_equal(decrypt_backup_json(sealed, phrase), json, "round-trip");
    check(sealed.find("oyama-hrt-complete-backup") == std::string::npos,
          "plaintext absent");
    check_equal(sealed.substr(0, 8), std::string("OYAMAEBK"),
                "magic is exactly OYAMAEBK");
    check_equal(static_cast<std::uint8_t>(
                    static_cast<unsigned char>(sealed[8])),
                std::uint8_t{1}, "format version is 1");
    const auto algorithm =
        static_cast<std::uint8_t>(static_cast<unsigned char>(sealed[9]));
    check(algorithm == 1 || algorithm == 2, "algorithm is known");
    check_equal(static_cast<unsigned int>(
                    static_cast<unsigned char>(sealed[10])) |
                    (static_cast<unsigned int>(
                         static_cast<unsigned char>(sealed[11]))
                     << 8U),
                0U, "reserved bytes are zero");
    const auto ops = read_u32(sealed, 12);
    const auto mem = read_u32(sealed, 16);
    check(ops >= 1 && ops <= 10, "stored opslimit is within bounds");
    check(mem >= 8U * 1024U * 1024U && mem <= 256U * 1024U * 1024U,
          "stored memlimit is within bounds");
    check_equal(sealed.size(), header_length(sealed) + json.size() + 16,
                "ciphertext length is implicit");

    const auto sealed_again = encrypt_backup_json(json, phrase);
    check(sealed != sealed_again, "two seals of the same JSON differ");

    const std::string messy = "  Correct\tHORSE  battery ";
    const auto normalized_seal = encrypt_backup_json(json, messy);
    check_equal(decrypt_backup_json(normalized_seal, "correct horse battery"),
                json, "phrase normalization matches on both sides");

    check(!is_encrypted_backup(""), "empty input is not encrypted");
    check(!is_encrypted_backup("OYAMAEB"), "short magic is not encrypted");
    check(!is_encrypted_backup(json), "plaintext JSON is not encrypted");
    check_throws([&] { (void)decrypt_backup_json(json, phrase); },
                 "plaintext input is rejected by decrypt");
    check_throws([&] { (void)encrypt_backup_json(json, "   "); },
                 "empty phrase is rejected by encrypt");
    check_throws([&] { (void)decrypt_backup_json(sealed, "wrong phrase"); },
                 "wrong phrase is rejected");
}

void test_tampering() {
    const std::string json = R"({"a":1})";
    const std::string phrase = "correct horse battery staple";
    const auto sealed = encrypt_backup_json(json, phrase);
    const auto header = header_length(sealed);

    for (std::size_t index = 0; index < header; ++index) {
        auto tampered = sealed;
        tampered[index] = static_cast<char>(
            static_cast<unsigned char>(tampered[index]) ^ 0x01U);
        check_throws([&] { (void)decrypt_backup_json(tampered, phrase); },
                     "header byte " + std::to_string(index) +
                         " flip is rejected");
    }

    for (std::size_t index = header; index < sealed.size(); ++index) {
        auto tampered = sealed;
        tampered[index] = static_cast<char>(
            static_cast<unsigned char>(tampered[index]) ^ 0x01U);
        check_throws([&] { (void)decrypt_backup_json(tampered, phrase); },
                     "ciphertext byte " + std::to_string(index - header) +
                         " flip is rejected");
    }
}

void test_truncation() {
    const std::string json = R"({"format":"oyama-hrt-complete-backup"})";
    const std::string phrase = "correct horse battery staple";
    const auto sealed = encrypt_backup_json(json, phrase);
    const auto header = header_length(sealed);

    const std::vector<std::size_t> cuts = {
        0,      1,          7,          8,          9,   10,
        11,     19,         20,         35,         header - 1,
        header, header + 1, sealed.size() - 1};
    for (const auto cut : cuts) {
        if (cut >= sealed.size()) {
            continue;
        }
        const auto truncated = sealed.substr(0, cut);
        check_throws([&] { (void)decrypt_backup_json(truncated, phrase); },
                     "truncation at " + std::to_string(cut) +
                         " byte(s) is rejected");
    }
}

void test_kdf_header_bounds() {
    const std::string json = R"({"a":1})";
    const std::string phrase = "correct horse battery staple";
    const auto sealed = encrypt_backup_json(json, phrase);

    const auto expect_rejected = [&](std::uint32_t ops, std::uint32_t mem,
                                     const std::string& message) {
        auto tampered = sealed;
        write_u32(tampered, 12, ops);
        write_u32(tampered, 16, mem);
        check_throws([&] { (void)decrypt_backup_json(tampered, phrase); },
                     message);
    };

    expect_rejected(0, 64U * 1024U * 1024U, "opslimit 0 is rejected");
    expect_rejected(11, 64U * 1024U * 1024U, "opslimit 11 is rejected");
    expect_rejected(0xffffffffU, 64U * 1024U * 1024U,
                    "huge opslimit is rejected before the KDF");
    expect_rejected(3, 8U * 1024U * 1024U - 1U,
                    "memlimit below 8 MiB is rejected");
    expect_rejected(3, 256U * 1024U * 1024U + 1U,
                    "memlimit above 256 MiB is rejected");
    expect_rejected(3, 0xffffffffU,
                    "huge memlimit is rejected before the KDF");

    // The rejected headers must leave the original file intact.
    check_equal(decrypt_backup_json(sealed, phrase), json,
                "original seal still opens after rejected variants");
}

}  // namespace

int main() {
    try {
        test_round_trip();
        test_tampering();
        test_truncation();
        test_kdf_header_bounds();
    } catch (const std::exception& error) {
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
        return 1;
    }
    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "backup_crypto_test: ok\n";
    return 0;
}
