#pragma once
#include "config.hpp"
#include <vector>
namespace gemm {
void verify_output(const Config&, const std::vector<std::int32_t>&);
}
