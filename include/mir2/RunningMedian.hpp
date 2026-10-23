// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace mir2 {

// Exact rolling median with caller-owned storage and O(log N) updates.
// Heap entries are ring-slot indices; signed inverse positions permit removal
// of the oldest slot without searching a heap. Double-encoded indices are exact
// throughout the supported capacity range. The initial history is all zeros.
class RunningMedian {
public:
    static constexpr int maxCapacity = 65536;

    static std::size_t storageCount(int capacity) noexcept {
        return capacity >= 1 && capacity <= maxCapacity
            ? 3 * static_cast<std::size_t>(capacity) + 2 : 0;
    }

    bool initialize(int capacity, double* memory) noexcept {
        capacity_ = cursor_ = lowerSize_ = upperSize_ = 0;
        values_ = positions_ = lower_ = upper_ = nullptr;
        const auto count = storageCount(capacity);
        if (count == 0 || memory == nullptr) return false;
        capacity_ = capacity;
        lowerSize_ = (capacity + 1) / 2;
        upperSize_ = capacity - lowerSize_;
        std::fill(memory, memory + count, 0.0);
        values_ = memory;
        positions_ = values_ + capacity;
        lower_ = positions_ + capacity;
        // One spare entry in each heap permits insertion before rebalancing.
        upper_ = lower_ + lowerSize_ + 1;
        for (int i = 0; i < lowerSize_; ++i) {
            lower_[i] = static_cast<double>(i);
            positions_[i] = static_cast<double>(i + 1);
        }
        for (int i = 0; i < upperSize_; ++i) {
            const int slot = lowerSize_ + i;
            upper_[i] = static_cast<double>(slot);
            positions_[slot] = -static_cast<double>(i + 1);
        }
        return true;
    }

    void update(double input) noexcept {
        if (capacity_ == 0) return;
        const int slot = cursor_;
        const int location = static_cast<int>(positions_[slot]);
        erase(location > 0, std::abs(location) - 1);
        values_[slot] = std::isfinite(input) ? input : 0.0;
        const bool toLower = lowerSize_ > 0
            ? values_[slot] <= values_[static_cast<int>(lower_[0])]
            : upperSize_ == 0;
        insert(toLower, slot);
        const int target = (capacity_ + 1) / 2;
        if (lowerSize_ > target) {
            const int move = static_cast<int>(lower_[0]);
            erase(true, 0);
            insert(false, move);
        } else if (lowerSize_ < target) {
            const int move = static_cast<int>(upper_[0]);
            erase(false, 0);
            insert(true, move);
        }
        cursor_ = (cursor_ + 1) % capacity_;
    }

    double value() const noexcept {
        if (capacity_ == 0) return 0.0;
        const double low = values_[static_cast<int>(lower_[0])];
        if ((capacity_ & 1) != 0) return low;
        const double high = values_[static_cast<int>(upper_[0])];
        // Halving before addition avoids overflow for finite extreme inputs.
        return 0.5 * low + 0.5 * high;
    }

private:
    int capacity_ = 0, cursor_ = 0, lowerSize_ = 0, upperSize_ = 0;
    double* values_ = nullptr;
    double* positions_ = nullptr;
    double* lower_ = nullptr;
    double* upper_ = nullptr;

    bool precedes(bool lower, int left, int right) const noexcept {
        return lower ? values_[left] > values_[right] : values_[left] < values_[right];
    }
    void place(bool lower, int position, int slot) noexcept {
        (lower ? lower_ : upper_)[position] = static_cast<double>(slot);
        positions_[slot] = static_cast<double>(lower ? position + 1 : -position - 1);
    }
    void siftUp(bool lower, int position) noexcept {
        double* heap = lower ? lower_ : upper_;
        const int slot = static_cast<int>(heap[position]);
        while (position > 0) {
            const int parent = (position - 1) / 2;
            const int parentSlot = static_cast<int>(heap[parent]);
            if (!precedes(lower, slot, parentSlot)) break;
            place(lower, position, parentSlot);
            position = parent;
        }
        place(lower, position, slot);
    }
    void siftDown(bool lower, int position) noexcept {
        double* heap = lower ? lower_ : upper_;
        const int size = lower ? lowerSize_ : upperSize_;
        const int slot = static_cast<int>(heap[position]);
        while (2 * position + 1 < size) {
            int child = 2 * position + 1;
            if (child + 1 < size && precedes(lower,
                    static_cast<int>(heap[child + 1]), static_cast<int>(heap[child])))
                ++child;
            const int childSlot = static_cast<int>(heap[child]);
            if (!precedes(lower, childSlot, slot)) break;
            place(lower, position, childSlot);
            position = child;
        }
        place(lower, position, slot);
    }
    void insert(bool lower, int slot) noexcept {
        int& size = lower ? lowerSize_ : upperSize_;
        const int position = size++;
        place(lower, position, slot);
        siftUp(lower, position);
    }
    void erase(bool lower, int position) noexcept {
        double* heap = lower ? lower_ : upper_;
        int& size = lower ? lowerSize_ : upperSize_;
        positions_[static_cast<int>(heap[position])] = 0.0;
        --size;
        if (position == size) return;
        const int slot = static_cast<int>(heap[size]);
        place(lower, position, slot);
        if (position > 0 && precedes(lower, slot, static_cast<int>(heap[(position - 1) / 2])))
            siftUp(lower, position);
        else
            siftDown(lower, position);
    }
};

} // namespace mir2
