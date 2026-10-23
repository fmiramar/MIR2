// SPDX-License-Identifier: GPL-3.0-or-later
#include "mir2/RunningMedian.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

namespace {
double reference(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    return (values.size() & 1U) != 0 ? values[middle]
        : 0.5 * values[middle - 1] + 0.5 * values[middle];
}

void exercise(int capacity, int updates, unsigned seed) {
    std::vector<double> memory(mir2::RunningMedian::storageCount(capacity) + 2, 123456.0);
    std::vector<double> ring(static_cast<std::size_t>(capacity), 0.0);
    mir2::RunningMedian median;
    assert(median.initialize(capacity, memory.data() + 1));
    assert(median.value() == 0.0);
    std::mt19937 random(seed);
    const double infinity = std::numeric_limits<double>::infinity();
    const double maximum = std::numeric_limits<double>::max();
    const double extremes[] = {infinity, -infinity,
        std::numeric_limits<double>::quiet_NaN(), maximum, -maximum, -0.0, 0.0};
    for (int i = 0; i < updates; ++i) {
        // A small integer support stresses duplicates, equal partitions and
        // heap deletions; monotone stretches exercise both sift directions.
        double input = static_cast<double>(static_cast<int>(random() % 31U) - 15);
        if (i % 211 < 31) input = static_cast<double>(i % 211);
        if (i % 211 >= 31 && i % 211 < 62) input = -static_cast<double>(i % 211);
        if (i % 97 < 7) input = extremes[i % 97];
        median.update(input);
        ring[static_cast<std::size_t>(i % capacity)] = std::isfinite(input) ? input : 0.0;
        // Large capacities use periodic exact checks to keep this a practical
        // test while still traversing and replacing the complete ring twice.
        if (capacity < 1024 || i % 997 == 0 || i + 1 == updates)
            assert(median.value() == reference(ring));
        assert(std::isfinite(median.value()));
        assert(memory.front() == 123456.0 && memory.back() == 123456.0);
    }
    // Reinitialization must clear history and reset its ring and heap indices.
    assert(median.initialize(capacity, memory.data() + 1));
    assert(median.value() == 0.0);
}
} // namespace

int main() {
    mir2::RunningMedian invalid;
    assert(mir2::RunningMedian::storageCount(0) == 0);
    assert(mir2::RunningMedian::storageCount(65537) == 0);
    assert(!invalid.initialize(3, nullptr));
    invalid.update(2.0);
    assert(invalid.value() == 0.0);
    for (int capacity : {1, 2, 3, 4, 5, 16, 17, 64, 127, 128})
        exercise(capacity, 10000, static_cast<unsigned>(capacity));
    exercise(8191, 20000, 71);
    exercise(65536, 131073, 123);
    std::cout << "Rolling median agrees with independently sorted histories.\n";
}
