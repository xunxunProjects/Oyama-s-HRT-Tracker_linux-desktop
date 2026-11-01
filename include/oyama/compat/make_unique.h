#ifndef OYAMA_COMPAT_MAKE_UNIQUE_H
#define OYAMA_COMPAT_MAKE_UNIQUE_H

#include <memory>
#include <utility>

namespace oyama {
#if __cplusplus >= 201402L
using std::make_unique;
#else
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#endif
}

#endif
