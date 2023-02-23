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

    constexpr TypeSet(VM& tvm, TypeInfo&& type_info) noexcept {
        add(tvm, std::move(type_info));
    }

    constexpr void destroy(VM& tvm) noexcept;

    [[nodiscard]] constexpr TypeSet copy(VM& tvm) const noexcept;

    constexpr void add(VM& tvm, TypeInfo&& val) noexcept;
    constexpr void move_all_from(VM& tvm, TypeSet&& other) noexcept;

    [[nodiscard]] constexpr bool contains(const TypeInfo& val) const noexcept;

    [[nodiscard]] constexpr bool is_empty() const noexcept {
        return types.is_empty();
    }

    friend constexpr bool
    operator==(const TypeSet& lhs, const TypeSet& rhs) noexcept;
};

struct TypeInfoNone {};

struct TypeInfoFunction {
    DynArray<TypeSet> parameter_types;
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

    [[nodiscard]] constexpr TypeInfoFunction& as_function() noexcept {
        assert(is<Type::FUNCTION>());  // NOLINT(*-decay)
        return fun;  // NOLINT(*-union-access)
    }

    [[nodiscard]] constexpr const TypeInfoFunction& as_function(
    ) const noexcept {
        assert(is<Type::FUNCTION>());  // NOLINT(*-decay)
        return fun;  // NOLINT(*-union-access)
    }

    friend constexpr bool
    operator==(const TypeInfo& lhs, const TypeInfo& rhs) noexcept;
};

[[nodiscard]] constexpr bool
type_check_assign(const TypeSet& src, const TypeSet& dst) noexcept;

[[nodiscard]] constexpr bool
type_check_assign(const TypeInfo& src, const TypeSet& dst) noexcept;

[[nodiscard]] constexpr bool
type_check_assign(const TypeInfo& src, const TypeInfo& dst) noexcept;

[[nodiscard]] constexpr TypeSet type_check_binary(
    VM& tvm,
    TokenType token_type,
    const TypeSet& lhs,
    const TypeSet& rhs
) noexcept;

[[nodiscard]] constexpr bool
type_check_unary(const TypeInfo& src, const TypeInfo& dst) noexcept;

}  // namespace tx
