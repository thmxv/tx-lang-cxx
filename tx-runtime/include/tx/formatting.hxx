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
            using enum tx::ValueType;
            case NONE: return fmt::format_to(ctx.out(), "none");
            case NIL: return fmt::format_to(ctx.out(), "nil");
            case BOOL: return fmt::format_to(ctx.out(), "{}", value.as_bool());
            case INT: return fmt::format_to(ctx.out(), "{}", value.as_int());
            case FLOAT:
                return fmt::format_to(ctx.out(), "{}", value.as_float());
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
    constexpr auto format(const tx::Obj& obj, FormatContext& ctx) {
        switch (obj.type) {
            // case tx::ObjType::CLOSURE: {
            //     const auto& closure = obj.as<tx::ObjClosure>();
            //     return format_function(*closure.function, ctx);
            // }
            case tx::ObjType::FUNCTION: {
                const auto& function = obj.as<tx::ObjFunction>();
                // return format_function(function, ctx);
                return fmt::format_to(
                    ctx.out(),
                    "<fn {:s}>",
                    function.get_name()
                );
            }
            case tx::ObjType::NATIVE:
                return fmt::format_to(ctx.out(), "<native fn>");
            case tx::ObjType::STRING: {
                const auto& str = obj.as<tx::ObjString>();
                return fmt::format_to(ctx.out(), "{:s}", std::string_view(str));
            }
                // case tx::ObjType::UPVALUE:
                //     return fmt::format_to(ctx.out(), "upvalue");
        }
        tx::unreachable();
    }
};
