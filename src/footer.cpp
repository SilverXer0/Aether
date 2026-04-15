#include "aether/footer.h"

#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace aether {
namespace footer {

size_t write(IOBackend& io,
             uint64_t write_offset,
             const std::vector<SeekEntry>& table,
             uint64_t original_size,
             uint32_t chunk_size,
             uint32_t compression_level)
{
    uint64_t seek_table_offset = write_offset;
    size_t bytes_written = 0;

    if (!table.empty()) {
        size_t table_bytes = table.size() * sizeof(SeekEntry);
        io.submit_write(table.data(), table_bytes, write_offset);
        write_offset += table_bytes;
        bytes_written += table_bytes;
    }

    FileFooter footer{};
    footer.original_file_size = original_size;
    footer.seek_table_offset  = seek_table_offset;
    footer.num_entries        = static_cast<uint32_t>(table.size());
    footer.chunk_size         = chunk_size;
    footer.compression_level  = compression_level;
    footer.magic_number       = AETHER_MAGIC;
    footer.version            = AETHER_VERSION;

    io.submit_write(&footer, sizeof(FileFooter), write_offset);
    bytes_written += sizeof(FileFooter);

    io.flush();

    return bytes_written;
}

FileFooter read_footer(int fd) {
    off_t end = ::lseek(fd, 0, SEEK_END);
    if (end < 0) {
        throw std::runtime_error(
            "footer::read_footer: lseek SEEK_END failed: " +
            std::string(std::strerror(errno))
        );
    }

    if (static_cast<size_t>(end) < sizeof(FileFooter)) {
        throw std::runtime_error(
            "footer::read_footer: file too small to contain a footer"
        );
    }

    off_t footer_offset = end - static_cast<off_t>(sizeof(FileFooter));

    FileFooter footer;
    ssize_t n = ::pread(fd, &footer, sizeof(FileFooter), footer_offset);
    if (n != static_cast<ssize_t>(sizeof(FileFooter))) {
        throw std::runtime_error(
            "footer::read_footer: failed to read footer: " +
            std::string(std::strerror(errno))
        );
    }

    if (footer.magic_number != AETHER_MAGIC) {
        throw std::runtime_error(
            "footer::read_footer: invalid magic number (not an .aeth file)"
        );
    }

    if (footer.version != AETHER_VERSION) {
        throw std::runtime_error(
            "footer::read_footer: unsupported format version " +
            std::to_string(footer.version) + " (expected " +
            std::to_string(AETHER_VERSION) + ")"
        );
    }

    return footer;
}

std::vector<SeekEntry> read_seek_table(int fd, const FileFooter& footer) {
    if (footer.num_entries == 0) {
        return {};
    }

    std::vector<SeekEntry> table(footer.num_entries);
    size_t table_bytes = footer.num_entries * sizeof(SeekEntry);

    ssize_t n = ::pread(fd, table.data(), table_bytes,
                        static_cast<off_t>(footer.seek_table_offset));
    if (n != static_cast<ssize_t>(table_bytes)) {
        throw std::runtime_error(
            "footer::read_seek_table: failed to read seek table: " +
            std::string(std::strerror(errno))
        );
    }

    return table;
}

} // namespace footer
} // namespace aether
