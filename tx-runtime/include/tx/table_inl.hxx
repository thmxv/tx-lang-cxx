#pragma once

#include "tx/table.hxx"
//
#include "tx/object.hxx"

#include <string_view>

namespace tx {

[[nodiscard]] inline constexpr ObjString*
Table::find_string(std::string_view str_v, u32 hash) noexcept {
    if (count == 0) { return nullptr; }
    std::size_t index = hash % static_cast<std::size_t>(capacity);
    while (true) {
        const Entry& entry = data_ptr[index];
        if (is_entry_empty(entry)) {
            if (!is_entry_tombstone(entry)) { return nullptr; }
        } else if (entry.first->hash == hash && std::string_view(*entry.first) == str_v) {
            return entry.first;
        }
        index = (index + 1) % static_cast<std::size_t>(capacity);
    }
    return nullptr;
}

}  // namespace tx
