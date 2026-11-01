#include "oyama/storage/vault_store.h"

#include "oyama/storage/passphrase.h"

#include <oyama/vault.h>

#include <cstring>
#include <utility>

namespace oyama { namespace storage {
namespace {

[[noreturn]] void throw_status(int status, const oyama_error& err) {
    const char* message = err.message[0] != '\0'
                              ? err.message
                              : oyama_vault_status_name(status);
    throw VaultError(status, message);
}

struct ScanContext {
    std::vector<VaultRecord> records;
};

int scan_callback(void* user_data, const char* id, const char* kind,
                  std::uint64_t updated_at_ms, const unsigned char* payload,
                  std::size_t payload_len) {
    auto* context = static_cast<ScanContext*>(user_data);
    VaultRecord record;
    record.id = id != nullptr ? id : "";
    record.kind = kind != nullptr ? kind : "";
    record.updated_at_ms = updated_at_ms;
    if (payload != nullptr && payload_len != 0) {
        record.payload.assign(reinterpret_cast<const char*>(payload),
                              payload_len);
    }
    context->records.push_back(std::move(record));
    return 0;
}

}  // namespace

struct VaultStore::Impl {
    ConnectionConfig config;
    oyama_vault* vault{nullptr};

    ~Impl() {
        if (vault != nullptr) {
            oyama_vault_close(vault);
        }
    }
};

VaultError::VaultError(int code, const std::string& message)
    : std::runtime_error(message), code_(code) {}

int VaultError::code() const noexcept { return code_; }

VaultStore::VaultStore(const ConnectionConfig& config)
    : impl_(oyama::make_unique<Impl>()) {
    impl_->config = config;
}

VaultStore::~VaultStore() = default;
VaultStore::VaultStore(VaultStore&&) noexcept = default;
VaultStore& VaultStore::operator=(VaultStore&&) noexcept = default;

void VaultStore::open() {
    if (impl_->vault != nullptr) {
        throw VaultError(OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                         "vault is already open");
    }
    oyama_error err{};
    const int status = oyama_vault_open(impl_->config.to_conninfo().c_str(),
                                        &impl_->vault, &err);
    if (status != OYAMA_VAULT_OK) {
        throw_status(status, err);
    }
}

VaultState VaultStore::state() const {
    if (impl_->vault == nullptr) {
        throw VaultError(OYAMA_VAULT_ERR_INVALID_ARGUMENT,
                         "vault is not open");
    }
    oyama_vault_state state = OYAMA_VAULT_UNINITIALIZED;
    oyama_error err{};
    const int status = oyama_vault_get_state(impl_->vault, &state, &err);
    if (status != OYAMA_VAULT_OK) {
        throw_status(status, err);
    }
    switch (state) {
    case OYAMA_VAULT_UNINITIALIZED:
        return VaultState::Uninitialized;
    case OYAMA_VAULT_LOCKED:
        return VaultState::Locked;
    case OYAMA_VAULT_UNLOCKED:
        return VaultState::Unlocked;
    }
    throw VaultError(OYAMA_VAULT_ERR_CORRUPT, "unknown vault state");
}

void VaultStore::setup(oyama::string_view phrase,
                       const VaultKdfParams& params) {
    const std::string normalized = Passphrase::normalize(phrase);
    const oyama_vault_kdf_params c_params{params.opslimit, params.memlimit};
    oyama_error err{};
    const int status = oyama_vault_setup_with_params(
        impl_->vault, normalized.c_str(), normalized.size(), &c_params, &err);
    if (status != OYAMA_VAULT_OK) {
        throw_status(status, err);
    }
}

void VaultStore::unlock(oyama::string_view phrase) {
    const std::string normalized = Passphrase::normalize(phrase);
    oyama_error err{};
    const int status =
        oyama_vault_unlock(impl_->vault, normalized.c_str(), normalized.size(),
                           &err);
    if (status != OYAMA_VAULT_OK) {
        throw_status(status, err);
    }
}

void VaultStore::change_phrase(oyama::string_view new_phrase) {
    const std::string normalized = Passphrase::normalize(new_phrase);
    oyama_error err{};
    const int status = oyama_vault_change_phrase(
        impl_->vault, normalized.c_str(), normalized.size(), &err);
    if (status != OYAMA_VAULT_OK) {
        throw_status(status, err);
    }
}

void VaultStore::lock() noexcept {
    if (impl_ != nullptr && impl_->vault != nullptr) {
        oyama_vault_lock(impl_->vault);
    }
}

bool VaultStore::is_unlocked() const {
    return impl_->vault != nullptr && state() == VaultState::Unlocked;
}

void VaultStore::put(const VaultRecord& record) {
    oyama_error err{};
    const void* payload = record.payload.empty() ? nullptr : record.payload.data();
    const int status =
        oyama_vault_put(impl_->vault, record.id.c_str(), record.kind.c_str(),
                        record.updated_at_ms, payload, record.payload.size(),
                        &err);
    if (status != OYAMA_VAULT_OK) {
        throw_status(status, err);
    }
}

oyama::optional<VaultRecord> VaultStore::get(oyama::string_view id) {
    const std::string id_string(id);
    oyama_record c_record{};
    oyama_error err{};
    const int status =
        oyama_vault_get(impl_->vault, id_string.c_str(), &c_record, &err);
    if (status == OYAMA_VAULT_ERR_NOT_FOUND) {
        return oyama::nullopt;
    }
    if (status != OYAMA_VAULT_OK) {
        throw_status(status, err);
    }
    VaultRecord record;
    record.id = c_record.id;
    record.kind = c_record.kind;
    record.updated_at_ms = c_record.updated_at_ms;
    if (c_record.payload.data != nullptr && c_record.payload.len != 0) {
        record.payload.assign(
            reinterpret_cast<const char*>(c_record.payload.data),
            c_record.payload.len);
    }
    oyama_record_free(&c_record);
    return record;
}

bool VaultStore::remove(oyama::string_view id) {
    const std::string id_string(id);
    oyama_error err{};
    const int status =
        oyama_vault_remove(impl_->vault, id_string.c_str(), &err);
    if (status == OYAMA_VAULT_ERR_NOT_FOUND) {
        return false;
    }
    if (status != OYAMA_VAULT_OK) {
        throw_status(status, err);
    }
    return true;
}

std::vector<VaultRecord> VaultStore::scan(oyama::string_view kind) {
    const std::string kind_string(kind);
    const char* filter = kind_string.empty() ? nullptr : kind_string.c_str();
    ScanContext context;
    oyama_error err{};
    const int status = oyama_vault_scan(impl_->vault, filter, &scan_callback,
                                        &context, &err);
    if (status != OYAMA_VAULT_OK) {
        throw_status(status, err);
    }
    return std::move(context.records);
}

void VaultStore::clear() {
    oyama_error err{};
    const int status = oyama_vault_clear(impl_->vault, &err);
    if (status != OYAMA_VAULT_OK) {
        throw_status(status, err);
    }
}

void VaultStore::begin() {
    oyama_error err{};
    const int status = oyama_vault_begin(impl_->vault, &err);
    if (status != OYAMA_VAULT_OK) {
        throw_status(status, err);
    }
}

void VaultStore::commit() {
    oyama_error err{};
    const int status = oyama_vault_commit(impl_->vault, &err);
    if (status != OYAMA_VAULT_OK) {
        throw_status(status, err);
    }
}

void VaultStore::rollback() {
    oyama_error err{};
    const int status = oyama_vault_rollback(impl_->vault, &err);
    if (status != OYAMA_VAULT_OK) {
        throw_status(status, err);
    }
}

} }  // namespace oyama::storage
