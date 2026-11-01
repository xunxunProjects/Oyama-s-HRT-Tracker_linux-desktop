#pragma once
#include <oyama/compat/all.h>

#include "local_cluster.h"

#include <filesystem>
#include <string>
#include <vector>

namespace oyama { namespace storage {

/// Sibling paths used by a managed plaintext-to-vault migration.  The old and
/// target clusters can run at the same time because their socket directories
/// differ.  These paths are derived, never written into AppConfig.
struct LegacyPaths {
    oyama::fs::path data_dir;
    oyama::fs::path target_data_dir;
    oyama::fs::path socket_dir;
    oyama::fs::path target_socket_dir;
};

/// The directory names are the only promotion journal.  The states follow the
/// recovery table in the design document.
enum class PromotionState {
    Clean,
    TargetPending,
    Promoted,
    ManualReview,
};

OYAMA_NODISCARD std::string to_string(PromotionState state);

/// Derives `<data_dir>-migrate` and `<socket_dir>-migrate`.  A socket directory
/// equal to `<data_dir>/run` (or an empty socket directory) maps to
/// `<target_data_dir>/run`.  Both inputs are resolved to absolute lexical
/// paths.  Existing symlinks and non-directories are rejected, as is a target
/// that is not exactly the expected sibling.
OYAMA_NODISCARD LegacyPaths make_legacy_paths(
    const oyama::fs::path& data_dir,
    const oyama::fs::path& socket_dir);

/// Returns all `<data_dir>-plaintext-*` siblings.  Symlinks and non-directories
/// with that prefix are rejected.  The function never starts PostgreSQL.
OYAMA_NODISCARD std::vector<oyama::fs::path> find_plaintext_dirs(
    const LegacyPaths& paths);

/// Inspects only directory names and existence.  Never starts PostgreSQL.
OYAMA_NODISCARD PromotionState inspect_promotion(const LegacyPaths& paths);

/// Stops both clusters, renames the old directory to
/// `<data_dir>-plaintext-YYYYMMDDTHHMMSSZ`, then renames the target into the
/// old path.  Never updates AppConfig.  Throws unless the current state is
/// TargetPending (or already Promoted, which is an idempotent no-op).
void promote_after_verification(LocalCluster& old_cluster,
                                LocalCluster& target_cluster,
                                const LegacyPaths& paths);

} }  // namespace oyama::storage
