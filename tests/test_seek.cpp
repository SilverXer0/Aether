#include "aether/compressor.h"
#include "aether/decompressor.h"
#include "aether/config.h"

#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <random>
#include <vector>

namespace fs = std::filesystem;

class SeekTest : public ::testing::Test {
protected:
    std::string input_file_;
    std::string compressed_file_;

    void SetUp() override {
        auto tmp = fs::temp_directory_path() / "aether_test";
        fs::create_directories(tmp);
        input_file_ = (tmp / "seek_input.bin").string();
        compressed_file_ = (tmp / "seek_input.bin.aeth").string();
    }

    void TearDown() override {
        std::remove(input_file_.c_str());
        std::remove(compressed_file_.c_str());
    }

    void create_random_file(size_t size) {
        std::mt19937 rng(1337);
        std::uniform_int_distribution<int> dist(0, 255);

        std::ofstream ofs(input_file_, std::ios::binary);
        ASSERT_TRUE(ofs.is_open());

        constexpr size_t BUF_SIZE = 4096;
        std::vector<char> buf(BUF_SIZE);

        size_t remaining = size;
        while (remaining > 0) {
            size_t to_write = std::min(remaining, BUF_SIZE);
            for (size_t i = 0; i < to_write; ++i) {
                buf[i] = static_cast<char>(dist(rng));
            }
            ofs.write(buf.data(), static_cast<std::streamsize>(to_write));
            remaining -= to_write;
        }
    }

    std::vector<uint8_t> read_uncompressed(uint64_t offset, size_t length) {
        std::ifstream ifs(input_file_, std::ios::binary);
        ifs.seekg(offset);
        std::vector<uint8_t> result(length);
        ifs.read(reinterpret_cast<char*>(result.data()), length);
        result.resize(ifs.gcount());
        return result;
    }
};

TEST_F(SeekTest, RandomAccessWithinChunk) {
    create_random_file(5 * 1024 * 1024); // 5 MB

    int rc = aether::compress_file(input_file_, compressed_file_, 1,
                                   aether::config::DEFAULT_CHUNK_SIZE, 0, false, false);
    ASSERT_EQ(rc, 0);

    aether::SeekDecompressor decompressor(compressed_file_);

    auto expected = read_uncompressed(0, 100);
    auto actual = decompressor.read_at(0, 100);
    EXPECT_EQ(expected, actual);
}

TEST_F(SeekTest, RandomAccessSpanningChunks) {
    create_random_file(5 * 1024 * 1024); // 5 MB

    int rc = aether::compress_file(input_file_, compressed_file_, 1,
                                   aether::config::DEFAULT_CHUNK_SIZE, 0, false, false);
    ASSERT_EQ(rc, 0);

    aether::SeekDecompressor decompressor(compressed_file_);

    // Read 100 bytes directly across the 4MB boundary
    uint64_t offset = aether::config::DEFAULT_CHUNK_SIZE - 50; 
    auto expected = read_uncompressed(offset, 100);
    auto actual = decompressor.read_at(offset, 100);
    EXPECT_EQ(expected, actual);
}

TEST_F(SeekTest, RandomAccessEndOfFile) {
    create_random_file(5 * 1024 * 1024);

    int rc = aether::compress_file(input_file_, compressed_file_, 1,
                                   aether::config::DEFAULT_CHUNK_SIZE, 0, false, false);
    ASSERT_EQ(rc, 0);

    aether::SeekDecompressor decompressor(compressed_file_);

    uint64_t offset = 5 * 1024 * 1024 - 100;
    auto expected = read_uncompressed(offset, 100);
    auto actual = decompressor.read_at(offset, 100);
    EXPECT_EQ(expected, actual);
}

TEST_F(SeekTest, ReadPastEndOfFile) {
    create_random_file(1024 * 1024);

    int rc = aether::compress_file(input_file_, compressed_file_, 1,
                                   aether::config::DEFAULT_CHUNK_SIZE, 0, false, false);
    ASSERT_EQ(rc, 0);

    aether::SeekDecompressor decompressor(compressed_file_);

    uint64_t offset = 1024 * 1024 - 10;
    auto expected = read_uncompressed(offset, 10);
    // Ask for much more than what's available
    auto actual = decompressor.read_at(offset, 100); 
    EXPECT_EQ(expected, actual);
    EXPECT_EQ(actual.size(), 10u);
}
