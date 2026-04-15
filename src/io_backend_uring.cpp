#include "aether/io_backend.h"

#if defined(AETHER_USE_URING)

#include <liburing.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <atomic>

namespace aether {

class UringIOBackend : public IOBackend {
public:
    explicit UringIOBackend(const std::string& path) {
        fd_ = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd_ < 0) {
            throw std::runtime_error("UringIOBackend: failed to open file");
        }

        int ret = io_uring_queue_init(QUEUE_DEPTH, &ring_, 0);
        if (ret < 0) {
            ::close(fd_);
            throw std::runtime_error("UringIOBackend: failed to init io_uring: " + std::string(std::strerror(-ret)));
        }
    }

    ~UringIOBackend() override {
        drain_cqe(pending_count_);
        io_uring_queue_exit(&ring_);
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    void submit_write(const void* data, size_t len, uint64_t offset) override {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
        if (!sqe) {
            // Queue full: submit current batch and wait for at least one completion
            io_uring_submit(&ring_);
            drain_cqe(1);
            sqe = io_uring_get_sqe(&ring_);
            if (!sqe) {
                throw std::runtime_error("UringIOBackend: failed to get SQE even after draining");
            }
        }

        io_uring_prep_write(sqe, fd_, data, len, offset);
        io_uring_sqe_set_data(sqe, nullptr);

        pending_count_++;
        bytes_written_.fetch_add(len, std::memory_order_relaxed);

        if (pending_count_ >= BATCH_SIZE) {
            io_uring_submit(&ring_);
            drain_cqe(pending_count_ - BATCH_SIZE + 1); // Keep it draining
        }
    }

    void flush() override {
        if (pending_count_ > 0) {
            io_uring_submit(&ring_);
            drain_cqe(pending_count_);
        }
        ::fsync(fd_);
    }

private:
    static constexpr unsigned QUEUE_DEPTH = 64;
    static constexpr unsigned BATCH_SIZE = 32;

    struct io_uring ring_;
    int fd_;
    size_t pending_count_ = 0;
    std::atomic<uint64_t> bytes_written_{0};

    void drain_cqe(size_t count) {
        struct io_uring_cqe* cqe;
        for (size_t i = 0; i < count; ++i) {
            int ret = io_uring_wait_cqe(&ring_, &cqe);
            if (ret < 0) {
                throw std::runtime_error("UringIOBackend: wait_cqe failed: " + std::string(std::strerror(-ret)));
            }
            if (cqe->res < 0) {
                throw std::runtime_error("UringIOBackend: async write failed: " + std::string(std::strerror(-cqe->res)));
            }
            io_uring_cqe_seen(&ring_, cqe);
            pending_count_--;
        }
    }
};

std::unique_ptr<IOBackend> create_uring_backend(const std::string& path) {
    return std::make_unique<UringIOBackend>(path);
}

} // namespace aether

#endif // AETHER_USE_URING
