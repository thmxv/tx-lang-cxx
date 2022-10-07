#pragma once

#include <array>
#include <cstdlib>
#include <type_traits>

namespace tx {

template <typename T, typename SizeT, SizeT C>
    requires std::is_integral_v<SizeT>
class FixedCapacityArray {
    static_assert(C > 0, "Capacity must be positive.");

    using MutDataType = std::array<std::byte, sizeof(T)>;
    using DataType =
        std::conditional_t<std::is_const_v<T>, const MutDataType, MutDataType>;

    // using StorageType = std::array<DataType, static_cast<std::size_t>(C)>;
    using StorageType = DataType[static_cast<std::size_t>(C)];

    SizeT count = 0;
    alignas(T) StorageType data_buff;

  public:
    using value_type = T;
    using size_type = SizeT;
    using iterator = T*;
    using const_iterator = const T*;
    using difference_type = std::ptrdiff_t;

    constexpr ~FixedCapacityArray() noexcept
        requires(std::is_trivially_destructible_v<T>)
    = default;

    constexpr ~FixedCapacityArray() noexcept(std::is_nothrow_destructible_v<T>)
        requires(!std::is_trivially_destructible_v<T>)
    {
        // TODO: make sure destroyed was called
        clear();
    }

    [[nodiscard]] constexpr FixedCapacityArray& operator=(
        FixedCapacityArray&& other
    ) noexcept = delete;

    // void destroy() noexcept {
    //     // TODO
    // }

    [[nodiscard]] constexpr SizeT size() const noexcept { return count; }

    [[nodiscard]] constexpr bool empty() const noexcept { return count == 0; }

    constexpr void clear() noexcept(std::is_nothrow_destructible_v<T>) {
        for (SizeT i = 0; i < count; ++i) { std::destroy_at(&operator[](i)); }
        count = 0;
    }

    constexpr T* erase(
        const T* first,
        const T* last
    ) noexcept(std::is_nothrow_destructible_v<T>&&
                   std::is_nothrow_assignable_v<T, T>) {
        T* write_iterator = begin()
                            + static_cast<SizeT>(std::distance(cbegin(), first)
                            );
        while (last != cend()) {
            std::destroy_at(write_iterator);
            *write_iterator++ = *last++;
        }
        count -= static_cast<SizeT>(std::distance(first, last));
        return write_iterator;
    }

    constexpr void push_back(T value
    ) noexcept(std::is_nothrow_copy_constructible_v<T>) {
        if (count == C) { std::abort(); }
        // NOTE: Do not launder
        std::construct_at(reinterpret_cast<T*>(&data_buff[count++]), value);
    }

    constexpr void push_back_unsafe(T value
    ) noexcept(std::is_nothrow_copy_constructible_v<T>) {
        assert(count < C);
        // NOTE: Do not launder
        std::construct_at(reinterpret_cast<T*>(&data_buff[count++]), value);
    }

    template <typename... Args>
    constexpr T& emplace_back(Args&&... args) noexcept(
        noexcept(std::construct_at(
            reinterpret_cast<T*>(&data_buff[count++]),
            std::forward<Args>(args)...
        ))
    ) {
        assert(count < C);
        // NOTE: Do not launder
        return *std::construct_at(
            reinterpret_cast<T*>(&data_buff[count++]),
            std::forward<Args>(args)...
        );
    }

    constexpr void pop_back() noexcept(std::is_nothrow_destructible_v<T>) {
        std::destroy_at(&back());
        count--;
    }

    [[nodiscard]] constexpr T& operator[](SizeT idx) noexcept {
        return *std::launder(reinterpret_cast<T*>(&data_buff[idx]));
    }

    [[nodiscard]] constexpr const T& operator[](SizeT idx) const noexcept {
        return *std::launder(reinterpret_cast<const T*>(&data_buff[idx]));
    }

    [[nodiscard]] constexpr T& back() noexcept { return operator[](count - 1); }

    [[nodiscard]] constexpr const T& back() const noexcept {
        return operator[](count - 1);
    }

    [[nodiscard]] constexpr const T* data() const noexcept {
        return &operator[](0);
    }

    [[nodiscard]] constexpr T* begin() noexcept { return &operator[](0); }

    [[nodiscard]] constexpr const T* begin() const noexcept {
        return &operator[](0);
    }

    [[nodiscard]] constexpr T* end() noexcept { return &operator[](count); }

    [[nodiscard]] constexpr const T* end() const noexcept {
        return &operator[](count);
    }

    [[nodiscard]] constexpr const T* cbegin() const noexcept {
        return &operator[](0);
    }

    [[nodiscard]] constexpr const T* cend() const noexcept {
        return &operator[](count);
    }
};
}  // namespace tx
