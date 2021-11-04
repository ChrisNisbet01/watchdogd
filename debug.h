#pragma once

#ifdef DEBUG

#include <stdio.h>

#define debug(fmt, ...) \
do { \
    fprintf(stderr, "%s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, fmt, ## __VA_ARGS__); \
} while (0)

#else

#define debug(fmt, ...) do { } while (0)

#endif

