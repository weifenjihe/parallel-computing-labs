#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace spmm {
struct Config {
    bool verify = false;
    std::size_t m = 4096, k = 8192, n = 1024, nnz_per_row = 32;
    std::size_t tile = 128, block_nnz = 256;
    int repeat = 7;
    std::uint32_t seed = 2026;
    std::string profile = "uniform", kernel = "halo", partition = "nnz";
};
}
