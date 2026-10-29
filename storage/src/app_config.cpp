#include <oyama/storage/app_config.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

namespace oyama::storage {
namespace {

[[nodiscard]] std::string env_value(const char* name) {
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string(value);
}

[[nodiscard]] std::filesystem::path home_dir() {
    const auto home = env_value("HOME");
    if (home.empty()) {
        throw Error("HOME is not set");
    }
    return home;
}

[[nodiscard]] std::filesystem::path xdg_dir(const char* variable,
                                            const std::filesystem::path& fallback) {
    const std::string value = env_value(variable);
    if (!value.empty()) {
        const std::filesystem::path path(value);
        if (path.is_absolute()) {
            return path;
        }
    }
    return fallback;
}

void ensure_private_dir(const std::filesystem::path& path) {
    if (path.empty()) {
        throw Error("directory path is empty");
    }
    struct stat st {};
    if (::lstat(path.c_str(), &st) == 0) {
        if (S_ISLNK(st.st_mode) || !S_ISDIR(st.st_mode)) {
            throw Error("not a directory: " + path.string());
        }
        if (st.st_uid != ::geteuid()) {
            throw Error("directory is not owned by the current user: " +
                        path.string());
        }
        if ((st.st_mode & 0777) != 0700) {
            if (::chmod(path.c_str(), 0700) != 0) {
                throw Error("could not set 0700 on " + path.string() + ": " +
                            std::strerror(errno));
            }
        }
        return;
    }
    if (errno != ENOENT) {
        throw Error("could not inspect " + path.string() + ": " +
                    std::strerror(errno));
    }

    std::error_code error;
    std::filesystem::create_directories(path, error);
    if (error) {
        throw Error("could not create " + path.string() + ": " +
                    error.message());
    }
    if (::chmod(path.c_str(), 0700) != 0) {
        throw Error("could not set 0700 on " + path.string() + ": " +
                    std::strerror(errno));
    }
}

[[nodiscard]] int parse_port(const std::string& value) {
    try {
        std::size_t consumed = 0;
        const int port = std::stoi(value, &consumed);
        if (consumed != value.size() || port < 1 || port > 65535) {
            throw std::invalid_argument("out of range");
        }
        return port;
    } catch (const std::exception&) {
        throw Error("invalid port in config: " + value);
    }
}

[[nodiscard]] bool contains_newline(const std::string& value) {
    return value.find('\n') != std::string::npos ||
           value.find('\r') != std::string::npos;
}

}  // namespace

std::string to_string(Backend backend) {
    switch (backend) {
        case Backend::Existing:
            return "existing";
        case Backend::Managed:
            return "managed";
    }
    return "existing";
}

std::optional<Backend> parse_backend(std::string_view value) {
    if (value == "existing") {
        return Backend::Existing;
    }
    if (value == "managed") {
        return Backend::Managed;
    }
    return std::nullopt;
}

ConnectionConfig AppConfig::connection_config() const {
    ConnectionConfig config;
    config.socket_dir = socket_dir;
    config.port = port;
    config.dbname = dbname;
    config.user = user;
    return config;
}

LocalClusterConfig AppConfig::cluster_config() const {
    LocalClusterConfig config;
    config.data_dir = data_dir;
    config.socket_dir = socket_dir;
    config.server_bin_dir = server_bin_dir;
    config.port = port;
    return config;
}

std::filesystem::path default_config_path() {
    const auto override = env_value("OYAMA_CONFIG_FILE");
    if (!override.empty()) {
        const std::filesystem::path path(override);
        if (!path.is_absolute()) {
            throw Error("OYAMA_CONFIG_FILE must be an absolute path");
        }
        return path;
    }
    const auto config_home =
        xdg_dir("XDG_CONFIG_HOME", home_dir() / ".config");
    return config_home / "oyama-hrt" / "config";
}

std::filesystem::path default_data_dir() {
    const auto data_home =
        xdg_dir("XDG_DATA_HOME", home_dir() / ".local" / "share");
    return data_home / "oyama-hrt" / "pgdata";
}

std::filesystem::path default_socket_dir() {
    const auto override = env_value("OYAMA_PG_RUNTIME_DIR");
    if (!override.empty()) {
        const std::filesystem::path path(override);
        if (!path.is_absolute()) {
            throw Error("OYAMA_PG_RUNTIME_DIR must be an absolute path");
        }
        return path;
    }
    const auto runtime = env_value("XDG_RUNTIME_DIR");
    if (!runtime.empty() && std::filesystem::path(runtime).is_absolute()) {
        return std::filesystem::path(runtime) / "oyama-hrt";
    }
    const auto data_home =
        xdg_dir("XDG_DATA_HOME", home_dir() / ".local" / "share");
    return data_home / "oyama-hrt" / "run";
}

std::optional<AppConfig> load_app_config(const std::filesystem::path& path) {
    std::error_code exists_error;
    if (!std::filesystem::exists(path, exists_error)) {
        if (exists_error) {
            throw Error("could not inspect config " + path.string() + ": " +
                        exists_error.message());
        }
        return std::nullopt;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw Error("could not open config " + path.string());
    }

    AppConfig config;
    bool saw_backend = false;
    std::string line;
    int line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const auto equals = line.find('=');
        if (equals == std::string::npos) {
            throw Error("invalid config line " + std::to_string(line_number) +
                        " in " + path.string());
        }
        const std::string key = line.substr(0, equals);
        const std::string value = line.substr(equals + 1);
        if (key == "version") {
            if (value != "1") {
                throw Error("unsupported config version: " + value);
            }
        } else if (key == "backend") {
            const auto backend = parse_backend(value);
            if (!backend.has_value()) {
                throw Error("invalid backend in config: " + value);
            }
            config.backend = *backend;
            saw_backend = true;
        } else if (key == "socket_dir") {
            config.socket_dir = value;
        } else if (key == "data_dir") {
            config.data_dir = value;
        } else if (key == "server_bin_dir") {
            config.server_bin_dir = value;
        } else if (key == "port") {
            config.port = parse_port(value);
        } else if (key == "dbname") {
            config.dbname = value;
        } else if (key == "user") {
            config.user = value;
        }
    }
    if (input.bad()) {
        throw Error("could not read config " + path.string());
    }
    if (!saw_backend) {
        throw Error("config has no backend: " + path.string());
    }
    if (config.socket_dir.empty()) {
        throw Error("config has no socket_dir: " + path.string());
    }
    if (!std::filesystem::path(config.socket_dir).is_absolute()) {
        throw Error("config socket_dir must be absolute: " + path.string());
    }
    if (config.backend == Backend::Managed) {
        if (config.data_dir.empty()) {
            throw Error("managed config has no data_dir: " + path.string());
        }
        if (!std::filesystem::path(config.data_dir).is_absolute()) {
            throw Error("config data_dir must be absolute: " + path.string());
        }
    }
    if (!config.server_bin_dir.empty() &&
        !std::filesystem::path(config.server_bin_dir).is_absolute()) {
        throw Error("config server_bin_dir must be absolute: " +
                    path.string());
    }
    if (config.dbname.empty()) {
        throw Error("config has no dbname: " + path.string());
    }
    return config;
}

void save_app_config(const AppConfig& config,
                     const std::filesystem::path& path) {
    if (!path.is_absolute()) {
        throw Error("config path must be absolute: " + path.string());
    }
    if (config.socket_dir.empty() || config.dbname.empty()) {
        throw Error("config socket_dir and dbname are required");
    }
    if (!std::filesystem::path(config.socket_dir).is_absolute()) {
        throw Error("config socket_dir must be absolute");
    }
    if (config.backend == Backend::Managed) {
        if (config.data_dir.empty() ||
            !std::filesystem::path(config.data_dir).is_absolute()) {
            throw Error("managed config data_dir must be absolute");
        }
    }
    if (!config.server_bin_dir.empty() &&
        !std::filesystem::path(config.server_bin_dir).is_absolute()) {
        throw Error("config server_bin_dir must be absolute");
    }
    for (const auto* value : {&config.socket_dir, &config.data_dir,
                              &config.server_bin_dir, &config.dbname,
                              &config.user}) {
        if (contains_newline(*value)) {
            throw Error("config values must not contain newlines");
        }
    }

    ensure_private_dir(path.parent_path());
    const auto temporary =
        path.string() + ".tmp." + std::to_string(::getpid());
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw Error("could not open " + temporary + " for writing");
        }
        output << "version=1\n"
               << "backend=" << to_string(config.backend) << '\n'
               << "socket_dir=" << config.socket_dir << '\n'
               << "data_dir=" << config.data_dir << '\n'
               << "server_bin_dir=" << config.server_bin_dir << '\n'
               << "port=" << config.port << '\n'
               << "dbname=" << config.dbname << '\n'
               << "user=" << config.user << '\n';
        if (!output) {
            throw Error("could not write " + temporary);
        }
    }
    if (::chmod(temporary.c_str(), 0600) != 0) {
        std::filesystem::remove(temporary);
        throw Error("could not set 0600 on " + temporary + ": " +
                    std::strerror(errno));
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary);
        throw Error("could not replace " + path.string() + ": " +
                    error.message());
    }
}

}  // namespace oyama::storage
