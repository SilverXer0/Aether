#pragma once

#include <cstddef>
#include <cstdint>
#include <thread>

namespace aether {
namespace config {

constexpr size_t DEFAULT_CHUNK_SIZE = 4ULL * 1024 * 1024;  // 4 MB
constexpr size_t MIN_CHUNK_SIZE     = 64ULL * 1024;         // 64 KB
constexpr size_t MAX_CHUNK_SIZE     = 32ULL * 1024 * 1024;  // 32 MB

constexpr int DEFAULT_COMPRESSION_LEVEL = 3;
constexpr int MIN_COMPRESSION_LEVEL     = 1;
constexpr int MAX_COMPRESSION_LEVEL     = 22;

inline unsigned default_worker_threads() {
    unsigned hw = std::thread::hardware_concurrency();
    return (hw > 2) ? (hw - 2) : 1;
}

inline size_t default_rob_capacity(unsigned num_workers) {
    return static_cast<size_t>(num_workers) * 2;
}

constexpr int URING_QUEUE_DEPTH = 64;
constexpr size_t DIRECT_IO_ALIGNMENT = 4096;

} // namespace config
} // namespace aether
