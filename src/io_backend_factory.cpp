#include "aether/io_backend.h"
#include <stdexcept>

namespace aether {

#if defined(AETHER_USE_URING)
// Provided by io_backend_uring.cpp
std::unique_ptr<IOBackend> create_uring_backend(const std::string& path);
#endif

std::unique_ptr<IOBackend> create_io_backend(const std::string& path, bool use_uring) {
#if defined(AETHER_USE_URING)
    if (use_uring) {
        return create_uring_backend(path);
    }
#else
    if (use_uring) {
        throw std::runtime_error("Aether was built without io_uring support");
    }
#endif
    return std::make_unique<SyncIOBackend>(path);
}

} // namespace aether
