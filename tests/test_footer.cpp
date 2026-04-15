#include "aether/types.h"
#include "aether/footer.h"
#include "aether/io_backend.h"

#include <gtest/gtest.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

class FooterTest : public ::testing::Test {
protected:
    std::string test_file_;

    void SetUp() override {
        test_file_ = (fs::temp_directory_path() / "aether_footer_test.bin").string();
    }

    void TearDown() override {
        std::remove(test_file_.c_str());
    }
};

TEST_F(FooterTest, StaticAssertSizes) {
    // Already enforced at compile time, but let's double check
    EXPECT_EQ(sizeof(aether::SeekEntry), 20);
    EXPECT_EQ(sizeof(aether::FileFooter), 36);
}

TEST_F(FooterTest, WriteAndReadEmptyTable) {
    aether::SyncIOBackend io(test_file_);
    std::vector<aether::SeekEntry> empty_table;

    size_t written = aether::footer::write(io, 0, empty_table, 0, 4096, 3);
    EXPECT_EQ(written, sizeof(aether::FileFooter));

    // Now read it back
    int fd = ::open(test_file_.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);

    auto footer = aether::footer::read_footer(fd);
    EXPECT_EQ(footer.magic_number, aether::AETHER_MAGIC);
    EXPECT_EQ(footer.version, aether::AETHER_VERSION);
    EXPECT_EQ(footer.original_file_size, 0u);
    EXPECT_EQ(footer.num_entries, 0u);
    EXPECT_EQ(footer.chunk_size, 4096u);
    EXPECT_EQ(footer.compression_level, 3u);

    auto table = aether::footer::read_seek_table(fd, footer);
    EXPECT_TRUE(table.empty());

    ::close(fd);
}

TEST_F(FooterTest, WriteAndReadMultipleEntries) {
    aether::SyncIOBackend io(test_file_);

    std::vector<aether::SeekEntry> table = {
        {0,      0,    1000},
        {4096,   1000, 950},
        {8192,   1950, 1100},
        {12288,  3050, 800},
    };

    uint64_t original_size = 16384;
    uint32_t chunk_size = 4096;
    uint32_t level = 5;

    // Write at offset 0 (as if there are no compressed blocks before the footer)
    size_t written = aether::footer::write(io, 0, table, original_size, chunk_size, level);
    EXPECT_EQ(written, table.size() * sizeof(aether::SeekEntry) + sizeof(aether::FileFooter));

    // Read it back
    int fd = ::open(test_file_.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);

    auto footer = aether::footer::read_footer(fd);
    EXPECT_EQ(footer.original_file_size, original_size);
    EXPECT_EQ(footer.num_entries, 4u);
    EXPECT_EQ(footer.chunk_size, chunk_size);
    EXPECT_EQ(footer.compression_level, level);

    auto read_table = aether::footer::read_seek_table(fd, footer);
    ASSERT_EQ(read_table.size(), 4u);

    for (size_t i = 0; i < table.size(); ++i) {
        EXPECT_EQ(read_table[i].uncompressed_offset, table[i].uncompressed_offset);
        EXPECT_EQ(read_table[i].compressed_offset, table[i].compressed_offset);
        EXPECT_EQ(read_table[i].compressed_size, table[i].compressed_size);
    }

    ::close(fd);
}

TEST_F(FooterTest, InvalidMagicThrows) {
    // Write garbage to the file
    {
        aether::SyncIOBackend io(test_file_);
        std::vector<uint8_t> garbage(64, 0xFF);
        io.submit_write(garbage.data(), garbage.size(), 0);
        io.flush();
    }

    int fd = ::open(test_file_.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);

    EXPECT_THROW(aether::footer::read_footer(fd), std::runtime_error);

    ::close(fd);
}

TEST_F(FooterTest, FileTooSmallThrows) {
    // Write a file smaller than sizeof(FileFooter)
    {
        aether::SyncIOBackend io(test_file_);
        uint8_t byte = 0;
        io.submit_write(&byte, 1, 0);
        io.flush();
    }

    int fd = ::open(test_file_.c_str(), O_RDONLY);
    ASSERT_GE(fd, 0);

    EXPECT_THROW(aether::footer::read_footer(fd), std::runtime_error);

    ::close(fd);
}
