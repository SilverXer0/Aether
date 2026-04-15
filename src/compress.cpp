#include "aether/compressor.h"
#include "aether/mmap_reader.h"
#include "aether/io_backend.h"
#include "aether/footer.h"
#include "aether/types.h"
#include "aether/config.h"
#include "aether/spmc_queue.h"
#include "aether/reorder_buffer.h"
#include "aether/thread_pool.h"
#include "aether/serializer.h"
#include "aether/memory_pool.h"
#include <zstd.h>

#include <iostream>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>

namespace aether {

int compress_file(const std::string& input_path,
                  const std::string& output_path,
                  int compression_level,
                  size_t chunk_size,
                  size_t num_threads,
                  bool use_uring,
                  bool verbose)
{
    auto t_start = std::chrono::steady_clock::now();

    if (verbose) {
        std::cout << "[compress] Opening input: " << input_path << "\n";
    }

    MmapReader reader(input_path, chunk_size);

    if (reader.file_size() == 0) {
        std::cerr << "[compress] Warning: input file is empty\n";
    }

    auto chunks = reader.generate_chunks();

    size_t num_workers = (num_threads == 0) ? config::default_worker_threads() : num_threads;
    size_t rob_capacity = config::default_rob_capacity(static_cast<unsigned>(num_workers));

    if (verbose) {
        std::cout << "[compress] File size: " << reader.file_size() << " bytes\n";
        std::cout << "[compress] Chunks: " << chunks.size()
                  << " x " << chunk_size << " bytes\n";
        std::cout << "[compress] Compression level: " << compression_level << "\n";
        std::cout << "[compress] Workers: " << num_workers << "\n";
    }

    auto io = create_io_backend(output_path, use_uring);

    // SPMC Queue sizing: must be power of 2. We make it large enough to hold all chunks if possible,
    // or cap it. Actually, power of two >= chunks.size().
    size_t queue_cap = 1;
    while (queue_cap < chunks.size() && queue_cap < 8192) {
        queue_cap *= 2;
    }
    // ensure at least a minimum capacity
    if (queue_cap < 16) queue_cap = 16;
    
    SPMCQueue<ChunkDescriptor> queue(queue_cap);
    ReorderBuffer rob(rob_capacity);
    std::vector<SeekEntry> seek_table;
    seek_table.reserve(chunks.size());

    size_t bound = ZSTD_compressBound(chunk_size);
    MemoryPool mem_pool(bound);

    // Start Worker Pool
    ThreadPool pool(num_workers, compression_level, queue, rob, mem_pool);

    uint64_t write_offset = 0;

    // Start Serializer thread
    std::jthread serializer_thread([&rob, &io, &seek_table, &write_offset, total=chunks.size(), verbose, &mem_pool]() {
        Serializer::run(rob, *io, seek_table, write_offset, total, verbose, &mem_pool);
    });

    // Reader acts as the producer: push chunks into the queue
    for (const auto& chunk : chunks) {
        queue.push(chunk);
    }

    // Producer is done, mark ROB complete
    rob.mark_complete(chunks.size());

    // Wait for the serializer thread to finish writing everything
    serializer_thread.join();
    
    // Workers will be cleanly stopped when ThreadPool goes out of scope and their jthreads are destructed,
    // which triggers stop_requested() -> workers exit loop, but we must make sure the queue isn't empty first.
    // Actually, workers might be blocking on try_pop (yield loop) but they take stop_token so they will exit.
    // Wait, ThreadPool is destroyed at the end of the function.

    if (verbose) {
        std::cout << "\n";
    }

    footer::write(*io, write_offset, seek_table,
                  reader.file_size(), static_cast<uint32_t>(chunk_size),
                  static_cast<uint32_t>(compression_level));

    auto t_end = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(t_end - t_start).count();

    if (verbose) {
        double original_mb = static_cast<double>(reader.file_size()) / (1024.0 * 1024.0);
        double compressed_mb = static_cast<double>(write_offset) / (1024.0 * 1024.0);
        double ratio = (reader.file_size() > 0)
            ? static_cast<double>(write_offset) / static_cast<double>(reader.file_size())
            : 0.0;
        double throughput = original_mb / elapsed;

        std::cout << "[compress] Done!\n";
        std::cout << "[compress] Original:   " << original_mb << " MB\n";
        std::cout << "[compress] Compressed: " << compressed_mb << " MB\n";
        std::cout << "[compress] Ratio:      " << ratio << "\n";
        std::cout << "[compress] Time:       " << elapsed << " s\n";
        std::cout << "[compress] Throughput: " << throughput << " MB/s\n";
    }

    // Workers stop automatically when pool is destroyed
    return 0;
}

} // namespace aether
