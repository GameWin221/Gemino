#ifndef GEMINO_HANDLE_ALLOCATOR_HPP
#define GEMINO_HANDLE_ALLOCATOR_HPP

#include <common/debug.hpp>
#include <common/types.hpp>

#include <vector>
#include <unordered_set>

template<typename T>
struct Handle {
    Handle() = default;
    Handle(const u32 &other) : _internal(other) {}
    Handle(const Handle &other) = default;

    Handle &operator=(const Handle &other) = default;
    Handle &operator=(const u32 &other) {
        _internal = other;
        return *this;
    }

    bool operator==(const Handle &other) const {
        return _internal == other._internal;
    }
    bool operator==(const u32 &other) const {
        return _internal == other;
    }

    [[nodiscard]] u32 as_u32() const {
        return _internal;
    }
    template<typename U>
    [[nodiscard]] Handle<U> into() const {
        return Handle<U>(_internal);
    }

    u32 _internal{};
};

template<typename T>
std::ostream& operator<<(std::ostream& os, const Handle<T> &handle) {
    os << handle.as_u32();
    return os;
}

namespace std {
    template<typename T>
    struct hash<Handle<T>> {
        std::size_t operator()(const Handle<T> &handle) const noexcept {
            return std::hash<u32>()(handle.as_u32());
        }
    };
}

template <typename T>
class HandleAllocator {
public:
    // Allocate element using m_elements.emplace_back(std::forward<Args>(args)...)
    template<typename ... Args>
    Handle<T> alloc_in_place(Args&& ... args) {
        Handle<T> handle;

        if(!m_free_handles.empty()) {
            handle = m_free_handles.back();
            m_free_handles.pop_back();
            m_elements[handle.as_u32()] = T(std::forward<Args>(args)...);
        } else {
            handle = static_cast<u32>(m_elements.size());
            m_elements.emplace_back(std::forward<Args>(args)...);
        }

        m_valid_handles.insert(handle);

        return handle;
    }

    // Allocate element using m_elements.push_back(object)
    Handle<T> alloc(const T& object) {
        Handle<T> handle{};

        if(!m_free_handles.empty()) {
            handle = m_free_handles.back();
            m_free_handles.pop_back();
            m_elements[handle.as_u32()] = object;
        } else {
            handle = static_cast<u32>(m_elements.size());
            m_elements.push_back(object);
        }

        m_valid_handles.insert(handle);

        return handle;
    }

    // Marks a handle as free so that it can be reused in later allocations
    void free(Handle<T> handle) {
        if(!is_handle_valid(handle)) {
            return;
        }

        m_valid_handles.erase(handle);
        m_free_handles.push_back(handle);
    }

    const T& get_element(Handle<T> handle) const {
#if DEBUG_MODE
        return m_elements.at(handle.as_u32());
#else
        return m_elements[handle].as_u32();
#endif
    }

    T& get_element_mutable(Handle<T> handle) {
#if DEBUG_MODE
        return m_elements.at(handle.as_u32());
#else
        return m_elements[handle.as_u32()];
#endif
    }

    bool is_handle_valid(Handle<T> handle) const {
        return m_valid_handles.contains(handle);
    }

    // This method may copy a lot of data (every valid handle index) so do not use it use when real-time performance is expected.
    std::vector<Handle<T>> get_valid_handles_copy() const {
        std::vector<Handle<T>> handles{};
        handles.reserve(m_valid_handles.size());

        for(const auto& handle : m_valid_handles) {
            handles.push_back(handle);
        }

        return handles;
    }

    const std::unordered_set<Handle<T>> &get_valid_handles() const {
        return m_valid_handles;
    }

    // This will return ALL elements, even the invalid ones
    const std::vector<T> &get_all_elements() const {
        return m_elements;
    }

private:
    std::vector<T> m_elements{};
    std::vector<Handle<T>> m_free_handles{};
    std::unordered_set<Handle<T>> m_valid_handles{};
};

#endif