#pragma once
#include <oyama/compat/all.h>

#include "connection.h"
#include "local_cluster.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace oyama { namespace storage {

enum class Backend {
    Existing,
    Managed,
};

OYAMA_NODISCARD std::string to_string(Backend backend);
OYAMA_NODISCARD oyama::optional<Backend> parse_backend(oyama::string_view value);

/// Persistent first-run choice and connection defaults.  Weight/birth year/PK
/// settings remain in PostgreSQL; this file only records where the database is.
struct AppConfig {
    Backend backend{Backend::Existing};
    std::string socket_dir;
    std::string data_dir;
    std::string server_bin_dir;
    int port{5432};
    std::string dbname{"oyama_hrt"};
    std::string user;

    OYAMA_NODISCARD ConnectionConfig connection_config() const;
    OYAMA_NODISCARD LocalClusterConfig cluster_config() const;
};

/// $OYAMA_CONFIG_FILE, else $XDG_CONFIG_HOME/oyama-hrt/config, else
/// $HOME/.config/oyama-hrt/config.
OYAMA_NODISCARD oyama::fs::path default_config_path();

/// $XDG_DATA_HOME/oyama-hrt/pgdata, else $HOME/.local/share/oyama-hrt/pgdata.
OYAMA_NODISCARD oyama::fs::path default_data_dir();

/// $OYAMA_PG_RUNTIME_DIR, else $XDG_RUNTIME_DIR/oyama-hrt, else
/// $XDG_DATA_HOME/oyama-hrt/run.  The returned path is not created.
OYAMA_NODISCARD oyama::fs::path default_socket_dir();

OYAMA_NODISCARD oyama::optional<AppConfig> load_app_config(
    const oyama::fs::path& path = default_config_path());

/// Atomically writes a 0600 key=value config file.  Parent directories are
/// created with 0700 permissions.
void save_app_config(const AppConfig& config,
                     const oyama::fs::path& path = default_config_path());

} }  // namespace oyama::storage
