#include "verification.hpp"
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace spmm {
void verify_output(const CsrMatrix& a, const DenseMatrix& b, const DenseMatrix& c) {
    double max_error = 0;
    // Reference computes one output at a time, independently of the parallel/tiled kernels.
    for (std::size_t i = 0; i < a.rows; ++i)
        for (std::size_t j = 0; j < b.cols; ++j) {
            double expected = 0;
            for (std::size_t p = a.row_ptr[i]; p < a.row_ptr[i + 1]; ++p)
                expected += a.values[p] * b.values[std::size_t(a.col_idx[p]) * b.cols + j];
            const double actual = c.values[i * c.cols + j];
            const double error = std::abs(actual - expected);
            max_error = std::max(max_error, error);
            if (!std::isfinite(actual) || error > 1e-10 + 1e-9 * std::abs(expected)) {
                std::ostringstream message;
                message << "VERIFY FAIL at (" << i << ',' << j << "): expected "
                        << expected << ", got " << actual << ", error " << error;
                throw std::runtime_error(message.str());
            }
        }
    std::cerr << "VERIFY PASS: " << c.values.size() << " elements, max_abs_error=" << max_error << "\n";
}
}
