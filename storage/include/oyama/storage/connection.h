#pragma once

#include "error.h"

#include <libpq-fe.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace oyama::storage {

/// Connection settings for a local PostgreSQL server.  Only Unix-domain
/// sockets are supported: an empty socket_dir is resolved by searching the
/// standard local directories for ".s.PGSQL.<port>".
struct ConnectionConfig {
    std::string socket_dir;
    std::string dbname{"oyama_hrt"};
    std::string user;
    int port{5432};
    std::string application_name{"oyama-hrt"};
    int connect_timeout_seconds{5};

    /// Reads OYAMA_PG_* first, then the corresponding libpq PG* variables.
    /// PG* values are never used to enable a TCP host.
    [[nodiscard]] static ConnectionConfig from_environment();

    [[nodiscard]] static std::vector<std::string> candidate_socket_dirs(int port);

    /// Returns the first candidate directory containing the server socket.
    [[nodiscard]] static std::optional<std::string> find_socket_dir(int port);
};

/// Owning wrapper around a libpq result.  Server errors are thrown by
/// Connection::exec / exec_params, so a constructed Result always has a
/// command or tuples status.
class Result {
public:
    Result() = default;
    explicit Result(PGresult* result);
    ~Result();

    Result(Result&& other) noexcept;
    Result& operator=(Result&& other) noexcept;
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

    [[nodiscard]] int rows() const noexcept;
    [[nodiscard]] int columns() const noexcept;
    [[nodiscard]] bool is_null(int row, int column) const;
    [[nodiscard]] std::optional<std::string> value(int row, int column) const;
    [[nodiscard]] std::string text(int row, int column) const;
    [[nodiscard]] std::int64_t integer(int row, int column) const;
    [[nodiscard]] double number(int row, int column) const;
    [[nodiscard]] bool boolean(int row, int column) const;
    [[nodiscard]] std::string command_tag() const;
    [[nodiscard]] int affected_rows() const;

private:
    struct Deleter {
        void operator()(PGresult* result) const noexcept;
    };

    std::unique_ptr<PGresult, Deleter> result_;
};

/// A single libpq connection.  Connections are not thread-safe and must not be
/// shared between threads; each worker owns its own connection.
class Connection {
public:
    explicit Connection(const ConnectionConfig& config);
    ~Connection();

    Connection(Connection&& other) noexcept;
    Connection& operator=(Connection&& other) noexcept;
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    [[nodiscard]] Result exec(const std::string& sql);
    [[nodiscard]] Result exec_params(
        const std::string& sql,
        const std::vector<std::optional<std::string>>& params);

    void begin();
    void commit();
    void rollback();

    [[nodiscard]] int server_version() const noexcept;
    [[nodiscard]] bool is_open() const noexcept;

    /// Quotes an SQL identifier using the server connection's encoding rules.
    [[nodiscard]] std::string quote_identifier(const std::string& identifier);

private:
    PGconn* connection_{nullptr};
};

/// RAII transaction.  Destruction rolls back unless commit() succeeded.
class Transaction {
public:
    explicit Transaction(Connection& connection);
    ~Transaction();

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    void commit();
    void rollback();

private:
    Connection* connection_{nullptr};
    bool active_{false};
};

}  // namespace oyama::storage
