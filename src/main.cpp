#include "aether/compressor.h"
#include "aether/decompressor.h"
#include "aether/config.h"

#include <CLI/CLI.hpp>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    CLI::App app{"AetherCompress — Parallel ZSTD compression with random-access decompression"};
    app.require_subcommand(1);

    auto* compress_cmd = app.add_subcommand("compress", "Compress a file into .aeth format");

    std::string compress_input;
    std::string compress_output;
    int compress_level = aether::config::DEFAULT_COMPRESSION_LEVEL;
    size_t compress_chunk = aether::config::DEFAULT_CHUNK_SIZE;
    size_t compress_threads = 0;
    bool compress_verbose = false;
    std::string compress_backend = "sync";

    compress_cmd->add_option("-i,--input", compress_input, "Input file")
        ->required()
        ->check(CLI::ExistingFile);
    compress_cmd->add_option("-o,--output", compress_output, "Output file (default: <input>.aeth)");
    compress_cmd->add_option("-l,--level", compress_level, "ZSTD compression level (1-22)")
        ->check(CLI::Range(aether::config::MIN_COMPRESSION_LEVEL,
                           aether::config::MAX_COMPRESSION_LEVEL));
    compress_cmd->add_option("-c,--chunk-size", compress_chunk, "Chunk size in bytes")
        ->check(CLI::Range(aether::config::MIN_CHUNK_SIZE,
                           aether::config::MAX_CHUNK_SIZE));
    compress_cmd->add_option("-t,--threads", compress_threads, "Number of worker threads (default: hardware concurrency)");
    compress_cmd->add_option("--backend", compress_backend, "I/O Backend (sync or uring)")
        ->check(CLI::IsMember({"sync", "uring"}));
    compress_cmd->add_flag("-v,--verbose", compress_verbose, "Show progress and stats");

    auto* decompress_cmd = app.add_subcommand("decompress", "Decompress an .aeth archive");

    std::string decompress_input;
    std::string decompress_output;
    bool decompress_verbose = false;

    decompress_cmd->add_option("-i,--input", decompress_input, ".aeth archive file")
        ->required()
        ->check(CLI::ExistingFile);
    decompress_cmd->add_option("-o,--output", decompress_output, "Output file")
        ->required();
    decompress_cmd->add_flag("-v,--verbose", decompress_verbose, "Show progress and stats");

    auto* info_cmd = app.add_subcommand("info", "Display archive metadata");

    std::string info_input;

    info_cmd->add_option("-i,--input", info_input, ".aeth archive file")
        ->required()
        ->check(CLI::ExistingFile);

    auto* read_cmd = app.add_subcommand("read", "Random-access read from an .aeth archive");

    std::string read_input;
    uint64_t read_offset = 0;
    size_t read_length = 0;
    bool read_hex = false;

    read_cmd->add_option("-i,--input", read_input, ".aeth archive file")
        ->required()
        ->check(CLI::ExistingFile);
    read_cmd->add_option("--offset", read_offset, "Logical byte offset to read from")
        ->required();
    read_cmd->add_option("--length", read_length, "Number of bytes to read");
    read_cmd->add_flag("--hex", read_hex, "Output as hex dump");

    CLI11_PARSE(app, argc, argv);

    if (compress_cmd->parsed()) {
        if (compress_output.empty()) {
            compress_output = compress_input + ".aeth";
        }
        bool use_uring = (compress_backend == "uring");
        return aether::compress_file(compress_input, compress_output,
                                     compress_level, compress_chunk,
                                     compress_threads, use_uring, compress_verbose);
    }

    if (decompress_cmd->parsed()) {
        return aether::decompress_file(decompress_input, decompress_output,
                                       decompress_verbose);
    }

    if (info_cmd->parsed()) {
        try {
            aether::SeekDecompressor decompressor(info_input);
            decompressor.print_info();
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "[info] Error: " << e.what() << "\n";
            return 1;
        }
    }

    if (read_cmd->parsed()) {
        try {
            aether::SeekDecompressor decompressor(read_input);

            if (read_length == 0) {
                read_length = decompressor.chunk_size();
            }

            auto data = decompressor.read_at(read_offset, read_length);

            if (read_hex) {
                for (size_t i = 0; i < data.size(); ++i) {
                    if (i > 0 && i % 16 == 0) std::cout << "\n";
                    else if (i > 0 && i % 8 == 0) std::cout << " ";

                    char hex[4];
                    std::snprintf(hex, sizeof(hex), "%02x ", data[i]);
                    std::cout << hex;
                }
                std::cout << "\n";
            } else {
                std::cout.write(reinterpret_cast<const char*>(data.data()),
                                static_cast<std::streamsize>(data.size()));
            }

            return 0;
        } catch (const std::exception& e) {
            std::cerr << "[read] Error: " << e.what() << "\n";
            return 1;
        }
    }

    return 0;
}
