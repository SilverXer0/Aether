#include "aether/spmc_queue.h"

#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>
#include <numeric>
#include <thread>
#include <vector>

// ── Single-threaded correctness ───────────────────────────────────────────────

TEST(SPMCQueue, PushPopSingleThread) {
    aether::SPMCQueue<int> q(8);

    for (int i = 0; i < 8; ++i) {
        EXPECT_TRUE(q.try_push(i));
    }

    // Queue is full
    EXPECT_FALSE(q.try_push(99));

    for (int i = 0; i < 8; ++i) {
        int val = -1;
        EXPECT_TRUE(q.try_pop(val));
        EXPECT_EQ(val, i);
    }

    // Queue is empty
    int val = -1;
    EXPECT_FALSE(q.try_pop(val));
}

TEST(SPMCQueue, EmptyOnConstruction) {
    aether::SPMCQueue<int> q(4);
    EXPECT_TRUE(q.empty());
    EXPECT_EQ(q.size_approx(), 0u);

    int val;
    EXPECT_FALSE(q.try_pop(val));
}

TEST(SPMCQueue, BlockingPush) {
    aether::SPMCQueue<int> q(4);

    // Fill the queue
    for (int i = 0; i < 4; ++i)
        q.push(i);

    // Start a consumer that drains after a delay
    std::thread consumer([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        int val;
        EXPECT_TRUE(q.try_pop(val));
        EXPECT_EQ(val, 0);
    });

    // This should block until the consumer frees a slot
    q.push(42);
    consumer.join();

    // Drain remaining
    int val;
    for (int expected = 1; expected <= 3; ++expected) {
        EXPECT_TRUE(q.try_pop(val));
        EXPECT_EQ(val, expected);
    }
    EXPECT_TRUE(q.try_pop(val));
    EXPECT_EQ(val, 42);
}

TEST(SPMCQueue, WrapAround) {
    aether::SPMCQueue<int> q(4);

    // Push and pop more items than capacity to exercise turn counter wrapping
    for (int round = 0; round < 100; ++round) {
        for (int i = 0; i < 4; ++i) {
            EXPECT_TRUE(q.try_push(round * 4 + i));
        }
        for (int i = 0; i < 4; ++i) {
            int val = -1;
            EXPECT_TRUE(q.try_pop(val));
            EXPECT_EQ(val, round * 4 + i);
        }
    }
}

TEST(SPMCQueue, CapacityOne) {
    aether::SPMCQueue<int> q(1);

    for (int i = 0; i < 50; ++i) {
        EXPECT_TRUE(q.try_push(i));
        EXPECT_FALSE(q.try_push(i + 100));

        int val = -1;
        EXPECT_TRUE(q.try_pop(val));
        EXPECT_EQ(val, i);
        EXPECT_FALSE(q.try_pop(val));
    }
}

TEST(SPMCQueue, SizeApprox) {
    aether::SPMCQueue<int> q(8);

    for (int i = 0; i < 5; ++i)
        q.push(i);

    EXPECT_EQ(q.size_approx(), 5u);
    EXPECT_FALSE(q.empty());

    int val;
    q.try_pop(val);
    q.try_pop(val);

    EXPECT_EQ(q.size_approx(), 3u);
}

// ── Multi-threaded: exactly-once delivery ─────────────────────────────────────

TEST(SPMCQueue, MultiConsumerExactlyOnce) {
    constexpr size_t NUM_ITEMS    = 200'000;
    constexpr int    NUM_CONSUMERS = 4;

    aether::SPMCQueue<int> q(1024);

    std::atomic<size_t> items_consumed{0};
    std::atomic<int64_t> global_sum{0};

    // Producer: push 1..NUM_ITEMS
    std::thread producer([&] {
        for (size_t i = 1; i <= NUM_ITEMS; ++i) {
            q.push(static_cast<int>(i));
        }
    });

    // Consumers: pop and accumulate
    std::vector<std::thread> consumers;
    consumers.reserve(NUM_CONSUMERS);

    for (int c = 0; c < NUM_CONSUMERS; ++c) {
        consumers.emplace_back([&] {
            int64_t local_sum = 0;
            size_t  local_count = 0;

            while (items_consumed.load(std::memory_order_relaxed) < NUM_ITEMS) {
                int val;
                if (q.try_pop(val)) {
                    local_sum += val;
                    ++local_count;
                    items_consumed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }

            global_sum.fetch_add(local_sum, std::memory_order_relaxed);
        });
    }

    producer.join();
    for (auto& c : consumers) c.join();

    // Verify: sum(1..N) = N*(N+1)/2
    const int64_t expected_sum =
        static_cast<int64_t>(NUM_ITEMS) * static_cast<int64_t>(NUM_ITEMS + 1) / 2;

    EXPECT_EQ(items_consumed.load(), NUM_ITEMS);
    EXPECT_EQ(global_sum.load(), expected_sum);
}

// ── Multi-threaded: high contention, small queue ──────────────────────────────

TEST(SPMCQueue, HighContention) {
    constexpr size_t NUM_ITEMS    = 50'000;
    constexpr int    NUM_CONSUMERS = 8;

    // Tiny queue to maximize backpressure and turn wrapping
    aether::SPMCQueue<int> q(16);

    std::atomic<size_t> items_consumed{0};
    std::atomic<int64_t> global_sum{0};

    std::thread producer([&] {
        for (size_t i = 1; i <= NUM_ITEMS; ++i) {
            q.push(static_cast<int>(i));
        }
    });

    std::vector<std::thread> consumers;
    consumers.reserve(NUM_CONSUMERS);

    for (int c = 0; c < NUM_CONSUMERS; ++c) {
        consumers.emplace_back([&] {
            int64_t local_sum = 0;

            while (items_consumed.load(std::memory_order_relaxed) < NUM_ITEMS) {
                int val;
                if (q.try_pop(val)) {
                    local_sum += val;
                    items_consumed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }

            global_sum.fetch_add(local_sum, std::memory_order_relaxed);
        });
    }

    producer.join();
    for (auto& c : consumers) c.join();

    const int64_t expected_sum =
        static_cast<int64_t>(NUM_ITEMS) * static_cast<int64_t>(NUM_ITEMS + 1) / 2;

    EXPECT_EQ(items_consumed.load(), NUM_ITEMS);
    EXPECT_EQ(global_sum.load(), expected_sum);
}

// ── Struct type (closer to real usage with ChunkDescriptor) ───────────────────

struct TestChunk {
    uint64_t id;
    uint64_t offset;
    size_t   size;
};

TEST(SPMCQueue, StructType) {
    aether::SPMCQueue<TestChunk> q(32);

    for (uint64_t i = 0; i < 32; ++i) {
        EXPECT_TRUE(q.try_push(TestChunk{i, i * 4096, 4096}));
    }
    EXPECT_FALSE(q.try_push(TestChunk{99, 0, 0}));

    for (uint64_t i = 0; i < 32; ++i) {
        TestChunk out{};
        EXPECT_TRUE(q.try_pop(out));
        EXPECT_EQ(out.id, i);
        EXPECT_EQ(out.offset, i * 4096);
        EXPECT_EQ(out.size, 4096u);
    }
}

// ── SPMC with struct, multi-threaded ──────────────────────────────────────────

TEST(SPMCQueue, StructMultiConsumer) {
    constexpr size_t NUM_ITEMS    = 100'000;
    constexpr int    NUM_CONSUMERS = 4;

    aether::SPMCQueue<TestChunk> q(256);

    std::atomic<size_t> items_consumed{0};
    std::atomic<int64_t> id_sum{0};

    std::thread producer([&] {
        for (size_t i = 0; i < NUM_ITEMS; ++i) {
            q.push(TestChunk{i, i * 4096, 4096});
        }
    });

    std::vector<std::thread> consumers;
    consumers.reserve(NUM_CONSUMERS);

    for (int c = 0; c < NUM_CONSUMERS; ++c) {
        consumers.emplace_back([&] {
            int64_t local_sum = 0;

            while (items_consumed.load(std::memory_order_relaxed) < NUM_ITEMS) {
                TestChunk out{};
                if (q.try_pop(out)) {
                    local_sum += static_cast<int64_t>(out.id);
                    items_consumed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }

            id_sum.fetch_add(local_sum, std::memory_order_relaxed);
        });
    }

    producer.join();
    for (auto& c : consumers) c.join();

    // sum(0..N-1) = N*(N-1)/2
    const int64_t expected =
        static_cast<int64_t>(NUM_ITEMS) * static_cast<int64_t>(NUM_ITEMS - 1) / 2;

    EXPECT_EQ(items_consumed.load(), NUM_ITEMS);
    EXPECT_EQ(id_sum.load(), expected);
}
