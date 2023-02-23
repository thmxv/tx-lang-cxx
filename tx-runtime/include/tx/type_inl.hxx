#pragma once

#include "tx/type.hxx"
//

#include <algorithm>
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
            assert(rhs.type == Type::FUNCTION); // NOLINT(*-decay)
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
type_check_assign(const TypeSet& src, const TypeSet& dst) noexcept {
    return std::ranges::all_of(src.types, [&](const auto& current_src) {
        return type_check_assign(current_src, dst);
    });
}

[[nodiscard]] inline constexpr bool
type_check_assign(const TypeInfo& src, const TypeSet& dst) noexcept {
    return std::ranges::find_if(
               dst.types,
               [&](const auto& current_dst) {
                   return type_check_assign(src, current_dst);
               }
           )
           != dst.types.end();
}

[[nodiscard]] inline constexpr bool
type_check_assign(const TypeInfo& src, const TypeInfo& dst) noexcept {
    if (dst.type == TypeInfo::Type::ANY) { return true; }
    if (dst == src) { return true; }
    if (dst.type == TypeInfo::Type::FLOAT && src.type == TypeInfo::Type::INT) {
        return true;
    }
    if (dst.type == TypeInfo::Type::FUNCTION
        && src.type == TypeInfo::Type::FUNCTION) {
        if (dst.as_function().parameter_types.size()
            != src.as_function().parameter_types.size()) {
            return false;
        }
        // TODO: Use ranges (C++23)
        // for (const auto pair :
        //      std::views::zip(from.fun.parameter_types, fun.parameter_types))
        for (size_t i = 0; i < dst.as_function().parameter_types.size(); ++i) {
            if (!type_check_assign(
                    src.as_function().parameter_types[i],
                    dst.as_function().parameter_types[i]
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
        case GREATER_EQUAL:
            // FIXME: check operand
            (void)lhs;
            (void)rhs;
            // cppcheck-suppress returnDanglingLifetime
            return {tvm, TypeInfo(TypeInfo::Type::BOOL)};
        case PLUS:
        case MINUS:
        case STAR:
        case SLASH:
            // FIXME: check operand
            (void)lhs;
            (void)rhs;
            // cppcheck-suppress returnDanglingLifetime
            return {tvm, TypeInfo(TypeInfo::Type::FLOAT)};
        default: unreachable();
    }
}

}  // namespace tx
