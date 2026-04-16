# Aether

A high-performance, multithreaded compression utility utilizing `ZSTD`. Aether is built explicitly to construct a highly-optimized parallel C++ pipeline to achieve maximum disk-to-disk throughput on modern multi-core processors.

## Features

- **Lock-Free Multithreading:** Uses a custom `SPMCQueue` (Single-Producer, Multi-Consumer) to dispatch chunks with practically zero OS-level thread contention.
- **Parallel Compression & Decompression:** Both archival and extraction workflows are deeply parallelized using C++20 `std::jthread` thread pools and a dedicated synchronous `Serializer` orchestrator.
- **Zero-Copy I/O:** Mmaps files directly into virtual memory (`MAP_PRIVATE`) with POSIX read-ahead hinting for maximum throughput.
- **Random-Access Reads:** `aether read` enables random-access chunk-aligned `pread` extractions in O(1) time without decompressing the surrounding file.
- **Memory Pooling:** Reuses `std::unique_ptr<uint8_t[]>` allocation blocks inside a thread-safe object pool layer to drastically reduce page faults and heap fragmentation.
- **Linux `io_uring` support:** When compiled on Linux, cleanly switches the file output pipeline to a kernel-level asynchronous ring-buffer I/O layer.

---

## Building Natively (macOS / Linux)

### Dependencies
Aether uses `cmake` with `vcpkg`. Standard compilers (Clang on macOS or GCC 13+ on Linux) are required.
- `cli11`
- `zstd`
- `gtest` (for unit tests)
- `pkg-config`, `liburing`, and `libzstd-dev` (Linux Native only)

### Build Steps

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j $(nproc)
```

### Running Native Unit Tests
To verify thread-safety and mathematical correctness on your host machine:
```bash
cd build && ctest --output-on-failure
```

---

## Running through Docker (Linux Evaluator)

To seamlessly test Aether leveraging the proprietary Linux `io_uring` kernel logic (especially if you are on macOS), a prebuilt `Dockerfile` is provided. This safely isolates kernel-privileged instructions off of your machine.

**1. Build the container:**
```bash
docker build -t aether-linux .
```
*(Ensure you have a `.dockerignore` targeting the `build/` folder so MacOS CMakeCache contexts don't conflict!)*

**2. Execute the benchmark suite:**
```bash
docker run --rm --security-opt seccomp=unconfined aether-linux
```
*(Note: `--security-opt seccomp=unconfined` is required to allow the Docker container to execute the `io_uring` kernel space system calls)*

**3. Process Host Files through Docker (Testing Only)**
If you want to use the Linux variant to process a file located physically on your host/macOS machine, you must bind-mount the directory into the container. 
*(Warning: Translating zero-copy kernel reads through the Docker hypervisor virtualization layer adds significant overhead. For actual data ingestion on macOS, the native binary will be dramatically faster than the bind-mounted Docker binary!)*

```bash
# Example: Mounting your local Desktop to compress a file
docker run --rm -v ~/Desktop:/data --security-opt seccomp=unconfined aether-linux \
  /workspace/Aether/build/aether compress -i /data/my_file.bin -o /data/my_file.aeth --backend=uring -v
```

---

## CLI Usage

The compiled binary acts as a standalone standard pipeline interface natively targeting any file path on your system.

**Compression** (Auto-detects thread count based on hardware concurrency)
```bash
./build/aether compress -i /path/to/massive_dataset.bin -o ~/Desktop/massive_dataset.aeth -v
```

**Decompression**
```bash
./build/aether decompress -i ~/Desktop/massive_dataset.aeth -o /path/to/massive_restored.bin -v
```

**Partial Byte Extraction** (Extract bytes dynamically from the center of an archive)
```bash
aether read -i massive_dataset.aeth --offset 52428800 --length 1024 > output_snippet.bin
```

**Metadata Inspection**
```bash
aether info -i massive_dataset.aeth
```

---

## Performance & Diagnostics 

Aether heavily relies on kernel-layer instructions to maximize storage throughput. While basic `pwrite` pipelines scale exceptionally, utilizing the **io_uring** backend natively nearly doubles total bandwidth by dropping POSIX context switches.

### Quick Benchmark: 500MB /dev/urandom Generation
On a standard modern compiler environment mapping exactly 6 parallel background compression workers on an NVMe SSD:

| Backend | Processing Time | Throughput |
| :--- | :--- | :--- |
| **Sync** (`pwrite`) | `1.01` s | `493.3 MB/s` |
| **Uring** (`io_uring`) | `0.56` s | `879.7 MB/s` |

> See [Aether Benchmarks & Notes](benchmarks_and_notes.md) for deeper technical profiling, scaling limitations, and SPMC lock-free metrics.
