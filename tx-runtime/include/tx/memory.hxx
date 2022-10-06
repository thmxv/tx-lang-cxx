#pragma once

#include "common.hxx"

#include <gsl/gsl>

namespace tx {

inline constexpr size_t MIN_CAPACIY = 8;
inline constexpr size_t CAPACITY_SCALE_FACTOR = 2;

[[nodiscard]] constexpr inline size_t grow_capacity(size_t capacity) noexcept {
    return (capacity < MIN_CAPACIY)  //
               ? MIN_CAPACIY
               : (capacity * CAPACITY_SCALE_FACTOR);
}

gsl::owner<void*>
reallocate_impl(gsl::owner<void*> pointer, size_t old_size, size_t new_size);

template <typename T>
[[nodiscard]] constexpr inline gsl::owner<T*>
reallocate(gsl::owner<T*> pointer, size_t old_size, size_t new_size) {
    return static_cast<gsl::owner<T*>>(reallocate_impl(
        pointer,
        static_cast<size_t>(sizeof(T)) * old_size,
        static_cast<size_t>(sizeof(T)) * new_size
    ));
}

template <typename T>
[[nodiscard]] constexpr gsl::owner<T*>
grow_array(gsl::owner<T*> pointer, size_t old_size, size_t new_size) {
    return reallocate(pointer, old_size, new_size);
}

template <typename T>
constexpr void free_array(gsl::owner<T*> pointer, size_t old_size) {
    (void)reallocate(pointer, old_size, 0);
}

}  // namespace tx
