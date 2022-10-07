#pragma once

#include "memory.hxx"
#include "vm.hxx"

#include <gsl/gsl>

#include <memory>

namespace tx {

inline gsl::owner<void*> reallocate_impl(
    VM& tvm,
    gsl::owner<void*> pointer,  //
    size_t /*old_size*/,
    size_t new_size
) {
    (void) tvm;
    if (new_size == 0) {
        // NOLINTNEXTLINE(TODO)
        std::free(pointer);
        return nullptr;
    }
    // NOLINTNEXTLINE(TODO)
    gsl::owner<void*> result = std::realloc(
        pointer,
        static_cast<std::size_t>(new_size)
    );
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    if (result == nullptr) { std::abort(); }
    return result;
}

}  // namespace tx
