#pragma once

#include "memory.hxx"
#include "vm.hxx"

#include <gsl/gsl>

#include <cassert>
#include <memory>

namespace tx {

// TODO: A lot depends on making allocation constexpr
// It should be C++23 compatible but std libs might trail behind

[[nodiscard]] inline constexpr size_t grow_capacity(size_t capacity) noexcept {
    return (capacity < MIN_CAPACIY)  //
               ? MIN_CAPACIY
               : (capacity * CAPACITY_SCALE_FACTOR);
}

// pmr::polymorphic_allocator does not provide a reallocate() function :(
// constexpr
inline gsl::owner<void*> allocator_reallocate(
    Allocator alloc,
    gsl::owner<void*> pointer,
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    size_t old_size,
    size_t new_size,
    size_t alignment
) {
    auto* result = alloc.allocate_bytes(
        static_cast<std::size_t>(new_size),
        static_cast<std::size_t>(alignment)
    );
    if (pointer != nullptr) {
        std::copy_n(
            static_cast<std::byte*>(pointer),
            old_size,
            static_cast<std::byte*>(result)
        );
        alloc.deallocate_bytes(
            pointer,
            static_cast<std::size_t>(old_size),
            static_cast<std::size_t>(alignment)
        );
    }
    return result;
}

// constexpr
inline gsl::owner<void*> reallocate_impl(
    VM& tvm,
    gsl::owner<void*> pointer,
    size_t old_size,
    size_t new_size,
    size_t alignment
) {
    // NOLINTNEXTLINE(*-decay)
    assert(old_size >= 0);
    // NOLINTNEXTLINE(*-decay)
    assert(new_size >= 0);
    if (new_size == 0 and pointer != nullptr) {
        tvm.get_allocator().deallocate_bytes(
            pointer,
            static_cast<std::size_t>(old_size),
            static_cast<std::size_t>(alignment)
        );
        return nullptr;
    }
    gsl::owner<void*> result = allocator_reallocate(
        tvm.get_allocator(),
        pointer,
        old_size,
        new_size,
        alignment
    );
    if (result == nullptr) { report_and_abort("Out of memory."); }
    return result;
}

}  // namespace tx
