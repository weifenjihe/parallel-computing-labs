#pragma once
#include "distribution.hpp"
namespace spmm {
void verify_output(const CsrMatrix&, const DenseMatrix&, const Config&, const Distribution&, MPI_Comm);
}
