#include "aether/spmc_queue.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// Mutex+Queue baseline 
template <typename T>
class MutexQueue {
public:
    explicit MutexQueue(size_t capacity) : capacity_(capacity) {}

    void push(T item) {
        for (;;) {
            std::lock_guard<std::mutex> lock(mu_);
            if (queue_.size() < capacity_) {
                queue_.push(std::move(item));
                return;
            }
        }
    }

    bool try_pop(T& out) {
        std::lock_guard<std::mutex> lock(mu_);
        if (queue_.empty()) return false;
        out = std::move(queue_.front());
        queue_.pop();
        return true;
    }

private:
    std::mutex      mu_;
    std::queue<T>   queue_;
    size_t          capacity_;
};

// Benchmark harness
struct BenchResult {
    double elapsed_ms;
    double ops_per_sec;
};

template <typename Queue>
BenchResult run_bench(size_t num_items, int num_consumers, size_t queue_capacity) {
    Queue q(queue_capacity);
    std::atomic<size_t> consumed{0};

    auto t0 = std::chrono::steady_clock::now();

    std::thread producer([&] {
        for (size_t i = 0; i < num_items; ++i) {
            q.push(static_cast<int>(i));
        }
    });

    std::vector<std::thread> consumers;
    consumers.reserve(num_consumers);
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&] {
            while (consumed.load(std::memory_order_relaxed) < num_items) {
                int val;
                if (q.try_pop(val)) {
                    consumed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    producer.join();
    for (auto& c : consumers) c.join();

    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double ops = static_cast<double>(num_items) / (ms / 1000.0);

    return {ms, ops};
}

int main() {
    constexpr size_t NUM_ITEMS     = 2'000'000;
    constexpr size_t QUEUE_CAPACITY = 1024;

    std::printf("╔══════════════════════════════════════════════════════════════╗\n");
    std::printf("║          SPMC Queue Benchmark: %zu items, queue=%zu       ║\n",
                NUM_ITEMS, QUEUE_CAPACITY);
    std::printf("╠══════════════════════════════════════════════════════════════╣\n");
    std::printf("║ Consumers │    Lock-Free (ms) │    Mutex (ms) │   Speedup  ║\n");
    std::printf("╠══════════════════════════════════════════════════════════════╣\n");

    for (int consumers : {1, 2, 4, 8}) {
        // Warmup
        run_bench<aether::SPMCQueue<int>>(10'000, consumers, QUEUE_CAPACITY);
        run_bench<MutexQueue<int>>(10'000, consumers, QUEUE_CAPACITY);

        // Actual measurement (best of 3)
        BenchResult best_lf{1e9, 0}, best_mx{1e9, 0};
        for (int trial = 0; trial < 3; ++trial) {
            auto lf = run_bench<aether::SPMCQueue<int>>(NUM_ITEMS, consumers, QUEUE_CAPACITY);
            auto mx = run_bench<MutexQueue<int>>(NUM_ITEMS, consumers, QUEUE_CAPACITY);
            if (lf.elapsed_ms < best_lf.elapsed_ms) best_lf = lf;
            if (mx.elapsed_ms < best_mx.elapsed_ms) best_mx = mx;
        }

        double speedup = best_mx.elapsed_ms / best_lf.elapsed_ms;
        std::printf("║    %d      │     %8.2f      │   %8.2f    │   %5.2fx   ║\n",
                    consumers, best_lf.elapsed_ms, best_mx.elapsed_ms, speedup);
    }

    std::printf("╚══════════════════════════════════════════════════════════════╝\n");
    return 0;
}
