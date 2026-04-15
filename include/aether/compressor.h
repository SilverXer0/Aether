#pragma once

#include <string>
#include <cstdint>

namespace aether {

int compress_file(const std::string& input_path,
                  const std::string& output_path,
                  int compression_level,
                  size_t chunk_size,
                  size_t num_threads,
                  bool use_uring,
                  bool verbose);

int decompress_file(const std::string& input_path,
                    const std::string& output_path,
                    bool verbose);

} // namespace aether
