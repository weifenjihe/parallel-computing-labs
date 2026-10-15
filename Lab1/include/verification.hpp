#pragma once
#include "matrix.hpp"
namespace spmm {
void verify_output(const CsrMatrix& a, const DenseMatrix& b, const DenseMatrix& c);
}
