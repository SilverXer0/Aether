#include "aether/mmap_reader.h"
#include "aether/platform.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdexcept>
#include <string>

#if AETHER_PLATFORM_MACOS
    #include <sys/fcntl.h>
#endif

namespace aether {

MmapReader::MmapReader(const std::string& path, size_t chunk_size)
    : chunk_size_(chunk_size)
{
    fd_ = ::open(path.c_str(), O_RDONLY);
    if (fd_ < 0) {
        throw std::runtime_error("MmapReader: failed to open file: " + path);
    }

    struct stat st;
    if (::fstat(fd_, &st) != 0) {
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error("MmapReader: failed to stat file: " + path);
    }

    file_size_ = static_cast<uint64_t>(st.st_size);

    if (file_size_ == 0) {
        return;
    }

    mapped_addr_ = static_cast<uint8_t*>(
        ::mmap(nullptr, file_size_, PROT_READ, MAP_PRIVATE, fd_, 0)
    );

    if (mapped_addr_ == MAP_FAILED) {
        mapped_addr_ = nullptr;
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error("MmapReader: mmap failed for file: " + path);
    }

    ::madvise(mapped_addr_, file_size_, MADV_SEQUENTIAL);

#if AETHER_PLATFORM_MACOS
    ::fcntl(fd_, F_RDAHEAD, 1);
#endif

    ::madvise(mapped_addr_, file_size_, MADV_WILLNEED);
}

MmapReader::~MmapReader() {
    if (mapped_addr_ != nullptr && mapped_addr_ != MAP_FAILED) {
        ::munmap(mapped_addr_, file_size_);
    }
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

size_t MmapReader::num_chunks() const {
    if (file_size_ == 0) return 0;
    return (file_size_ + chunk_size_ - 1) / chunk_size_;
}

std::vector<ChunkDescriptor> MmapReader::generate_chunks() const {
    std::vector<ChunkDescriptor> chunks;
    size_t total = num_chunks();
    chunks.reserve(total);

    for (size_t i = 0; i < total; ++i) {
        uint64_t offset = static_cast<uint64_t>(i) * chunk_size_;
        size_t size = chunk_size_;

        if (offset + size > file_size_) {
            size = static_cast<size_t>(file_size_ - offset);
        }

        chunks.push_back(ChunkDescriptor{
            .sequence_id = i,
            .data = mapped_addr_ + offset,
            .size = size,
            .uncompressed_offset = offset,
        });
    }

    return chunks;
}

} // namespace aether
