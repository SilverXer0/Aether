FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies required for AetherCompress and vcpkg
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    liburing-dev \
    libzstd-dev \
    git \
    curl \
    zip \
    unzip \
    tar \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

# Copy Aether directory over
COPY . /workspace/Aether

WORKDIR /workspace/Aether

# Build project with Uring support
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DAETHER_USE_URING=ON && \
    cmake --build . -j $(nproc)

# Expose a default script to run both tests and basic benchmarks natively
RUN echo '#!/bin/bash\n\
set -e\n\
echo "=== Running Linux Native Tests ==="\n\
cd /workspace/Aether/build\n\
ctest --output-on-failure\n\
\n\
echo ""\n\
echo "=== Generating 500MB Dummy Dataset for io_uring Benchmark ==="\n\
head -c 524288000 /dev/urandom > /workspace/Aether/bench_data.bin\n\
\n\
echo ""\n\
echo "[Sync IO Backend]" \n\
time ./aether compress -i /workspace/Aether/bench_data.bin -o /workspace/Aether/bench_sync.aeth --backend=sync -v\n\
\n\
echo ""\n\
echo "[io_uring IO Backend]" \n\
time ./aether compress -i /workspace/Aether/bench_data.bin -o /workspace/Aether/bench_uring.aeth --backend=uring -v\n\
\n\
' > /workspace/Aether/run_benchmarks.sh && chmod +x /workspace/Aether/run_benchmarks.sh

CMD ["/workspace/Aether/run_benchmarks.sh"]
