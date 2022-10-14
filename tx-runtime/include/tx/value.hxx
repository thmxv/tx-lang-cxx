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
    CHAR,
    OBJECT,
};

struct Obj;

struct Value {
    using enum ValueType;

    ValueType type = ValueType::NIL;
    union {
        bool boolean;
        int_t integer;
        float_t scalar;
        char32_t chr;
        Obj* obj = nullptr;
    } as;

    constexpr Value() noexcept = default;

    constexpr explicit Value(bool val) noexcept
            : type(BOOL)
            , as{.boolean = val} {}

    constexpr explicit Value(int_t val) noexcept
            : type(INT)
            , as{.integer = val} {}

    constexpr explicit Value(float_t val) noexcept
            : type(FLOAT)
            , as{.scalar = val} {}

    constexpr explicit Value(char32_t val) noexcept
            : type(CHAR)
            , as{.chr = val} {}

    constexpr explicit Value(Obj* val) noexcept
            : type(OBJECT)
            , as{.obj = val} {}

    // NOLINTNEXTLINE(*-union-access)
    [[nodiscard]] constexpr bool as_bool() const noexcept { return as.boolean; }

    // NOLINTNEXTLINE(*-union-access)
    [[nodiscard]] constexpr int_t as_int() const noexcept { return as.integer; }

    [[nodiscard]] constexpr float_t as_float() const noexcept {
        // NOLINTNEXTLINE(*-union-access)
        return as.scalar;
    }

    [[nodiscard]] constexpr float_t as_float_force() const noexcept {
        return type == INT ? static_cast<float_t>(as_int()) : as_float();
    }

    // NOLINTNEXTLINE(*-union-access)
    [[nodiscard]] constexpr int_t as_char() const noexcept { return as.chr; }

    // NOLINTNEXTLINE(*-union-access)
    [[nodiscard]] constexpr Obj& as_object() const noexcept { return *as.obj; }

    [[nodiscard]] constexpr bool is_nil() const noexcept { return type == NIL; }

    [[nodiscard]] constexpr bool is_bool() const noexcept {
        return type == BOOL;
    }

    [[nodiscard]] constexpr bool is_int() const noexcept { return type == INT; }

    [[nodiscard]] constexpr bool is_float() const noexcept {
        return type == FLOAT;
    }

    [[nodiscard]] constexpr bool is_number() const noexcept {
        return type == FLOAT || type == INT;
    }

    [[nodiscard]] constexpr bool is_char() const noexcept {
        return type == CHAR;
    }

    [[nodiscard]] constexpr bool is_object() const noexcept {
        return type == OBJECT;
    }

    [[nodiscard]] constexpr bool is_falsey() const noexcept {
        return is_nil() || (is_bool() && !as_bool());
    }

    friend constexpr std::partial_ordering
    operator<=>(const Value& lhs, const Value& rhs) noexcept;

    friend constexpr bool
    operator==(const Value& lhs, const Value& rhs) noexcept;
    
};

template <>
struct is_trivially_relocatable<Value> : std::__is_bitwise_relocatable<Value> {
    static constexpr value_type value = true;
};

// template <>
// struct is_trivially_relocatable<const Value>
//         : std::__is_bitwise_relocatable<const Value> {
//     static constexpr value_type value = true;
// };

using ValueArray = DynArray<Value, size_t>;
// using ConstValueArray = DynArray<const Value, size_t>;

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
            case CHAR: return fmt::format_to(ctx.out(), "{}", value.as_char());
            case OBJECT:
                return fmt::format_to(ctx.out(), "{}", value.as_object());
        }
        tx::unreachable();
    }
};
