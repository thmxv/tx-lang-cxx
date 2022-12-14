#pragma once

#include "tx/memory.hxx"

#include "tx/vm.hxx"

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
inline void* allocator_reallocate(
    Allocator alloc,
    void* pointer,
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
            std::min(old_size, new_size),
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
inline void* reallocate_impl(
    VM& tvm,
    void* pointer,
    size_t old_size,
    size_t new_size,
    size_t alignment
) {
    // NOLINTNEXTLINE(*-decay)
    assert(old_size >= 0);
    // NOLINTNEXTLINE(*-decay)
    assert(new_size >= 0);
    if (new_size == 0) {
        tvm.get_allocator().deallocate_bytes(
            pointer,
            static_cast<std::size_t>(old_size),
            static_cast<std::size_t>(alignment)
        );
        return nullptr;
    }
    void* result = allocator_reallocate(
        tvm.get_allocator(),
        pointer,
        old_size,
        new_size,
        alignment
    );
    if (result == nullptr) { report_and_abort("Out of memory."); }
    return result;
}

template <typename T>
    requires std::is_base_of_v<Obj, T>
constexpr void free_object_impl(VM& tvm, T* object) noexcept {
    // TODO: call destroy if exist
    std::destroy_at(object);
    free<T>(tvm, object);
}

inline void free_object(VM& tvm, Obj* object) noexcept {
    // if constexpr (DEBUG_LOG_GC) {
    //     fmt::print(
    //         FMT_STRING("{} free type {}\n"),
    //         fmt::ptr(object),
    //         object->type
    //     );
    // }
    switch (object->type) {
        using enum ObjType;
        case CLOSURE: {
            auto& closure = object->as<ObjClosure>();
            closure.destroy(tvm);
            free_object_impl(tvm, &closure);
            return;
        }
        case FUNCTION: {
            auto& fun = object->as<ObjFunction>();
            fun.destroy(tvm);
            free_object_impl(tvm, &fun);
            return;
        }
        case NATIVE: free_object_impl(tvm, &object->as<ObjNative>()); return;
        case STRING: {
            auto& str = object->as<ObjString>();
            std::destroy_at(&str);
            (void)reallocate_impl(
                tvm,
                &str,
                static_cast<size_t>(sizeof(ObjString))
                    + (str.owns_chars ? str.length : 0),
                0,
                alignof(ObjString)
            );
            return;
        }
        case UPVALUE: free_object_impl(tvm, &object->as<ObjUpvalue>()); return;
    }
    unreachable();
}

inline constexpr void free_objects(VM& tvm, Obj* objects) {
    Obj* object = objects;
    while (object != nullptr) {
        Obj* next = object->next_object;
        object->next_object = nullptr;
        free_object(tvm, object);
        object = next;
    }
}

}  // namespace tx
