#pragma once

#include "tx/type.hxx"
#include <bits/ranges_algo.h>
//
#include <algorithm>
#include <optional>
#include <ranges>

namespace tx {

// NOLINTNEXTLINE(misc-no-recursion)
inline constexpr void TypeInfoFunction::destroy(VM& tvm) noexcept {
    for (auto& ptype : parameter_types) { ptype.destroy(tvm); }
    parameter_types.destroy(tvm);
    return_type.destroy(tvm);
}

inline constexpr TypeInfoFunction TypeInfoFunction::copy(VM& tvm
) const noexcept {
    return TypeInfoFunction{parameter_types.copy(tvm), return_type.copy(tvm)};
}

inline constexpr TypeInfo::TypeInfo() noexcept : type() {}

inline constexpr TypeInfo::TypeInfo(Type kind) noexcept : type(kind) {}

inline constexpr TypeInfo::TypeInfo(TypeInfoFunction&& func) noexcept
        : type(Type::FUNCTION) {
    std::construct_at(&as_function(), std::move(func));
}

inline constexpr TypeInfo::TypeInfo(TypeInfo&& other) noexcept
        : type(other.type) {
    switch (type) {
        using enum Type;
        case FUNCTION:
            std::construct_at(&as_function(), std::move(other.as_function()));
            break;
        default: break;
    }
}

inline constexpr TypeInfo& TypeInfo::operator=(TypeInfo&& rhs) noexcept {
    using std::swap;
    switch (type) {
        using enum Type;
        case FUNCTION:
            // FIXME: call move constructor if not FUNCTION
            assert(rhs.type == Type::FUNCTION);  // NOLINT(*-decay)
            swap(as_function(), rhs.as_function());
            break;
        default: break;
    }
    swap(type, rhs.type);
    return *this;
}

inline constexpr TypeInfo::~TypeInfo() noexcept {
    switch (type) {
        using enum Type;
        case FUNCTION: std::destroy_at(&as_function()); break;
        default: break;
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
inline constexpr void TypeInfo::destroy(VM& tvm) noexcept {
    switch (type) {
        using enum Type;
        case FUNCTION: as_function().destroy(tvm); break;
        default: break;
    }
}

inline constexpr TypeInfo TypeInfo::copy(VM& tvm) const noexcept {
    TypeInfo result;
    result.type = type;
    switch (type) {
        case Type::ANY:
        case Type::BOOL:
        case Type::CHAR:
        case Type::FLOAT:
        case Type::INT:
        case Type::NIL:
        case Type::NONE:
        case Type::STRING: break;
        case Type::FUNCTION:
            result.as_function() = as_function().copy(tvm);
            break;
    }
    return result;
}

[[nodiscard]] inline constexpr bool
operator==(const TypeInfo& lhs, const TypeInfo& rhs) noexcept {
    if (lhs.type != rhs.type) { return false; }
    switch (lhs.type) {
        using enum TypeInfo::Type;
        case FUNCTION: return lhs.as_function() == rhs.as_function();
        default: return true;
    }
}

// NOLINTNEXTLINE(misc-no-recursion)
inline constexpr void TypeSet::destroy(VM& tvm) noexcept {
    for (auto& type : types) { type.destroy(tvm); }
    types.destroy(tvm);
}

inline constexpr TypeSet TypeSet::copy(VM& tvm) const noexcept {
    return TypeSet(types.copy(tvm));
}

inline constexpr void TypeSet::add(VM& tvm, TypeInfo&& val) noexcept {
    if (std::ranges::find(types, val) != types.end()) { return; }
    types.push_back(tvm, std::move(val));
}

inline constexpr void
TypeSet::move_all_from(VM& tvm, TypeSet&& other) noexcept {
    types.reserve(tvm, types.size() + other.types.size());
    for (auto&& val : other.types) { types.push_back_unsafe(std::move(val)); }
}

[[nodiscard]] inline constexpr bool TypeSet::contains(const TypeInfo& val
) const noexcept {
    return std::ranges::find(types, val) != types.end();
}

inline constexpr bool
operator==(const TypeSet& lhs, const TypeSet& rhs) noexcept {
    if (lhs.types.size() != rhs.types.size()) { return false; }
    return std::ranges::all_of(lhs.types, [&](const auto& current) {
        return rhs.contains(current);
    });
}

[[nodiscard]] inline constexpr bool
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
type_check_assign(const TypeSet& lhs, const TypeSet& rhs) noexcept {
    return std::ranges::all_of(rhs.types, [&](const auto& current_rhs) {
        return type_check_assign(lhs, current_rhs);
    });
}

[[nodiscard]] inline constexpr bool
type_check_assign(const TypeSet& lhs, const TypeInfo& rhs) noexcept {
    return std::ranges::find_if(
               lhs.types,
               [&](const auto& current_lhs) {
                   return type_check_assign(current_lhs, rhs);
               }
           )
           != lhs.types.end();
}

[[nodiscard]] inline constexpr bool
type_check_assign(const TypeInfo& lhs, const TypeInfo& rhs) noexcept {
    if (lhs.type == TypeInfo::Type::ANY) { return true; }
    if (lhs == rhs) { return true; }
    if (lhs.type == TypeInfo::Type::FLOAT && rhs.type == TypeInfo::Type::INT) {
        return true;
    }
    if (lhs.type == TypeInfo::Type::FUNCTION
        && rhs.type == TypeInfo::Type::FUNCTION) {
        if (lhs.as_function().parameter_types.size()
            != rhs.as_function().parameter_types.size()) {
            return false;
        }
        // TODO: Use ranges (C++23)
        // for (const auto pair :
        //      std::views::zip(from.fun.parameter_types, fun.parameter_types))
        for (size_t i = 0; i < lhs.as_function().parameter_types.size(); ++i) {
            if (!type_check_assign(
                    rhs.as_function().parameter_types[i],
                    lhs.as_function().parameter_types[i]
                )) {
                return false;
            }
        }
        return true;
    }
    return false;
}

[[nodiscard]] constexpr TypeSet type_check_binary(
    VM& tvm,
    TokenType token_type,
    const TypeSet& lhs,
    const TypeSet& rhs
) noexcept {
    switch (token_type) {
        using enum TokenType;
        case BANG_EQUAL:
        case EQUAL_EQUAL:
            // cppcheck-suppress returnDanglingLifetime
            return {tvm, TypeInfo(TypeInfo::Type::BOOL)};
        case LEFT_CHEVRON:
        case LESS_EQUAL:
        case RIGHT_CHEVRON:
        case GREATER_EQUAL: {
            if (!type_check_comparison(lhs, rhs)) { return {}; }
            // cppcheck-suppress returnDanglingLifetime
            return {tvm, TypeInfo(TypeInfo::Type::BOOL)};
        }
        case PLUS:
        case MINUS:
        case STAR:
        case SLASH: {
            return type_check_arithmetic(tvm, lhs, rhs);
        }
        default: unreachable();
    }
}

[[nodiscard]] constexpr bool
type_check_comparison(const TypeSet& lhs, const TypeSet& rhs) noexcept {
    return std::ranges::all_of(lhs.types, [&](const auto& current_lhs) {
        return type_check_comparison(current_lhs, rhs);
    });
}

[[nodiscard]] constexpr bool
type_check_comparison(const TypeInfo& lhs, const TypeSet& rhs) noexcept {
    return std::ranges::all_of(rhs.types, [&](const auto& current_rhs) {
        return type_check_comparison(lhs, current_rhs);
    });
}

[[nodiscard]] constexpr bool
type_check_comparison(const TypeInfo& lhs, const TypeInfo& rhs) noexcept {
    return lhs.is_number() && rhs.is_number();
}

[[nodiscard]] constexpr TypeSet type_check_arithmetic(
    VM& tvm,
    const TypeSet& lhs,
    const TypeSet& rhs
) noexcept {
    TypeSet result{};
    for (const TypeInfo& current_lhs : lhs.types) {
        auto type_set = type_check_arithmetic(tvm, current_lhs, rhs);
        if (type_set.is_empty()) { return {}; }
        result.move_all_from(tvm, std::move(type_set));
        // cppcheck-suppress[accessMoved]
        type_set.destroy(tvm);
    }
    return result;
}

[[nodiscard]] constexpr TypeSet type_check_arithmetic(
    VM& tvm,
    const TypeInfo& lhs,
    const TypeSet& rhs
) noexcept {
    TypeSet result{};
    for (const TypeInfo& current_rhs : rhs.types) {
        auto type = type_check_arithmetic(lhs, current_rhs);
        if (type.is<TypeInfo::Type::NONE>()) {
            result.destroy(tvm);
            return {};
        }
        result.add(tvm, std::move(type));
        // cppcheck-suppress[accessMoved]
        type.destroy(tvm);
    }
    return result;
}

[[nodiscard]] constexpr TypeInfo
type_check_arithmetic(const TypeInfo& lhs, const TypeInfo& rhs) noexcept {
    if (lhs.is<TypeInfo::Type::INT>() && rhs.is<TypeInfo::Type::INT>()) {
        return TypeInfo{TypeInfo::Type::INT};
    }
    if (lhs.is_number() && rhs.is_number()) {
        return TypeInfo{TypeInfo::Type::FLOAT};
    }
    return TypeInfo{TypeInfo::Type::NONE};
}

[[nodiscard]] constexpr TypeSet type_check_call(
    VM& tvm,
    const TypeSet& callee,
    const TypeSetArray& arg_types
) noexcept {
    TypeSet result{};
    for (const auto& current_callee : callee.types) {
        auto type_set = type_check_call(tvm, current_callee, arg_types);
        if (type_set.is_empty()) {
            result.destroy(tvm);
            return {};
        }
        result.move_all_from(tvm, std::move(type_set));
        // cppcheck-suppress[accessMoved]
        type_set.destroy(tvm);
    }
    return result;
}

[[nodiscard]] constexpr TypeSet type_check_call(
    VM& tvm,
    const TypeInfo& callee,
    const TypeSetArray& args_types
) noexcept {
    if (!callee.is<TypeInfo::Type::FUNCTION>()) { return {}; }
    const auto& fun = callee.as_function();
    if (fun.parameter_types.size() != args_types.size()) { return {}; }
    for (i32 i = 0; i < args_types.size(); ++i) {
        const auto& param_types = fun.parameter_types[i];
        const auto& arg_types = args_types[i];
        if (!type_check_assign(param_types, arg_types)) { return {}; }
    }
    return fun.return_type.copy(tvm);
}

}  // namespace tx
