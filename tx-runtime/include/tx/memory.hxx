#pragma once

#include "common.hxx"
#include "type_traits.hxx"

#include <gsl/gsl>

namespace tx {

class VM;

inline constexpr size_t MIN_CAPACIY = 8;
inline constexpr size_t CAPACITY_SCALE_FACTOR = 2;

[[nodiscard]] constexpr size_t grow_capacity(size_t capacity) noexcept;

[[nodiscard]]
// constexpr
gsl::owner<void*>
reallocate_impl(
    VM& tvm,
    gsl::owner<void*> pointer,
    size_t old_size,
    size_t new_size,
    size_t alignment
);

template <typename T>
    requires is_trivially_relocatable_v<T>
[[nodiscard]] constexpr gsl::owner<T*>
reallocate(VM& tvm, gsl::owner<T*> pointer, size_t old_size, size_t new_size) {
    return static_cast<gsl::owner<T*>>(reallocate_impl(
        tvm,
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        const_cast<gsl::owner<std::remove_cv_t<T>*>>(pointer),
        static_cast<size_t>(sizeof(T)) * old_size,
        static_cast<size_t>(sizeof(T)) * new_size,
        alignof(T)
    ));
}

template <typename T>
    requires is_trivially_relocatable_v<T>
[[nodiscard]] constexpr gsl::owner<T*>
grow_array(VM& tvm, gsl::owner<T*> pointer, size_t old_size, size_t new_size) {
    return reallocate(tvm, pointer, old_size, new_size);
}

template <typename T>
    requires is_trivially_relocatable_v<T>
constexpr void free_array(VM& tvm, gsl::owner<T*> pointer, size_t old_size) {
    (void)reallocate(tvm, pointer, old_size, 0);
}

}  // namespace tx
