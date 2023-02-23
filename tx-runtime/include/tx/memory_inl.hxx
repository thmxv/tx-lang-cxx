#pragma once

#include "tx/common.hxx"
#include "tx/compiler.hxx"
#include "tx/memory.hxx"

#include "tx/vm.hxx"

#include <gsl/gsl>

#include <cassert>
#include <memory>

namespace tx {

// TODO: A lot depends on making allocation constexpr
// It should be C++23 compatible but std libs might trail behind

inline constexpr size_t GC_HEAP_GROW_FACTOR = 2;

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
    size_t old_size,
    size_t new_size,
    size_t alignment
) noexcept {
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

template <bool TRIGGER_GC>
// constexpr
inline void* reallocate_impl(
    VM& tvm,
    void* pointer,
    size_t old_size,
    size_t new_size,
    size_t alignment
) noexcept {
    if constexpr (TRIGGER_GC) {
        tvm.bytes_allocated += new_size - old_size;
        if (new_size > old_size) {
            if constexpr (IS_DEBUG_BUILD) {
                collect_garbage(tvm);
            } else {
                if (tvm.bytes_allocated > tvm.next_gc) {
                    tvm.next_gc *= GC_HEAP_GROW_FACTOR;
                    collect_garbage(tvm);
                }
            }
        }
    }

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
    if constexpr (HAS_DEBUG_FEATURES) {
        if (tvm.get_options().trace_gc) {
            fmt::print(
                FMT_STRING("{} free type {} {}\n"),
                fmt::ptr(object),
                to_underlying(object->type),
                *object
            );
        }
    }
    switch (object->type) {
        using enum Obj::ObjType;
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

inline constexpr void free_objects(VM& tvm, Obj* objects) noexcept {
    Obj* object = objects;
    while (object != nullptr) {
        Obj* next = object->next_object;
        object->next_object = nullptr;
        free_object(tvm, object);
        object = next;
    }
}

inline constexpr void mark_object(VM& tvm, Obj* obj) noexcept {
    if (obj == nullptr) { return; }
    if (obj->is_marked) { return; }
    if constexpr (HAS_DEBUG_FEATURES) {
        if (tvm.get_options().trace_gc) {
            fmt::print(FMT_STRING("{} mark {}\n"), fmt::ptr(obj), Value(obj));
        }
    }
    obj->is_marked = true;
    tvm.gray_stack.emplace_back(tvm, obj);
}

inline constexpr void mark_value(VM& tvm, const Value& value) noexcept {
    if (value.is_object()) { mark_object(tvm, &value.as_object()); }
}

inline constexpr void mark_table(VM& tvm, const ValueMap& table) noexcept {
    for (const auto& entry : table) {
        mark_value(tvm, entry.first);
        mark_value(tvm, entry.second);
    }
}

inline constexpr void mark_compiler_roots(VM& tvm) noexcept {
    if (tvm.parser != nullptr) {
        mark_value(tvm, tvm.parser->previous.value);
        mark_value(tvm, tvm.parser->current.value);
        Compiler* compiler = tvm.parser->current_compiler;
        while (compiler != nullptr) {
            mark_object(tvm, compiler->function);
            compiler = compiler->enclosing;
        }
    }
}

inline constexpr void mark_array(VM& tvm, const ValueArray& array) noexcept {
    for (const auto& value : array) { mark_value(tvm, value); }
}

inline constexpr void mark_roots(VM& tvm) noexcept {
    mark_array(tvm, tvm.stack);
    for (auto& frame : tvm.frames) { mark_object(tvm, &frame.closure); }
    for (ObjUpvalue* upvalue = tvm.open_upvalues; upvalue != nullptr;
         upvalue = upvalue->next_upvalue) {
        mark_object(tvm, upvalue);
    }
    mark_array(tvm, tvm.global_values);
    mark_table(tvm, tvm.global_indices);
    mark_compiler_roots(tvm);
}

inline constexpr void blacken_object(VM& tvm, Obj* obj) noexcept {
    if constexpr (HAS_DEBUG_FEATURES) {
        if (tvm.get_options().trace_gc) {
            fmt::print(
                FMT_STRING("{} blacken {}\n"),
                fmt::ptr(obj),
                Value(obj)
            );
        }
    }
    switch (obj->type) {
        using enum Obj::ObjType;
        case CLOSURE: {
            auto& closure = obj->as<ObjClosure>();
            mark_object(tvm, &closure.function);
            for (const auto& upvalue : closure.upvalues) {
                mark_object(tvm, upvalue);
            }
            break;
        }
        case FUNCTION: {
            const auto& function = obj->as<ObjFunction>();
            mark_object(tvm, function.name);
            mark_array(tvm, function.chunk.constants);
            break;
        }
        case UPVALUE: mark_value(tvm, obj->as<ObjUpvalue>().closed); break;
        case NATIVE:
        case STRING: break;
    }
}

inline constexpr void trace_references(VM& tvm) noexcept {
    while (!tvm.gray_stack.empty()) {
        auto* object = tvm.gray_stack.back();
        tvm.gray_stack.pop_back();
        blacken_object(tvm, object);
    }
}

inline constexpr void sweep(VM& tvm) noexcept {
    Obj* previous = nullptr;
    Obj* object = tvm.objects;
    while (object != nullptr) {
        if (object->is_marked) {
            object->is_marked = false;
            previous = object;
            object = object->next_object;
        } else {
            Obj* unreached = object;
            object = object->next_object;
            if (previous != nullptr) {
                previous->next_object = unreached->next_object;
            } else {
                tvm.objects = unreached->next_object;
            }
            unreached->next_object = nullptr;
            free_object(tvm, unreached);
        }
    }
}

inline constexpr void
table_remove_white(VM& /*tvm*/, ValueMap& table) noexcept {
    for (auto& entry : table) {
        if (!entry.first.as_object().is_marked) { table.erase(entry.first); }
    }
}

inline constexpr void collect_garbage(VM& tvm) noexcept {
    if constexpr (HAS_DEBUG_FEATURES) {
        if (tvm.get_options().trace_gc) {
            fmt::print(FMT_STRING("-- GC begin\n"));
        }
    }
    auto before = tvm.bytes_allocated;
    mark_roots(tvm);
    trace_references(tvm);
    table_remove_white(tvm, tvm.strings);
    sweep(tvm);
    if constexpr (HAS_DEBUG_FEATURES) {
        if (tvm.get_options().trace_gc) {
            fmt::print(
                FMT_STRING(
                    "-- GC end\n"
                    "   Collected {:d} bytes (from {} to {}) next at {}\n"
                ),
                before - tvm.bytes_allocated,
                before,
                tvm.bytes_allocated,
                tvm.next_gc
            );
        }
    }
}

}  // namespace tx
