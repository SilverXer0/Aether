# AetherCompress Benchmarks & Notes

This document will track the performance benchmarks of the `AetherCompress` tool across different configurations, machines, and thread counts.

## System Configuration 1 (macOS)
**CPU**: `<Insert CPU Model>`
**RAM**: `<Insert RAM>`
**Storage**: `<Insert SSD Type / Speed>`
**Compiler**: Apple Clang (Release build)

### Benchmark 1: SPMC Lock-Free Queue Throughput
*Ran via `bench_spmc_queue`*

Queue Size | Items | Consumers | Mutex Time (ms) | Lock-Free Time (ms) | Gain
--- | --- | --- | --- | --- | ---
1024 | 2,000,000 | 1 | | | 
1024 | 2,000,000 | 2 | | | 
1024 | 2,000,000 | 4 | | | 

### Benchmark 2: End-to-End Compression (Sync pwrite)
*Dataset: `<Insert Dataset name and size>`*
*Chunk Size: 4 MB*
*Compression Level: 3*

Workers | Processing Time (s) | Throughput (MB/s) | Compression Ratio
--- | --- | --- | ---
1 | | | 
2 | | | 
4 | | | 
8 | | | 

## System Configuration 2 (Linux VM / Native)
**OS**: Ubuntu 24.04 (via Docker / OrbStack on macOS)
**Compiler**: GNU GCC 13.3.0
**Dataset**: 500 MB Random Binaries (`/dev/urandom`)
**Chunk Size**: 4 MB
**Compression Level**: 3

### Benchmark 3: I/O Backend Comparison (Linux)
*Comparing `sync` (pwrite) with `uring` (io_uring batched)*

Backend | Threads | Processing Time (s) | Throughput (MB/s)
--- | --- | --- | ---
Sync (`pwrite`) | 6 | 1.0135 s | 493.31 MB/s
Uring (`io_uring`) | 6 | 0.5684 s | 879.71 MB/s

---

## Technical Notes

- **ThreadSanitizer:** Used heavily during the SPMC lock-free queue development. Ensure you build with `-DAETHER_SANITIZE=ON` if you make any modifications to the lock-free code.
- **Memory Pool:** Currently `std::make_unique` is used for chunk allocations. A global memory pool implementation is planned for future optimizations to reduce heap allocations during the dispatch phase.
- **io_uring limitation:** Due to Apple Silicon macOS underlying architecture, the true zero-copy `io_uring` backend cannot be run locally and gracefully falls back to `pwrite`. A Linux environment is strictly required for testing that path.
