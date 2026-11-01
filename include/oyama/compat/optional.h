#ifndef OYAMA_COMPAT_OPTIONAL_H
#define OYAMA_COMPAT_OPTIONAL_H

#include <new>
#include <utility>
#include <stdexcept>
#include <type_traits>

#if __cplusplus >= 201703L
#include <optional>
namespace oyama {
using std::optional;
using std::nullopt;
using std::nullopt_t;
}
#else
namespace oyama {
struct nullopt_t {};
constexpr nullopt_t nullopt{};

template <typename T>
class optional {
    bool has_;
    typename std::aligned_storage<sizeof(T), alignof(T)>::type storage_;

    T* ptr() { return reinterpret_cast<T*>(&storage_); }
    const T* ptr() const { return reinterpret_cast<const T*>(&storage_); }

public:
    optional() : has_(false) {}
    optional(nullopt_t) : has_(false) {}

    template <typename U = T,
              typename = typename std::enable_if<std::is_copy_constructible<U>::value>::type>
    optional(const optional& o) : has_(o.has_) {
        if (has_) new (ptr()) T(*o.ptr());
    }

    optional(optional&& o) : has_(o.has_) {
        if (has_) new (ptr()) T(std::move(*o.ptr()));
    }

    template <typename U = T,
              typename = typename std::enable_if<std::is_copy_constructible<U>::value>::type>
    optional(const T& v) : has_(true) { new (ptr()) T(v); }

    optional(T&& v) : has_(true) { new (ptr()) T(std::move(v)); }

    ~optional() { if (has_) ptr()->~T(); }

    template <typename U = T,
              typename = typename std::enable_if<std::is_copy_constructible<U>::value>::type>
    optional& operator=(const optional& o) {
        if (o.has_) {
            if (has_) *ptr() = *o.ptr();
            else { new (ptr()) T(*o.ptr()); has_ = true; }
        } else reset();
        return *this;
    }

    optional& operator=(optional&& o) {
        if (o.has_) {
            if (has_) *ptr() = std::move(*o.ptr());
            else { new (ptr()) T(std::move(*o.ptr())); has_ = true; }
        } else reset();
        return *this;
    }

    optional& operator=(nullopt_t) { reset(); return *this; }

    template <typename U = T,
              typename = typename std::enable_if<std::is_copy_assignable<U>::value ||
                                                std::is_copy_constructible<U>::value>::type>
    optional& operator=(const T& v) {
        if (has_) *ptr() = v;
        else { new (ptr()) T(v); has_ = true; }
        return *this;
    }

    optional& operator=(T&& v) {
        if (has_) *ptr() = std::move(v);
        else { new (ptr()) T(std::move(v)); has_ = true; }
        return *this;
    }

    bool has_value() const { return has_; }
    explicit operator bool() const { return has_; }

    T& value() {
        if (!has_) throw std::runtime_error("bad optional access");
        return *ptr();
    }
    const T& value() const {
        if (!has_) throw std::runtime_error("bad optional access");
        return *ptr();
    }
    T& operator*() { return *ptr(); }
    const T& operator*() const { return *ptr(); }
    T* operator->() { return ptr(); }
    const T* operator->() const { return ptr(); }

    template <typename U>
    T value_or(U&& u) const {
        if (has_) return *ptr();
        return static_cast<T>(std::forward<U>(u));
    }

    void reset() {
        if (has_) { ptr()->~T(); has_ = false; }
    }

    template <typename... Args>
    void emplace(Args&&... args) {
        reset();
        new (ptr()) T(std::forward<Args>(args)...);
        has_ = true;
    }

    bool operator==(const optional& o) const {
        if (has_ != o.has_) return false;
        if (!has_) return true;
        return *ptr() == *o.ptr();
    }
    bool operator!=(const optional& o) const { return !(*this == o); }
    bool operator==(nullopt_t) const { return !has_; }
    bool operator!=(nullopt_t) const { return has_; }
};

template <typename T>
bool operator==(nullopt_t, const optional<T>& o) { return !o.has_value(); }
template <typename T>
bool operator!=(nullopt_t, const optional<T>& o) { return o.has_value(); }
}
#endif

#endif
