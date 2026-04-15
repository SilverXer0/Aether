#include "aether/decompressor.h"
#include "aether/footer.h"

#include <zstd.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <iomanip>
#include <thread>
#include "aether/spmc_queue.h"
#include "aether/reorder_buffer.h"
#include "aether/memory_pool.h"
#include "aether/config.h"
#include "aether/io_backend.h"

namespace aether {

SeekDecompressor::SeekDecompressor(const std::string& archive_path) {
    fd_ = ::open(archive_path.c_str(), O_RDONLY);
    if (fd_ < 0) {
        throw std::runtime_error(
            "SeekDecompressor: failed to open archive: " + archive_path +
            " (" + std::strerror(errno) + ")"
        );
    }

    footer_ = footer::read_footer(fd_);
    seek_table_ = footer::read_seek_table(fd_, footer_);

    dctx_ = ZSTD_createDCtx();
    if (!dctx_) {
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error("SeekDecompressor: failed to create ZSTD decompression context");
    }
}

SeekDecompressor::~SeekDecompressor() {
    if (dctx_) {
        ZSTD_freeDCtx(dctx_);
    }
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

std::vector<uint8_t> SeekDecompressor::decompress_block(size_t block_index) {
    if (block_index >= seek_table_.size()) {
        throw std::runtime_error("SeekDecompressor: block index out of range");
    }

    const auto& entry = seek_table_[block_index];

    std::vector<uint8_t> compressed(entry.compressed_size);
    ssize_t n = ::pread(fd_, compressed.data(), entry.compressed_size,
                        static_cast<off_t>(entry.compressed_offset));
    if (n != static_cast<ssize_t>(entry.compressed_size)) {
        throw std::runtime_error(
            "SeekDecompressor: failed to read compressed block " +
            std::to_string(block_index)
        );
    }

    size_t decomp_size = footer_.chunk_size;

    if (block_index == seek_table_.size() - 1) {
        uint64_t remaining = footer_.original_file_size - entry.uncompressed_offset;
        decomp_size = static_cast<size_t>(remaining);
    }

    std::vector<uint8_t> decompressed(decomp_size);
    size_t result = ZSTD_decompressDCtx(
        dctx_,
        decompressed.data(), decomp_size,
        compressed.data(), entry.compressed_size
    );

    if (ZSTD_isError(result)) {
        throw std::runtime_error(
            "SeekDecompressor: decompression failed for block " +
            std::to_string(block_index) + ": " + ZSTD_getErrorName(result)
        );
    }

    decompressed.resize(result);
    return decompressed;
}

void SeekDecompressor::decompress_all(const std::string& output_path, bool verbose) {
    auto io = create_io_backend(output_path, false);
    
    size_t num_workers = config::default_worker_threads();
    size_t rob_capacity = config::default_rob_capacity(static_cast<unsigned>(num_workers));

    size_t queue_cap = 16;
    while (queue_cap <= seek_table_.size() && queue_cap < 8192) queue_cap *= 2;
    SPMCQueue<size_t> queue(queue_cap);

    ReorderBuffer rob(rob_capacity);
    MemoryPool pool(footer_.chunk_size); // Max uncompressed block size

    std::vector<std::jthread> workers;
    for (size_t i = 0; i < num_workers; ++i) {
        workers.emplace_back([this, &queue, &rob, &pool](std::stop_token stoken) {
            ZSTD_DCtx* dctx = ZSTD_createDCtx();
            while (!stoken.stop_requested()) {
                size_t block_index;
                if (queue.try_pop(block_index)) {
                    const auto& entry = seek_table_[block_index];
                    std::vector<uint8_t> compressed(entry.compressed_size);
                    ssize_t n = ::pread(this->fd_, compressed.data(), entry.compressed_size, entry.compressed_offset);
                    if (n != static_cast<ssize_t>(entry.compressed_size)) {
                        ZSTD_freeDCtx(dctx);
                        throw std::runtime_error("SeekDecompressor: failed to read compressed block");
                    }

                    size_t decomp_size = footer_.chunk_size;
                    if (block_index == seek_table_.size() - 1) {
                        decomp_size = static_cast<size_t>(footer_.original_file_size - entry.uncompressed_offset);
                    }

                    auto uncompressed = pool.get();
                    size_t result = ZSTD_decompressDCtx(dctx, uncompressed.get(), decomp_size, compressed.data(), entry.compressed_size);
                    if (ZSTD_isError(result)) {
                        ZSTD_freeDCtx(dctx);
                        throw std::runtime_error("SeekDecompressor: decompression failed: " + std::string(ZSTD_getErrorName(result)));
                    }

                    rob.deposit(ProcessedBlock{
                        .sequence_id = block_index,
                        .data = std::move(uncompressed),
                        .compressed_size = result, // Pass bytes to write via compressed_size field
                        .uncompressed_size = entry.compressed_size,
                        .uncompressed_offset = entry.uncompressed_offset
                    });
                } else {
                    std::this_thread::yield();
                }
            }
            ZSTD_freeDCtx(dctx);
        });
    }

    uint64_t write_offset = 0;
    
    std::jthread serializer_thread([this, &rob, &io, &write_offset, &pool, verbose]() {
        while (true) {
            ProcessedBlock block = rob.take_next();
            if (block.is_sentinel()) return;

            io->submit_write(block.data.get(), block.compressed_size, write_offset);
            
            write_offset += block.compressed_size;
            pool.put(std::move(block.data));
            
            if (verbose) {
                int progress = static_cast<int>(100.0 * write_offset / footer_.original_file_size);
                int bars = progress / 2;
                std::cout << "\r[progress] [";
                for (int i = 0; i < 50; ++i) {
                    if (i < bars) std::cout << "#";
                    else std::cout << ".";
                }
                std::cout << "] " << std::setw(3) << progress << "% " << std::flush;
            }
        }
    });

    for (size_t i = 0; i < seek_table_.size(); ++i) {
        queue.push(i);
    }
    
    rob.mark_complete(seek_table_.size());
    serializer_thread.join();
    io->flush();
    if (verbose) std::cout << "\n";
}

std::vector<uint8_t> SeekDecompressor::read_at(uint64_t offset, size_t length) {
    if (offset >= footer_.original_file_size) {
        return {};
    }

    if (offset + length > footer_.original_file_size) {
        length = static_cast<size_t>(footer_.original_file_size - offset);
    }

    std::vector<uint8_t> result;
    result.reserve(length);

    uint64_t current_offset = offset;
    size_t remaining = length;

    while (remaining > 0) {
        size_t block_index = static_cast<size_t>(current_offset / footer_.chunk_size);
        size_t offset_within_block = static_cast<size_t>(current_offset % footer_.chunk_size);

        auto block = decompress_block(block_index);

        size_t available = block.size() - offset_within_block;
        size_t to_copy = std::min(remaining, available);

        result.insert(result.end(),
                      block.begin() + offset_within_block,
                      block.begin() + offset_within_block + to_copy);

        current_offset += to_copy;
        remaining -= to_copy;
    }

    return result;
}

void SeekDecompressor::print_info() const {
    std::cout << "═══ AetherCompress Archive Info ═══\n";
    std::cout << "Magic:             0x" << std::hex << std::uppercase
              << footer_.magic_number << std::dec << "\n";
    std::cout << "Version:           " << footer_.version << "\n";
    std::cout << "Original size:     " << footer_.original_file_size << " bytes ("
              << std::fixed << std::setprecision(2)
              << static_cast<double>(footer_.original_file_size) / (1024.0 * 1024.0)
              << " MB)\n";
    std::cout << "Chunk size:        " << footer_.chunk_size << " bytes ("
              << footer_.chunk_size / 1024 << " KB)\n";
    std::cout << "Compression level: " << footer_.compression_level << "\n";
    std::cout << "Blocks:            " << footer_.num_entries << "\n";

    uint64_t total_compressed = 0;
    for (const auto& entry : seek_table_) {
        total_compressed += entry.compressed_size;
    }

    double ratio = (footer_.original_file_size > 0)
        ? static_cast<double>(total_compressed) / static_cast<double>(footer_.original_file_size)
        : 0.0;

    std::cout << "Compressed size:   " << total_compressed << " bytes ("
              << std::fixed << std::setprecision(2)
              << static_cast<double>(total_compressed) / (1024.0 * 1024.0)
              << " MB)\n";
    std::cout << "Ratio:             " << std::fixed << std::setprecision(4) << ratio << "\n";
    std::cout << "═══════════════════════════════════\n";
}

} // namespace aether
