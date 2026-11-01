#pragma once
#include <oyama/compat/all.h>

#include "connection.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace oyama { namespace storage {

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
    VaultRecord() = default;
    VaultRecord(std::string i, std::string k, std::uint64_t t, std::string p)
        : id(std::move(i)), kind(std::move(k)), updated_at_ms(t),
          payload(std::move(p)) {}
};

class VaultError : public std::runtime_error {
public:
    VaultError(int code, const std::string& message);
    OYAMA_NODISCARD int code() const noexcept;

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
    OYAMA_NODISCARD VaultState state() const;
    void setup(oyama::string_view phrase, const VaultKdfParams& params = {});
    void unlock(oyama::string_view phrase);
    void change_phrase(oyama::string_view new_phrase);
    void lock() noexcept;
    OYAMA_NODISCARD bool is_unlocked() const;

    void put(const VaultRecord& record);
    OYAMA_NODISCARD oyama::optional<VaultRecord> get(oyama::string_view id);
    OYAMA_NODISCARD bool remove(oyama::string_view id);
    OYAMA_NODISCARD std::vector<VaultRecord> scan(oyama::string_view kind = {});
    void clear();

    void begin();
    void commit();
    void rollback();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} }  // namespace oyama::storage
