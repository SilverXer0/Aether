#include "aether/reorder_buffer.h"

namespace aether {

ReorderBuffer::ReorderBuffer(size_t capacity)
    : capacity_(capacity)
    , slots_(capacity)
{}

void ReorderBuffer::deposit(ProcessedBlock block) {
    uint64_t seq = block.sequence_id;

    std::unique_lock<std::mutex> lock(mtx_);
    cv_producer_.wait(lock, [&]() {
        return seq < next_expected_.load(std::memory_order_relaxed) + capacity_;
    });

    slots_[seq % capacity_] = std::move(block);

    if (seq == next_expected_.load(std::memory_order_relaxed)) {
        cv_consumer_.notify_one();
    }
}

ProcessedBlock ReorderBuffer::take_next() {
    std::unique_lock<std::mutex> lock(mtx_);
    uint64_t expected = next_expected_.load(std::memory_order_relaxed);

    cv_consumer_.wait(lock, [&]() {
        return slots_[expected % capacity_].has_value() ||
               expected >= total_blocks_.load(std::memory_order_relaxed);
    });

    if (expected >= total_blocks_.load(std::memory_order_relaxed)) {
        return ProcessedBlock::make_sentinel();
    }

    ProcessedBlock block = std::move(*slots_[expected % capacity_]);
    slots_[expected % capacity_].reset();

    next_expected_.store(expected + 1, std::memory_order_relaxed);
    cv_producer_.notify_all();

    return block;
}

void ReorderBuffer::mark_complete(uint64_t total_blocks) {
    total_blocks_.store(total_blocks, std::memory_order_relaxed);
    cv_consumer_.notify_all();
}

} // namespace aether
