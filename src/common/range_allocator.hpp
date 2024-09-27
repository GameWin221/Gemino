#ifndef GEMINO_RANGE_ALLOCATOR_HPP
#define GEMINO_RANGE_ALLOCATOR_HPP

// TODO: Implement
/*
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

namespace std {
    template<typename T>
    struct hash<Range<T>> {
        std::size_t operator()(const Range<T> &range) const noexcept {
            return std::hash(range.start) ^ (std::hash(range.count * 11) >> 1);
        }
    };
}

template <typename T>
class RangeAllocator {
public:
    RangeAllocator() {
        m_free_ranges.insert(Range<T>{
            .start = 0u,
            .count = UINT32_MAX
        });
    }

    Range<T> alloc(u32 count) {
        Range<T> range = find_free_range(count);

        m_free_ranges.erase(range);

        // Insert what is left of the free Range
        if (range.count > count) {
            m_free_ranges.insert(Range<T>{
                .start = range.start + count,
                .count = range.count - count
            });
        }

        range.count = count;

        m_valid_ranges.insert(range);

        return range;
    }

    void free(Range<T> range) {

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
};
*/
#endif