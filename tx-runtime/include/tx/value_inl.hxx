#pragma once

#include "tx/value.hxx"

#include "tx/object.hxx"

#include <compare>

namespace tx {

[[nodiscard]] constexpr std::partial_ordering
operator<=>(const Value& lhs, const Value& rhs) noexcept {
    if (lhs.type != rhs.type) { return std::partial_ordering::unordered; }
    switch (lhs.type) {
        using enum ValueType;
        case NONE:
        case NIL: return std::strong_ordering::equal;
        case BOOL: return std::strong_order(lhs.as_bool(), rhs.as_bool());
        case INT: return lhs.as_int() <=> rhs.as_int();
        case FLOAT: return lhs.as_float() <=> rhs.as_float();
        case CHAR: return lhs.as_char() <=> rhs.as_char();
        case OBJECT: return lhs.as_object() <=> rhs.as_object();
    }
    unreachable();
}


}  // namespace tx
