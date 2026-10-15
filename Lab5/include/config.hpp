#pragma once
#include <cstdint>
#include <string>

namespace gemm {
struct Config {
    bool verify = false;
    std::uint32_t m = 256, k = 256, n = 256;
    std::uint32_t tile_m = 32, tile_k = 32, tile_n = 32;
    std::uint32_t latency = 12, beat_cycles = 1, stall_percent = 6, seed = 2026;
    std::uint64_t trace_start = 0, trace_cycles = 4096;
    int repeat = 1;
    std::uint64_t max_cycles = 100000000;
    bool double_buffer = true;
    std::string trace, dump_output;
};
}
