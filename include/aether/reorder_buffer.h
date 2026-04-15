#pragma once

#include "aether/types.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <vector>

namespace aether {

class ReorderBuffer {
public:
    explicit ReorderBuffer(size_t capacity);

    ReorderBuffer(const ReorderBuffer&) = delete;
    ReorderBuffer& operator=(const ReorderBuffer&) = delete;

    void deposit(ProcessedBlock block);
    ProcessedBlock take_next();
    void mark_complete(uint64_t total_blocks);

private:
    size_t capacity_;
    std::vector<std::optional<ProcessedBlock>> slots_;

    alignas(64) std::atomic<uint64_t> next_expected_{0};

    std::mutex mtx_;
    std::condition_variable cv_producer_;
    std::condition_variable cv_consumer_;
    
    std::atomic<uint64_t> total_blocks_{UINT64_MAX};
};

} // namespace aether
