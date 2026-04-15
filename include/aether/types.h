#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>

namespace aether {

constexpr uint32_t AETHER_MAGIC   = 0x41455448;  // "AETH"
constexpr uint32_t AETHER_VERSION = 1;

#pragma pack(push, 1)

struct SeekEntry {
    uint64_t uncompressed_offset;
    uint64_t compressed_offset;
    uint32_t compressed_size;
};

static_assert(sizeof(SeekEntry) == 20, "SeekEntry must be exactly 20 bytes (packed)");

struct FileFooter {
    uint64_t original_file_size;
    uint64_t seek_table_offset;
    uint32_t num_entries;
    uint32_t chunk_size;
    uint32_t compression_level;
    uint32_t magic_number;
    uint32_t version;
};

static_assert(sizeof(FileFooter) == 36, "FileFooter must be exactly 36 bytes (packed)");

#pragma pack(pop)

struct ChunkDescriptor {
    uint64_t       sequence_id;
    const uint8_t* data;
    size_t         size;
    uint64_t       uncompressed_offset;
};

struct ProcessedBlock {
    uint64_t                    sequence_id;
    std::unique_ptr<uint8_t[]>  data;
    size_t                      compressed_size;
    size_t                      uncompressed_size;
    uint64_t                    uncompressed_offset;

    bool is_sentinel() const { return data == nullptr; }

    static ProcessedBlock make_sentinel() {
        return ProcessedBlock{
            .sequence_id = UINT64_MAX,
            .data = nullptr,
            .compressed_size = 0,
            .uncompressed_size = 0,
            .uncompressed_offset = 0,
        };
    }
};

} // namespace aether
