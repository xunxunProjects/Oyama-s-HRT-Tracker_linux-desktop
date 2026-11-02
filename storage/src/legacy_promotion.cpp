#include "oyama/storage/legacy_promotion.h"

#include "oyama/storage/error.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <system_error>
#include <string_view>
#include <unistd.h>

namespace oyama { namespace storage {
namespace {

namespace fs = oyama::fs;

OYAMA_NODISCARD fs::path absolute_lexical(const fs::path& path,
                                        oyama::string_view label) {
    std::error_code error;
    fs::path result = fs::absolute(path, error);
    if (error) {
        throw Error("could not resolve " + std::string(label) + " " +
                    path.string() + ": " + error.message());
    }
    return result.lexically_normal();
}

OYAMA_NODISCARD bool is_missing_error(const std::error_code& error) {
    return error == std::errc::no_such_file_or_directory ||
           error.value() == ENOENT;
}

OYAMA_NODISCARD bool require_directory(const fs::path& path,
                                     oyama::string_view label) {
    std::error_code error;
    const fs::file_status status = fs::symlink_status(path, error);
    if (error) {
        if (is_missing_error(error)) {
            return false;
        }
        throw Error("could not inspect " + std::string(label) + " " +
                    path.string() + ": " + error.message());
    }
    if (status.type() == fs::file_type::not_found) {
        return false;
    }
    if (fs::is_symlink(status)) {
        throw Error(std::string(label) + " is a symlink: " + path.string());
    }
    if (!fs::is_directory(status)) {
        throw Error(std::string(label) + " is not a directory: " +
                    path.string());
    }
    return true;
}

OYAMA_NODISCARD bool directory_present(const fs::path& path) {
    std::error_code error;
    const fs::file_status status = fs::symlink_status(path, error);
    if (error) {
        if (is_missing_error(error)) {
            return false;
        }
        throw Error("could not inspect " + path.string() + ": " +
                    error.message());
    }
    if (status.type() == fs::file_type::not_found) {
        return false;
    }
    if (fs::is_symlink(status)) {
        throw Error("path is a symlink: " + path.string());
    }
    if (!fs::is_directory(status)) {
        throw Error("path is not a directory: " + path.string());
    }
    return true;
}

void validate_paths(const LegacyPaths& paths) {
    if (paths.data_dir.empty()) {
        throw Error("migration data directory is empty");
    }
    if (!paths.data_dir.is_absolute()) {
        throw Error("migration data directory must be absolute: " +
                    paths.data_dir.string());
    }
    if (!paths.target_data_dir.is_absolute()) {
        throw Error("migration target directory must be absolute: " +
                    paths.target_data_dir.string());
    }

    const fs::path old = paths.data_dir.lexically_normal();
    const fs::path target = paths.target_data_dir.lexically_normal();
    if (old == old.root_path() || old.filename().empty()) {
        throw Error("refusing to use the filesystem root as a migration data "
                    "directory");
    }
    if (target == old) {
        throw Error("migration target must not equal the old data directory");
    }
    const fs::path expected =
        old.parent_path() / (old.filename().string() + "-migrate");
    if (target != expected) {
        throw Error("migration target " + target.string() +
                    " is not the expected sibling " + expected.string());
    }

    (void)require_directory(old, "old data directory");
    (void)require_directory(target, "migration target directory");
}

OYAMA_NODISCARD fs::path migrate_sibling(const fs::path& path) {
    if (path == path.root_path() || path.filename().empty()) {
        throw Error("refusing to use the filesystem root as a migration path");
    }
    return path.parent_path() / (path.filename().string() + "-migrate");
}

OYAMA_NODISCARD std::string utc_timestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm utc{};
    if (::gmtime_r(&now, &utc) == nullptr) {
        throw Error("could not compute the promotion timestamp");
    }
    char buffer[32];
    if (std::strftime(buffer, sizeof(buffer), "%Y%m%dT%H%M%SZ", &utc) == 0) {
        throw Error("could not format the promotion timestamp");
    }
    return buffer;
}

void flush_directory(const fs::path& directory) {
#ifdef O_DIRECTORY
    const int descriptor =
        ::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
#else
    const int descriptor = ::open(directory.c_str(), O_RDONLY | O_CLOEXEC);
#endif
    if (descriptor < 0) {
        return;
    }
    const int result = ::fsync(descriptor);
    const int saved_errno = errno;
    (void)::close(descriptor);
    if (result == 0 || saved_errno == EINVAL || saved_errno == ENOTSUP ||
        saved_errno == EOPNOTSUPP) {
        return;
    }
    throw Error("could not flush " + directory.string() + ": " +
                std::string(std::strerror(saved_errno)));
}

OYAMA_NODISCARD std::vector<fs::path> find_plaintext_dirs_unchecked(
    const LegacyPaths& paths) {
    const fs::path parent = paths.data_dir.parent_path();
    const std::string prefix =
        paths.data_dir.filename().string() + "-plaintext-";
    std::vector<fs::path> found;

    std::error_code error;
    if (!fs::exists(parent, error)) {
        if (error) {
            throw Error("could not inspect " + parent.string() + ": " +
                        error.message());
        }
        return found;
    }

    fs::directory_iterator iterator(parent, error);
    if (error) {
        throw Error("could not read " + parent.string() + ": " +
                    error.message());
    }
    const fs::directory_iterator end;
    while (iterator != end) {
        const fs::path candidate = (*iterator).path();
        const std::string name = candidate.filename().string();
        if (name.rfind(prefix, 0) == 0) {
            std::error_code status_error;
            const fs::file_status status =
                fs::symlink_status(candidate, status_error);
            if (status_error) {
                throw Error("could not inspect " + candidate.string() + ": " +
                            status_error.message());
            }
            if (fs::is_symlink(status)) {
                throw Error("plaintext path is a symlink: " +
                            candidate.string());
            }
            if (!fs::is_directory(status)) {
                throw Error("plaintext path is not a directory: " +
                            candidate.string());
            }
            found.push_back(candidate.lexically_normal());
        }
        iterator.increment(error);
        if (error) {
            throw Error("could not read " + parent.string() + ": " +
                        error.message());
        }
    }

    std::sort(found.begin(), found.end());
    return found;
}

OYAMA_NODISCARD std::string describe_state(const LegacyPaths& paths) {
    const bool old_present = directory_present(paths.data_dir);
    const bool target_present = directory_present(paths.target_data_dir);
    const std::size_t plaintext_count =
        find_plaintext_dirs_unchecked(paths).size();
    return "old=" + std::string(old_present ? "present" : "missing") +
           " target=" + std::string(target_present ? "present" : "missing") +
           " plaintext=" + std::to_string(plaintext_count);
}

}  // namespace

std::string to_string(PromotionState state) {
    switch (state) {
        case PromotionState::Clean:
            return "clean";
        case PromotionState::TargetPending:
            return "target-pending";
        case PromotionState::Promoted:
            return "promoted";
        case PromotionState::ManualReview:
            return "manual-review";
    }
    return "unknown";
}

LegacyPaths make_legacy_paths(const fs::path& data_dir,
                              const fs::path& socket_dir) {
    if (data_dir.empty()) {
        throw Error("migration data directory is empty");
    }

    LegacyPaths paths;
    paths.data_dir = absolute_lexical(data_dir, "data directory");
    paths.target_data_dir = migrate_sibling(paths.data_dir);

    if (socket_dir.empty()) {
        paths.socket_dir = paths.data_dir / "run";
    } else {
        paths.socket_dir = absolute_lexical(socket_dir, "socket directory");
    }
    if (paths.socket_dir == paths.data_dir / "run") {
        paths.target_socket_dir = paths.target_data_dir / "run";
    } else {
        paths.target_socket_dir = migrate_sibling(paths.socket_dir);
    }

    validate_paths(paths);
    (void)require_directory(paths.socket_dir, "old socket directory");
    (void)require_directory(paths.target_socket_dir,
                            "migration socket directory");
    return paths;
}

std::vector<fs::path> find_plaintext_dirs(const LegacyPaths& paths) {
    validate_paths(paths);
    return find_plaintext_dirs_unchecked(paths);
}

PromotionState inspect_promotion(const LegacyPaths& paths) {
    validate_paths(paths);

    const bool old_present = directory_present(paths.data_dir);
    const bool target_present = directory_present(paths.target_data_dir);
    const std::vector<fs::path> plaintext =
        find_plaintext_dirs_unchecked(paths);

    if (plaintext.size() > 1) {
        return PromotionState::ManualReview;
    }
    if (old_present && target_present) {
        return plaintext.empty() ? PromotionState::TargetPending
                                 : PromotionState::ManualReview;
    }
    if (old_present) {
        return plaintext.empty() ? PromotionState::Clean
                                 : PromotionState::Promoted;
    }
    if (target_present) {
        return plaintext.size() == 1 ? PromotionState::TargetPending
                                     : PromotionState::ManualReview;
    }
    return PromotionState::ManualReview;
}

void promote_after_verification(LocalCluster& old_cluster,
                                LocalCluster& target_cluster,
                                const LegacyPaths& paths) {
    validate_paths(paths);
    if (old_cluster.config().data_dir.lexically_normal() !=
        paths.data_dir.lexically_normal()) {
        throw Error("old cluster data directory does not match the migration "
                    "paths");
    }
    if (target_cluster.config().data_dir.lexically_normal() !=
        paths.target_data_dir.lexically_normal()) {
        throw Error("target cluster data directory does not match the "
                    "migration paths");
    }

    const PromotionState initial = inspect_promotion(paths);
    if (initial == PromotionState::Promoted) {
        return;
    }
    if (initial == PromotionState::ManualReview) {
        throw Error("migration directories require manual inspection: " +
                    describe_state(paths));
    }
    if (initial == PromotionState::Clean) {
        throw Error("no migration target to promote at " +
                    paths.target_data_dir.string());
    }

    old_cluster.stop();
    target_cluster.stop();

    const PromotionState stopped = inspect_promotion(paths);
    if (stopped != PromotionState::TargetPending) {
        throw Error("migration state changed while stopping the clusters: " +
                    std::string(to_string(stopped)));
    }

    if (directory_present(paths.data_dir)) {
        const fs::path plaintext =
            paths.data_dir.parent_path() /
            (paths.data_dir.filename().string() + "-plaintext-" +
             utc_timestamp());
        std::error_code exists_error;
        if (fs::exists(plaintext, exists_error)) {
            throw Error("plaintext path already exists: " +
                        plaintext.string());
        }
        if (exists_error) {
            throw Error("could not inspect " + plaintext.string() + ": " +
                        exists_error.message());
        }

        std::error_code rename_error;
        fs::rename(paths.data_dir, plaintext, rename_error);
        if (rename_error) {
            throw Error("could not rename " + paths.data_dir.string() +
                        " to " + plaintext.string() + ": " +
                        rename_error.message());
        }
        flush_directory(paths.data_dir.parent_path());
    }

    if (!directory_present(paths.target_data_dir)) {
        throw Error("migration target disappeared before promotion: " +
                    paths.target_data_dir.string());
    }
    std::error_code rename_error;
    fs::rename(paths.target_data_dir, paths.data_dir, rename_error);
    if (rename_error) {
        throw Error("could not rename " + paths.target_data_dir.string() +
                    " to " + paths.data_dir.string() + ": " +
                    rename_error.message());
    }
    flush_directory(paths.data_dir.parent_path());

    if (inspect_promotion(paths) != PromotionState::Promoted) {
        throw Error("promotion did not reach the promoted state: " +
                    describe_state(paths));
    }
}

} }  // namespace oyama::storage
