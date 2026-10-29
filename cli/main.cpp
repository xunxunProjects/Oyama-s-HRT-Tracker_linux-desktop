#include <oyama/storage/storage.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <pwd.h>
#include <sstream>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

namespace {

using oyama::storage::AppConfig;
using oyama::storage::Backend;
using oyama::storage::Connection;
using oyama::storage::ConnectionConfig;
using oyama::storage::Database;
using oyama::storage::DoseEvent;
using oyama::storage::DoseEventRepository;
using oyama::storage::Error;
using oyama::storage::LocalCluster;
using oyama::storage::LocalClusterConfig;
using oyama::storage::Mode;
using oyama::storage::Settings;
using oyama::storage::SettingsRepository;

constexpr const char* kVersion = "0.1.0";

struct GlobalOptions {
    ConnectionConfig config;
    std::filesystem::path migrations;
    std::optional<AppConfig> app_config;
    std::filesystem::path data_dir;
    std::string pg_bin_dir;
    bool socket_set{false};
    bool dbname_set{false};
    bool user_set{false};
    bool port_set{false};
    bool data_dir_set{false};
    bool pg_bin_dir_set{false};
};

std::filesystem::path default_migrations_dir() {
    if (const char* value = std::getenv("OYAMA_MIGRATIONS_DIR");
        value != nullptr && *value != '\0') {
        return value;
    }
    std::error_code error;
#ifdef OYAMA_DEFAULT_MIGRATIONS_DIR
    if (std::filesystem::is_directory(OYAMA_DEFAULT_MIGRATIONS_DIR, error)) {
        return OYAMA_DEFAULT_MIGRATIONS_DIR;
    }
#endif
#ifdef OYAMA_INSTALL_MIGRATIONS_DIR
    if (std::filesystem::is_directory(OYAMA_INSTALL_MIGRATIONS_DIR, error)) {
        return OYAMA_INSTALL_MIGRATIONS_DIR;
    }
#endif
#ifdef OYAMA_DEFAULT_MIGRATIONS_DIR
    return OYAMA_DEFAULT_MIGRATIONS_DIR;
#else
    return "storage/migrations";
#endif
}

void print_usage(std::ostream& output) {
    output
        << "oyama-hrt-cli " << kVersion << "\n\n"
        << "Usage: oyama-hrt-cli [options] <command> [args]\n\n"
        << "Local PostgreSQL options:\n"
        << "  --socket-dir DIR   Unix socket directory (default: auto-detect)\n"
        << "  --dbname NAME      database name (default: oyama_hrt)\n"
        << "  --user NAME        database role (default: current system user)\n"
        << "  --port PORT        PostgreSQL port (default: 5432)\n"
        << "  --migrations DIR   migration directory\n"
        << "  --data-dir DIR     managed cluster data directory\n"
        << "  --pg-bin-dir DIR   directory containing initdb/pg_ctl/postgres\n\n"
        << "Commands:\n"
        << "  version\n"
        << "  db setup [--existing|--managed] [--yes] [--force]\n"
        << "            first-run selection of an existing or managed cluster\n"
        << "  db start           start the application-managed cluster\n"
        << "  db stop            stop the application-managed cluster\n"
        << "  db cluster-status  show the configured cluster and its state\n"
        << "  db ping\n"
        << "  db init            create the database if missing, then migrate\n"
        << "  db migrate\n"
        << "  db status          show migration status\n"
        << "  settings show\n"
        << "  settings set [--weight KG] [--birth-year YEAR]\n"
        << "               [--calibration off|ekf|ou_kalman|mipd]\n"
        << "               [--history-mode forward|retrospective]\n"
        << "               [--pk-params JSON|@FILE]\n"
        << "               [--pk-params-updated-at MS] [--updated-at MS]\n"
        << "  doses list --mode transfem|transmasc\n"
        << "  doses add --mode MODE --route ROUTE --ester ESTER\n"
        << "            --dose-mg N --time-h H [--extras JSON]\n"
        << "            [--updated-at MS] [--created-at MS] [--id UUID]\n"
        << "  doses remove --id UUID\n";
}

std::string require_value(const std::vector<std::string>& args, std::size_t& index,
                          std::string_view option) {
    if (index + 1 >= args.size()) {
        throw Error("missing value for " + std::string(option));
    }
    ++index;
    return args[index];
}

int parse_int(const std::string& value, std::string_view option) {
    try {
        std::size_t consumed = 0;
        const int result = std::stoi(value, &consumed);
        if (consumed != value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return result;
    } catch (const std::exception&) {
        throw Error("invalid integer for " + std::string(option) + ": " + value);
    }
}

double parse_double(const std::string& value, std::string_view option) {
    try {
        std::size_t consumed = 0;
        const double result = std::stod(value, &consumed);
        if (consumed != value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return result;
    } catch (const std::exception&) {
        throw Error("invalid number for " + std::string(option) + ": " + value);
    }
}

std::int64_t parse_i64(const std::string& value, std::string_view option) {
    try {
        std::size_t consumed = 0;
        const auto result = std::stoll(value, &consumed);
        if (consumed != value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return result;
    } catch (const std::exception&) {
        throw Error("invalid integer for " + std::string(option) + ": " + value);
    }
}

std::string read_json_argument(const std::string& value) {
    if (value.empty() || value.front() != '@') {
        return value;
    }
    const std::filesystem::path path(value.substr(1));
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw Error("could not open JSON file: " + path.string());
    }
    std::string contents((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
    if (input.bad()) {
        throw Error("could not read JSON file: " + path.string());
    }
    return contents;
}

GlobalOptions parse_global_options(const std::vector<std::string>& args,
                                   std::size_t& index) {
    GlobalOptions options;
    options.config = ConnectionConfig::from_environment();
    options.migrations = default_migrations_dir();
    while (index < args.size() && args[index].rfind("--", 0) == 0) {
        const std::string option = args[index];
        if (option == "--help" || option == "-h") {
            print_usage(std::cout);
            std::exit(0);
        }
        if (option == "--socket-dir") {
            options.config.socket_dir =
                require_value(args, index, "--socket-dir");
            options.socket_set = true;
        } else if (option == "--dbname") {
            options.config.dbname = require_value(args, index, "--dbname");
            options.dbname_set = true;
        } else if (option == "--user") {
            options.config.user = require_value(args, index, "--user");
            options.user_set = true;
        } else if (option == "--port") {
            options.config.port =
                parse_int(require_value(args, index, "--port"), "--port");
            options.port_set = true;
        } else if (option == "--migrations") {
            options.migrations = require_value(args, index, "--migrations");
        } else if (option == "--data-dir") {
            options.data_dir =
                require_value(args, index, "--data-dir");
            options.data_dir_set = true;
        } else if (option == "--pg-bin-dir") {
            options.pg_bin_dir =
                require_value(args, index, "--pg-bin-dir");
            options.pg_bin_dir_set = true;
        } else {
            throw Error("unknown global option: " + option);
        }
        ++index;
    }
    try {
        options.app_config = oyama::storage::load_app_config();
    } catch (const Error& error) {
        throw Error(std::string("could not load application config: ") +
                    error.what());
    }
    return options;
}

std::string current_os_user() {
    const passwd* entry = ::getpwuid(::geteuid());
    if (entry == nullptr || entry->pw_name == nullptr ||
        *entry->pw_name == '\0') {
        throw Error("could not determine the current OS user");
    }
    return entry->pw_name;
}

void apply_app_config_defaults(GlobalOptions& options) {
    if (!options.app_config.has_value()) {
        return;
    }
    const auto& config = *options.app_config;
    if (!options.socket_set) {
        options.config.socket_dir = config.socket_dir;
    }
    if (!options.port_set) {
        options.config.port = config.port;
    }
    if (!options.dbname_set) {
        options.config.dbname = config.dbname;
    }
    if (!options.user_set) {
        options.config.user = config.user;
    }
}

std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

Mode parse_mode_or_throw(const std::string& value) {
    const auto mode = oyama::storage::parse_mode(value);
    if (!mode.has_value()) {
        throw Error("invalid mode: " + value + " (use transfem or transmasc)");
    }
    return *mode;
}

void print_settings(const Settings& settings) {
    const auto optional_double = [](const std::optional<double>& value) {
        return value.has_value() ? std::to_string(*value) : std::string("not set");
    };
    const auto optional_int = [](const std::optional<int>& value) {
        return value.has_value() ? std::to_string(*value) : std::string("not set");
    };
    const auto optional_i64 = [](const std::optional<std::int64_t>& value) {
        return value.has_value() ? std::to_string(*value) : std::string("not set");
    };

    std::cout << "body_weight_kg\t" << optional_double(settings.body_weight_kg)
              << '\n';
    std::cout << "birth_year\t" << optional_int(settings.birth_year) << '\n';
    std::cout << "pk_params\t"
              << (settings.pk_params_json.has_value() ? *settings.pk_params_json
                                                      : std::string("not set"))
              << '\n';
    std::cout << "pk_params_updated_at\t"
              << optional_i64(settings.pk_params_updated_at) << '\n';
    std::cout << "calibration_method\t" << settings.calibration_method << '\n';
    std::cout << "calibration_history_mode\t"
              << settings.calibration_history_mode << '\n';
    std::cout << "updated_at\t" << settings.updated_at << '\n';
}

struct DatabaseInitResult {
    bool created{false};
    std::size_t applied{0};
    int version{0};
};

DatabaseInitResult initialize_database(const ConnectionConfig& config,
                                       const std::filesystem::path& migrations) {
    ConnectionConfig admin_config = config;
    admin_config.dbname = "postgres";
    Connection admin(admin_config);
    const auto exists = admin.exec_params(
        "SELECT 1 FROM pg_database WHERE datname = $1",
        {config.dbname});

    DatabaseInitResult result;
    if (exists.rows() == 0) {
        (void)admin.exec("CREATE DATABASE " +
                         admin.quote_identifier(config.dbname));
        result.created = true;
        std::cout << "created database " << config.dbname << '\n';
    } else {
        std::cout << "database " << config.dbname << " already exists\n";
    }

    Database database(config, migrations);
    const auto applied = database.migrate();
    result.applied = applied.size();
    result.version = database.current_version();
    std::cout << "schema version " << result.version << '\n';
    std::cout << "applied " << result.applied << " migration(s)\n";
    return result;
}

std::filesystem::path default_managed_data_dir() {
    return oyama::storage::default_data_dir();
}

std::filesystem::path default_managed_socket_dir() {
    return oyama::storage::default_socket_dir();
}

AppConfig managed_config_from_options(const GlobalOptions& global,
                                      bool force) {
    if (!force && global.app_config.has_value()) {
        if (global.app_config->backend != Backend::Managed) {
            throw Error("already configured for an existing PostgreSQL "
                        "cluster; use --force to reconfigure");
        }
        AppConfig config = *global.app_config;
        if (global.data_dir_set) {
            config.data_dir = global.data_dir.string();
        }
        if (global.socket_set) {
            config.socket_dir = global.config.socket_dir;
        }
        if (global.port_set) {
            config.port = global.config.port;
        }
        if (global.dbname_set) {
            config.dbname = global.config.dbname;
        }
        if (global.pg_bin_dir_set) {
            config.server_bin_dir = global.pg_bin_dir;
        }
        config.user = current_os_user();
        return config;
    }

    if (global.user_set && global.config.user != current_os_user()) {
        throw Error("managed clusters authenticate as the current OS user (" +
                    current_os_user() +
                    "); remove --user or create the role separately");
    }

    AppConfig config;
    config.backend = Backend::Managed;
    config.socket_dir = global.socket_set
                            ? global.config.socket_dir
                            : default_managed_socket_dir().string();
    config.data_dir = global.data_dir_set
                          ? global.data_dir.string()
                          : default_managed_data_dir().string();
    config.server_bin_dir = global.pg_bin_dir;
    if (config.server_bin_dir.empty()) {
        if (const char* value = std::getenv("PG_BINDIR");
            value != nullptr && *value != '\0') {
            config.server_bin_dir = value;
        }
    }
    config.port = global.config.port;
    config.dbname = global.config.dbname;
    config.user = current_os_user();
    return config;
}

AppConfig existing_config_from_options(const GlobalOptions& global,
                                       bool force) {
    if (!force && global.app_config.has_value()) {
        if (global.app_config->backend != Backend::Existing) {
            throw Error("already configured for an application-managed "
                        "cluster; use --force to reconfigure");
        }
        AppConfig config = *global.app_config;
        if (global.socket_set) {
            config.socket_dir = global.config.socket_dir;
        }
        if (global.port_set) {
            config.port = global.config.port;
        }
        if (global.dbname_set) {
            config.dbname = global.config.dbname;
        }
        if (global.user_set) {
            config.user = global.config.user;
        }
        if (config.user.empty()) {
            config.user = current_os_user();
        }
        return config;
    }

    AppConfig config;
    config.backend = Backend::Existing;
    config.socket_dir = global.socket_set ? global.config.socket_dir : "";
    config.port = global.config.port;
    config.dbname = global.config.dbname;
    config.user = global.config.user;
    if (config.user.empty()) {
        config.user = current_os_user();
    }
    if (config.socket_dir.empty()) {
        const auto found = ConnectionConfig::find_socket_dir(config.port);
        if (found.has_value()) {
            config.socket_dir = *found;
        }
    }
    return config;
}

void print_setup_summary(const AppConfig& config,
                         const DatabaseInitResult& result, bool running) {
    std::cout << "backend\t" << oyama::storage::to_string(config.backend)
              << '\n';
    std::cout << "socket_dir\t" << config.socket_dir << '\n';
    if (config.backend == Backend::Managed) {
        std::cout << "data_dir\t" << config.data_dir << '\n';
        if (!config.server_bin_dir.empty()) {
            std::cout << "server_bin_dir\t" << config.server_bin_dir << '\n';
        }
    }
    std::cout << "port\t" << config.port << '\n';
    std::cout << "database\t" << config.dbname << '\n';
    std::cout << "user\t" << config.user << '\n';
    std::cout << "cluster\t"
              << (config.backend == Backend::Managed
                      ? (running ? "running" : "stopped")
                      : "external")
              << '\n';
    std::cout << "schema_version\t" << result.version << '\n';
    std::cout << "applied\t" << result.applied << '\n';
    std::cout << "config\t" << oyama::storage::default_config_path().string()
              << '\n';
}

LocalClusterConfig cluster_config_with_overrides(const AppConfig& config,
                                                 const GlobalOptions& global) {
    LocalClusterConfig cluster_config = config.cluster_config();
    if (global.pg_bin_dir_set) {
        cluster_config.server_bin_dir = global.pg_bin_dir;
    }
    if (global.data_dir_set) {
        cluster_config.data_dir = global.data_dir;
    }
    return cluster_config;
}

void ensure_managed_running(const GlobalOptions& global) {
    if (!global.app_config.has_value() ||
        global.app_config->backend != Backend::Managed ||
        global.socket_set) {
        return;
    }
    LocalCluster cluster(
        cluster_config_with_overrides(*global.app_config, global));
    cluster.ensure_running();
}

int command_db_setup(const GlobalOptions& global,
                     const std::vector<std::string>& args,
                     std::size_t index) {
    bool managed = false;
    bool existing = false;
    bool yes = false;
    bool force = false;
    while (index < args.size()) {
        const std::string option = args[index];
        if (option == "--managed") {
            managed = true;
        } else if (option == "--existing") {
            existing = true;
        } else if (option == "--yes" || option == "-y") {
            yes = true;
        } else if (option == "--force") {
            force = true;
        } else if (option == "--help" || option == "-h") {
            print_usage(std::cout);
            return 0;
        } else {
            throw Error("unknown db setup option: " + option);
        }
        ++index;
    }
    if (managed && existing) {
        throw Error("use either --managed or --existing, not both");
    }

    if (!force && global.app_config.has_value()) {
        const auto& configured = *global.app_config;
        if (managed && configured.backend != Backend::Managed) {
            throw Error("already configured for an existing cluster; use "
                        "--force to reconfigure");
        }
        if (existing && configured.backend != Backend::Existing) {
            throw Error("already configured for a managed cluster; use "
                        "--force to reconfigure");
        }
        std::cout << "using existing configuration\n";
        if (configured.backend == Backend::Managed) {
            LocalCluster cluster(
                cluster_config_with_overrides(configured, global));
            cluster.ensure_running();
        }
        const auto result = initialize_database(
            configured.connection_config(), global.migrations);
        print_setup_summary(configured, result,
                            configured.backend == Backend::Managed);
        return 0;
    }

    const bool interactive = !yes && ::isatty(STDIN_FILENO) == 1;
    if (!managed && !existing) {
        const auto found = ConnectionConfig::find_socket_dir(
            global.config.port);
        LocalClusterConfig probe_config;
        probe_config.data_dir = global.data_dir_set
                                    ? global.data_dir
                                    : default_managed_data_dir();
        probe_config.socket_dir = global.socket_set
                                      ? std::filesystem::path(
                                            global.config.socket_dir)
                                      : default_managed_socket_dir();
        probe_config.server_bin_dir = global.pg_bin_dir;
        probe_config.port = global.config.port;
        bool tools_available = false;
        try {
            tools_available = LocalCluster(probe_config)
                                  .find_binaries()
                                  .has_value();
        } catch (const Error&) {
            tools_available = false;
        }

        if (interactive) {
            std::cout << "Oyama HRT local database setup\n";
            if (found.has_value()) {
                std::cout << "Found an existing PostgreSQL socket in "
                          << *found << '\n';
            }
            if (tools_available) {
                std::cout << "PostgreSQL server tools were found in PATH.\n";
            }
            std::cout << "  1) Use an existing local PostgreSQL server"
                      << (found.has_value() ? " (recommended)" : "") << '\n';
            std::cout << "  2) Create an application-managed PostgreSQL "
                         "cluster"
                      << (tools_available ? "" : " (tools missing)") << '\n';
            std::cout << "Choice [1]: ";
            std::string choice;
            if (!std::getline(std::cin, choice)) {
                throw Error("setup cancelled");
            }
            if (choice.empty() || choice == "1") {
                existing = true;
            } else if (choice == "2") {
                managed = true;
            } else {
                throw Error("invalid setup choice: " + choice);
            }
        } else if (found.has_value()) {
            existing = true;
        } else if (tools_available) {
            managed = true;
        } else {
            throw Error("no local PostgreSQL socket was found and the server "
                        "tools are unavailable\n" +
                        oyama::storage::postgres_install_hint());
        }
    }

    if (existing) {
        AppConfig config = existing_config_from_options(global, force);
        if (config.socket_dir.empty()) {
            if (!interactive) {
                throw Error("no PostgreSQL socket found; pass --socket-dir");
            }
            std::cout << "PostgreSQL socket directory: ";
            std::string socket_dir;
            if (!std::getline(std::cin, socket_dir) || socket_dir.empty()) {
                throw Error("setup cancelled");
            }
            config.socket_dir = socket_dir;
        }
        std::cout << "checking existing PostgreSQL server at "
                  << config.socket_dir << '\n';
        const auto result = initialize_database(
            config.connection_config(), global.migrations);
        oyama::storage::save_app_config(config);
        print_setup_summary(config, result, false);
        return 0;
    }

    if (managed) {
        AppConfig config = managed_config_from_options(global, force);
        LocalCluster cluster(
            cluster_config_with_overrides(config, global));
        const auto binaries = cluster.find_binaries();
        if (!binaries.has_value()) {
            throw Error("PostgreSQL server tools were not found\n" +
                        oyama::storage::postgres_install_hint());
        }
        config.server_bin_dir = binaries->string();
        std::cout << "initializing managed cluster in " << config.data_dir
                  << '\n';
        cluster.initialize();
        std::cout << "starting managed cluster\n";
        cluster.start();
        ConnectionConfig connection = config.connection_config();
        const auto result = initialize_database(connection, global.migrations);
        oyama::storage::save_app_config(config);
        print_setup_summary(config, result, true);
        return 0;
    }

    throw Error("db setup did not select a backend");
}

int command_db(const GlobalOptions& global,
               const std::vector<std::string>& args, std::size_t index) {
    if (index >= args.size()) {
        throw Error("db needs setup, start, stop, cluster-status, ping, init, "
                    "migrate or status");
    }
    const std::string command = args[index++];

    if (command == "setup") {
        return command_db_setup(global, args, index);
    }

    if (command == "start" || command == "stop" ||
        command == "cluster-status") {
        if (index != args.size()) {
            throw Error("unexpected argument for db " + command + ": " +
                        args[index]);
        }
        if (!global.app_config.has_value()) {
            throw Error("no database configuration found; run `db setup`");
        }
        const auto& configured = *global.app_config;
        if (configured.backend != Backend::Managed) {
            if (command == "start" || command == "stop") {
                throw Error("the configured cluster is managed by the system, "
                            "not by Oyama; use your service manager");
            }
            std::cout << "configured\tyes\n"
                      << "backend\texisting\n"
                      << "socket_dir\t" << configured.socket_dir << '\n'
                      << "port\t" << configured.port << '\n'
                      << "database\t" << configured.dbname << '\n'
                      << "cluster\texternal\n";
            return 0;
        }
        LocalCluster cluster(
            cluster_config_with_overrides(configured, global));
        if (command == "start") {
            cluster.start();
            std::cout << "managed cluster running\n";
            return 0;
        }
        if (command == "stop") {
            cluster.stop();
            std::cout << "managed cluster stopped\n";
            return 0;
        }
        std::cout << "configured\tyes\n"
                  << "backend\tmanaged\n"
                  << "data_dir\t" << configured.data_dir << '\n'
                  << "socket_dir\t" << configured.socket_dir << '\n'
                  << "port\t" << configured.port << '\n'
                  << "database\t" << configured.dbname << '\n'
                  << "user\t" << configured.user << '\n'
                  << "initialized\t"
                  << (cluster.is_initialized() ? "yes" : "no") << '\n';
        if (cluster.is_initialized()) {
            std::cout << "running\t"
                      << (cluster.is_running() ? "yes" : "no") << '\n';
        }
        return 0;
    }

    if (index != args.size()) {
        throw Error("unexpected argument for db " + command + ": " + args[index]);
    }

    if (command == "ping") {
        Connection connection(global.config);
        const auto result = connection.exec(
            "SELECT current_database(), current_user, version()");
        std::cout << "database\t" << result.text(0, 0) << '\n';
        std::cout << "user\t" << result.text(0, 1) << '\n';
        std::cout << "server\t" << result.text(0, 2) << '\n';
        std::cout << "server_version\t" << connection.server_version() << '\n';
        return 0;
    }

    if (command == "init") {
        ensure_managed_running(global);
        (void)initialize_database(global.config, global.migrations);
        return 0;
    }

    if (command == "migrate") {
        ensure_managed_running(global);
    }

    Database database(global.config, global.migrations);
    if (command == "migrate") {
        const auto applied = database.migrate();
        for (const auto& migration : applied) {
            std::cout << "applied " << migration.version << " "
                      << migration.name << '\n';
        }
        std::cout << "schema version " << database.current_version() << '\n';
        return 0;
    }
    if (command == "status") {
        const auto status = database.migration_status();
        std::cout << "version\tstate\tchecksum\tname\n";
        for (const auto& migration : status) {
            std::cout << migration.version << '\t'
                      << (migration.applied ? "applied" : "pending") << '\t'
                      << (migration.applied
                              ? (migration.checksum_matches ? "ok" : "MISMATCH")
                              : "-")
                      << '\t' << migration.name;
            if (migration.applied && !migration.applied_at.empty()) {
                std::cout << '\t' << migration.applied_at;
            }
            std::cout << '\n';
        }
        return 0;
    }
    throw Error("unknown db command: " + command);
}

int command_settings(const GlobalOptions& global,
                     const std::vector<std::string>& args, std::size_t index) {
    if (index >= args.size()) {
        throw Error("settings needs show or set");
    }
    const std::string command = args[index++];
    Database database(global.config, global.migrations);
    SettingsRepository repository(database.connection());

    if (command == "show") {
        if (index != args.size()) {
            throw Error("settings show takes no arguments");
        }
        print_settings(repository.get());
        return 0;
    }

    if (command == "set") {
        Settings settings = repository.get();
        bool saw_pk_params = false;
        bool saw_pk_params_updated_at = false;
        bool saw_updated_at = false;
        while (index < args.size()) {
            const std::string option = args[index];
            if (option == "--weight") {
                settings.body_weight_kg =
                    parse_double(require_value(args, index, option), option);
            } else if (option == "--birth-year") {
                settings.birth_year =
                    parse_int(require_value(args, index, option), option);
            } else if (option == "--calibration") {
                settings.calibration_method = require_value(args, index, option);
            } else if (option == "--history-mode") {
                settings.calibration_history_mode =
                    require_value(args, index, option);
            } else if (option == "--pk-params") {
                settings.pk_params_json =
                    read_json_argument(require_value(args, index, option));
                saw_pk_params = true;
            } else if (option == "--pk-params-updated-at") {
                settings.pk_params_updated_at =
                    parse_i64(require_value(args, index, option), option);
                saw_pk_params_updated_at = true;
            } else if (option == "--updated-at") {
                settings.updated_at =
                    parse_i64(require_value(args, index, option), option);
                saw_updated_at = true;
            } else {
                throw Error("unknown settings option: " + option);
            }
            ++index;
        }
        if (saw_pk_params && !saw_pk_params_updated_at) {
            settings.pk_params_updated_at = now_ms();
        }
        if (!saw_updated_at) {
            settings.updated_at = now_ms();
        }
        repository.save(settings);
        print_settings(repository.get());
        return 0;
    }

    throw Error("unknown settings command: " + command);
}

int command_doses(const GlobalOptions& global,
                  const std::vector<std::string>& args, std::size_t index) {
    if (index >= args.size()) {
        throw Error("doses needs list, add or remove");
    }
    const std::string command = args[index++];
    Database database(global.config, global.migrations);
    DoseEventRepository repository(database.connection());

    if (command == "list") {
        Mode mode = Mode::Transfem;
        bool saw_mode = false;
        while (index < args.size()) {
            const std::string option = args[index];
            if (option == "--mode") {
                mode = parse_mode_or_throw(require_value(args, index, option));
                saw_mode = true;
            } else {
                throw Error("unknown doses list option: " + option);
            }
            ++index;
        }
        if (!saw_mode) {
            throw Error("doses list requires --mode");
        }
        std::cout << "id\tmode\troute\tester\ttime_h\tdose_mg\textras\n";
        for (const auto& dose : repository.list(mode)) {
            std::cout << dose.id << '\t' << oyama::storage::to_string(dose.mode)
                      << '\t' << dose.route << '\t' << dose.ester << '\t'
                      << std::setprecision(17) << dose.time_h << '\t'
                      << dose.dose_mg << '\t' << dose.extras_json << '\n';
        }
        return 0;
    }

    if (command == "add") {
        DoseEvent dose;
        dose.id = oyama::storage::generate_uuid_v4();
        const std::int64_t timestamp = now_ms();
        dose.created_at = timestamp;
        dose.updated_at = timestamp;
        dose.mode = Mode::Transfem;
        bool saw_mode = false;
        bool saw_route = false;
        bool saw_ester = false;
        bool saw_dose = false;
        bool saw_time = false;

        while (index < args.size()) {
            const std::string option = args[index];
            if (option == "--mode") {
                dose.mode = parse_mode_or_throw(require_value(args, index, option));
                saw_mode = true;
            } else if (option == "--route") {
                dose.route = require_value(args, index, option);
                saw_route = true;
            } else if (option == "--ester") {
                dose.ester = require_value(args, index, option);
                saw_ester = true;
            } else if (option == "--dose-mg") {
                dose.dose_mg =
                    parse_double(require_value(args, index, option), option);
                saw_dose = true;
            } else if (option == "--time-h") {
                dose.time_h =
                    parse_double(require_value(args, index, option), option);
                saw_time = true;
            } else if (option == "--extras") {
                dose.extras_json = read_json_argument(
                    require_value(args, index, option));
            } else if (option == "--updated-at") {
                dose.updated_at =
                    parse_i64(require_value(args, index, option), option);
            } else if (option == "--created-at") {
                dose.created_at =
                    parse_i64(require_value(args, index, option), option);
            } else if (option == "--id") {
                dose.id = require_value(args, index, option);
            } else {
                throw Error("unknown doses add option: " + option);
            }
            ++index;
        }
        if (!saw_mode || !saw_route || !saw_ester || !saw_dose || !saw_time) {
            throw Error("doses add requires --mode, --route, --ester, "
                        "--dose-mg and --time-h");
        }
        repository.upsert(dose);
        std::cout << "added " << dose.id << '\n';
        return 0;
    }

    if (command == "remove") {
        std::string id;
        while (index < args.size()) {
            const std::string option = args[index];
            if (option == "--id") {
                id = require_value(args, index, option);
            } else {
                throw Error("unknown doses remove option: " + option);
            }
            ++index;
        }
        if (id.empty()) {
            throw Error("doses remove requires --id");
        }
        if (!repository.remove(id)) {
            std::cerr << "dose not found: " << id << '\n';
            return 1;
        }
        std::cout << "removed " << id << '\n';
        return 0;
    }

    throw Error("unknown doses command: " + command);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::vector<std::string> args;
        args.reserve(static_cast<std::size_t>(argc > 0 ? argc - 1 : 0));
        for (int index = 1; index < argc; ++index) {
            args.emplace_back(argv[index]);
        }

        std::size_t index = 0;
        GlobalOptions global = parse_global_options(args, index);
        if (index >= args.size()) {
            print_usage(std::cerr);
            return 2;
        }

        const bool setup_command =
            args[index] == "db" && index + 1 < args.size() &&
            args[index + 1] == "setup";
        if (!setup_command) {
            apply_app_config_defaults(global);
        }

        const std::string command = args[index++];
        if (command == "version") {
            std::cout << "oyama-hrt-cli " << kVersion << '\n';
            return 0;
        }
        if (command == "db") {
            return command_db(global, args, index);
        }
        if (command == "settings") {
            return command_settings(global, args, index);
        }
        if (command == "doses") {
            return command_doses(global, args, index);
        }
        throw Error("unknown command: " + command);
    } catch (const Error& error) {
        std::cerr << "error: " << error.what();
        if (!error.sqlstate().empty()) {
            std::cerr << " [SQLSTATE " << error.sqlstate() << "]";
        }
        std::cerr << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
