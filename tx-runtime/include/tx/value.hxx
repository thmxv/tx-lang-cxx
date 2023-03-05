#pragma once

#include "tx/common.hxx"
#include "tx/dyn_array.hxx"
#include "tx/type_traits.hxx"
#include "tx/utils.hxx"

namespace tx {

struct Obj;

struct ValNone {};
struct ValNil {};

inline constexpr ValNone val_none;
inline constexpr ValNil val_nil;

struct Value {
    static constexpr bool IS_TRIVIALLY_RELOCATABLE = true;

    enum class Type {
        NONE,
        NIL,
        BOOL,
        INT,
        FLOAT,
        CHAR,
        OBJECT,
    };

    Type type;
    union {
        bool boolean;
        int_t integer;
        float_t scalar;
        char32_t chr;
        Obj* obj = nullptr;
    } as;

    constexpr Value() noexcept = delete;

    constexpr explicit Value(const ValNone& /*tag*/) noexcept
            : type(Type::NONE) {}
    constexpr explicit Value(const ValNil& /*tag*/) noexcept

            : type(Type::NIL) {}

    constexpr explicit Value(bool val) noexcept
            : type(Type::BOOL)
            , as{.boolean = val} {}

    constexpr explicit Value(int_t val) noexcept
            : type(Type::INT)
            , as{.integer = val} {}

    constexpr explicit Value(float_t val) noexcept
            : type(Type::FLOAT)
            , as{.scalar = val} {}

    constexpr explicit Value(char32_t val) noexcept
            : type(Type::CHAR)
            , as{.chr = val} {}

    constexpr explicit Value(Obj* val) noexcept
            : type(Type::OBJECT)
            , as{.obj = val} {}

    [[nodiscard]] constexpr bool as_bool() const noexcept {
        assert(is_bool());
        // NOLINTNEXTLINE(*-union-access)
        return as.boolean;
    }

    [[nodiscard]] constexpr int_t as_int() const noexcept {
        assert(is_int());
        // NOLINTNEXTLINE(*-union-access)
        return as.integer;
    }

    [[nodiscard]] constexpr float_t as_float() const noexcept {
        assert(is_float());
        // NOLINTNEXTLINE(*-union-access)
        return as.scalar;
    }

    [[nodiscard]] constexpr float_t as_float_force() const noexcept {
        assert(is_int() || is_float());
        return type == Type::INT ? static_cast<float_t>(as_int()) : as_float();
    }

    [[nodiscard]] constexpr char32_t as_char() const noexcept {
        assert(is_char());
        // NOLINTNEXTLINE(*-union-access)
        return as.chr;
    }

    [[nodiscard]] constexpr Obj& as_object() const noexcept {
        assert(is_object());
        // NOLINTNEXTLINE(*-union-access)
        return *as.obj;
    }

    [[nodiscard]] constexpr bool is_none() const noexcept {
        return type == Type::NONE;
    }

    [[nodiscard]] constexpr bool is_nil() const noexcept {
        return type == Type::NIL;
    }

    [[nodiscard]] constexpr bool is_bool() const noexcept {
        return type == Type::BOOL;
    }

    [[nodiscard]] constexpr bool is_int() const noexcept {
        return type == Type::INT;
    }

    [[nodiscard]] constexpr bool is_float() const noexcept {
        return type == Type::FLOAT;
    }

    [[nodiscard]] constexpr bool is_number() const noexcept {
        return type == Type::FLOAT || type == Type::INT;
    }

    [[nodiscard]] constexpr bool is_char() const noexcept {
        return type == Type::CHAR;
    }

    [[nodiscard]] constexpr bool is_object() const noexcept {
        return type == Type::OBJECT;
    }

    [[nodiscard]] constexpr bool is_falsey() const noexcept {
        return is_nil() || (is_bool() && !as_bool());
    }

    friend constexpr std::partial_ordering
    operator<=>(const Value& lhs, const Value& rhs) noexcept;

    [[nodiscard]] friend constexpr bool
    operator==(const Value& lhs, const Value& rhs) noexcept {
        if (lhs.type != rhs.type) { return false; }
        switch (lhs.type) {
            using enum Value::Type;
            case NONE:
            case NIL: return true;
            case BOOL: return lhs.as_bool() == rhs.as_bool();
            case INT: return lhs.as_int() == rhs.as_int();
            case FLOAT: return lhs.as_float() == rhs.as_float();
            case CHAR: return lhs.as_char() == rhs.as_char();
            case OBJECT: return &lhs.as_object() == &rhs.as_object();
        }
        unreachable();
    }
};

using ValueArray = DynArray<Value>;
// using ConstValueArray = DynArray<const Value>;

}  // namespace tx
