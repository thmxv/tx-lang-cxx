#pragma once

#include "tx/unicode.hxx"
#include "tx/value.hxx"
#include "tx/object.hxx"

#include <fmt/format.h>

template <>
struct fmt::formatter<tx::Value> : formatter<string_view> {
    template <typename FormatContext>
    constexpr auto format(const tx::Value value, FormatContext& ctx) noexcept {
        switch (value.type) {
            using enum tx::Value::Type;
            case NONE: return fmt::format_to(ctx.out(), "<none>");
            case NIL: return fmt::format_to(ctx.out(), "nil");
            case BOOL: return fmt::format_to(ctx.out(), "{}", value.as_bool());
            case INT: return fmt::format_to(ctx.out(), "{:d}", value.as_int());
            case FLOAT: {
                const auto val = value.as_float();
                return fmt::format_to(ctx.out(), "{:#}", val);
            }
            case CHAR: {
                auto char_array = tx::utf8_encode_single(value.as_char());
                std::string_view strv{
                    char_array.cbegin(),
                    gsl::narrow_cast<std::size_t>(char_array.size())};
                return fmt::format_to(ctx.out(), "{}", strv);
            }
            case OBJECT:
                return fmt::format_to(ctx.out(), "{}", value.as_object());
        }
        tx::unreachable();
    }
};

template <>
struct fmt::formatter<tx::Obj> : formatter<string_view> {
    template <typename FormatContext>
    constexpr auto
    format_function(const tx::ObjFunction& fun, FormatContext& ctx) {
        if (fun.name == nullptr) {
            return fmt::format_to(ctx.out(), "<script>");
        }
        auto result = std::string_view(*fun.name);
        if (result.empty()) { return fmt::format_to(ctx.out(), "<fn>"); }
        return fmt::format_to(ctx.out(), "<fn {:s}>", result);
    }

    template <typename FormatContext>
    constexpr auto format(const tx::Obj& obj, FormatContext& ctx) {
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
            case NATIVE: return fmt::format_to(ctx.out(), "<native fn>");
            case STRING: {
                const auto& str = obj.as<tx::ObjString>();
                return fmt::format_to(ctx.out(), "{:s}", std::string_view(str));
            }
            case UPVALUE: return fmt::format_to(ctx.out(), "<upvalue>");
        }
        tx::unreachable();
    }
};
