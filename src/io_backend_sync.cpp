#include "aether/io_backend.h"

#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <cerrno>
#include <cstring>

namespace aether {

SyncIOBackend::SyncIOBackend(const std::string& path) {
    fd_ = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_ < 0) {
        throw std::runtime_error(
            "SyncIOBackend: failed to open output file: " + path +
            " (" + std::strerror(errno) + ")"
        );
    }
}

SyncIOBackend::~SyncIOBackend() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

void SyncIOBackend::submit_write(const void* data, size_t len, uint64_t offset) {
    const uint8_t* buf = static_cast<const uint8_t*>(data);
    size_t remaining = len;
    uint64_t pos = offset;

    while (remaining > 0) {
        ssize_t written = ::pwrite(fd_, buf, remaining, static_cast<off_t>(pos));
        if (written < 0) {
            if (errno == EINTR) continue;
            throw std::runtime_error(
                "SyncIOBackend: pwrite failed: " + std::string(std::strerror(errno))
            );
        }
        buf += written;
        remaining -= static_cast<size_t>(written);
        pos += static_cast<uint64_t>(written);
    }
}

void SyncIOBackend::flush() {
    if (fd_ >= 0) {
        ::fsync(fd_);
    }
}

} // namespace aether
