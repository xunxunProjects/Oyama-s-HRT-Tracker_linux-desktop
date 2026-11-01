#ifndef OYAMA_SECURITY_PROFILE_H
#define OYAMA_SECURITY_PROFILE_H

#ifndef OYAMA_SECURITY_PROFILE_OFF
#define OYAMA_SECURITY_PROFILE_OFF 0
#endif
#ifndef OYAMA_SECURITY_PROFILE_STANDARD
#define OYAMA_SECURITY_PROFILE_STANDARD 0
#endif
#ifndef OYAMA_SECURITY_PROFILE_FULL
#define OYAMA_SECURITY_PROFILE_FULL 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

static inline const char *oyama_security_profile_name(void) {
#if OYAMA_SECURITY_PROFILE_FULL
    return "full";
#elif OYAMA_SECURITY_PROFILE_STANDARD
    return "standard";
#else
    return "off";
#endif
}

#ifdef __cplusplus
}
#endif

#endif
