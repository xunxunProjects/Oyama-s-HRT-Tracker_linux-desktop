#pragma once

#include "connection.h"

#include <filesystem>
#include <optional>
#include <string>

namespace oyama::storage {

/// State of an application-managed cluster.  Missing server tools are reported
/// separately by find_binaries(); state() never invents a running state when
/// pg_ctl is absent.
enum class LocalClusterState {
    NotInitialized,
    Stopped,
    Running,
};

[[nodiscard]] std::string to_string(LocalClusterState state);

struct LocalClusterConfig {
    std::filesystem::path data_dir;
    std::filesystem::path socket_dir;
    /// Empty means <data_dir>/../postgres.log.
    std::filesystem::path log_file;
    /// Empty means search PG_BINDIR, then PATH.
    std::string server_bin_dir;
    int port{5432};
    int start_timeout_seconds{30};
    bool initdb_no_sync{false};
};

/// A PostgreSQL cluster whose lifecycle is owned by Oyama.  The cluster is
/// initialized with peer authentication and listens only on a Unix socket;
/// no TCP address is opened.
///
/// The class is deliberately explicit: constructing it never starts anything.
/// Call initialize() during first-run setup, ensure_running() at application
/// startup, and stop() at application shutdown.
class LocalCluster {
public:
    explicit LocalCluster(LocalClusterConfig config);

    [[nodiscard]] const LocalClusterConfig& config() const noexcept;

    /// True when <data_dir>/PG_VERSION exists.  Does not require pg_ctl.
    [[nodiscard]] bool is_initialized() const;

    /// Runs `pg_ctl status`.  Returns false when the cluster is initialized but
    /// stopped.  Throws if the server tools cannot be found.
    [[nodiscard]] bool is_running() const;

    [[nodiscard]] LocalClusterState state() const;

    /// Returns the directory containing initdb, pg_ctl, and postgres.
    [[nodiscard]] std::optional<std::filesystem::path> find_binaries() const;

    /// Connection settings for libpq.  The caller supplies the database name
    /// and user through AppConfig; this method only fills socket/port.
    [[nodiscard]] ConnectionConfig connection_config() const;

    /// Creates the data directory and runs initdb.  No-op if already
    /// initialized.
    void initialize() const;

    /// Starts a stopped cluster and waits for readiness.  No-op if running.
    void start() const;

    /// Stops a running cluster with fast shutdown.  No-op if stopped.
    void stop() const;

    /// Starts the cluster if it is stopped; throws if it was never
    /// initialized.  Intended for application startup.
    void ensure_running() const;

private:
    LocalClusterConfig config_;
};

/// Human-readable installation guidance for a missing PostgreSQL server.
[[nodiscard]] std::string postgres_install_hint();

}  // namespace oyama::storage
