#pragma once

#include "tx/table.hxx"
//
#include "tx/object.hxx"

#include <string_view>

namespace tx {

// TODO: move to hash_map 
[[nodiscard]] inline constexpr ObjString*
Table::find_string(std::string_view str_v, u32 hash) noexcept {
    if (count == 0) { return nullptr; }
    u32 index = hash % static_cast<u32>(capacity);
    while (true) {
        const Entry& entry = *std::next(data_ptr, static_cast<gsl::index>(index));
        if (is_entry_empty(entry)) {
            if (!is_entry_tombstone(entry)) { return nullptr; }
        } else if ( //
                entry.first->hash == hash
                && std::string_view(*entry.first) == str_v
        ) {
            return entry.first;
        }
        index = (index + 1) % static_cast<u32>(capacity);
    }
    return nullptr;
}

}  // namespace tx
