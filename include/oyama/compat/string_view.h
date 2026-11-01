#ifndef OYAMA_COMPAT_STRING_VIEW_H
#define OYAMA_COMPAT_STRING_VIEW_H

#include <cstddef>
#include <cstring>
#include <string>
#include <ostream>

#if __cplusplus >= 201703L
#include <string_view>
namespace oyama {
using std::string_view;
}
#else
namespace oyama {
class string_view {
    const char* data_;
    std::size_t size_;
public:
    static const std::size_t npos = static_cast<std::size_t>(-1);

    string_view() : data_(""), size_(0) {}
    string_view(const char* s)
        : data_(s ? s : ""), size_(s ? std::strlen(s) : 0) {}
    string_view(const char* s, std::size_t n) : data_(s ? s : ""), size_(n) {}
    string_view(const std::string& s) : data_(s.data()), size_(s.size()) {}

    const char* data() const { return data_; }
    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    char operator[](std::size_t i) const { return data_[i]; }
    char front() const { return data_[0]; }
    char back() const { return data_[size_ - 1]; }

    string_view substr(std::size_t pos, std::size_t n = npos) const {
        if (pos > size_) pos = size_;
        std::size_t avail = size_ - pos;
        std::size_t len = (n == npos || n > avail) ? avail : n;
        return string_view(data_ + pos, len);
    }

    int compare(string_view o) const {
        std::size_t n = size_ < o.size_ ? size_ : o.size_;
        int r = std::memcmp(data_, o.data_, n);
        if (r != 0) return r;
        if (size_ < o.size_) return -1;
        if (size_ > o.size_) return 1;
        return 0;
    }

    friend bool operator==(string_view a, string_view b) { return a.compare(b) == 0; }
    friend bool operator!=(string_view a, string_view b) { return a.compare(b) != 0; }
    friend bool operator<(string_view a, string_view b) { return a.compare(b) < 0; }
    friend bool operator<=(string_view a, string_view b) { return a.compare(b) <= 0; }
    friend bool operator>(string_view a, string_view b) { return a.compare(b) > 0; }
    friend bool operator>=(string_view a, string_view b) { return a.compare(b) >= 0; }

    friend std::ostream& operator<<(std::ostream& os, string_view sv) {
        return os.write(sv.data_, static_cast<std::streamsize>(sv.size_));
    }

    operator std::string() const { return std::string(data_, size_); }
};
}
#endif

#endif
