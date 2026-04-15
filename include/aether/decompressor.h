#pragma once

#include "aether/types.h"
#include <cstdint>
#include <string>
#include <vector>

typedef struct ZSTD_DCtx_s ZSTD_DCtx;

namespace aether {

class SeekDecompressor {
public:
    explicit SeekDecompressor(const std::string& archive_path);
    ~SeekDecompressor();

    SeekDecompressor(const SeekDecompressor&) = delete;
    SeekDecompressor& operator=(const SeekDecompressor&) = delete;

    void decompress_all(const std::string& output_path, bool verbose = false);
    std::vector<uint8_t> read_at(uint64_t offset, size_t length);

    uint64_t original_size() const { return footer_.original_file_size; }
    size_t num_blocks() const { return seek_table_.size(); }
    uint32_t chunk_size() const { return footer_.chunk_size; }
    const FileFooter& footer() const { return footer_; }
    const std::vector<SeekEntry>& seek_table() const { return seek_table_; }

    void print_info() const;

private:
    int                     fd_ = -1;
    FileFooter              footer_;
    std::vector<SeekEntry>  seek_table_;
    ZSTD_DCtx*              dctx_ = nullptr;

    std::vector<uint8_t> decompress_block(size_t block_index);
};

} // namespace aether
