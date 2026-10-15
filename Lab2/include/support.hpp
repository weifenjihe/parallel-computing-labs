#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>
#include <stdexcept>

namespace lab {
inline double now() {
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}
inline std::size_t median_index(const std::vector<double>& times) {
    if (times.empty()) throw std::invalid_argument("repeat must be greater than zero.");
    std::vector<std::size_t> order(times.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](auto a, auto b) { return times[a] < times[b]; });
    return order[order.size() / 2];
}
inline std::uint32_t mix(std::uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    return x ^ (x >> 16);
}
inline void csv_precision() { std::cout << std::setprecision(10); }
}
