#pragma once

#include "aether/types.h"
#include "aether/io_backend.h"
#include "aether/reorder_buffer.h"
#include "aether/memory_pool.h"

#include <cstdint>
#include <vector>

namespace aether {

class Serializer {
public:
    static void run(ReorderBuffer& rob,
                    IOBackend& io,
                    std::vector<SeekEntry>& seek_table,
                    uint64_t& write_offset,
                    size_t total_chunks,
                    bool verbose,
                    MemoryPool* mem_pool = nullptr);
};

} // namespace aether
