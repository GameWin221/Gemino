#ifndef GEMINO_RANGE_ALLOCATOR_HPP
#define GEMINO_RANGE_ALLOCATOR_HPP

#include <common/debug.hpp>
#include <common/types.hpp>
#include <vector>
#include <unordered_set>
#include <set>

template<typename T>
struct Range {
    u32 start{};
    u32 count{};

    bool operator<(const Range &other) const {
        return start < other.start;
    }
    bool operator==(const Range &other) const {
        return start == other.start && count == other.count;
    }
};

template<typename T>
std::ostream& operator<<(std::ostream& os, const Range<T> &handle) {
    os << "( " << handle.start << ", " << handle.count << " )";
    return os;
}

namespace std {
    template<typename T>
    struct hash<Range<T>> {
        std::size_t operator()(const Range<T> &range) const noexcept {
            return std::hash<u32>()(range.start) ^ (std::hash<u32>()(range.count * 11u) >> 1u);
        }
    };
}

enum struct RangeAllocatorType {
    InPlace,
    External
};

template <typename T, RangeAllocatorType alloc_type = RangeAllocatorType::External>
class RangeAllocator {
public:
    RangeAllocator() {
        m_free_ranges.insert(Range<T>{
            .start = 0u,
            .count = UINT32_MAX
        });
    }

    Range<T> alloc(u32 count, const T *data = nullptr) {
        Range<T> range = find_free_range(count);

        m_free_ranges.erase(range);

        // Insert what remained of the free Range
        if (range.count > count) {
            m_free_ranges.insert(Range<T>{
                .start = range.start + count,
                .count = range.count - count
            });
        }

        range.count = count;

        m_valid_ranges.insert(range);

        if (alloc_type == RangeAllocatorType::InPlace) {
            if (data != nullptr) {
                for (u32 i{}; i < count; ++i) {
                    if (range.start + i < static_cast<u32>(m_elements.size())) {
                        m_elements[range.start + i] = data[i];
                    } else {
                        m_elements.push_back(data[i]);
                    }
                }
            } else {
                if (range.start + count >= static_cast<u32>(m_elements.size())) {
                    m_elements.resize(range.start + count);
                }
            }
        }

        return range;
    }

    void free(Range<T> range) {
        if (!is_range_valid(range)) {
            return;
        }

        m_free_ranges.insert(range);

        auto it = m_free_ranges.find(range);

        Range<T> prev_range{};
        auto it_prev = it;
        std::advance(it_prev, -1);
        if (it != m_free_ranges.begin()) {
            prev_range = *it_prev;
        }

        Range<T> next_range{};
        auto it_next = it;
        std::advance(it_next, 1);
        if (it_next != m_free_ranges.end()) {
            next_range = *it_next;
        }

        // Merge with neighboring ranges if adjacent.
        // This makes sure that there is no fragmentation.
        if(prev_range.count != 0u || next_range.count != 0u) {
            Range<T> merged_range = range;
            if (prev_range.start + prev_range.count == range.start) {
                merged_range.count += prev_range.count;
                merged_range.start -= prev_range.count;
                m_free_ranges.erase(prev_range);
            }
            if (range.start + range.count == next_range.start) {
                merged_range.count += next_range.count;
                m_free_ranges.erase(next_range);
            }

            m_free_ranges.erase(range);
            m_free_ranges.insert(merged_range);
        }

        m_valid_ranges.erase(range);
    }

    const T &get_element(u32 idx) const {
        if (alloc_type == RangeAllocatorType::InPlace) {
#if DEBUG_MODE
            return m_elements.at(idx);
#else
            return m_elements[idx];
#endif
        } else {
            DEBUG_PANIC("Cannot use the get_element method if RangeAllocatorType is not RangeAllocatorType::InPlace!")
        }
    }

    T &get_element_mutable(u32 idx) {
        if (alloc_type == RangeAllocatorType::InPlace) {
#if DEBUG_MODE
            return m_elements.at(idx);
#else
            return m_elements[idx];
#endif
        } else {
            DEBUG_PANIC("Cannot use the get_element_mutable method if RangeAllocatorType is not RangeAllocatorType::InPlace!")
        }
    }

    bool is_range_valid(Range<T> range) const {
        return m_valid_ranges.contains(range);
    }

    // This method may copy a lot of data (every valid handle index) so do not use it when real-time performance is expected.
    std::vector<Range<T>> get_valid_ranges_copy() const {
        std::vector<Range<T>> ranges{};
        ranges.reserve(m_valid_ranges.size());

        for(const auto &range : m_valid_ranges) {
            ranges.push_back(range);
        }

        return ranges;
    }

    const std::unordered_set<Range<T>> &get_valid_ranges() const {
        return m_valid_ranges;
    }
    const std::set<Range<T>> &get_free_ranges() const {
        return m_free_ranges;
    }

private:
    Range<T> find_free_range(u32 count) {
        // Return the first range that is at big enough to fit 'count' elements
        for (const auto &free_range : m_free_ranges) {
            if (free_range.count >= count) {
                return free_range;
            }
        }

        return Range<T>{
            .start = INVALID_HANDLE,
            .count = 0
        };
    }

    std::set<Range<T>> m_free_ranges{};
    std::unordered_set<Range<T>> m_valid_ranges{};
    std::vector<T> m_elements{};
};

#endif