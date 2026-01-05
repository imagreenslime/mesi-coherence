#pragma once
#include <cstdio>

extern bool QUIET;

// Intercept printf globally
#define printf(...)                         \
    do {                                   \
        if (!QUIET) {                      \
            std::printf(__VA_ARGS__);      \
        }                                  \
    } while (0)
