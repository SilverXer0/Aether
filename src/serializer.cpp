#include "aether/serializer.h"
#include <iostream>
#include <iomanip>

namespace aether {

void Serializer::run(ReorderBuffer& rob,
                     IOBackend& io,
                     std::vector<SeekEntry>& seek_table,
                     uint64_t& write_offset,
                     size_t total_chunks,
                     bool verbose,
                     MemoryPool* mem_pool)
{
    while (true) {
        ProcessedBlock block = rob.take_next();
        if (block.is_sentinel()) {
            break;
        }

        io.submit_write(block.data.get(), block.compressed_size, write_offset);
        
        if (mem_pool) {
            mem_pool->put(std::move(block.data));
        }

        seek_table.push_back(SeekEntry{
            .uncompressed_offset = block.uncompressed_offset,
            .compressed_offset   = write_offset,
            .compressed_size     = static_cast<uint32_t>(block.compressed_size),
        });

        write_offset += block.compressed_size;

        if (verbose) {
            double ratio = static_cast<double>(write_offset) /
                           static_cast<double>(block.uncompressed_offset + block.uncompressed_size);
            
            int progress = static_cast<int>(100.0 * seek_table.size() / total_chunks);
            int bars = progress / 2;
            std::cout << "\r[progress] [";
            for (int i = 0; i < 50; ++i) {
                if (i < bars) std::cout << "#";
                else std::cout << ".";
            }
            std::cout << "] " << std::setw(3) << progress << "% | Ratio: " << std::fixed
                      << std::setprecision(4) << ratio << " " << std::flush;
        }
    }
}

} // namespace aether
