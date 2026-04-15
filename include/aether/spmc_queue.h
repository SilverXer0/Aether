#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <memory>
#include <thread>
#include <type_traits>

namespace aether {

template <typename T>
class SPMCQueue {
    static_assert(std::is_nothrow_move_constructible_v<T>);
    static_assert(std::is_default_constructible_v<T>);

public:
    explicit SPMCQueue(size_t capacity)
        : capacity_(capacity)
        , mask_(capacity - 1)
        , slots_(std::make_unique<Slot[]>(capacity))
    {
        assert(capacity > 0 && (capacity & (capacity - 1)) == 0);
    }

    SPMCQueue(const SPMCQueue&) = delete;
    SPMCQueue& operator=(const SPMCQueue&) = delete;

    bool try_push(const T& item) { return try_push_impl(item); }
    bool try_push(T&& item)      { return try_push_impl(std::move(item)); }

    void push(const T& item) {
        while (!try_push_impl(item))
            std::this_thread::yield();
    }

    void push(T&& item) {
        while (!try_push_impl(std::move(item)))
            std::this_thread::yield();
    }

    bool try_pop(T& out) {
        size_t pos = head_.load(std::memory_order_relaxed);

        for (;;) {
            Slot& slot = slots_[pos & mask_];
            const size_t expected_turn = 2 * (pos / capacity_) + 1;
            const size_t current_turn = slot.turn.load(std::memory_order_acquire);

            if (current_turn == expected_turn) {
                if (head_.compare_exchange_weak(
                        pos, pos + 1,
                        std::memory_order_acq_rel,
                        std::memory_order_relaxed)) {
                    out = std::move(slot.data);
                    slot.turn.store(expected_turn + 1, std::memory_order_release);
                    return true;
                }
            } else if (current_turn < expected_turn) {
                return false;
            } else {
                pos = head_.load(std::memory_order_relaxed);
            }
        }
    }

    size_t size_approx() const {
        const size_t t = tail_.load(std::memory_order_relaxed);
        const size_t h = head_.load(std::memory_order_relaxed);
        return (t >= h) ? (t - h) : 0;
    }

    bool empty() const {
        return head_.load(std::memory_order_relaxed)
            >= tail_.load(std::memory_order_relaxed);
    }

    size_t capacity() const { return capacity_; }

private:
    template <typename U>
    bool try_push_impl(U&& item) {
        const size_t pos = tail_.load(std::memory_order_relaxed);
        Slot& slot = slots_[pos & mask_];
        const size_t expected_turn = 2 * (pos / capacity_);

        if (slot.turn.load(std::memory_order_acquire) != expected_turn)
            return false;

        slot.data = std::forward<U>(item);
        slot.turn.store(expected_turn + 1, std::memory_order_release);
        tail_.store(pos + 1, std::memory_order_relaxed);
        return true;
    }

    struct alignas(64) Slot {
        std::atomic<size_t> turn{0};
        T data{};
    };

    const size_t capacity_;
    const size_t mask_;
    std::unique_ptr<Slot[]> slots_;

    alignas(64) std::atomic<size_t> tail_{0};
    alignas(64) std::atomic<size_t> head_{0};
};

} // namespace aether
