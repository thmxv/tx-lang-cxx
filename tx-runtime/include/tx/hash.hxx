#pragma once

#include "tx/common.hxx"

#include <string_view>

namespace tx {

// TODO switch to murmur3 hasing

template <typename Key>
struct Hash;

template <>
struct Hash<std::string_view> {
    constexpr u32 operator()(const std::string_view strv) const noexcept {
        // NOLINTNEXTLINE(*-magic-numbers)
        u32 hash = 2166136261U;
        for (auto chr : strv) {
            hash ^= static_cast<u8>(chr);
            // NOLINTNEXTLINE(*-magic-numbers)
            hash *= 16777619;
        }
        return hash;
    }
};

}  // namespace tx
