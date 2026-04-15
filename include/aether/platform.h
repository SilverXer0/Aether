#pragma once

#if defined(__linux__)
    #define AETHER_PLATFORM_LINUX 1
    #define AETHER_PLATFORM_MACOS 0
#elif defined(__APPLE__)
    #define AETHER_PLATFORM_LINUX 0
    #define AETHER_PLATFORM_MACOS 1
#else
    #error "AetherCompress: Unsupported platform. Only Linux and macOS are supported."
#endif

#if defined(AETHER_USE_URING) && AETHER_USE_URING && AETHER_PLATFORM_LINUX
    #define AETHER_HAS_URING 1
#else
    #define AETHER_HAS_URING 0
#endif

#define AETHER_HAS_MMAP 1

#if AETHER_PLATFORM_LINUX
    #define AETHER_HAS_POSIX_FADVISE 1
#else
    #define AETHER_HAS_POSIX_FADVISE 0
#endif
