#pragma once

#include "tx/memory.hxx"
#include "tx/type_traits.hxx"

#include <gsl/gsl>

#include <cassert>
#include <cstddef>
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
    SizeT capacity_ = 0;
    gsl::owner<T*> data_ptr = nullptr;

  public:
    using value_type = T;
    using size_type = SizeT;
    using difference_type = std::ptrdiff_t;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    constexpr DynArray() noexcept = default;

    constexpr DynArray(VM& tvm, SizeT size, T value) noexcept
            : count(size)
            , capacity_(size)
            , data_ptr(grow_array(tvm, nullptr, 0, size)) {
        std::uninitialized_fill_n(data_ptr, count, value);
    }

    constexpr DynArray(const DynArray& other) noexcept = delete;

    constexpr DynArray(DynArray&& other) noexcept
            : count(other.count)
            , capacity_(other.capacity_)
            , data_ptr(other.data_ptr) {
        other.count = 0;
        other.capacity_ = 0;
        other.data_ptr = nullptr;
    }

    constexpr DynArray& operator=(const DynArray& other) noexcept = delete;

    constexpr DynArray& operator=(DynArray&& other) noexcept {
        destroy();
        count = other.count;
        capacity_ = other.capacity_;
        data_ptr = other.data_ptr;  // NOLINT
        other.count = 0;
        other.capacity_ = 0;
        other.data_ptr = nullptr;
        return *this;
    }

    constexpr ~DynArray() noexcept {
        assert(data_ptr == nullptr);
        assert(count == 0);
        assert(capacity_ == 0);
    }

    constexpr void destroy(VM& tvm) noexcept {
        clear();
        if (data_ptr != nullptr) {
            free_array(tvm, data_ptr, capacity_);
            capacity_ = 0;
            data_ptr = nullptr;  // NOLINT
        }
    }

    constexpr void clear() noexcept(std::is_nothrow_destructible_v<T>) {
        std::destroy_n(data_ptr, count);
        count = 0;
    }

    constexpr T* erase(
        const T* first,
        const T* last
    ) noexcept(std::is_nothrow_destructible_v<T>&&
                   std::is_nothrow_move_assignable_v<T>) {
        assert(first < last);
        assert((data_ptr <= first) && (first <= cend()));
        assert((data_ptr <= last) && (last <= cend()));
        T* write_first = begin()
                         + static_cast<SizeT>(std::distance(cbegin(), first));
        T* write_last = begin()
                        + static_cast<SizeT>(std::distance(cbegin(), last));
        const auto diff = std::distance(first, last);
        const auto [_, new_end] = std::ranges::move(
            write_last,
            end(),
            write_first
        );
        std::destroy(new_end, end());
        count -= static_cast<SizeT>(diff);
        return write_last;
    }

    [[nodiscard]] constexpr SizeT size() const noexcept { return count; }

    [[nodiscard]] constexpr SizeT capacity() const noexcept {
        return capacity_;
    }

    [[nodiscard]] constexpr bool empty() const noexcept { return count == 0; }

    constexpr void resize(VM& tvm, SizeT new_size) noexcept {
        resize(tvm, new_size, T());
    }

    constexpr void resize(VM& tvm, SizeT new_size, const T& val) noexcept {
        // NOTE: maybe best to use next power of 2
        if (new_size > capacity_) { reserve(tvm, new_size); }
        if (new_size > count) {
            std::uninitialized_fill_n(end(), new_size - count, val);
            count = new_size;
        } else if (new_size < count) {
            std::destroy_n(&data_ptr[new_size], count - new_size);
            count = new_size;
        }
    }

    constexpr void reserve(VM& tvm, SizeT new_cap) noexcept {
        data_ptr = grow_array(tvm, data_ptr, capacity_, new_cap);
        capacity_ = new_cap;
    }

    constexpr void push_back(VM& tvm, T value) noexcept {
        if (capacity_ == count) { reserve(tvm, grow_capacity(capacity_)); }
        push_back_unsafe(value);
    }

    constexpr void push_back_unsafe(T value) noexcept {
        assert(count < capacity_);
        std::construct_at(std::next(data_ptr, count++), value);
    }

    template <typename... Args>
    constexpr T& emplace_back(VM& tvm, Args&&... args) noexcept(noexcept(
        std::construct_at(&data_ptr[count++], std::forward<Args>(args)...)
    )) {
        if (capacity_ == count) { reserve(tvm, grow_capacity(capacity_)); }
        return *std::construct_at(
            std::next(data_ptr, count++),
            std::forward<Args>(args)...
        );
    }

    constexpr void pop_back() noexcept {
        std::destroy_at(std::next(data_ptr, --count));
    }

    [[nodiscard]] constexpr T& operator[](SizeT idx) noexcept {
        return data_ptr[idx];
    }

    [[nodiscard]] constexpr const T& operator[](SizeT idx) const noexcept {
        return data_ptr[idx];
    }

    [[nodiscard]] constexpr T& back() noexcept {
        return *std::next(data_ptr, count - 1);
    }

    [[nodiscard]] constexpr const T& back() const noexcept {
        return *std::next(data_ptr, count - 1);
    }

    [[nodiscard]] constexpr T* data() noexcept {
        return data_ptr;  // NOLINT
    }

    [[nodiscard]] constexpr const T* data() const noexcept {
        return data_ptr;  // NOLINT
    }

    [[nodiscard]] constexpr T* begin() noexcept { return data_ptr; }

    [[nodiscard]] constexpr const T* begin() const noexcept { return data_ptr; }

    [[nodiscard]] constexpr const T* cbegin() const noexcept {
        return data_ptr;
    }

    [[nodiscard]] constexpr T* end() noexcept {
        return std::next(data_ptr, count);
    }

    [[nodiscard]] constexpr const T* end() const noexcept {
        return std::next(data_ptr, count);
    }

    [[nodiscard]] constexpr const T* cend() const noexcept {
        return std::next(data_ptr, count);
    }

    [[nodiscard]] constexpr reverse_iterator rbegin() noexcept {
        return std::reverse_iterator(end());
    }

    [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept {
        return crbegin();
    }

    [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept {
        return std::reverse_iterator(cend());
    }

    [[nodiscard]] constexpr reverse_iterator rend() noexcept {
        return std::reverse_iterator(begin());
    }

    [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept {
        return crend();
    }

    [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept {
        return std::reverse_iterator(cbegin());
    }
};
}  // namespace tx
