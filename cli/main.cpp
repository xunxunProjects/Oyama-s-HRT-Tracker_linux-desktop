#include <oyama/storage/storage.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using oyama::storage::Connection;
using oyama::storage::ConnectionConfig;
using oyama::storage::Database;
using oyama::storage::DoseEvent;
using oyama::storage::DoseEventRepository;
using oyama::storage::Error;
using oyama::storage::Mode;
using oyama::storage::Settings;
using oyama::storage::SettingsRepository;

constexpr const char* kVersion = "0.1.0";

struct GlobalOptions {
    ConnectionConfig config;
    std::filesystem::path migrations;
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
        << "  --migrations DIR   migration directory\n\n"
        << "Commands:\n"
        << "  version\n"
        << "  db ping\n"
        << "  db init            create the database if missing, then migrate\n"
        << "  db migrate\n"
        << "  db status\n"
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
    GlobalOptions options{ConnectionConfig::from_environment(),
                          default_migrations_dir()};
    while (index < args.size() && args[index].rfind("--", 0) == 0) {
        const std::string option = args[index];
        if (option == "--help" || option == "-h") {
            print_usage(std::cout);
            std::exit(0);
        }
        if (option == "--socket-dir") {
            options.config.socket_dir =
                require_value(args, index, "--socket-dir");
        } else if (option == "--dbname") {
            options.config.dbname = require_value(args, index, "--dbname");
        } else if (option == "--user") {
            options.config.user = require_value(args, index, "--user");
        } else if (option == "--port") {
            options.config.port =
                parse_int(require_value(args, index, "--port"), "--port");
        } else if (option == "--migrations") {
            options.migrations = require_value(args, index, "--migrations");
        } else {
            throw Error("unknown global option: " + option);
        }
        ++index;
    }
    return options;
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

int command_db(const GlobalOptions& global,
               const std::vector<std::string>& args, std::size_t index) {
    if (index >= args.size()) {
        throw Error("db needs ping, init, migrate or status");
    }
    const std::string command = args[index++];
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
        ConnectionConfig admin_config = global.config;
        admin_config.dbname = "postgres";
        Connection admin(admin_config);
        const auto exists = admin.exec_params(
            "SELECT 1 FROM pg_database WHERE datname = $1",
            {global.config.dbname});
        if (exists.rows() == 0) {
            (void)admin.exec("CREATE DATABASE " +
                             admin.quote_identifier(global.config.dbname));
            std::cout << "created database " << global.config.dbname << '\n';
        } else {
            std::cout << "database " << global.config.dbname
                      << " already exists\n";
        }
        Database database(global.config, global.migrations);
        const auto applied = database.migrate();
        std::cout << "schema version " << database.current_version() << '\n';
        std::cout << "applied " << applied.size() << " migration(s)\n";
        return 0;
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
        const GlobalOptions global = parse_global_options(args, index);
        if (index >= args.size()) {
            print_usage(std::cerr);
            return 2;
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
