#pragma once

#include "tx/common.hxx"
#include "tx/object.hxx"
#include "tx/value.hxx"

#include <span>
#include <string_view>

namespace tx {

// TODO switch to murmur3 hasing

constexpr u32 hash_function(const std::span<const char> span) noexcept {
    u32 hash = 2166136261U;  // NOLINT(*-magic-numbers)
    for (const auto byte : span) {
        hash ^= static_cast<u8>(byte);
        hash *= 16777619;  // NOLINT(*-magic-numbers)
    }
    return hash;
}

template <typename Key>
struct Hash;

template <>
struct Hash<char32_t> {
    constexpr u32 operator()(const char32_t& val) const noexcept {
        return hash_function(std::span<const char>(
            static_cast<const char*>(static_cast<const void*>(&val)),
            sizeof(char32_t)
        ));
    }
};

template <>
struct Hash<int_t> {
    constexpr u32 operator()(const int_t& val) const noexcept {
        return hash_function(std::span<const char>(
            static_cast<const char*>(static_cast<const void*>(&val)),
            sizeof(int_t)
        ));
    }
};

template <>
struct Hash<float_t> {
    constexpr u32 operator()(const float_t& val) const noexcept {
        return hash_function(std::span<const char>(
            static_cast<const char*>(static_cast<const void*>(&val)),
            sizeof(float_t)
        ));
    }
};

template <>
struct Hash<std::string_view> {
    constexpr u32 operator()(const std::string_view& strv) const noexcept {
        return hash_function(std::span(strv.data(), strv.length()));
    }
};

template <>
struct Hash<ObjString*> {
    constexpr u32 operator()(const ObjString* const& obj) const noexcept {
        return obj->hash;
    }
};

template <>
struct Hash<Obj> {
    constexpr u32 operator()(Obj const& obj) const noexcept {
        switch (obj.type) {
            using enum ObjType;
            case STRING: return Hash<ObjString*>()(&obj.as<ObjString>());
        }
        unreachable();
    }
};

template <>
struct Hash<Value> {
    constexpr u32 operator()(Value const& val) const noexcept {
        switch (val.type) {
            using enum ValueType;
            case NONE: unreachable();
            case NIL: return 7;                       // NOLINT(*-magic-numbers)
            case BOOL: return val.as_bool() ? 3 : 5;  // NOLINT(*-magic-numbers)
            case CHAR: return Hash<char32_t>()(val.as_char());
            case INT: return Hash<int_t>()(val.as_int());
            case FLOAT: return Hash<float_t>()(val.as_float());
            case OBJECT: return Hash<Obj>()(val.as_object());
        }
        unreachable();
    }
};

}  // namespace tx
