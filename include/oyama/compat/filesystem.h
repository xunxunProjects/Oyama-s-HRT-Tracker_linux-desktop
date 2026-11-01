#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef OYAMA_COMPAT_FILESYSTEM_H
#define OYAMA_COMPAT_FILESYSTEM_H

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <system_error>
#include <ostream>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if __cplusplus >= 201703L
#include <filesystem>
namespace oyama { namespace fs = std::filesystem; }
#else
namespace oyama { namespace fs {

class path {
    std::string s_;
public:
    path() {}
    path(const std::string& s) : s_(s) {}
    path(const char* s) : s_(s ? s : "") {}
    path(const path& o) : s_(o.s_) {}
    path(path&& o) : s_(std::move(o.s_)) {}
    path& operator=(const path& o) { s_ = o.s_; return *this; }
    path& operator=(path&& o) { s_ = std::move(o.s_); return *this; }

    const std::string& string() const { return s_; }
    const char* c_str() const { return s_.c_str(); }
    bool empty() const { return s_.empty(); }

    path& operator/=(const path& o) {
        if (s_.empty()) s_ = o.s_;
        else if (s_.back() != '/') s_ += "/" + o.s_;
        else s_ += o.s_;
        return *this;
    }
    friend path operator/(const path& a, const path& b) {
        path r = a; r /= b; return r;
    }

    path filename() const {
        std::size_t pos = s_.find_last_of("/\\");
        if (pos == std::string::npos) return path(s_);
        return path(s_.substr(pos + 1));
    }
    path parent_path() const {
        std::size_t pos = s_.find_last_of("/\\");
        if (pos == std::string::npos) return path();
        if (pos == 0) return path("/");
        return path(s_.substr(0, pos));
    }
    path extension() const {
        std::string f = filename().string();
        std::size_t pos = f.find_last_of('.');
        if (pos == std::string::npos || pos == 0) return path();
        return path(f.substr(pos));
    }

    bool operator==(const path& o) const { return s_ == o.s_; }
    bool operator!=(const path& o) const { return s_ != o.s_; }
    friend std::ostream& operator<<(std::ostream& os, const path& p) { return os << p.s_; }
};

inline bool exists(const path& p, std::error_code& ec) {
    struct stat st;
    if (::stat(p.c_str(), &st) == 0) { ec.clear(); return true; }
    ec = std::error_code(errno, std::generic_category());
    return false;
}
inline bool exists(const path& p) {
    std::error_code ec;
    return exists(p, ec);
}
inline bool is_directory(const path& p, std::error_code& ec) {
    struct stat st;
    if (::stat(p.c_str(), &st) == 0) {
        ec.clear();
        return S_ISDIR(st.st_mode);
    }
    ec = std::error_code(errno, std::generic_category());
    return false;
}
inline bool is_directory(const path& p) {
    std::error_code ec;
    return is_directory(p, ec);
}
inline bool create_directories(const path& p, std::error_code& ec) {
    if (p.string().empty()) { ec.clear(); return true; }
    std::string s = p.string();
    std::string cur;
    std::size_t start = 0;
    if (s[0] == '/') { cur = "/"; start = 1; }
    std::size_t i = start;
    while (true) {
        std::size_t slash = s.find('/', i);
        std::string part = (slash == std::string::npos) ? s.substr(i) : s.substr(i, slash - i);
        if (!part.empty()) {
            if (!cur.empty() && cur.back() != '/') cur += "/";
            cur += part;
            struct stat st;
            if (::stat(cur.c_str(), &st) != 0) {
                if (::mkdir(cur.c_str(), 0777) != 0 && errno != EEXIST) {
                    ec = std::error_code(errno, std::generic_category());
                    return false;
                }
            }
        }
        if (slash == std::string::npos) break;
        i = slash + 1;
    }
    ec.clear();
    return true;
}
inline bool create_directories(const path& p) {
    std::error_code ec;
    return create_directories(p, ec);
}
inline bool remove(const path& p, std::error_code& ec) {
    if (::unlink(p.c_str()) == 0 || ::rmdir(p.c_str()) == 0) { ec.clear(); return true; }
    ec = std::error_code(errno, std::generic_category());
    return false;
}
inline bool remove(const path& p) {
    std::error_code ec;
    return remove(p, ec);
}
inline std::uintmax_t remove_all(const path& p, std::error_code& ec) {
    struct stat st;
    if (::lstat(p.c_str(), &st) != 0) { ec.clear(); return 0; }
    std::uintmax_t count = 0;
    if (S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)) {
        DIR* d = ::opendir(p.c_str());
        if (d) {
            struct dirent* entry;
            while ((entry = ::readdir(d)) != nullptr) {
                if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) continue;
                path child = p / entry->d_name;
                count += remove_all(child, ec);
                if (ec) { ::closedir(d); return count; }
            }
            ::closedir(d);
        }
        if (::rmdir(p.c_str()) == 0) count++;
        else ec = std::error_code(errno, std::generic_category());
    } else {
        if (::unlink(p.c_str()) == 0) count++;
        else ec = std::error_code(errno, std::generic_category());
    }
    if (!ec) ec.clear();
    return count;
}
inline std::uintmax_t remove_all(const path& p) {
    std::error_code ec;
    return remove_all(p, ec);
}
inline void rename(const path& from, const path& to, std::error_code& ec) {
    if (::rename(from.c_str(), to.c_str()) == 0) ec.clear();
    else ec = std::error_code(errno, std::generic_category());
}
inline void rename(const path& from, const path& to) {
    std::error_code ec;
    rename(from, to, ec);
}

class directory_iterator {
    DIR* dir_;
    std::string base_;
    struct ::dirent* current_;
    bool end_;
public:
    directory_iterator() : dir_(nullptr), current_(nullptr), end_(true) {}
    directory_iterator(const path& p)
        : dir_(::opendir(p.c_str())), base_(p.string()), current_(nullptr), end_(false) {
        if (!dir_) { end_ = true; return; }
        advance();
    }
    ~directory_iterator() { if (dir_) ::closedir(dir_); }
    directory_iterator(const directory_iterator&) = delete;
    directory_iterator(directory_iterator&& o)
        : dir_(o.dir_), base_(std::move(o.base_)), current_(o.current_), end_(o.end_) {
        o.dir_ = nullptr; o.end_ = true;
    }
    directory_iterator& operator=(directory_iterator&& o) {
        if (this != &o) {
            if (dir_) ::closedir(dir_);
            dir_ = o.dir_; base_ = std::move(o.base_); current_ = o.current_; end_ = o.end_;
            o.dir_ = nullptr; o.end_ = true;
        }
        return *this;
    }
    path operator*() const {
        if (!current_) return path();
        if (base_.empty() || base_.back() == '/') return path(base_ + current_->d_name);
        return path(base_ + "/" + current_->d_name);
    }
    directory_iterator& operator++() { advance(); return *this; }
    directory_iterator begin() { return std::move(*this); }
    directory_iterator end() { return directory_iterator(); }
    bool operator!=(const directory_iterator& o) const {
        // end sentinel comparison: an end iterator has dir_ == nullptr
        if (end_ && o.end_) return false;
        if (end_ != o.end_) return true;
        return current_ != o.current_;
    }
private:
    void advance() {
        if (!dir_) { end_ = true; return; }
        current_ = ::readdir(dir_);
        while (current_ && (std::strcmp(current_->d_name, ".") == 0 ||
                            std::strcmp(current_->d_name, "..") == 0)) {
            current_ = ::readdir(dir_);
        }
        if (!current_) end_ = true;
    }
};

inline path temp_directory_path() {
    const char* t = std::getenv("TMPDIR");
    if (t && t[0]) return path(t);
    return path("/tmp");
}
inline path current_path(std::error_code& ec) {
    char buf[4096];
    if (::getcwd(buf, sizeof(buf))) { ec.clear(); return path(buf); }
    ec = std::error_code(errno, std::generic_category());
    return path();
}
inline path current_path() {
    std::error_code ec;
    return current_path(ec);
}

} } // namespace oyama::fs
#endif

#endif
