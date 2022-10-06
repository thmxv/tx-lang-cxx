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
//         //     case tx::ValueType::NIL:
//         //         return fmt::format_to(ctx.out(), "nil");
//         //     case tx::ValueType::BOOL:
//         //         return fmt::format_to(ctx.out(), "{}", value.as_bool());
//         //     case tx::ValueType::NUMBER:
//         //         return fmt::format_to(ctx.out(), "{}", value.as_number());
//         //     case tx::ValueType::OBJECT:
//         //         return fmt::format_to(ctx.out(), "{}", value.as_object());
//         // }
//         // __builtin_unreachable();
//         return fmt::format_to(ctx.out(), "{}", value);
//     }
// };

