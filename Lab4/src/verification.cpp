#include "verification.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace spmm {
void verify_output(const CsrMatrix& a, const DenseMatrix& c, const Config& cfg,
                   const Distribution& d, MPI_Comm comm) {
    const auto full_b = generate_dense_rows(0, cfg.k, cfg.n, cfg.seed + 1);
    double max_error = 0;
    std::string failure;
    for (std::size_t i = 0; i < a.rows; ++i)
        for (std::size_t j = 0; j < cfg.n; ++j) {
            double expected = 0;
            for (std::size_t p = a.row_ptr[i]; p < a.row_ptr[i + 1]; ++p)
                expected += a.values[p] * full_b.values[std::size_t(a.col_idx[p]) * cfg.n + j];
            const double actual = c.values[i * cfg.n + j];
            const double error = std::abs(actual - expected);
            max_error = std::max(max_error, error);
            if (failure.empty() && (!std::isfinite(actual) || error > 1e-10 + 1e-9 * std::abs(expected))) {
                std::ostringstream message;
                message << "VERIFY FAIL at global (" << d.row_begin + i << ',' << j
                        << "), rank " << d.rank << ": expected " << expected << ", got " << actual;
                failure = message.str();
            }
        }
    int failed_rank = failure.empty() ? std::numeric_limits<int>::max() : d.rank;
    int first_failed_rank = 0;
    MPI_Allreduce(&failed_rank, &first_failed_rank, 1, MPI_INT, MPI_MIN, comm);
    if (first_failed_rank != std::numeric_limits<int>::max()) {
        int length = static_cast<int>(failure.size());
        MPI_Bcast(&length, 1, MPI_INT, first_failed_rank, comm);
        failure.resize(length);
        MPI_Bcast(failure.data(), length, MPI_CHAR, first_failed_rank, comm);
        throw std::runtime_error(failure);
    }
    double global_error = 0;
    MPI_Allreduce(&max_error, &global_error, 1, MPI_DOUBLE, MPI_MAX, comm);
    if (d.rank == 0)
        std::cerr << "VERIFY PASS: " << cfg.m * cfg.n << " distributed elements, max_abs_error=" << global_error << "\n";
}
}
