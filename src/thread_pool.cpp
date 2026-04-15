#include "aether/thread_pool.h"

#include <zstd.h>
#include <memory>
#include <stdexcept>
#include <string>

namespace aether {

ThreadPool::ThreadPool(size_t num_workers,
                       int compression_level,
                       SPMCQueue<ChunkDescriptor>& in_queue,
                       ReorderBuffer& rob,
                       MemoryPool& mem_pool)
{
    workers_.reserve(num_workers);

    for (size_t i = 0; i < num_workers; ++i) {
        workers_.emplace_back([&in_queue, &rob, &mem_pool, compression_level](std::stop_token stoken) {
            ZSTD_CCtx* cctx = ZSTD_createCCtx();
            if (!cctx) {
                throw std::runtime_error("ThreadPool: failed to create ZSTD context");
            }
            ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, compression_level);

            while (!stoken.stop_requested()) {
                ChunkDescriptor chunk;
                if (in_queue.try_pop(chunk)) {
                    size_t bound = ZSTD_compressBound(chunk.size);
                    auto out_buf = mem_pool.get();

                    size_t compressed_size = ZSTD_compressCCtx(
                        cctx,
                        out_buf.get(), bound,
                        chunk.data, chunk.size,
                        compression_level
                    );

                    if (ZSTD_isError(compressed_size)) {
                        ZSTD_freeCCtx(cctx);
                        throw std::runtime_error(
                            "ThreadPool: ZSTD compression failed: " +
                            std::string(ZSTD_getErrorName(compressed_size))
                        );
                    }

                    rob.deposit(ProcessedBlock{
                        .sequence_id = chunk.sequence_id,
                        .data = std::move(out_buf),
                        .compressed_size = compressed_size,
                        .uncompressed_size = chunk.size,
                        .uncompressed_offset = chunk.uncompressed_offset
                    });
                } else {
                    // Backoff when queue is empty, waiting for Reader to push more
                    std::this_thread::yield();
                }
            }

            ZSTD_freeCCtx(cctx);
        });
    }
}

} // namespace aether
