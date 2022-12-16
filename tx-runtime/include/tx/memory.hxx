#pragma once

#include "tx/common.hxx"
#include "tx/type_traits.hxx"

#include <gsl/gsl>

#include <cassert>

namespace tx {

class VM;
struct Obj;

inline constexpr size_t MIN_CAPACIY = 8;
inline constexpr size_t CAPACITY_SCALE_FACTOR = 2;

template <typename T>
[[nodiscard]] T* allocate(VM& tvm, size_t count) noexcept {
    return reallocate_no_reloc<T>(tvm, nullptr, 0, count);
}

template <typename T>
T* free(VM& tvm, T* ptr) noexcept {
    return reallocate_no_reloc<T>(tvm, ptr, 1, 0);
}

[[nodiscard]] constexpr size_t grow_capacity(size_t capacity) noexcept;

template<bool TIGGER_GC=true>
[[nodiscard]]
// constexpr
void* reallocate_impl(
    VM& tvm,
    void* pointer,
    size_t old_size,
    size_t new_size,
    size_t alignment
) noexcept;

template <typename T, bool TRIGGER_GC = true>
    requires is_trivially_relocatable_v<T>
[[nodiscard]] constexpr T*
reallocate(VM& tvm, T* pointer, size_t old_size, size_t new_size) noexcept {
    return static_cast<T*>(reallocate_impl<TRIGGER_GC>(
        tvm,
        pointer,
        static_cast<size_t>(sizeof(T)) * old_size,
        static_cast<size_t>(sizeof(T)) * new_size,
        alignof(T)
    ));
}

template <typename T>
[[nodiscard]] constexpr T* reallocate_no_reloc(
    VM& tvm,
    T* pointer,
    size_t old_size,
    size_t new_size
) noexcept {
    assert((pointer == nullptr && old_size == 0) || (new_size == 0));
    return static_cast<T*>(reallocate_impl(
        tvm,
        pointer,
        static_cast<size_t>(sizeof(T)) * old_size,
        static_cast<size_t>(sizeof(T)) * new_size,
        alignof(T)
    ));
}

template <typename T, bool TRIGGER_GC = true>
    requires is_trivially_relocatable_v<T>
[[nodiscard]] constexpr T*
grow_array(VM& tvm, T* pointer, size_t old_size, size_t new_size) noexcept {
    return reallocate<T, TRIGGER_GC>(tvm, pointer, old_size, new_size);
}

template <typename T>
constexpr void free_array(VM& tvm, T* pointer, size_t old_size) noexcept {
    (void)reallocate_no_reloc(tvm, pointer, old_size, 0);
}

inline constexpr void free_objects(VM& tvm, Obj* objects) noexcept;

constexpr void collect_garbage(VM& tvm) noexcept;

}  // namespace tx
