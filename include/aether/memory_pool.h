#pragma once

#include <vector>
#include <mutex>
#include <memory>
#include <cstdint>

namespace aether {

class MemoryPool {
public:
    explicit MemoryPool(size_t chunk_size) : chunk_size_(chunk_size) {}

    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;

    std::unique_ptr<uint8_t[]> get() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!pool_.empty()) {
            auto ptr = std::move(pool_.back());
            pool_.pop_back();
            return ptr;
        }
        return std::make_unique<uint8_t[]>(chunk_size_);
    }

    void put(std::unique_ptr<uint8_t[]> ptr) {
        if (!ptr) return;
        std::lock_guard<std::mutex> lock(mtx_);
        pool_.push_back(std::move(ptr));
    }

private:
    size_t chunk_size_;
    std::mutex mtx_;
    std::vector<std::unique_ptr<uint8_t[]>> pool_;
};

} // namespace aether
