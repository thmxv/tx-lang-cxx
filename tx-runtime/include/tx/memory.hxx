#pragma once

#include "tx/common.hxx"
#include "tx/type_traits.hxx"

#include <gsl/gsl>

namespace tx {

class VM;

inline constexpr size_t MIN_CAPACIY = 8;
inline constexpr size_t CAPACITY_SCALE_FACTOR = 2;

[[nodiscard]] constexpr size_t grow_capacity(size_t capacity) noexcept;

[[nodiscard]]
// constexpr
void*
reallocate_impl(
    VM& tvm,
    void* pointer,
    size_t old_size,
    size_t new_size,
    size_t alignment
);

template <typename T>
    requires is_trivially_relocatable_v<T>
[[nodiscard]] constexpr T*
reallocate(VM& tvm, T* pointer, size_t old_size, size_t new_size) {
    return static_cast<T*>(reallocate_impl(
        tvm,
        pointer,
        static_cast<size_t>(sizeof(T)) * old_size,
        static_cast<size_t>(sizeof(T)) * new_size,
        alignof(T)
    ));
}

template <typename T>
    requires is_trivially_relocatable_v<T>
[[nodiscard]] constexpr T*
grow_array(VM& tvm, T* pointer, size_t old_size, size_t new_size) {
    return reallocate(tvm, pointer, old_size, new_size);
}

template <typename T>
    requires is_trivially_relocatable_v<T>
constexpr void free_array(VM& tvm, T* pointer, size_t old_size) {
    (void)reallocate(tvm, pointer, old_size, 0);
}

}  // namespace tx
