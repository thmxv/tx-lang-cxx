#pragma once

#include "tx/common.hxx"
#include "tx/dyn_array.hxx"
#include "tx/type_traits.hxx"
#include "tx/utils.hxx"

#include <fmt/format.h>

namespace tx {

enum class ValueType {
    NIL,
    BOOL,
    INT,
    FLOAT,
    OBJECT,
};

struct Obj;

struct Value {
    ValueType type = ValueType::NIL;
    union {
        bool boolean;
        int_t integer;
        float_t scalar;
        Obj* obj = nullptr;
    } as;

    constexpr Value() noexcept = default;

    constexpr explicit Value(bool val) noexcept
            : type(ValueType::BOOL)
            , as{.boolean = val} {}

    constexpr explicit Value(int_t val) noexcept
            : type(ValueType::INT)
            , as{.integer = val} {}

    constexpr explicit Value(float_t val) noexcept
            : type(ValueType::FLOAT)
            , as{.scalar = val} {}

    constexpr explicit Value(Obj* val) noexcept
            : type(ValueType::OBJECT)
            , as{.obj = val} {}

    [[nodiscard]] constexpr bool as_bool() const noexcept { return as.boolean; }

    [[nodiscard]] constexpr int_t as_int() const noexcept { return as.integer; }

    [[nodiscard]] constexpr float_t as_float() const noexcept {
        return as.scalar;
    }

    [[nodiscard]] constexpr Obj& as_object() const noexcept { return *as.obj; }

    [[nodiscard]] constexpr bool is_nil() const noexcept {
        return type == ValueType::NIL;
    }

    [[nodiscard]] constexpr bool is_bool() const noexcept {
        return type == ValueType::BOOL;
    }

    [[nodiscard]] constexpr bool is_int() const noexcept {
        return type == ValueType::INT;
    }

    [[nodiscard]] constexpr bool is_float() const noexcept {
        return type == ValueType::FLOAT;
    }

    [[nodiscard]] constexpr bool is_object() const noexcept {
        return type == ValueType::OBJECT;
    }

    [[nodiscard]] constexpr bool is_falsey() const noexcept {
        return is_nil() || (is_bool() && !as_bool());
    }

    [[nodiscard]] friend constexpr std::partial_ordering
    operator<=>(const Value& lhs, const Value& rhs) noexcept {
        if (lhs.type != rhs.type) { return std::partial_ordering::unordered; }
        switch (lhs.type) {
            case ValueType::NIL: return std::strong_ordering::equal;
            case ValueType::BOOL: return std::strong_order(lhs.as_bool(), rhs.as_bool()); //(lhs.as_bool()) <=> (rhs.as_bool());
            case ValueType::INT: return lhs.as_int() <=> rhs.as_int();
            case ValueType::FLOAT: return lhs.as_float() <=> rhs.as_float();
            case ValueType::OBJECT:
                return &lhs.as_object() <=> &rhs.as_object();
        }
        unreachable();
    }

    [[nodiscard]] friend constexpr bool
    operator==(const Value& lhs, const Value& rhs) noexcept {
        return (lhs <=> rhs) == std::partial_ordering::equivalent;
    }
};

template <>
struct is_trivially_relocatable<Value> : std::__is_bitwise_relocatable<Value> {
    static constexpr value_type value = true;
};

template <>
struct is_trivially_relocatable<const Value>
        : std::__is_bitwise_relocatable<const Value> {
    static constexpr value_type value = true;
};

using ValueArray = DynArray<Value, size_t>;
using ConstValueArray = DynArray<const Value, size_t>;

}  // namespace tx

template <>
struct fmt::formatter<tx::Value> : formatter<string_view> {
    template <typename FormatContext>
    constexpr auto format(const tx::Value value, FormatContext& ctx) noexcept {
        switch (value.type) {
            using enum tx::ValueType;
            case NIL: return fmt::format_to(ctx.out(), "nil");
            case BOOL: return fmt::format_to(ctx.out(), "{}", value.as_bool());
            case INT: return fmt::format_to(ctx.out(), "{}", value.as_int());
            case FLOAT:
                return fmt::format_to(ctx.out(), "{}", value.as_float());
            case OBJECT:
                break;
                // return fmt::format_to(ctx.out(), "{}", value.as_object());
                // return fmt::format_to(ctx.out(), "");
        }
        tx::unreachable();
    }
};
