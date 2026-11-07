/*
 * Oyama's HRT Tracker - C99 pharmacokinetic core (public umbrella header).
 *
 * This header is split into `oyama/pk/types.h` and `oyama/pk/api.h` for
 * development convenience; including this file remains the supported entry
 * point for all consumers.
 */
#ifndef OYAMA_PK_H
#define OYAMA_PK_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "oyama/pk/types.h"
#include "oyama/pk/api.h"

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* OYAMA_PK_H */
