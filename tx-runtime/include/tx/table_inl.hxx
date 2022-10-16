#pragma once

#include "tx/table.hxx"
//
#include "tx/object.hxx"

#include <optional>
#include <string_view>

namespace tx {

[[nodiscard]] inline constexpr ObjString*
Table::find_string(std::string_view str_v, u32 hash) noexcept {
    auto* result = find_in_bucket(hash, [&](const auto& entry) {
        return entry.first->hash == hash
               && std::string_view(*entry.first) == str_v;
    });
    if (result == nullptr) { return nullptr; }
    return result->first;
}

}  // namespace tx
