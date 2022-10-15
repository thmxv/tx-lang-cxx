#pragma once

#include "tx/hash.hxx"
#include "tx/object.hxx"
#include "tx/memory.hxx"
#include "tx/vm.hxx"

#include <compare>

namespace tx {

// inline constexpr void ObjString::destroy(VM& tvm) noexcept {
//     if (data_ptr != nullptr) { free_array(tvm, data_ptr, length); }
//     data_ptr = nullptr;
// }

[[nodiscard]] inline constexpr std::partial_ordering
operator<=>(const Obj& lhs, const Obj& rhs) noexcept {
    if (lhs.type != rhs.type) { return std::partial_ordering::unordered; }
    switch (lhs.type) {
        using enum ObjType;
        case STRING: return &lhs.as<ObjString>() <=> &rhs.as<ObjString>();
    }
    unreachable();
}
[[nodiscard]] inline constexpr bool
operator==(const Obj& lhs, const Obj& rhs) noexcept {
    if (lhs.type != rhs.type) { return false; }
    switch (lhs.type) {
        using enum ObjType;
        case STRING: return lhs.as<ObjString>() == rhs.as<ObjString>();
    }
    unreachable();
}

[[nodiscard]] inline constexpr std::partial_ordering
operator<=>(const ObjString& lhs, const ObjString& rhs) noexcept {
    return std::string_view(lhs) <=> std::string_view(rhs);
}

[[nodiscard]] inline constexpr bool
operator==(const ObjString& lhs, const ObjString& rhs) noexcept {
    return std::string_view(lhs) == std::string_view(rhs);
}

template <typename T, typename... Args>
T* allocate_object(VM& tvm, size_t extra, Args&&... args) noexcept {
    // T* object_ptr = reallocate<T>(tvm, nullptr, 0, 1);
    T* object_ptr = static_cast<T*>(reallocate_impl(
        tvm,
        nullptr,
        0,
        static_cast<size_t>(sizeof(T)) + extra,
        alignof(T)
    ));
    object_ptr = std::construct_at<T>(object_ptr, std::forward<Args>(args)...);
    object_ptr->next_object = tvm.objects;
    tvm.objects = object_ptr;
    // if constexpr (DEBUG_LOG_GC) {
    //     fmt::print(
    //         FMT_STRING("{} allocate {:d} for {}\n"),
    //         fmt::ptr(ptr),
    //         sizeof(T),
    //         object->type
    //     );
    // }
    return object_ptr;
}

// constexpr
// inline ObjString*
// allocate_string(VM& tvm, char* chars, size_t length, u32 hash) noexcept {
//     auto* string = allocate_object<ObjString>(tvm);
//     string->length = length;
//     string->data_ptr = chars;
//     string->hash = hash;
//     tvm.strings.set(tvm, string, Value());
//     // XXX push(Value{string});
//     // tvm.strings[string] = Value{};
//     // XXX pop();
//     return string;
// }

inline ObjString* make_string(VM& tvm, size_t length) {
    auto* string = allocate_object<ObjString>(tvm, length);
    string->length = length;
    return string;
}

// constexpt
inline ObjString* copy_string(VM& tvm, std::string_view strv) {
    auto hash = Hash<std::string_view>()(strv);
    auto* interned = tvm.strings.find_string(strv, hash);
    if (interned != nullptr) { return interned; }
    auto len = static_cast<size_t>(strv.length());
    // char* heap_chars = allocate<char>(tvm, len);
    // std::memcpy(heap_chars, strv.data(), strv.length());
    // return allocate_string(tvm, heap_chars, len, hash);
    ObjString* string = make_string(tvm, len);
    std::memcpy(&string->data[0], strv.data(), strv.length());
    return string;
}

}  // namespace tx
