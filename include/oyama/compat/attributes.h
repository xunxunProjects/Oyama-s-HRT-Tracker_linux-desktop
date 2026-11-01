#ifndef OYAMA_COMPAT_ATTRIBUTES_H
#define OYAMA_COMPAT_ATTRIBUTES_H

#if __cplusplus >= 201703L
#define OYAMA_NODISCARD [[nodiscard]]
#define OYAMA_MAYBE_UNUSED [[maybe_unused]]
#elif defined(__GNUC__) || defined(__clang__)
#define OYAMA_NODISCARD __attribute__((warn_unused_result))
#define OYAMA_MAYBE_UNUSED __attribute__((unused))
#else
#define OYAMA_NODISCARD
#define OYAMA_MAYBE_UNUSED
#endif

#endif
