#include <oyama/storage/storage.h>
#include <oyama/storage/backup_crypto.h>
#include <oyama/storage/legacy_migration.h>
#include <oyama/storage/legacy_promotion.h>
#include <oyama/storage/passphrase.h>
#include <oyama/storage/vault_dose_event_repository.h>
#include <oyama/storage/vault_settings_repository.h>
#include <oyama/storage/vault_store.h>

#include <sodium.h>

#include <array>
#include <chrono>
#include <cstdint>
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
#include <utility>
#include <vector>

namespace {

using oyama::storage::AppConfig;
using oyama::storage::Backend;
using oyama::storage::BackupPreview;
using oyama::storage::Connection;
using oyama::storage::ConnectionConfig;
using oyama::storage::Database;
using oyama::storage::DoseEvent;
using oyama::storage::Error;
using oyama::storage::LegacyCounts;
using oyama::storage::LegacyDetection;
using oyama::storage::LegacyImportResult;
using oyama::storage::LegacyMigrator;
using oyama::storage::LegacyPaths;
using oyama::storage::LocalCluster;
using oyama::storage::LocalClusterConfig;
using oyama::storage::Mode;
using oyama::storage::PromotionState;
using oyama::storage::is_encrypted_backup;
using oyama::storage::Passphrase;
using oyama::storage::Settings;
using oyama::storage::VaultDoseEventRepository;
using oyama::storage::VaultKdfParams;
using oyama::storage::VaultSettingsRepository;
using oyama::storage::VaultState;
using oyama::storage::VaultStore;

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
    bool phrase_stdin{false};
    bool backup_phrase_stdin{false};
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
        << "  --pg-bin-dir DIR   directory containing initdb/pg_ctl/postgres\n"
        << "  --phrase-stdin     read the vault phrase from one stdin line\n"
        << "  --backup-phrase-stdin\n"
        << "                     read a separate backup phrase from one stdin line\n\n"
        << "Commands:\n"
        << "  version\n"
        << "  vault status      show uninitialized, locked, or unlocked\n"
        << "  vault setup [--phrase-stdin] [--kdf-ops N --kdf-mem BYTES]\n"
        << "  vault unlock-check --phrase-stdin\n"
        << "  vault change-phrase --phrase-stdin --new-phrase-stdin\n"
        << "  db setup [--existing|--managed] [--yes] [--force]\n"
        << "            first-run selection of an existing or managed cluster\n"
        << "  db start           start the application-managed cluster\n"
        << "  db stop            stop the application-managed cluster\n"
        << "  db cluster-status  show the configured cluster and its state\n"
        << "  db ping\n"
        << "  db init            create the database if missing, then migrate\n"
        << "  db migrate\n"
        << "  db migrate-vault [--yes] [--phrase-stdin] [--purge-old PATH]\n"
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
        << "  doses remove --id UUID\n"
        << "  backup export FILE [--plaintext]\n"
        << "  backup preview FILE [--backup-phrase-stdin]\n"
        << "  backup import FILE [--dry-run] [--plaintext]\n"
        << "                [--backup-phrase-stdin]\n";
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

std::uint64_t parse_u64(const std::string& value, std::string_view option) {
    try {
        std::size_t consumed = 0;
        const auto result = std::stoull(value, &consumed);
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
        } else if (option == "--phrase-stdin") {
            options.phrase_stdin = true;
        } else if (option == "--backup-phrase-stdin") {
            options.backup_phrase_stdin = true;
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

constexpr const char* kPlaintextWarning =
    "Deleting the old database directory removes the files this application "
    "knows about. It does not guarantee erasure on SSDs, copy-on-write "
    "filesystems, snapshots, RAID, or backups. Full-disk encryption and "
    "destruction of the old medium are the only reliable remedies.";

class SecretString {
public:
    SecretString() = default;
    ~SecretString() { clear(); }

    SecretString(const SecretString&) = delete;
    SecretString& operator=(const SecretString&) = delete;

    SecretString(SecretString&& other) noexcept
        : value_(std::move(other.value_)) {
        other.clear();
    }

    SecretString& operator=(SecretString&& other) noexcept {
        if (this != &other) {
            clear();
            value_ = std::move(other.value_);
            other.clear();
        }
        return *this;
    }

    void assign(std::string value) {
        clear();
        value_ = std::move(value);
    }

    [[nodiscard]] const std::string& value() const noexcept { return value_; }

    void clear() noexcept {
        if (!value_.empty()) {
            sodium_memzero(value_.data(), value_.size());
        }
        value_.clear();
    }

private:
    std::string value_;
};

std::string read_secret_from_stdin() {
    std::array<char, 4098> buffer{};
    if (!std::cin.getline(buffer.data(), buffer.size())) {
        const bool too_long = std::cin.fail() && !std::cin.eof();
        sodium_memzero(buffer.data(), buffer.size());
        if (too_long) {
            throw Error("phrase on stdin is longer than 4096 bytes");
        }
        throw Error("could not read the vault phrase from stdin");
    }
    const auto count = static_cast<std::size_t>(std::cin.gcount());
    std::string raw(buffer.data(), count);
    sodium_memzero(buffer.data(), buffer.size());
    if (!raw.empty() && raw.back() == '\r') {
        raw.pop_back();
    }
    std::string normalized = Passphrase::normalize(raw);
    sodium_memzero(raw.data(), raw.size());
    if (normalized.empty()) {
        throw Error("empty phrase on stdin");
    }
    return normalized;
}

struct UnlockedVault {
    explicit UnlockedVault(const ConnectionConfig& config) : vault(config) {}

    VaultStore vault;
    SecretString phrase;
};

UnlockedVault open_unlocked_vault(const GlobalOptions& global,
                                  bool phrase_stdin) {
    UnlockedVault unlocked(global.config);
    unlocked.vault.open();
    const VaultState state = unlocked.vault.state();
    if (state == VaultState::Uninitialized) {
        throw Error("vault is not initialized; run `oyama-hrt-cli vault setup`");
    }
    if (state == VaultState::Unlocked) {
        return unlocked;
    }
    if (!phrase_stdin) {
        throw Error(
            "vault is locked; pass --phrase-stdin to supply the vault phrase");
    }
    unlocked.phrase.assign(read_secret_from_stdin());
    unlocked.vault.unlock(unlocked.phrase.value());
    return unlocked;
}

bool backup_file_is_encrypted(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw Error("could not open backup: " + path.string());
    }
    char magic[8];
    input.read(magic, sizeof(magic));
    if (input.bad()) {
        throw Error("could not read backup: " + path.string());
    }
    const auto count = static_cast<std::size_t>(input.gcount());
    return is_encrypted_backup(std::string_view(magic, count));
}

void print_plaintext_warning() {
    std::cout << "warning\t" << kPlaintextWarning << '\n';
}

bool path_exists(const std::filesystem::path& path) {
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) {
        throw Error("could not inspect " + path.string() + ": " +
                    error.message());
    }
    return exists;
}

ConnectionConfig cluster_connection(const LocalCluster& cluster,
                                    const AppConfig& config) {
    ConnectionConfig connection = cluster.connection_config();
    connection.dbname = config.dbname;
    connection.user = config.user;
    return connection;
}

void ensure_database_exists(const ConnectionConfig& config) {
    ConnectionConfig admin_config = config;
    admin_config.dbname = "postgres";
    Connection admin(admin_config);
    const auto exists = admin.exec_params(
        "SELECT 1 FROM pg_database WHERE datname = $1", {config.dbname});
    if (exists.rows() == 0) {
        (void)admin.exec("CREATE DATABASE " +
                         admin.quote_identifier(config.dbname));
    }
}

void print_legacy_counts(const LegacyCounts& counts) {
    std::cout << "count\tsettings\t" << counts.settings << '\n';
    std::cout << "count\tpreferences\t" << counts.preferences << '\n';
    std::cout << "count\tdose_events\t" << counts.dose_events << '\n';
    std::cout << "count\tlab_results\t" << counts.lab_results << '\n';
    std::cout << "count\tdose_templates\t" << counts.dose_templates << '\n';
    std::cout << "count\tquick_doses\t" << counts.quick_doses << '\n';
    std::cout << "count\tsupplies\t" << counts.supplies << '\n';
    std::cout << "count\tplan_items\t" << counts.plan_items << '\n';
    std::cout << "count\ttombstones\t" << counts.tombstones << '\n';
}

void print_promotion_plaintext(const LegacyPaths& paths) {
    const auto plaintext = oyama::storage::find_plaintext_dirs(paths);
    if (plaintext.size() != 1) {
        throw Error("expected exactly one plaintext directory but found " +
                    std::to_string(plaintext.size()));
    }
    std::cout << "plaintext path: " << plaintext.front().string() << '\n';
    print_plaintext_warning();
}

void purge_plaintext(const LegacyPaths& paths,
                     const std::filesystem::path& requested, bool yes) {
    if (!yes) {
        throw Error("deleting the plaintext directory requires --yes");
    }
    if (requested.empty()) {
        throw Error("--purge-old requires a path");
    }
    std::error_code error;
    std::filesystem::path target = std::filesystem::absolute(requested, error);
    if (error) {
        throw Error("could not resolve --purge-old path: " + error.message());
    }
    target = target.lexically_normal();

    const auto plaintext = oyama::storage::find_plaintext_dirs(paths);
    if (plaintext.size() != 1) {
        throw Error("expected exactly one plaintext directory but found " +
                    std::to_string(plaintext.size()));
    }
    if (target != plaintext.front()) {
        throw Error("--purge-old path does not exactly match the plaintext "
                    "directory: " +
                    plaintext.front().string());
    }

    const std::filesystem::file_status status =
        std::filesystem::symlink_status(plaintext.front(), error);
    if (error) {
        throw Error("could not inspect " + plaintext.front().string() + ": " +
                    error.message());
    }
    if (std::filesystem::is_symlink(status)) {
        throw Error("refusing to delete a symlinked plaintext path: " +
                    plaintext.front().string());
    }
    if (!std::filesystem::is_directory(status)) {
        throw Error("plaintext path is not a directory: " +
                    plaintext.front().string());
    }

    std::filesystem::remove_all(plaintext.front(), error);
    if (error) {
        throw Error("could not remove " + plaintext.front().string() + ": " +
                    error.message());
    }
    std::cout << "purged\t" << plaintext.front().string() << '\n';
    print_plaintext_warning();
}

void require_promotion_confirmation(bool yes) {
    if (yes) {
        return;
    }
    if (::isatty(STDIN_FILENO) != 1) {
        throw Error("refusing to promote the migrated vault without --yes");
    }
    std::cout << "Promote the migrated vault and keep the old cluster as a "
                 "plaintext sibling? [y/N]: "
              << std::flush;
    std::string answer;
    if (!std::getline(std::cin, answer)) {
        throw Error("migration cancelled");
    }
    if (answer != "y" && answer != "Y" && answer != "yes" &&
        answer != "YES") {
        throw Error("migration cancelled");
    }
}

AppConfig migration_config(const GlobalOptions& global) {
    if (!global.app_config.has_value()) {
        throw Error("no database configuration found; run `db setup "
                    "--managed`");
    }
    if (global.app_config->backend != Backend::Managed) {
        throw Error(
            "db migrate-vault only supports an application-managed cluster; "
            "for an existing server, use the previous binary's JSON export "
            "(`oyama-hrt-cli backup export --out old.json`) and import it "
            "with `oyama-hrt-cli backup import old.json --plaintext "
            "--phrase-stdin`");
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
    if (global.user_set) {
        config.user = global.config.user;
    }
    if (global.pg_bin_dir_set) {
        config.server_bin_dir = global.pg_bin_dir;
    }
    return config;
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

int command_db_migrate_vault(const GlobalOptions& global,
                             const std::vector<std::string>& args,
                             std::size_t index) {
    bool yes = false;
    bool phrase_stdin = global.phrase_stdin;
    std::optional<std::filesystem::path> purge_old;
    while (index < args.size()) {
        const std::string option = args[index];
        if (option == "--yes" || option == "-y") {
            yes = true;
        } else if (option == "--phrase-stdin") {
            phrase_stdin = true;
        } else if (option == "--purge-old") {
            if (purge_old.has_value()) {
                throw Error("--purge-old specified more than once");
            }
            purge_old = require_value(args, index, option);
        } else if (option == "--help" || option == "-h") {
            print_usage(std::cout);
            return 0;
        } else {
            throw Error("unknown db migrate-vault option: " + option);
        }
        ++index;
    }

    const AppConfig config = migration_config(global);
    const LegacyPaths paths =
        oyama::storage::make_legacy_paths(config.data_dir, config.socket_dir);
    const PromotionState state = oyama::storage::inspect_promotion(paths);

    if (state == PromotionState::ManualReview) {
        throw Error("migration directory state requires manual inspection at " +
                    paths.data_dir.parent_path().string());
    }
    if (state == PromotionState::Promoted) {
        if (purge_old.has_value()) {
            purge_plaintext(paths, *purge_old, yes);
            return 0;
        }
        print_promotion_plaintext(paths);
        return 0;
    }

    LocalClusterConfig old_config;
    old_config.data_dir = paths.data_dir;
    old_config.socket_dir = paths.socket_dir;
    old_config.server_bin_dir = config.server_bin_dir;
    old_config.port = config.port;
    LocalCluster old_cluster(old_config);

    LocalClusterConfig target_config = old_config;
    target_config.data_dir = paths.target_data_dir;
    target_config.socket_dir = paths.target_socket_dir;
    target_config.log_file =
        paths.target_data_dir.parent_path() /
        (paths.target_data_dir.filename().string() + ".log");
    LocalCluster target_cluster(target_config);

    if (state == PromotionState::TargetPending &&
        !path_exists(paths.data_dir)) {
        require_promotion_confirmation(yes);
        if (!target_cluster.is_initialized()) {
            throw Error("migration target is not an initialized cluster: " +
                        paths.target_data_dir.string());
        }
        target_cluster.start();
        try {
            Connection target_connection(
                cluster_connection(target_cluster, config));
            const auto meta = target_connection.exec(
                "SELECT to_regclass('vault_meta') IS NOT NULL AND "
                "EXISTS (SELECT 1 FROM vault_meta)");
            if (!meta.boolean(0, 0)) {
                throw Error("migration target has no vault metadata; "
                            "refusing to finish promotion");
            }
        } catch (...) {
            try {
                target_cluster.stop();
            } catch (...) {
            }
            throw;
        }
        oyama::storage::promote_after_verification(old_cluster, target_cluster,
                                                   paths);
        print_promotion_plaintext(paths);
        if (purge_old.has_value()) {
            purge_plaintext(paths, *purge_old, yes);
        }
        return 0;
    }

    if (!phrase_stdin) {
        throw Error("db migrate-vault requires --phrase-stdin");
    }
    SecretString phrase;
    phrase.assign(read_secret_from_stdin());

    if (state == PromotionState::TargetPending) {
        std::cout << "resuming\t" << paths.target_data_dir.string() << '\n';
    }
    require_promotion_confirmation(yes);

    const bool old_was_running =
        old_cluster.is_initialized() && old_cluster.is_running();
    LegacyImportResult result;
    try {
        old_cluster.start();
        {
            Connection old_connection(
                cluster_connection(old_cluster, config));
            (void)old_connection.exec(
                "SET default_transaction_read_only = on");
            const LegacyDetection detection =
                oyama::storage::detect_legacy(old_connection);
            if (detection.has_meta) {
                throw Error("old cluster already contains a vault "
                            "(vault_meta); mixed legacy and vault data must "
                            "use the JSON export/restore path");
            }
            if (!detection.has_legacy) {
                throw Error("no legacy plaintext data found in " +
                            paths.data_dir.string());
            }

            target_cluster.initialize();
            target_cluster.start();
            const ConnectionConfig target_connection_config =
                cluster_connection(target_cluster, config);
            ensure_database_exists(target_connection_config);
            {
                Database target_database(target_connection_config,
                                         global.migrations);
                (void)target_database.migrate();
                std::cout << "target schema version "
                          << target_database.current_version() << '\n';
            }
            {
                VaultStore vault(target_connection_config);
                vault.open();
                if (vault.state() == VaultState::Uninitialized) {
                    vault.setup(phrase.value());
                    vault.unlock(phrase.value());
                } else if (vault.state() == VaultState::Locked) {
                    vault.unlock(phrase.value());
                }
                if (!vault.is_unlocked()) {
                    throw Error("target vault could not be unlocked");
                }

                LegacyMigrator migrator(
                    old_connection, vault,
                    [](std::string_view kind, std::size_t done,
                       std::size_t total) {
                        std::cout << "progress\t" << kind << '\t' << done
                                  << '/' << total << '\n';
                    });
                result = migrator.import_all();
                if (result.source_sha256 != result.target_sha256) {
                    throw Error("migration verification failed: source and "
                                "target digests differ");
                }
            }
        }
    } catch (...) {
        try {
            target_cluster.stop();
        } catch (...) {
        }
        if (!old_was_running) {
            try {
                old_cluster.stop();
            } catch (...) {
            }
        }
        throw;
    }

    print_legacy_counts(result.counts);
    std::cout << "source_sha256\t" << result.source_sha256 << '\n';
    std::cout << "target_sha256\t" << result.target_sha256 << '\n';
    std::cout << "verified\tyes\n";

    oyama::storage::promote_after_verification(old_cluster, target_cluster,
                                               paths);
    print_promotion_plaintext(paths);
    if (purge_old.has_value()) {
        purge_plaintext(paths, *purge_old, yes);
    }
    return 0;
}

int command_vault(const GlobalOptions& global,
                  const std::vector<std::string>& args, std::size_t index) {
    if (index >= args.size()) {
        throw Error("vault needs status, setup, unlock-check or change-phrase");
    }
    const std::string command = args[index++];
    ensure_managed_running(global);

    if (command == "status") {
        if (index != args.size()) {
            throw Error("unexpected argument for vault status: " + args[index]);
        }
        VaultStore vault(global.config);
        vault.open();
        switch (vault.state()) {
        case VaultState::Uninitialized:
            std::cout << "uninitialized\n";
            break;
        case VaultState::Locked:
            std::cout << "locked\n";
            break;
        case VaultState::Unlocked:
            std::cout << "unlocked\n";
            break;
        }
        return 0;
    }

    if (command == "setup") {
        bool phrase_stdin = global.phrase_stdin;
        bool saw_ops = false;
        bool saw_mem = false;
        VaultKdfParams params;
        while (index < args.size()) {
            const std::string option = args[index];
            if (option == "--phrase-stdin") {
                phrase_stdin = true;
            } else if (option == "--kdf-ops") {
                params.opslimit =
                    parse_u64(require_value(args, index, option), option);
                saw_ops = true;
            } else if (option == "--kdf-mem") {
                params.memlimit =
                    parse_u64(require_value(args, index, option), option);
                saw_mem = true;
            } else if (option == "--help" || option == "-h") {
                print_usage(std::cout);
                return 0;
            } else {
                throw Error("unknown vault setup option: " + option);
            }
            ++index;
        }
        if (saw_ops != saw_mem) {
            throw Error("--kdf-ops and --kdf-mem must be used together");
        }

        VaultStore vault(global.config);
        vault.open();
        if (vault.state() != VaultState::Uninitialized) {
            throw Error("vault is already initialized");
        }

        if (phrase_stdin) {
            SecretString phrase;
            phrase.assign(read_secret_from_stdin());
            vault.setup(phrase.value(), params);
        } else {
            const auto words = Passphrase::generate();
            SecretString phrase;
            phrase.assign(Passphrase::join(words));
            std::cout << "phrase\t" << phrase.value() << '\n' << std::flush;

            const auto positions = Passphrase::challenge(words.size());
            std::vector<std::string> answers;
            answers.reserve(positions.size());
            for (const std::size_t position : positions) {
                std::cout << "word " << (position + 1) << ": " << std::flush;
                std::string answer;
                if (!std::getline(std::cin, answer)) {
                    throw Error("vault setup cancelled");
                }
                answers.push_back(answer);
            }
            if (!Passphrase::verify_challenge(phrase.value(), positions,
                                              answers)) {
                throw Error("vault setup challenge did not match; no vault was "
                            "created");
            }
            vault.setup(phrase.value(), params);
        }
        std::cout << "initialized\n";
        return 0;
    }

    if (command == "unlock-check") {
        bool phrase_stdin = global.phrase_stdin;
        while (index < args.size()) {
            const std::string option = args[index];
            if (option == "--phrase-stdin") {
                phrase_stdin = true;
            } else if (option == "--help" || option == "-h") {
                print_usage(std::cout);
                return 0;
            } else {
                throw Error("unknown vault unlock-check option: " + option);
            }
            ++index;
        }
        (void)open_unlocked_vault(global, phrase_stdin);
        std::cout << "unlocked\n";
        return 0;
    }

    if (command == "change-phrase") {
        bool phrase_stdin = global.phrase_stdin;
        bool new_phrase_stdin = false;
        while (index < args.size()) {
            const std::string option = args[index];
            if (option == "--phrase-stdin") {
                phrase_stdin = true;
            } else if (option == "--new-phrase-stdin") {
                new_phrase_stdin = true;
            } else if (option == "--help" || option == "-h") {
                print_usage(std::cout);
                return 0;
            } else {
                throw Error("unknown vault change-phrase option: " + option);
            }
            ++index;
        }
        if (!new_phrase_stdin) {
            throw Error("vault change-phrase requires --new-phrase-stdin");
        }
        UnlockedVault unlocked = open_unlocked_vault(global, phrase_stdin);
        SecretString new_phrase;
        new_phrase.assign(read_secret_from_stdin());
        unlocked.vault.change_phrase(new_phrase.value());
        std::cout << "changed\n";
        return 0;
    }

    throw Error("unknown vault command: " + command);
}

int command_db(const GlobalOptions& global,
               const std::vector<std::string>& args, std::size_t index) {
    if (index >= args.size()) {
        throw Error("db needs setup, start, stop, cluster-status, ping, init, "
                    "migrate, migrate-vault or status");
    }
    const std::string command = args[index++];

    if (command == "migrate-vault") {
        return command_db_migrate_vault(global, args, index);
    }

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
    ensure_managed_running(global);
    UnlockedVault unlocked =
        open_unlocked_vault(global, global.phrase_stdin);
    VaultSettingsRepository repository(unlocked.vault);

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

void print_backup_preview(const BackupPreview& preview) {
    std::cout << "ok\t" << (preview.ok ? "yes" : "no") << '\n';
    for (const auto& error : preview.errors) {
        std::cout << "error\t" << error << '\n';
    }
    for (const auto& issue : preview.issues) {
        std::cout << "issue\t" << issue << '\n';
    }
    for (const auto& missing : preview.missing) {
        std::cout << "missing\t" << missing << '\n';
    }
    for (const auto& blocked : preview.blocked) {
        std::cout << "blocked\t" << blocked << '\n';
    }
    for (const auto& [mode, counts] : preview.counts) {
        std::cout << "count\t" << mode << "\tevents\t" << counts.events
                  << "\tlabResults\t" << counts.lab_results
                  << "\tdoseTemplates\t" << counts.dose_templates
                  << "\tquickDoses\t" << counts.quick_doses
                  << "\tsupplies\t" << counts.supplies << "\tplan\t"
                  << counts.plan << "\tdeletions\t" << counts.deletions
                  << '\n';
    }
    std::cout << "stats\tadded\t" << preview.stats.added << "\tupdated\t"
              << preview.stats.updated << "\tskipped\t"
              << preview.stats.skipped << "\tremoved\t"
              << preview.stats.removed << '\n';
    for (const auto& id : preview.removed_ids) {
        std::cout << "removed\t" << id << '\n';
    }
}

int command_backup(const GlobalOptions& global,
                   const std::vector<std::string>& args, std::size_t index) {
    if (index >= args.size()) {
        throw Error("backup needs export, preview or import");
    }
    const std::string command = args[index++];
    if (command != "export" && command != "preview" && command != "import") {
        throw Error("unknown backup command: " + command);
    }

    bool dry_run = false;
    bool plaintext = false;
    bool backup_phrase_stdin = global.backup_phrase_stdin;
    std::filesystem::path file;
    while (index < args.size()) {
        const std::string option = args[index];
        if (option == "--dry-run") {
            if (command != "import") {
                throw Error("--dry-run is only valid for backup import");
            }
            dry_run = true;
        } else if (option == "--plaintext") {
            plaintext = true;
        } else if (option == "--backup-phrase-stdin") {
            backup_phrase_stdin = true;
        } else if (option == "--help" || option == "-h") {
            print_usage(std::cout);
            return 0;
        } else if (option.rfind("--", 0) == 0) {
            throw Error("unknown backup " + command + " option: " + option);
        } else if (file.empty()) {
            file = option;
        } else {
            throw Error("unexpected argument for backup " + command + ": " +
                        option);
        }
        ++index;
    }
    if (file.empty()) {
        throw Error("backup " + command + " needs a file");
    }

    bool encrypted = false;
    if (command != "export") {
        encrypted = backup_file_is_encrypted(file);
        if (command == "import" && !encrypted && !plaintext) {
            throw Error("backup is not encrypted; pass --plaintext to import "
                        "an upstream JSON backup");
        }
    }

    ensure_managed_running(global);
    UnlockedVault unlocked =
        open_unlocked_vault(global, global.phrase_stdin);

    if (command == "export") {
        if (plaintext) {
            export_backup(unlocked.vault, file, now_ms());
            std::cout << "exported " << file.string() << '\n';
            std::cout << "warning\tbackup contains plaintext health data\n";
            return 0;
        }
        SecretString backup_phrase;
        std::string_view phrase = unlocked.phrase.value();
        if (backup_phrase_stdin) {
            backup_phrase.assign(read_secret_from_stdin());
            phrase = backup_phrase.value();
        }
        export_encrypted_backup(unlocked.vault, file, now_ms(), phrase);
        std::cout << "exported " << file.string() << '\n';
        return 0;
    }

    SecretString backup_phrase;
    std::string_view file_phrase;
    if (encrypted) {
        if (backup_phrase_stdin) {
            backup_phrase.assign(read_secret_from_stdin());
            file_phrase = backup_phrase.value();
        } else {
            file_phrase = unlocked.phrase.value();
        }
    }

    if (command == "preview" || (command == "import" && dry_run)) {
        const auto preview = preview_backup(unlocked.vault, file, now_ms(),
                                            file_phrase);
        print_backup_preview(preview);
        return preview.ok ? 0 : 1;
    }
    const auto result =
        import_backup(unlocked.vault, file, now_ms(), file_phrase);
    print_backup_preview(result.preview);
    std::cout << "applied\t" << (result.applied ? "yes" : "no") << '\n';
    return result.applied ? 0 : 1;
}

int command_doses(const GlobalOptions& global,
                  const std::vector<std::string>& args, std::size_t index) {
    if (index >= args.size()) {
        throw Error("doses needs list, add or remove");
    }
    const std::string command = args[index++];
    ensure_managed_running(global);
    UnlockedVault unlocked =
        open_unlocked_vault(global, global.phrase_stdin);
    VaultDoseEventRepository repository(unlocked.vault);

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
        if (command == "vault") {
            return command_vault(global, args, index);
        }
        if (command == "settings") {
            return command_settings(global, args, index);
        }
        if (command == "doses") {
            return command_doses(global, args, index);
        }
        if (command == "backup") {
            return command_backup(global, args, index);
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
