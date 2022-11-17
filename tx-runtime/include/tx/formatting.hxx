#pragma once

#include "tx/unicode.hxx"
#include "tx/value.hxx"
#include "tx/object.hxx"

#include <fmt/format.h>
#include <math.h>

#include <cmath>

template <>
struct fmt::formatter<tx::Value> : formatter<string_view> {
    template <typename FormatContext>
    constexpr auto format(const tx::Value value, FormatContext& ctx) noexcept {
        switch (value.type) {
            using enum tx::ValueType;
            case NONE: return fmt::format_to(ctx.out(), "none");
            case NIL: return fmt::format_to(ctx.out(), "nil");
            case BOOL: return fmt::format_to(ctx.out(), "{}", value.as_bool());
            case INT: return fmt::format_to(ctx.out(), "{:d}", value.as_int());
            case FLOAT: {
                // TODO: create utility has_integer_value(float val);
                tx::float_t val = value.as_float();
                tx::float_t int_part = 0.0;
                tx::float_t fract_part = std::modf(val, &int_part);
                bool is_int = (fract_part == 0.0 && !std::isinf(int_part));
                if (is_int) { return fmt::format_to(ctx.out(), "{:.1f}", val); }
                return fmt::format_to(ctx.out(), "{:g}", val);
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
    constexpr auto format(const tx::Obj& obj, FormatContext& ctx) {
        switch (obj.type) {
            // case tx::ObjType::CLOSURE: {
            //     const auto& closure = obj.as<tx::ObjClosure>();
            //     return format_function(*closure.function, ctx);
            // }
            case tx::ObjType::FUNCTION: {
                const auto& function = obj.as<tx::ObjFunction>();
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
