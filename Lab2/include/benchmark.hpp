#pragma once
#include "transpose.hpp"
#include <string>

namespace transpose {
struct Config {
    bool verify = false;
    std::size_t n = 2048, tile = 32;
    int threads = 4, repeat = 7;
    std::uint32_t seed = 2026;
    std::string kernel = "naive";
};
void benchmark(const Config&);
}
