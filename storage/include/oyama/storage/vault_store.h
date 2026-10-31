#pragma once

#include "connection.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace oyama::storage {

enum class VaultState {
    Uninitialized,
    Locked,
    Unlocked,
};

struct VaultKdfParams {
    std::uint64_t opslimit{3};
    std::uint64_t memlimit{64 * 1024 * 1024};
};

struct VaultRecord {
    std::string id;
    std::string kind;
    std::uint64_t updated_at_ms{0};
    std::string payload;
};

class VaultError : public std::runtime_error {
public:
    VaultError(int code, const std::string& message);
    [[nodiscard]] int code() const noexcept;

private:
    int code_;
};

/// RAII wrapper around the C vault ABI.  A VaultStore is single-threaded.
class VaultStore {
public:
    explicit VaultStore(const ConnectionConfig& config);
    ~VaultStore();

    VaultStore(const VaultStore&) = delete;
    VaultStore& operator=(const VaultStore&) = delete;
    VaultStore(VaultStore&&) noexcept;
    VaultStore& operator=(VaultStore&&) noexcept;

    void open();
    [[nodiscard]] VaultState state() const;
    void setup(std::string_view phrase, const VaultKdfParams& params = {});
    void unlock(std::string_view phrase);
    void change_phrase(std::string_view new_phrase);
    void lock() noexcept;
    [[nodiscard]] bool is_unlocked() const;

    void put(const VaultRecord& record);
    [[nodiscard]] std::optional<VaultRecord> get(std::string_view id);
    [[nodiscard]] bool remove(std::string_view id);
    [[nodiscard]] std::vector<VaultRecord> scan(std::string_view kind = {});
    void clear();

    void begin();
    void commit();
    void rollback();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace oyama::storage
