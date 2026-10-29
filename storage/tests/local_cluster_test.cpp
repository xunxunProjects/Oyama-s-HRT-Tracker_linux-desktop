#include <oyama/storage/app_config.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

using oyama::storage::AppConfig;
using oyama::storage::Backend;
using oyama::storage::LocalCluster;
using oyama::storage::LocalClusterConfig;
using oyama::storage::LocalClusterState;

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

std::filesystem::path make_temp_dir() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() /
                ("oyama-cluster-test-" + std::to_string(stamp));
    std::filesystem::create_directories(path);
    return path;
}

void test_config_round_trip() {
    const auto dir = make_temp_dir();
    const auto path = dir / "config";

    AppConfig config;
    config.backend = Backend::Managed;
    config.socket_dir = (dir / "run").string();
    config.data_dir = (dir / "pgdata").string();
    config.server_bin_dir = "/opt/postgresql/bin";
    config.port = 55432;
    config.dbname = "oyama_test";
    config.user = "alice";

    oyama::storage::save_app_config(config, path);
    const auto loaded = oyama::storage::load_app_config(path);
    check(loaded.has_value(), "managed config round-trips");
    if (loaded.has_value()) {
        check(loaded->backend == Backend::Managed, "backend round-trips");
        check_equal(loaded->socket_dir, config.socket_dir, "socket dir round-trips");
        check_equal(loaded->data_dir, config.data_dir, "data dir round-trips");
        check_equal(loaded->server_bin_dir, config.server_bin_dir,
                    "server bin dir round-trips");
        check_equal(loaded->port, config.port, "port round-trips");
        check_equal(loaded->dbname, config.dbname, "db name round-trips");
        check_equal(loaded->user, config.user, "user round-trips");

        const auto connection = loaded->connection_config();
        check_equal(connection.socket_dir, config.socket_dir,
                    "connection socket dir comes from config");
        check_equal(connection.port, config.port,
                    "connection port comes from config");
        check_equal(connection.dbname, config.dbname,
                    "connection db name comes from config");
        check_equal(connection.user, config.user,
                    "connection user comes from config");

        const auto cluster = loaded->cluster_config();
        check_equal(cluster.data_dir.string(), config.data_dir,
                    "cluster data dir comes from config");
        check_equal(cluster.socket_dir.string(), config.socket_dir,
                    "cluster socket dir comes from config");
    }

    std::filesystem::remove_all(dir);
}

void test_config_missing() {
    const auto dir = make_temp_dir();
    check(!oyama::storage::load_app_config(dir / "absent").has_value(),
          "missing config is reported as absent");
    std::filesystem::remove_all(dir);
}

void test_config_rejects_bad_port() {
    const auto dir = make_temp_dir();
    const auto path = dir / "config";
    {
        std::ofstream out(path);
        out << "backend=managed\nport=not-a-number\n";
    }
    bool rejected = false;
    try {
        (void)oyama::storage::load_app_config(path);
    } catch (const oyama::storage::Error&) {
        rejected = true;
    }
    check(rejected, "malformed config is rejected");
    std::filesystem::remove_all(dir);
}

void test_config_rejects_relative_paths() {
    const auto dir = make_temp_dir();

    AppConfig config;
    config.backend = Backend::Managed;
    config.socket_dir = (dir / "run").string();
    config.data_dir = (dir / "pgdata").string();
    config.server_bin_dir = "relative/bin";
    bool relative_bin_rejected = false;
    try {
        oyama::storage::save_app_config(config, dir / "config");
    } catch (const oyama::storage::Error&) {
        relative_bin_rejected = true;
    }
    check(relative_bin_rejected,
          "relative server bin directory is rejected on save");

    config.server_bin_dir.clear();
    config.data_dir = "relative/pgdata";
    bool relative_data_rejected = false;
    try {
        oyama::storage::save_app_config(config, dir / "config");
    } catch (const oyama::storage::Error&) {
        relative_data_rejected = true;
    }
    check(relative_data_rejected,
          "relative managed data directory is rejected on save");

    std::filesystem::remove_all(dir);
}

void test_config_existing_round_trip() {
    const auto dir = make_temp_dir();
    AppConfig config;
    config.backend = Backend::Existing;
    config.socket_dir = "/run/postgresql";
    config.port = 5432;
    config.dbname = "oyama_hrt";
    config.user = "alice";
    oyama::storage::save_app_config(config, dir / "config");
    const auto loaded = oyama::storage::load_app_config(dir / "config");
    check(loaded.has_value(), "existing config round-trips");
    if (loaded.has_value()) {
        check(loaded->backend == Backend::Existing,
              "existing backend round-trips");
        check_equal(loaded->socket_dir, std::string("/run/postgresql"),
                    "existing socket dir round-trips");
        check(loaded->data_dir.empty(), "existing config has no data dir");
    }
    std::filesystem::remove_all(dir);
}

void test_local_cluster_rejects_unsafe_config() {
    const auto dir = make_temp_dir();
    LocalClusterConfig config;
    config.data_dir = dir / "pgdata";
    config.socket_dir = dir / "run";

    config.server_bin_dir = "relative/bin";
    bool relative_bin_rejected = false;
    try {
        LocalCluster cluster(config);
    } catch (const oyama::storage::Error&) {
        relative_bin_rejected = true;
    }
    check(relative_bin_rejected,
          "relative server bin directory is rejected by the cluster");

    config.server_bin_dir.clear();
    config.socket_dir = dir / "run\nlisten_addresses = '*'";
    bool newline_rejected = false;
    try {
        LocalCluster cluster(config);
    } catch (const oyama::storage::Error&) {
        newline_rejected = true;
    }
    check(newline_rejected,
          "line breaks in managed cluster paths are rejected");

    std::filesystem::remove_all(dir);
}

void test_local_cluster_starts_uninitialized() {
    const auto dir = make_temp_dir();
    LocalClusterConfig config;
    config.data_dir = dir / "pgdata";
    config.socket_dir = dir / "run";
    config.server_bin_dir = (dir / "missing-bin").string();

    LocalCluster cluster(config);
    check(!cluster.is_initialized(), "new cluster is not initialized");
    check(cluster.state() == LocalClusterState::NotInitialized,
          "new cluster state is NotInitialized");
    check(!cluster.find_binaries().has_value(),
          "missing server binaries are reported");

    std::filesystem::remove_all(dir);
}

void test_local_cluster_finds_configured_binaries() {
    const auto dir = make_temp_dir();
    const auto bin = dir / "bin";
    std::filesystem::create_directories(bin);
    for (const char* name : {"initdb", "pg_ctl", "postgres"}) {
        std::ofstream out(bin / name);
        out << "#!/bin/sh\n";
    }
    for (const char* name : {"initdb", "pg_ctl", "postgres"}) {
        std::filesystem::permissions(
            bin / name,
            std::filesystem::perms::owner_exec |
                std::filesystem::perms::owner_read |
                std::filesystem::perms::owner_write,
            std::filesystem::perm_options::replace);
    }

    LocalClusterConfig config;
    config.data_dir = dir / "pgdata";
    config.socket_dir = dir / "run";
    config.server_bin_dir = bin.string();
    LocalCluster cluster(config);
    const auto found = cluster.find_binaries();
    check(found.has_value(), "configured binaries are found");
    if (found.has_value()) {
        check_equal(found->string(), bin.string(), "configured bin dir is used");
    }

    std::filesystem::remove_all(dir);
}

}  // namespace

int main() {
    try {
        test_config_round_trip();
        test_config_missing();
        test_config_rejects_bad_port();
        test_config_rejects_relative_paths();
        test_config_existing_round_trip();
        test_local_cluster_rejects_unsafe_config();
        test_local_cluster_starts_uninitialized();
        test_local_cluster_finds_configured_binaries();
    } catch (const std::exception& error) {
        std::cerr << "FAIL: unexpected exception: " << error.what() << '\n';
        return 1;
    }

    if (failures != 0) {
        std::cerr << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "local_cluster_test: ok\n";
    return 0;
}
