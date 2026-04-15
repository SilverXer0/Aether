#include "aether/compressor.h"
#include "aether/decompressor.h"
#include "aether/config.h"

#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <random>
#include <vector>
#include <cstring>

namespace fs = std::filesystem;

class RoundtripTest : public ::testing::Test {
protected:
    std::string input_file_;
    std::string compressed_file_;
    std::string output_file_;

    void SetUp() override {
        auto tmp = fs::temp_directory_path() / "aether_test";
        fs::create_directories(tmp);
        input_file_ = (tmp / "input.bin").string();
        compressed_file_ = (tmp / "input.bin.aeth").string();
        output_file_ = (tmp / "output.bin").string();
    }

    void TearDown() override {
        std::remove(input_file_.c_str());
        std::remove(compressed_file_.c_str());
        std::remove(output_file_.c_str());
    }

    /// Create a test file filled with pseudo-random data
    void create_random_file(size_t size) {
        std::mt19937 rng(42);  // Deterministic seed
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

    /// Create a test file filled with compressible repeated text
    void create_text_file(size_t size) {
        const std::string pattern = "AetherCompress is a high-performance compression tool! ";
        std::ofstream ofs(input_file_, std::ios::binary);
        ASSERT_TRUE(ofs.is_open());

        size_t remaining = size;
        while (remaining > 0) {
            size_t to_write = std::min(remaining, pattern.size());
            ofs.write(pattern.data(), static_cast<std::streamsize>(to_write));
            remaining -= to_write;
        }
    }

    /// Compare two files byte-by-byte. Returns true if identical.
    bool files_identical(const std::string& a, const std::string& b) {
        std::ifstream fa(a, std::ios::binary | std::ios::ate);
        std::ifstream fb(b, std::ios::binary | std::ios::ate);

        if (!fa.is_open() || !fb.is_open()) return false;

        auto size_a = fa.tellg();
        auto size_b = fb.tellg();

        if (size_a != size_b) return false;

        fa.seekg(0);
        fb.seekg(0);

        constexpr size_t BUF_SIZE = 4096;
        std::vector<char> buf_a(BUF_SIZE), buf_b(BUF_SIZE);

        while (fa && fb) {
            fa.read(buf_a.data(), BUF_SIZE);
            fb.read(buf_b.data(), BUF_SIZE);
            auto read_a = fa.gcount();
            auto read_b = fb.gcount();
            if (read_a != read_b) return false;
            if (std::memcmp(buf_a.data(), buf_b.data(), static_cast<size_t>(read_a)) != 0) {
                return false;
            }
        }

        return true;
    }
};

// ─── Small file: less than one chunk ───────────────────────────────────────────

TEST_F(RoundtripTest, SmallTextFile) {
    create_text_file(1024);  // 1 KB

    int rc = aether::compress_file(input_file_, compressed_file_, 3,
                                   aether::config::DEFAULT_CHUNK_SIZE, 0, false, false);
    ASSERT_EQ(rc, 0);

    rc = aether::decompress_file(compressed_file_, output_file_, false);
    ASSERT_EQ(rc, 0);

    EXPECT_TRUE(files_identical(input_file_, output_file_));
}

// ─── Medium file: spans multiple chunks ────────────────────────────────────────

TEST_F(RoundtripTest, MultiChunkTextFile) {
    // 10 MB of text — should compress well
    create_text_file(10 * 1024 * 1024);

    int rc = aether::compress_file(input_file_, compressed_file_, 3,
                                   aether::config::DEFAULT_CHUNK_SIZE, 0, false, false);
    ASSERT_EQ(rc, 0);

    rc = aether::decompress_file(compressed_file_, output_file_, false);
    ASSERT_EQ(rc, 0);

    EXPECT_TRUE(files_identical(input_file_, output_file_));
}

// ─── Random data (poor compression) ───────────────────────────────────────────

TEST_F(RoundtripTest, RandomDataRoundtrip) {
    // 5 MB of random data — tests worst-case compression
    create_random_file(5 * 1024 * 1024);

    int rc = aether::compress_file(input_file_, compressed_file_, 1,
                                   aether::config::DEFAULT_CHUNK_SIZE, 0, false, false);
    ASSERT_EQ(rc, 0);

    rc = aether::decompress_file(compressed_file_, output_file_, false);
    ASSERT_EQ(rc, 0);

    EXPECT_TRUE(files_identical(input_file_, output_file_));
}

// ─── File exactly one chunk ────────────────────────────────────────────────────

TEST_F(RoundtripTest, ExactlyOneChunk) {
    create_text_file(aether::config::DEFAULT_CHUNK_SIZE);

    int rc = aether::compress_file(input_file_, compressed_file_, 3,
                                   aether::config::DEFAULT_CHUNK_SIZE, 0, false, false);
    ASSERT_EQ(rc, 0);

    rc = aether::decompress_file(compressed_file_, output_file_, false);
    ASSERT_EQ(rc, 0);

    EXPECT_TRUE(files_identical(input_file_, output_file_));
}

// ─── Small chunk size ──────────────────────────────────────────────────────────

TEST_F(RoundtripTest, SmallChunkSize) {
    create_text_file(1024 * 1024);  // 1 MB

    // Use 64 KB chunks — many more blocks
    size_t small_chunk = 64 * 1024;
    int rc = aether::compress_file(input_file_, compressed_file_, 3,
                                   small_chunk, 0, false, false);
    ASSERT_EQ(rc, 0);

    rc = aether::decompress_file(compressed_file_, output_file_, false);
    ASSERT_EQ(rc, 0);

    EXPECT_TRUE(files_identical(input_file_, output_file_));
}

// ─── Archive info test ─────────────────────────────────────────────────────────

TEST_F(RoundtripTest, ArchiveInfoIsValid) {
    create_text_file(2 * 1024 * 1024);  // 2 MB

    int rc = aether::compress_file(input_file_, compressed_file_, 5,
                                   aether::config::DEFAULT_CHUNK_SIZE, 0, false, false);
    ASSERT_EQ(rc, 0);

    aether::SeekDecompressor decompressor(compressed_file_);
    EXPECT_EQ(decompressor.original_size(), 2u * 1024 * 1024);
    EXPECT_EQ(decompressor.chunk_size(), aether::config::DEFAULT_CHUNK_SIZE);
    EXPECT_EQ(decompressor.footer().compression_level, 5u);
    EXPECT_EQ(decompressor.footer().magic_number, aether::AETHER_MAGIC);
    EXPECT_EQ(decompressor.footer().version, aether::AETHER_VERSION);
    EXPECT_EQ(decompressor.num_blocks(), 1u);  // 2MB < 4MB chunk = 1 block
}

// ─── Parallel specifically ───────────────────────────────────────────────────

TEST_F(RoundtripTest, MultiThreadedExplicit) {
    create_text_file(15 * 1024 * 1024);  // 15 MB

    // Use 4 threads explicitly
    int rc = aether::compress_file(input_file_, compressed_file_, 3,
                                   aether::config::DEFAULT_CHUNK_SIZE, 4, false, false);
    ASSERT_EQ(rc, 0);

    rc = aether::decompress_file(compressed_file_, output_file_, false);
    ASSERT_EQ(rc, 0);

    EXPECT_TRUE(files_identical(input_file_, output_file_));
}

