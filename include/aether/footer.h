#pragma once

#include "aether/types.h"
#include "aether/io_backend.h"
#include <cstdint>
#include <string>
#include <vector>

namespace aether {
namespace footer {

size_t write(IOBackend& io,
             uint64_t write_offset,
             const std::vector<SeekEntry>& table,
             uint64_t original_size,
             uint32_t chunk_size,
             uint32_t compression_level);

FileFooter read_footer(int fd);

std::vector<SeekEntry> read_seek_table(int fd, const FileFooter& footer);

} // namespace footer
} // namespace aether
