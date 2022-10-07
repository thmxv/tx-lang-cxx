#pragma once

#include "common.hxx"
#include "dyn_array.hxx"

#include <fmt/format.h>

namespace tx {

    using Value = double;
    using ValueArray = DynArray<Value, size_t>;

}

// template<>
// struct fmt::formatter<tx::Value> : formatter<string_view> {
//
//     template <typename FormatContext>
//     auto format(const tx::Value value, FormatContext& ctx) {
//         // switch (value.type) {
//         //     using enum tx::ValueType;
//         //     case NIL:
//         //         return fmt::format_to(ctx.out(), "nil");
//         //     case BOOL:
//         //         return fmt::format_to(ctx.out(), "{}", value.as_bool());
//         //     case NUMBER:
//         //         return fmt::format_to(ctx.out(), "{}", value.as_number());
//         //     case OBJECT:
//         //         return fmt::format_to(ctx.out(), "{}", value.as_object());
//         // }
//         // unreachable();
//         return fmt::format_to(ctx.out(), "{}", value);
//     }
// };

