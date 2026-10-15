#pragma once
#include "histogram.hpp"

namespace histogram {
struct Config {
    bool verify = false;
    std::size_t n = 10000000, bins = 256, stripes = 16;
    int threads = 4, repeat = 7, hot_percent = 90;
    std::uint32_t seed = 2026;
    std::string distribution = "uniform", kernel = "critical";
};
void benchmark(const Config&);
}
