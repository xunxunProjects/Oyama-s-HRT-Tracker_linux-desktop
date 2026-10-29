#include <oyama/storage/connection.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

namespace oyama::storage {
namespace {

std::optional<std::string> env_value(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return std::nullopt;
    }
    return std::string(value);
}

int parse_port(const std::optional<std::string>& value, int fallback) {
    if (!value.has_value()) {
        return fallback;
    }
    try {
        std::size_t consumed = 0;
        const int port = std::stoi(*value, &consumed);
        if (consumed != value->size() || port < 1 || port > 65535) {
            throw std::invalid_argument("out of range");
        }
        return port;
    } catch (const std::exception&) {
        throw Error("invalid PostgreSQL port: " + *value);
    }
}

void append_unique(std::vector<std::string>& values, std::string value) {
    if (value.empty()) {
        return;
    }
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(std::move(value));
    }
}

std::string sqlstate_of(PGresult* result) {
    const char* state = PQresultErrorField(result, PG_DIAG_SQLSTATE);
    return state == nullptr ? std::string{} : std::string(state);
}

std::string server_error(PGresult* result) {
    const char* message = PQresultErrorMessage(result);
    return message == nullptr ? std::string("unknown PostgreSQL error")
                              : std::string(message);
}

}  // namespace

ConnectionConfig ConnectionConfig::from_environment() {
    ConnectionConfig config;

    if (const auto value = env_value("OYAMA_PG_DBNAME")) {
        config.dbname = *value;
    } else if (const auto value = env_value("PGDATABASE")) {
        config.dbname = *value;
    }

    if (const auto value = env_value("OYAMA_PG_USER")) {
        config.user = *value;
    } else if (const auto value = env_value("PGUSER")) {
        config.user = *value;
    }

    config.port = parse_port(env_value("OYAMA_PG_PORT").has_value()
                                 ? env_value("OYAMA_PG_PORT")
                                 : env_value("PGPORT"),
                             5432);

    if (const auto value = env_value("OYAMA_PG_SOCKET")) {
        config.socket_dir = *value;
    } else if (const auto value = env_value("PGHOST");
               value.has_value() && value->front() == '/') {
        config.socket_dir = *value;
    }

    return config;
}

std::vector<std::string> ConnectionConfig::candidate_socket_dirs(int port) {
    (void)port;
    std::vector<std::string> dirs;

    if (const auto value = env_value("OYAMA_PG_SOCKET")) {
        append_unique(dirs, *value);
    }
    if (const auto value = env_value("PGHOST");
        value.has_value() && value->front() == '/') {
        append_unique(dirs, *value);
    }
    if (const auto value = env_value("XDG_RUNTIME_DIR")) {
        append_unique(dirs, *value);
        append_unique(dirs, *value + "/postgresql");
    }
    append_unique(dirs, "/run/postgresql");
    append_unique(dirs, "/var/run/postgresql");
    append_unique(dirs, "/tmp");
    return dirs;
}

std::optional<std::string> ConnectionConfig::find_socket_dir(int port) {
    for (const auto& directory : candidate_socket_dirs(port)) {
        const auto socket = std::filesystem::path(directory) /
                            (".s.PGSQL." + std::to_string(port));
        std::error_code error;
        if (std::filesystem::exists(socket, error) && !error) {
            return directory;
        }
    }
    return std::nullopt;
}

void Result::Deleter::operator()(PGresult* result) const noexcept {
    if (result != nullptr) {
        PQclear(result);
    }
}

Result::Result(PGresult* result) : result_(result) {
    if (result == nullptr) {
        throw Error("libpq returned no result");
    }
    const ExecStatusType status = PQresultStatus(result);
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
        const std::string message = server_error(result);
        const std::string state = sqlstate_of(result);
        result_.reset();
        throw Error(message, state);
    }
}

Result::~Result() = default;
Result::Result(Result&& other) noexcept = default;
Result& Result::operator=(Result&& other) noexcept = default;

int Result::rows() const noexcept {
    return result_ == nullptr ? 0 : PQntuples(result_.get());
}

int Result::columns() const noexcept {
    return result_ == nullptr ? 0 : PQnfields(result_.get());
}

bool Result::is_null(int row, int column) const {
    if (result_ == nullptr) {
        throw Error("result is empty");
    }
    return PQgetisnull(result_.get(), row, column) != 0;
}

std::optional<std::string> Result::value(int row, int column) const {
    if (is_null(row, column)) {
        return std::nullopt;
    }
    return std::string(PQgetvalue(result_.get(), row, column),
                       static_cast<std::size_t>(PQgetlength(result_.get(), row, column)));
}

std::string Result::text(int row, int column) const {
    const auto value_ = value(row, column);
    if (!value_.has_value()) {
        throw Error("unexpected NULL in PostgreSQL result");
    }
    return *value_;
}

std::int64_t Result::integer(int row, int column) const {
    const auto text_ = text(row, column);
    try {
        return std::stoll(text_);
    } catch (const std::exception&) {
        throw Error("invalid integer in PostgreSQL result: " + text_);
    }
}

double Result::number(int row, int column) const {
    const auto text_ = text(row, column);
    try {
        return std::stod(text_);
    } catch (const std::exception&) {
        throw Error("invalid number in PostgreSQL result: " + text_);
    }
}

bool Result::boolean(int row, int column) const {
    const auto text_ = text(row, column);
    if (text_ == "t") {
        return true;
    }
    if (text_ == "f") {
        return false;
    }
    throw Error("invalid boolean in PostgreSQL result: " + text_);
}

std::string Result::command_tag() const {
    if (result_ == nullptr) {
        return {};
    }
    const char* tag = PQcmdStatus(result_.get());
    return tag == nullptr ? std::string{} : std::string(tag);
}

int Result::affected_rows() const {
    if (result_ == nullptr) {
        return 0;
    }
    const char* tuples = PQcmdTuples(result_.get());
    if (tuples == nullptr || *tuples == '\0') {
        return 0;
    }
    try {
        return std::stoi(tuples);
    } catch (const std::exception&) {
        return 0;
    }
}

Connection::Connection(const ConnectionConfig& config) {
    if (config.dbname.empty()) {
        throw Error("PostgreSQL database name is empty");
    }
    if (config.port < 1 || config.port > 65535) {
        throw Error("PostgreSQL port is out of range");
    }

    std::string socket_dir = config.socket_dir;
    if (socket_dir.empty()) {
        const auto found = ConnectionConfig::find_socket_dir(config.port);
        if (!found.has_value()) {
            std::ostringstream message;
            message << "could not find a local PostgreSQL socket for port "
                    << config.port << " in:";
            for (const auto& candidate :
                 ConnectionConfig::candidate_socket_dirs(config.port)) {
                message << ' ' << candidate;
            }
            message << " (start PostgreSQL or pass --socket-dir)";
            throw Error(message.str());
        }
        socket_dir = *found;
    }
    if (socket_dir.front() != '/') {
        throw Error("only Unix-domain PostgreSQL sockets are supported: " +
                    socket_dir);
    }

    const std::string port = std::to_string(config.port);
    const std::string timeout = std::to_string(config.connect_timeout_seconds);
    const char* user = config.user.empty() ? nullptr : config.user.c_str();

    const char* keywords[] = {
        "host",          "port",        "dbname",       "user",
        "application_name", "connect_timeout", "client_encoding",
        "options",       "password",    "gssencmode",   "sslmode",
        nullptr,
    };
    const char* values[] = {
        socket_dir.c_str(),
        port.c_str(),
        config.dbname.c_str(),
        user,
        config.application_name.c_str(),
        timeout.c_str(),
        "UTF8",
        "-c TimeZone=UTC",
        "",
        "disable",
        "disable",
        nullptr,
    };

    connection_ = PQconnectdbParams(keywords, values, 0);
    if (connection_ == nullptr) {
        throw Error("libpq could not allocate a connection");
    }
    if (PQstatus(connection_) != CONNECTION_OK) {
        const std::string message = PQerrorMessage(connection_);
        PQfinish(connection_);
        connection_ = nullptr;
        throw Error(message.empty() ? "could not connect to PostgreSQL" : message);
    }
}

Connection::~Connection() {
    if (connection_ != nullptr) {
        PQfinish(connection_);
    }
}

Connection::Connection(Connection&& other) noexcept
    : connection_(std::exchange(other.connection_, nullptr)) {}

Connection& Connection::operator=(Connection&& other) noexcept {
    if (this != &other) {
        if (connection_ != nullptr) {
            PQfinish(connection_);
        }
        connection_ = std::exchange(other.connection_, nullptr);
    }
    return *this;
}

Result Connection::exec(const std::string& sql) {
    if (connection_ == nullptr) {
        throw Error("PostgreSQL connection is closed");
    }
    return Result(PQexec(connection_, sql.c_str()));
}

Result Connection::exec_params(
    const std::string& sql,
    const std::vector<std::optional<std::string>>& params) {
    if (connection_ == nullptr) {
        throw Error("PostgreSQL connection is closed");
    }
    if (params.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw Error("too many PostgreSQL parameters");
    }

    std::vector<const char*> values;
    values.reserve(params.size());
    for (const auto& parameter : params) {
        values.push_back(parameter.has_value() ? parameter->c_str() : nullptr);
    }
    return Result(PQexecParams(connection_, sql.c_str(),
                               static_cast<int>(values.size()), nullptr,
                               values.empty() ? nullptr : values.data(), nullptr,
                               nullptr, 0));
}

void Connection::begin() {
    (void)exec("BEGIN");
}

void Connection::commit() {
    (void)exec("COMMIT");
}

void Connection::rollback() {
    (void)exec("ROLLBACK");
}

int Connection::server_version() const noexcept {
    return connection_ == nullptr ? 0 : PQserverVersion(connection_);
}

bool Connection::is_open() const noexcept {
    return connection_ != nullptr && PQstatus(connection_) == CONNECTION_OK;
}

std::string Connection::quote_identifier(const std::string& identifier) {
    if (connection_ == nullptr) {
        throw Error("PostgreSQL connection is closed");
    }
    char* quoted = PQescapeIdentifier(connection_, identifier.c_str(),
                                      identifier.size());
    if (quoted == nullptr) {
        throw Error(PQerrorMessage(connection_));
    }
    std::string result(quoted);
    PQfreemem(quoted);
    return result;
}

Transaction::Transaction(Connection& connection) : connection_(&connection) {
    connection_->begin();
    active_ = true;
}

Transaction::~Transaction() {
    if (active_ && connection_ != nullptr) {
        try {
            connection_->rollback();
        } catch (...) {  // NOLINT(bugprone-empty-catch)
            // Destructors must not throw; rollback failure leaves the
            // connection in an unusable state and the caller will see it on
            // the next query.
        }
    }
}

void Transaction::commit() {
    if (!active_ || connection_ == nullptr) {
        throw Error("transaction is not active");
    }
    connection_->commit();
    active_ = false;
}

void Transaction::rollback() {
    if (!active_ || connection_ == nullptr) {
        return;
    }
    try {
        connection_->rollback();
    } catch (...) {
        active_ = false;
        throw;
    }
    active_ = false;
}

}  // namespace oyama::storage
