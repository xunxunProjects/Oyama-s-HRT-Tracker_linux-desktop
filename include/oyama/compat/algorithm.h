#ifndef OYAMA_COMPAT_ALGORITHM_H
#define OYAMA_COMPAT_ALGORITHM_H

#include <cstddef>
#include <utility>
#include <algorithm>

namespace oyama {
#if __cplusplus >= 201703L
using std::clamp;
using std::size;
using std::data;
using std::as_const;
#else
template <typename T>
const T& clamp(const T& v, const T& lo, const T& hi) {
    if (v < lo) return lo;
    if (hi < v) return hi;
    return v;
}
template <typename C>
auto size(const C& c) -> decltype(c.size()) { return c.size(); }
template <typename T, std::size_t N>
std::size_t size(const T (&)[N]) { return N; }
template <typename C>
auto data(C& c) -> decltype(c.data()) { return c.data(); }
template <typename C>
auto data(const C& c) -> decltype(c.data()) { return c.data(); }
template <typename T, std::size_t N>
T* data(T (&a)[N]) { return a; }
template <typename T>
const T& as_const(T& t) { return t; }
template <typename T>
const T* as_const(T* t) { return t; }
#endif
}

#endif
