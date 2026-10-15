#pragma once
#include "spmm.hpp"
#include <string>

namespace spmm {
struct Config {
    bool verify = false;
    std::size_t m = 4096, k = 8192, n = 1024, nnz_per_row = 32;
    std::size_t block_nnz = 256, tile = 128;
    int threads = 4, repeat = 7, chunk = 0;
    std::uint32_t seed = 2026;
    std::string profile = "uniform", kernel = "rows", schedule = "static";
};
void benchmark(const Config&);
}
