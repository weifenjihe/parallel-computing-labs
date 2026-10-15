#include "verification.hpp"
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace histogram {
void verify_output(const Input& input, const Counts& counts) {
    Counts expected(counts.size(), 0);
    for (auto value : input) ++expected[value];
    for (std::size_t bucket = 0; bucket < counts.size(); ++bucket)
        if (counts[bucket] != expected[bucket]) {
            std::ostringstream message;
            message << "VERIFY FAIL at bucket " << bucket << ": expected "
                    << expected[bucket] << ", got " << counts[bucket];
            throw std::runtime_error(message.str());
        }
    std::cerr << "VERIFY PASS: " << counts.size() << " buckets, exact counts\n";
}
}
