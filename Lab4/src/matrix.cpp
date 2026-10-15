#include "matrix.hpp"
#include "support.hpp"
#include "validation.hpp"

namespace spmm {
DenseMatrix::DenseMatrix(std::size_t m, std::size_t n) : rows(m), cols(n), values(m * n) {}

std::size_t row_nonzeros(std::size_t row, std::size_t m, std::size_t k,
                         std::size_t nnz_per_row, const std::string& profile) {
    if (profile != "skewed" || m <= 1) return nnz_per_row;
    const auto hot_rows = std::max<std::size_t>(1, m / 8);
    const auto total = m * nnz_per_row;
    const auto hot_count = std::min(k, std::max(nnz_per_row, (total * 3 / 4) / hot_rows));
    const auto remaining = total - hot_rows * hot_count;
    const auto cold_rows = m - hot_rows;
    return row < hot_rows ? hot_count : remaining / cold_rows + (row - hot_rows < remaining % cold_rows);
}

CsrMatrix generate_sparse_rows(std::size_t begin, std::size_t end, std::size_t m,
                               std::size_t k, std::size_t nnz_per_row,
                               const std::string& profile, std::uint32_t seed) {
    lab::positive(k, "K");
    lab::require(nnz_per_row <= k, "nnz_per_row must not exceed K; duplicate CSR columns are not allowed.");
    CsrMatrix a;
    a.rows = end - begin; a.cols = k; a.row_ptr.push_back(0);
    for (std::size_t i = begin; i < end; ++i) {
        const auto count = row_nonzeros(i, m, k, nnz_per_row, profile);
        const auto width = profile == "blocked" || profile == "banded"
            ? std::min(k, std::max<std::size_t>(1, count * 4)) : k;
        const auto center = i * k / m;
        const auto base = profile == "blocked" ? (i / 32 * width) % k
            : profile == "banded" ? (center + k - width / 2) % k : 0;
        const auto offset = lab::mix(static_cast<std::uint32_t>(i) + seed) % width;
        std::size_t step = (lab::mix(static_cast<std::uint32_t>(i) + seed + 7919u) % width) | 1u;
        while (std::gcd(step, width) != 1) ++step;
        std::vector<std::uint32_t> columns;
        columns.reserve(count);
        for (std::size_t p = 0; p < count; ++p)
            columns.push_back(static_cast<std::uint32_t>((base + (offset + p * step) % width) % k));
        std::sort(columns.begin(), columns.end());
        for (auto column : columns) {
            const auto bits = lab::mix(static_cast<std::uint32_t>(i * k + column) + seed);
            a.col_idx.push_back(column);
            a.values.push_back((bits & 1u ? 1.0 : -1.0) * (0.25 + (bits % 1000u) / 1000.0));
        }
        a.row_ptr.push_back(a.values.size());
    }
    return a;
}

DenseMatrix generate_dense_rows(std::size_t begin, std::size_t end,
                                std::size_t n, std::uint32_t seed) {
    DenseMatrix b(end - begin, n);
    for (std::size_t i = 0; i < b.values.size(); ++i)
        b.values[i] = (static_cast<int>(lab::mix(static_cast<std::uint32_t>(begin * n + i) + seed) % 2001u) - 1000) / 1000.0;
    return b;
}

std::vector<RowBlock> make_row_blocks(const CsrMatrix& a, std::size_t target_nnz) {
    std::vector<RowBlock> blocks;
    std::size_t begin = 0;
    for (std::size_t i = 0; i < a.rows; ++i)
        if (a.row_ptr[i + 1] - a.row_ptr[begin] >= target_nnz) {
            blocks.push_back({begin, i + 1}); begin = i + 1;
        }
    if (begin < a.rows) blocks.push_back({begin, a.rows});
    return blocks;
}
}
