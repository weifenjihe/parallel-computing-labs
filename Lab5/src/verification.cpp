#include "verification.hpp"
#include "support.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <limits>

namespace gemm {
void verify_output(const Config& cfg, const std::vector<std::int32_t>& output) {
    std::vector<std::int16_t> a(std::size_t(cfg.m) * cfg.k), b(std::size_t(cfg.k) * cfg.n);
    for (std::size_t i = 0; i < a.size(); ++i) a[i] = std::int16_t(int(lab::mix(std::uint32_t(i) + cfg.seed) % 7) - 3);
    for (std::size_t i = 0; i < b.size(); ++i) b[i] = std::int16_t(int(lab::mix(std::uint32_t(i) + cfg.seed + 1) % 7) - 3);
    for (std::size_t i = 0; i < cfg.m; ++i)
        for (std::size_t j = 0; j < cfg.n; ++j) {
            std::int64_t expected = 0;
            for (std::size_t k = 0; k < cfg.k; ++k) expected += std::int64_t(a[i * cfg.k + k]) * b[k * cfg.n + j];
            if (expected < std::numeric_limits<std::int32_t>::min() || expected > std::numeric_limits<std::int32_t>::max())
                throw std::runtime_error("Reference output exceeds int32; reduce K.");
            if (output[i * cfg.n + j] != expected) {
                std::ostringstream message;
                message << "VERIFY FAIL at (" << i << ',' << j << "): expected " << expected
                        << ", got " << output[i * cfg.n + j];
                throw std::runtime_error(message.str());
            }
        }
    std::cerr << "VERIFY PASS: " << output.size() << " elements, exact integer GEMM\n";
}
}
