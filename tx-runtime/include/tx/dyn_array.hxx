#pragma once

#include "memory.hxx"
#include "type_traits.hxx"

#include <gsl/gsl>

#include <cassert>
#include <memory>
#include <type_traits>

namespace tx {

// NOTE: This is not an RAII container, you have to call "destroy(VM)"
// passing in the VM that was used to allocate the container's data
// storage in order to destroy the data and free the memory.

template <typename T, typename SizeT>
    requires is_trivially_relocatable_v<T> && std::is_integral_v<SizeT>
class DynArray {
    SizeT count = 0;
    SizeT capacity = 0;
    gsl::owner<T*> data_ptr = nullptr;

  public:
    using value_type = T;
    using size_type = SizeT;

    constexpr DynArray() noexcept = default;

    DynArray(SizeT size, T value) noexcept
            : count(size)
            , capacity(size)
            , data_ptr(allocate<T>(size)) {
        std::uninitialized_fill_n(data_ptr, count, value);
    }

    constexpr DynArray(const DynArray& other) noexcept
            : count(other.count)
            , capacity(other.count)
            , data_ptr(allocate<T>(capacity)) {
        std::uninitialized_copy_n(other.data(), other.count, data_ptr);
    }

    constexpr DynArray(DynArray&& other) noexcept
            : count(other.count)
            , capacity(other.capacity)
            , data_ptr(other.data_ptr) {
        other.count = 0;
        other.capacity = 0;
        other.data_ptr = nullptr;
    }

    constexpr DynArray& operator=(const DynArray& other) noexcept {
        if (&other == this) { return *this; }
        if (capacity < other.count) { reserve(other.count); }
        std::destroy_n(begin(), count);
        std::uninitialized_copy_n(other.data(), other.count, data_ptr);
        return *this;
    }

    constexpr DynArray& operator=(DynArray&& other) noexcept {
        count = other.count;
        capacity = other.capacity;
        data_ptr = gsl::owner<T*>{other.data_ptr};
        other.count = 0;
        other.capacity = 0;
        other.data_ptr = nullptr;
        return *this;
    }

    constexpr ~DynArray() noexcept {
        assert(data_ptr == nullptr);
        assert(count == 0);
        assert(capacity == 0);
    }

    void destroy() noexcept {
        std::destroy_n(data_ptr, count);
        free_array(data_ptr, capacity);
        count = 0;
        capacity = 0;
        data_ptr = gsl::owner<T*>{nullptr};
    }

    [[nodiscard]] constexpr SizeT size() const noexcept { return count; }

    void resize(SizeT new_size) noexcept {
        if (new_size > count) {
            reserve(new_size);
            std::uninitialized_default_construct_n(
                &data_ptr[count],
                new_size - count
            );
            count = new_size;
        } else if (new_size < count) {
            std::destroy_n(&data_ptr[new_size], count - new_size);
            count = new_size;
        }
    }

    void reserve(SizeT new_cap) noexcept {
        data_ptr = gsl::owner<T*>{grow_array(data_ptr, capacity, new_cap)};
        capacity = new_cap;
    }

    void push_back(T value) noexcept {
        if (capacity == count) { reserve(grow_capacity(capacity)); }
        data_ptr[count++] = value;
    }

    void pop_back() noexcept { std::destroy_at(data_ptr[--count]); }

    [[nodiscard]] constexpr T& operator[](SizeT idx) noexcept {
        return data_ptr[idx];
    }

    [[nodiscard]] constexpr const T& operator[](SizeT idx) const noexcept {
        return data_ptr[idx];
    }

    [[nodiscard]] constexpr T& back() noexcept { return data_ptr[count - 1]; }

    [[nodiscard]] constexpr const T& back() const noexcept {
        return data_ptr[count - 1];
    }

    [[nodiscard]] constexpr const T* data() const noexcept {
        return static_cast<T*>(data_ptr);
    }

    [[nodiscard]] constexpr T* begin() noexcept { return &data_ptr[0]; }

    [[nodiscard]] constexpr const T* begin() const noexcept {
        return &data_ptr[0];
    }

    [[nodiscard]] constexpr const T* cbegin() const noexcept {
        return &data_ptr[0];
    }

    [[nodiscard]] constexpr T* end() noexcept { return &data_ptr[count]; }

    [[nodiscard]] constexpr const T* end() const noexcept {
        return &data_ptr[count];
    }

    [[nodiscard]] constexpr const T* cend() const noexcept {
        return &data_ptr[count];
    }
};
}  // namespace tx
