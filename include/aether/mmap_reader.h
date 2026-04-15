#pragma once

#include "aether/types.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aether {

class MmapReader {
public:
    explicit MmapReader(const std::string& path, size_t chunk_size);
    ~MmapReader();

    MmapReader(const MmapReader&) = delete;
    MmapReader& operator=(const MmapReader&) = delete;

    std::vector<ChunkDescriptor> generate_chunks() const;
    uint64_t file_size() const { return file_size_; }
    size_t num_chunks() const;

private:
    int         fd_ = -1;
    uint8_t*    mapped_addr_ = nullptr;
    uint64_t    file_size_ = 0;
    size_t      chunk_size_;
};

} // namespace aether
