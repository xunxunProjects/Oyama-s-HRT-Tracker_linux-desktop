#ifndef OYAMA_SECURITY_EXPORT_H
#define OYAMA_SECURITY_EXPORT_H

#if defined(_WIN32)
#  define OYAMA_API __declspec(dllexport)
#else
#  define OYAMA_API __attribute__((visibility("default")))
#endif

#define OYAMA_NODISCARD __attribute__((warn_unused_result))
#define OYAMA_WARN_UNUSED __attribute__((warn_unused_result))

#endif
