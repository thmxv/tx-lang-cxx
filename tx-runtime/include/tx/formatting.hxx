#pragma once

#include "tx/object.hxx"
#include "tx/type.hxx"
#include "tx/unicode.hxx"
#include "tx/value.hxx"

#include <fmt/format.h>

namespace fmt {

template <>
struct formatter<tx::Value> : formatter<string_view> {
    template <typename FormatContext>
    constexpr auto format(const tx::Value& value, FormatContext& ctx)
        const noexcept {
        switch (value.type) {
            using enum tx::Value::Type;
            case NONE: return format_to(ctx.out(), "<none>");
            case NIL: return format_to(ctx.out(), "nil");
            case BOOL: return format_to(ctx.out(), "{}", value.as_bool());
            case INT: return format_to(ctx.out(), "{:d}", value.as_int());
            case FLOAT: {
                const auto val = value.as_float();
                return format_to(ctx.out(), "{:#}", val);
            }
            case CHAR: {
                auto char_array = tx::utf8_encode_single(value.as_char());
                std::string_view strv{
                    char_array.cbegin(),
                    gsl::narrow_cast<std::size_t>(char_array.size())};
                return format_to(ctx.out(), "{}", strv);
            }
            case OBJECT: return format_to(ctx.out(), "{}", value.as_object());
        }
        tx::unreachable();
    }
};

template <>
struct formatter<tx::Obj> : formatter<string_view> {
    template <typename FormatContext>
    constexpr auto format_function(
        const tx::ObjFunction& fun,
        FormatContext& ctx
    ) const noexcept {
        if (fun.name == nullptr) { return format_to(ctx.out(), "<script>"); }
        auto result = std::string_view(*fun.name);
        if (result.empty()) { return format_to(ctx.out(), "<fn>"); }
        return format_to(ctx.out(), "<fn {:s}>", result);
    }

    template <typename FormatContext>
    constexpr auto format(const tx::Obj& obj, FormatContext& ctx)
        const noexcept {
        switch (obj.type) {
            using enum tx::Obj::ObjType;
            case CLOSURE: {
                const auto& closure = obj.as<tx::ObjClosure>();
                return format_function(closure.function, ctx);
            }
            case FUNCTION: {
                const auto& function = obj.as<tx::ObjFunction>();
                return format_function(function, ctx);
            }
            case NATIVE: return format_to(ctx.out(), "<native fn>");
            case STRING: {
                const auto& str = obj.as<tx::ObjString>();
                return format_to(ctx.out(), "{:s}", std::string_view(str));
            }
            case UPVALUE: return format_to(ctx.out(), "<upvalue>");
        }
        tx::unreachable();
    }
};

template <>
struct formatter<tx::TypeSet> : formatter<string_view> {
    template <typename FormatContext>
    constexpr auto format(const tx::TypeSet& value, FormatContext& ctx)
        const noexcept -> decltype(ctx.out());
};

template <>
struct formatter<tx::TypeSetArray> : formatter<string_view> {
    template <typename FormatContext>
    constexpr auto format(const tx::TypeSetArray& value, FormatContext& ctx)
        const noexcept -> decltype(ctx.out());
};

template <>
struct formatter<tx::TypeInfo> : formatter<string_view> {
    template <typename FormatContext>
    constexpr auto format(const tx::TypeInfo& value, FormatContext& ctx)
        const noexcept {
        switch (value.type) {
            using enum tx::TypeInfo::Type;
            case NONE: return format_to(ctx.out(), "None");
            case NIL: return format_to(ctx.out(), "Nil");
            case BOOL: return format_to(ctx.out(), "Bool");
            case INT: return format_to(ctx.out(), "Int");
            case FLOAT: return format_to(ctx.out(), "Float");
            case CHAR: return format_to(ctx.out(), "Char");
            case ANY: return format_to(ctx.out(), "Any");
            case FUNCTION:
                return format_to(
                    ctx.out(),
                    FMT_STRING("Fn<<{}>, {}>"),
                    value.as_function().parameter_types,
                    value.as_function().return_type
                );
            case STRING: return format_to(ctx.out(), "Str");
        }
        tx::unreachable();
    }
};

template <typename FormatContext>
constexpr auto formatter<tx::TypeSet>::format(
    const tx::TypeSet& value,
    FormatContext& ctx
) const noexcept -> decltype(ctx.out()) {
    std::string_view sep;
    for (const auto& type_info : value.types) {
        format_to(ctx.out(), FMT_STRING("{}{}"), sep, type_info);
        sep = " or ";
    }
    return ctx.out();
}

template <typename FormatContext>
constexpr auto formatter<tx::TypeSetArray>::format(
    const tx::TypeSetArray& value,
    FormatContext& ctx
) const noexcept -> decltype(ctx.out()) {
    std::string_view sep;
    // format_to(ctx.out(), "[");
    for (const auto& type_info : value) {
        format_to(ctx.out(), FMT_STRING("{}{}"), sep, type_info);
        sep = ", ";
    }
    // format_to(ctx.out(), "]");
    return ctx.out();
}

}  // namespace fmt
