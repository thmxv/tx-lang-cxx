#pragma once

#include "tx/hash.hxx"
#include "tx/object.hxx"
#include "tx/memory.hxx"
#include "tx/vm.hxx"

#include <compare>

namespace tx {

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

// constexpt
inline ObjString*
make_string(VM& tvm, bool copy, std::string_view strv) noexcept {
    auto hash = Hash<std::string_view>()(strv);
    auto* interned = tvm.strings.find_string(strv, hash);
    if (interned != nullptr) { return interned; }
    auto len = static_cast<size_t>(strv.length());
    auto* string =
        allocate_object<ObjString>(tvm, copy ? len : 0, copy, strv, hash);
    tvm.strings.set(tvm, string, Value());
    return string;
}

}  // namespace tx
