#pragma once

#include "aether/spmc_queue.h"
#include "aether/reorder_buffer.h"
#include "aether/memory_pool.h"

#include <vector>
#include <thread>

namespace aether {

class ThreadPool {
public:
    ThreadPool(size_t num_workers,
               int compression_level,
               SPMCQueue<ChunkDescriptor>& in_queue,
               ReorderBuffer& rob,
               MemoryPool& mem_pool);

    ~ThreadPool() = default;

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    std::vector<std::jthread> workers_;
};

} // namespace aether
