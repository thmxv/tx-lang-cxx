#pragma once

#include "tx/dyn_array.hxx"
#include "tx/scanner.hxx"
#include "tx/utils.hxx"

namespace tx {

struct TypeInfo;

struct TypeSet {
    static constexpr bool IS_TRIVIALLY_RELOCATABLE = true;

    // NOTE: Needs c++23 for non literal type as non type template param
    // might not even be possible in c++23 because of the recursives type defs
    // HashSet<TypeInfo, TypeInfo{TypeInfo::Type::NONE}> types;
    DynArray<TypeInfo> types;

    constexpr TypeSet() noexcept = default;

    constexpr explicit TypeSet(DynArray<TypeInfo>&& types_) noexcept
            : types(std::move(types_)) {}

    // NOTE: Using C array to work around the lack of support for move
    // semantics by std::initializer_list (std::array does not works)
    template <std::size_t N>
    // NOLINTNEXTLINE(*-c-arrays)
    constexpr TypeSet(VM& tvm, TypeInfo (&&type_infos)[N]) noexcept;

    constexpr void destroy(VM& tvm) noexcept;

    [[nodiscard]] constexpr TypeSet copy(VM& tvm) const noexcept;

    constexpr void add(VM& tvm, TypeInfo&& val) noexcept;
    constexpr void move_all_from(VM& tvm, TypeSet&& other) noexcept;

    [[nodiscard]] constexpr bool contains(const TypeInfo& val) const noexcept;

    [[nodiscard]] constexpr bool is_empty() const noexcept {
        return types.is_empty();
    }

    [[nodiscard]] constexpr bool is_nil() const noexcept;

    friend constexpr bool
    operator==(const TypeSet& lhs, const TypeSet& rhs) noexcept;
};

struct TypeInfoNone {};

using TypeSetArray = DynArray<TypeSet>;

struct TypeInfoFunction {
    TypeSetArray parameter_types;
    TypeSet return_type;

    constexpr void destroy(VM& tvm) noexcept;

    [[nodiscard]] constexpr TypeInfoFunction copy(VM& tvm) const noexcept;

    friend constexpr bool operator==(
        const TypeInfoFunction& lhs,
        const TypeInfoFunction& rhs
    ) noexcept = default;
};

struct TypeInfo {
    enum struct Type {
        NONE,
        ANY,
        NIL,
        BOOL,
        INT,
        FLOAT,
        CHAR,
        FUNCTION,
        STRING,
    };

    static constexpr bool IS_TRIVIALLY_RELOCATABLE = true;

    // NOTE: Cannot use std::variant because it is not a strutural type
    Type type{Type::NONE};
    union {
        TypeInfoNone none{};
        TypeInfoFunction fun;
    };

    constexpr TypeInfo() noexcept;
    constexpr explicit TypeInfo(Type kind) noexcept;
    constexpr explicit TypeInfo(TypeInfoFunction&& func) noexcept;

    constexpr TypeInfo(const TypeInfo&) = delete;
    constexpr TypeInfo(TypeInfo&& other) noexcept;
    constexpr TypeInfo& operator=(const TypeInfo&) = delete;
    constexpr TypeInfo& operator=(TypeInfo&& rhs) noexcept;

    constexpr ~TypeInfo() noexcept;

    constexpr void destroy(VM& tvm) noexcept;

    [[nodiscard]] constexpr TypeInfo copy(VM& tvm) const noexcept;

    template <Type T>
    [[nodiscard]] constexpr bool is() const noexcept {
        return type == T;
    }

    [[nodiscard]] constexpr bool is_number() const noexcept {
        return (type == Type::INT) || (type == Type::FLOAT);
    }

    [[nodiscard]] constexpr TypeInfoFunction& as_function() noexcept {
        assert(is<Type::FUNCTION>());  // NOLINT(*-decay)
        return fun;                    // NOLINT(*-union-access)
    }

    [[nodiscard]] constexpr const TypeInfoFunction& as_function(
    ) const noexcept {
        assert(is<Type::FUNCTION>());  // NOLINT(*-decay)
        return fun;                    // NOLINT(*-union-access)
    }

    friend constexpr bool
    operator==(const TypeInfo& lhs, const TypeInfo& rhs) noexcept;
};

[[nodiscard]] constexpr bool
type_check_assign(const TypeSet& lhs, const TypeSet& rhs) noexcept;
[[nodiscard]] constexpr bool
type_check_assign(const TypeSet& lhs, const TypeInfo& rhs) noexcept;
[[nodiscard]] constexpr bool
type_check_assign(const TypeInfo& lhs, const TypeInfo& rhs) noexcept;

[[nodiscard]] constexpr TypeSet type_check_binary(
    VM& tvm,
    Token::Type token_type,
    const TypeSet& lhs,
    const TypeSet& rhs
) noexcept;

[[nodiscard]] constexpr bool
type_check_comparison(const TypeSet& lhs, const TypeSet& rhs) noexcept;
[[nodiscard]] constexpr bool
type_check_comparison(const TypeInfo& lhs, const TypeSet& rhs) noexcept;
[[nodiscard]] constexpr bool
type_check_comparison(const TypeInfo& lhs, const TypeInfo& rhs) noexcept;

[[nodiscard]] constexpr TypeSet
type_check_arithmetic(VM& tvm, const TypeSet& lhs, const TypeSet& rhs) noexcept;

[[nodiscard]] constexpr TypeSet type_check_arithmetic(
    VM& tvm,
    const TypeInfo& lhs,
    const TypeSet& rhs
) noexcept;

[[nodiscard]] constexpr TypeInfo
type_check_arithmetic(const TypeInfo& lhs, const TypeInfo& rhs) noexcept;

[[nodiscard]] constexpr bool type_check_negate(const TypeSet& rhs) noexcept;

[[nodiscard]] constexpr TypeSet type_check_call(
    VM& tvm,
    const TypeSet& callee,
    const TypeSetArray& arg_types
) noexcept;

[[nodiscard]] constexpr TypeSet type_check_call(
    VM& tvm,
    const TypeInfo& callee,
    const TypeSetArray& args_types
) noexcept;

[[nodiscard]] constexpr bool
type_check_cast(const TypeSet& src, const TypeSet& dst) noexcept;

}  // namespace tx
