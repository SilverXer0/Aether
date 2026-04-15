cmake_minimum_required(VERSION 3.20)
project(AetherCompress VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(AETHER_USE_URING  "Enable io_uring backend (Linux only)" OFF)
option(AETHER_USE_MMAP   "Use mmap for input (vs read)"         ON)
option(AETHER_BUILD_TESTS "Build unit tests"                    ON)
option(AETHER_BUILD_BENCH "Build benchmarks"                    OFF)
option(AETHER_SANITIZE    "Enable sanitizers (thread, address)" OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd)

include(FetchContent)
FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.4.2
)
FetchContent_MakeAvailable(cli11)

if(AETHER_USE_URING AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    pkg_check_modules(URING REQUIRED IMPORTED_TARGET liburing)
    add_compile_definitions(AETHER_USE_URING=1)
endif()

add_library(aether_lib STATIC
    src/mmap_reader.cpp
    src/compressor.cpp
    src/thread_pool.cpp
    src/reorder_buffer.cpp
    src/serializer.cpp
    src/io_backend_factory.cpp
    src/io_backend_sync.cpp
    src/footer.cpp
    src/decompressor.cpp
    src/compress.cpp
    src/decompress.cpp
)

target_include_directories(aether_lib PUBLIC include)
target_link_libraries(aether_lib PUBLIC PkgConfig::ZSTD)

if(AETHER_USE_URING AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(aether_lib PRIVATE src/io_backend_uring.cpp)
    target_link_libraries(aether_lib PUBLIC PkgConfig::URING)
endif()

if(AETHER_SANITIZE)
    target_compile_options(aether_lib PUBLIC -fsanitize=thread -g)
    target_link_options(aether_lib PUBLIC -fsanitize=thread)
endif()

add_executable(aether src/main.cpp)
target_link_libraries(aether PRIVATE aether_lib CLI11::CLI11)

if(AETHER_SANITIZE)
    target_compile_options(aether PRIVATE -fsanitize=thread -g)
    target_link_options(aether PRIVATE -fsanitize=thread)
endif()

if(AETHER_BUILD_TESTS)
    enable_testing()

    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
    )
    FetchContent_MakeAvailable(googletest)

    add_executable(test_footer tests/test_footer.cpp)
    target_link_libraries(test_footer PRIVATE aether_lib GTest::gtest_main)
    add_test(NAME test_footer COMMAND test_footer)

    add_executable(test_roundtrip tests/test_roundtrip.cpp)
    target_link_libraries(test_roundtrip PRIVATE aether_lib GTest::gtest_main)
    add_test(NAME test_roundtrip COMMAND test_roundtrip)

    add_executable(test_spmc_queue tests/test_spmc_queue.cpp)
    target_include_directories(test_spmc_queue PRIVATE include)
    target_link_libraries(test_spmc_queue PRIVATE GTest::gtest_main)
    add_test(NAME test_spmc_queue COMMAND test_spmc_queue)

    add_executable(test_seek tests/test_seek.cpp)
    target_link_libraries(test_seek PRIVATE aether_lib GTest::gtest_main)
    add_test(NAME test_seek COMMAND test_seek)

    if(AETHER_SANITIZE)
        target_compile_options(test_footer PRIVATE -fsanitize=thread -g)
        target_link_options(test_footer PRIVATE -fsanitize=thread)
        target_compile_options(test_roundtrip PRIVATE -fsanitize=thread -g)
        target_link_options(test_roundtrip PRIVATE -fsanitize=thread)
        target_compile_options(test_spmc_queue PRIVATE -fsanitize=thread -g)
        target_link_options(test_spmc_queue PRIVATE -fsanitize=thread)
        target_compile_options(test_seek PRIVATE -fsanitize=thread -g)
        target_link_options(test_seek PRIVATE -fsanitize=thread)
    endif()
endif()

if(AETHER_BUILD_BENCH)
    add_executable(bench_spmc_queue bench/bench_spmc_queue.cpp)
    target_include_directories(bench_spmc_queue PRIVATE include)
    target_compile_options(bench_spmc_queue PRIVATE -O3)
endif()
