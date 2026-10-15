#include "verification.hpp"
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace transpose {
void verify_output(const Matrix& input, const Matrix& output) {
    for (std::size_t i = 0; i < input.n; ++i)
        for (std::size_t j = 0; j < input.n; ++j)
            if (output.values[j * input.n + i] != input.values[i * input.n + j]) {
                std::ostringstream message;
                message << "VERIFY FAIL at output (" << j << ',' << i << "): expected "
                        << input.values[i * input.n + j] << ", got " << output.values[j * input.n + i];
                throw std::runtime_error(message.str());
            }
    std::cerr << "VERIFY PASS: " << output.values.size() << " elements, exact transpose\n";
}
}
