#ifndef GEMINO_HANDLE_ALLOCATOR_HPP
#define GEMINO_HANDLE_ALLOCATOR_HPP

#include <common/debug.hpp>
#include <common/types.hpp>
#include <vector>
#include <unordered_set>

template <typename T>
class HandleAllocator {
public:
    // Allocate element using elements.emplace_back(std::forward<Args>(args)...)
    template<typename ... Args>
    Handle<T> alloc_in_place(Args&& ... args) {
        Handle<T> handle;

        if(!free_handles.empty()) {
            handle = free_handles.back();
            free_handles.pop_back();
            elements[handle] = T(std::forward<Args>(args)...);
        } else {
            handle = static_cast<Handle<T>>(elements.size());
            elements.emplace_back(std::forward<Args>(args)...);
        }

        valid_handles.insert(handle);

        return handle;
    }

    // Allocate element using elements.push_back(object)
    Handle<T> alloc(const T& object) {
        Handle<T> handle;

        if(!free_handles.empty()) {
            handle = free_handles.back();
            free_handles.pop_back();
            elements[handle] = object;
        } else {
            handle = static_cast<Handle<T>>(elements.size());
            elements.push_back(object);
        }

        valid_handles.insert(handle);

        return handle;
    }

    // Marks a handle as free so that it can be reused in later allocations
    void free(Handle<T> handle) {
        valid_handles.erase(handle);
        free_handles.push_back(handle);
    }

    const T& get_element(Handle<T> handle) const {
#if DEBUG_MODE
        return elements.at(handle);
#else
        return elements[handle];
#endif
    }

    T& get_element_mutable(Handle<T> handle) {
#if DEBUG_MODE
        return elements.at(handle);
#else
        return elements[handle];
#endif
    }

    bool is_handle_valid(Handle<T> handle) const {
        return valid_handles.contains(handle);
    }

    // This method may copy a lot of data (every valid handle index) so never use when real-time performance is expected.
    std::vector<Handle<T>> get_valid_handles_copy() const {
        std::vector<Handle<T>> handles{};
        handles.reserve(valid_handles.size());

        for(const auto& handle : valid_handles) {
            handles.push_back(handle);
        }

        return handles;
    }

    const std::unordered_set<Handle<T>>& get_valid_handles() const {
        return valid_handles;
    }

    // This will return ALL elements, even the invalid ones
    const std::vector<T>& get_all_elements() const {
        return elements;
    }

private:
    std::vector<T> elements{};
    std::vector<Handle<T>> free_handles{};
    std::unordered_set<Handle<T>> valid_handles{};
};

#endif