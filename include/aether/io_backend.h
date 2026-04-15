#pragma once

#include "aether/types.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <memory>

namespace aether {

class IOBackend {
public:
    virtual ~IOBackend() = default;
    virtual void submit_write(const void* data, size_t len, uint64_t offset) = 0;
    virtual void flush() = 0;
};

class SyncIOBackend : public IOBackend {
public:
    explicit SyncIOBackend(const std::string& path);
    ~SyncIOBackend();

    SyncIOBackend(const SyncIOBackend&) = delete;
    SyncIOBackend& operator=(const SyncIOBackend&) = delete;

    void submit_write(const void* data, size_t len, uint64_t offset) override;
    void flush() override;

    int fd() const { return fd_; }

private:
    int fd_ = -1;
};

// Factory function
std::unique_ptr<IOBackend> create_io_backend(const std::string& path, bool use_uring);

} // namespace aether
