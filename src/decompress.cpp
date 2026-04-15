#include "aether/compressor.h"
#include "aether/decompressor.h"

#include <iostream>
#include <chrono>

namespace aether {

int decompress_file(const std::string& input_path,
                    const std::string& output_path,
                    bool verbose)
{
    try {
        if (verbose) {
            std::cout << "[decompress] Opening archive: " << input_path << "\n";
        }

        SeekDecompressor decompressor(input_path);

        if (verbose) {
            decompressor.print_info();
            std::cout << "[decompress] Decompressing to: " << output_path << "\n";
        }

        auto t_start = std::chrono::steady_clock::now();
        decompressor.decompress_all(output_path, verbose);
        auto t_end = std::chrono::steady_clock::now();

        if (verbose) {
            double elapsed = std::chrono::duration<double>(t_end - t_start).count();
            double mb = static_cast<double>(decompressor.original_size()) / (1024.0 * 1024.0);
            std::cout << "[decompress] Done!\n";
            std::cout << "[decompress] Time:       " << elapsed << " s\n";
            std::cout << "[decompress] Throughput: " << (mb / elapsed) << " MB/s\n";
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[decompress] Error: " << e.what() << "\n";
        return 1;
    }
}

} // namespace aether
