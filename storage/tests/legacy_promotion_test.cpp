#include "oyama/storage/legacy_promotion.h"

#include "oyama/storage/local_cluster.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
using namespace oyama::storage;

namespace {

int failures = 0;

void check(bool condition, std::string_view what) {
    if (!condition) {
        std::cerr << "FAIL: " << what << '\n';
        ++failures;
    }
}

template <typename Function>
void check_throws(Function&& function, std::string_view what) {
    try {
        function();
    } catch (const std::exception&) {
        return;
    }
    check(false, what);
}

class TempDir {
public:
    TempDir() {
        std::string pattern =
            (fs::temp_directory_path() / "oyama-promotion-test-XXXXXX").string();
        std::vector<char> buffer(pattern.begin(), pattern.end());
        buffer.push_back('\0');
        char* created = ::mkdtemp(buffer.data());
        if (created == nullptr) {
            throw std::runtime_error("could not create a temporary directory");
        }
        path_ = created;
    }

    ~TempDir() {
        std::error_code error;
        fs::remove_all(path_, error);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    [[nodiscard]] const fs::path& path() const noexcept { return path_; }

private:
    fs::path path_;
};

void write_file(const fs::path& path, std::string_view contents) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("could not open " + path.string());
    }
    output.write(contents.data(),
                 static_cast<std::streamsize>(contents.size()));
    if (!output) {
        throw std::runtime_error("could not write " + path.string());
    }
}

[[nodiscard]] std::string read_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not open " + path.string());
    }
    return std::string((std::istreambuf_iterator<char>(input)),
                       std::istreambuf_iterator<char>());
}

[[nodiscard]] LocalCluster make_cluster(const fs::path& data_dir,
                                        const fs::path& socket_dir) {
    LocalClusterConfig config;
    config.data_dir = data_dir;
    config.socket_dir = socket_dir;
    config.port = 55432;
    return LocalCluster(config);
}

[[nodiscard]] fs::path plaintext_path(const fs::path& data_dir,
                                      std::string_view suffix) {
    return fs::path(data_dir.string() + "-plaintext-" + std::string(suffix));
}

void test_path_derivation(const fs::path& root) {
    const auto paths = make_legacy_paths(root / "pgdata", root / "run");
    check(paths.data_dir == fs::absolute(root / "pgdata").lexically_normal(),
          "old data dir is absolute");
    check(paths.target_data_dir ==
              fs::absolute(root / "pgdata-migrate").lexically_normal(),
          "target data dir is the -migrate sibling");
    check(paths.socket_dir == fs::absolute(root / "run").lexically_normal(),
          "old socket dir is absolute");
    check(paths.target_socket_dir ==
              fs::absolute(root / "run-migrate").lexically_normal(),
          "target socket dir is the -migrate sibling");

    const auto custom =
        make_legacy_paths(root / "custom/nested/pg", root / "custom/sockets");
    check(custom.target_data_dir ==
              fs::absolute(root / "custom/nested/pg-migrate").lexically_normal(),
          "custom data dir keeps the sibling rule");
    check(custom.target_socket_dir ==
              fs::absolute(root / "custom/sockets-migrate").lexically_normal(),
          "custom socket dir keeps the sibling rule");

    const auto fallback =
        make_legacy_paths(root / "fallback/pg", root / "fallback/pg/run");
    check(fallback.target_socket_dir ==
              fs::absolute(root / "fallback/pg-migrate/run").lexically_normal(),
          "data_dir/run fallback maps into the target");

    const auto empty_socket = make_legacy_paths(root / "empty/pg", {});
    check(empty_socket.target_socket_dir ==
              fs::absolute(root / "empty/pg-migrate/run").lexically_normal(),
          "empty socket dir maps into the target");
}

void test_states(const fs::path& root) {
    auto paths = make_legacy_paths(root / "states/pgdata", root / "states/run");
    fs::create_directories(paths.data_dir);
    check(inspect_promotion(paths) == PromotionState::Clean, "clean state");
    check(inspect_promotion(paths) == PromotionState::Clean,
          "clean state is idempotent");

    fs::create_directories(paths.target_data_dir);
    check(inspect_promotion(paths) == PromotionState::TargetPending,
          "target pending");
    check(inspect_promotion(paths) == PromotionState::TargetPending,
          "target pending is idempotent");

    const fs::path plaintext =
        plaintext_path(paths.data_dir, "20260101T000000Z");
    fs::create_directories(plaintext);
    fs::rename(paths.data_dir, plaintext);
    check(inspect_promotion(paths) == PromotionState::TargetPending,
          "crashed between renames is target pending");

    fs::rename(paths.target_data_dir, paths.data_dir);
    check(inspect_promotion(paths) == PromotionState::Promoted, "promoted");
    check(inspect_promotion(paths) == PromotionState::Promoted,
          "promoted state is idempotent");
    const auto found = find_plaintext_dirs(paths);
    check(found.size() == 1 && found.front() == plaintext,
          "promoted state finds its plaintext sibling");
}

void test_ambiguous_states(const fs::path& root) {
    auto two_plaintext =
        make_legacy_paths(root / "ambiguous/two/pg", root / "ambiguous/two/run");
    fs::create_directories(two_plaintext.data_dir);
    fs::create_directories(
        plaintext_path(two_plaintext.data_dir, "20260101T000000Z"));
    fs::create_directories(
        plaintext_path(two_plaintext.data_dir, "20260102T000000Z"));
    check(inspect_promotion(two_plaintext) == PromotionState::ManualReview,
          "two plaintext siblings require manual review");

    auto old_target_plaintext = make_legacy_paths(
        root / "ambiguous/all/pg", root / "ambiguous/all/run");
    fs::create_directories(old_target_plaintext.data_dir);
    fs::create_directories(old_target_plaintext.target_data_dir);
    fs::create_directories(
        plaintext_path(old_target_plaintext.data_dir, "20260101T000000Z"));
    check(inspect_promotion(old_target_plaintext) ==
              PromotionState::ManualReview,
          "old, target, and plaintext require manual review");

    auto missing = make_legacy_paths(root / "ambiguous/missing/pg",
                                     root / "ambiguous/missing/run");
    check(inspect_promotion(missing) == PromotionState::ManualReview,
          "missing old and target require manual review");

    auto target_only = make_legacy_paths(root / "ambiguous/target/pg",
                                         root / "ambiguous/target/run");
    fs::create_directories(target_only.target_data_dir);
    check(inspect_promotion(target_only) == PromotionState::ManualReview,
          "target without old or plaintext requires manual review");
}

void test_rejections(const fs::path& root) {
    auto paths = make_legacy_paths(root / "reject/pgdata", root / "reject/run");
    fs::create_directories(paths.data_dir);

    const fs::path symlink_target = root / "reject/elsewhere";
    fs::create_directories(symlink_target);
    fs::create_symlink(symlink_target, paths.target_data_dir);
    check_throws([&] { (void)inspect_promotion(paths); },
                 "target symlink is rejected");
    fs::remove(paths.target_data_dir);

    LegacyPaths equal = paths;
    equal.target_data_dir = equal.data_dir;
    check_throws([&] { (void)inspect_promotion(equal); },
                 "target equal to old is rejected");

    LegacyPaths not_sibling = paths;
    not_sibling.target_data_dir = root / "reject/not-a-sibling";
    check_throws([&] { (void)inspect_promotion(not_sibling); },
                 "target outside the sibling rule is rejected");

    write_file(paths.target_data_dir, "not a directory");
    check_throws([&] { (void)make_legacy_paths(paths.data_dir,
                                               paths.socket_dir); },
                 "non-directory target is rejected");
    fs::remove(paths.target_data_dir);

    const fs::path real_data = root / "reject/real-data";
    fs::create_directories(real_data);
    const fs::path linked_data = root / "reject/linked-data";
    fs::create_symlink(real_data, linked_data);
    check_throws([&] { (void)make_legacy_paths(linked_data, paths.socket_dir); },
                 "symlinked old data directory is rejected");

    write_file(root / "reject/file-data", "not a directory");
    check_throws(
        [&] { (void)make_legacy_paths(root / "reject/file-data", {}); },
        "non-directory old data directory is rejected");
}

void test_promotion(const fs::path& root) {
    auto paths = make_legacy_paths(root / "promote/pgdata", root / "promote/run");
    fs::create_directories(paths.data_dir);
    fs::create_directories(paths.target_data_dir);
    write_file(paths.data_dir / "old.marker", "old");
    write_file(paths.target_data_dir / "target.marker", "target");

    LocalCluster old_cluster = make_cluster(paths.data_dir, paths.socket_dir);
    LocalCluster target_cluster =
        make_cluster(paths.target_data_dir, paths.target_socket_dir);
    promote_after_verification(old_cluster, target_cluster, paths);

    check(inspect_promotion(paths) == PromotionState::Promoted,
          "promotion lands in promoted state");
    check(read_file(paths.data_dir / "target.marker") == "target",
          "target directory took over the old path");
    check(!fs::exists(paths.target_data_dir), "target path is gone");
    const auto plaintext = find_plaintext_dirs(paths);
    check(plaintext.size() == 1, "exactly one plaintext sibling");
    if (plaintext.size() == 1) {
        check(read_file(plaintext.front() / "old.marker") == "old",
              "plaintext sibling contains the old cluster");
        const std::string name = plaintext.front().filename().string();
        check(std::regex_match(name,
                               std::regex(".*-plaintext-[0-9]{8}T[0-9]{6}Z")),
              "plaintext sibling uses the UTC timestamp form");
    }

    promote_after_verification(old_cluster, target_cluster, paths);
    check(inspect_promotion(paths) == PromotionState::Promoted,
          "promotion is idempotent");
}

void test_resume_after_old_rename(const fs::path& root) {
    auto paths = make_legacy_paths(root / "resume/pgdata", root / "resume/run");
    fs::create_directories(paths.target_data_dir);
    write_file(paths.target_data_dir / "target.marker", "target");
    const fs::path plaintext =
        plaintext_path(paths.data_dir, "20260101T000000Z");
    fs::create_directories(plaintext);
    write_file(plaintext / "old.marker", "old");

    LocalCluster old_cluster = make_cluster(paths.data_dir, paths.socket_dir);
    LocalCluster target_cluster =
        make_cluster(paths.target_data_dir, paths.target_socket_dir);
    check(inspect_promotion(paths) == PromotionState::TargetPending,
          "resume state is target pending");
    promote_after_verification(old_cluster, target_cluster, paths);

    check(inspect_promotion(paths) == PromotionState::Promoted,
          "resume finishes in promoted state");
    check(read_file(paths.data_dir / "target.marker") == "target",
          "resume moves the target into place");
    check(read_file(plaintext / "old.marker") == "old",
          "resume leaves the plaintext sibling untouched");
}

void test_refused_promotions(const fs::path& root) {
    auto clean = make_legacy_paths(root / "refuse/clean/pg", root / "refuse/clean/run");
    fs::create_directories(clean.data_dir);
    LocalCluster clean_old = make_cluster(clean.data_dir, clean.socket_dir);
    LocalCluster clean_target =
        make_cluster(clean.target_data_dir, clean.target_socket_dir);
    check_throws(
        [&] { promote_after_verification(clean_old, clean_target, clean); },
        "clean state is not promoted");

    auto ambiguous =
        make_legacy_paths(root / "refuse/ambiguous/pg", root / "refuse/ambiguous/run");
    fs::create_directories(ambiguous.data_dir);
    fs::create_directories(ambiguous.target_data_dir);
    const fs::path plaintext =
        plaintext_path(ambiguous.data_dir, "20260101T000000Z");
    fs::create_directories(plaintext);
    LocalCluster ambiguous_old =
        make_cluster(ambiguous.data_dir, ambiguous.socket_dir);
    LocalCluster ambiguous_target =
        make_cluster(ambiguous.target_data_dir, ambiguous.target_socket_dir);
    check_throws(
        [&] {
            promote_after_verification(ambiguous_old, ambiguous_target,
                                       ambiguous);
        },
        "manual review state is not promoted");
    check(fs::is_directory(ambiguous.data_dir) &&
              fs::is_directory(ambiguous.target_data_dir) &&
              fs::is_directory(plaintext),
          "refused promotion leaves the directories untouched");
}

}  // namespace

int main() {
    try {
        TempDir temporary;
        test_path_derivation(temporary.path());
        test_states(temporary.path());
        test_ambiguous_states(temporary.path());
        test_rejections(temporary.path());
        test_promotion(temporary.path());
        test_resume_after_old_rename(temporary.path());
        test_refused_promotions(temporary.path());
    } catch (const std::exception& error) {
        std::cerr << "unexpected exception: " << error.what() << '\n';
        return 1;
    }

    if (failures != 0) {
        std::cerr << "legacy_promotion_test: " << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "legacy_promotion_test: ok\n";
    return 0;
}
