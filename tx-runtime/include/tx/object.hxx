#pragma once

#include "tx/common.hxx"
#include "tx/hash.hxx"
#include "tx/type_traits.hxx"
#include "tx/utils.hxx"

#include <gsl/gsl>

#include <gsl/util>
#include <string_view>
#include <type_traits>
#include <cassert>

namespace tx {

class VM;

enum class ObjType : u8 {
    STRING,
};

struct Obj {
    ObjType type;
    // bool is_marked = false;
    gsl::owner<Obj*> next_object = nullptr;

    Obj() = delete;
    constexpr explicit Obj(ObjType typ) noexcept : type(typ) {}

    constexpr Obj(const Obj& other) noexcept = delete;
    constexpr Obj(Obj&& other) noexcept = delete;

    constexpr ~Obj() noexcept {
        // NOLINTNEXTLINE(*-decay)
        assert(next_object == nullptr);
    }

    constexpr Obj* operator=(const Obj& other) noexcept = delete;
    constexpr Obj* operator=(Obj&& other) noexcept = delete;

    template <typename T>
        requires std::is_base_of_v<Obj, T>
    [[nodiscard]] constexpr T& as() {
        return *static_cast<T*>(this);
    }

    template <typename T>
        requires std::is_base_of_v<Obj, T>
    [[nodiscard]] constexpr const T& as() const {
        return *static_cast<const T*>(this);
    }

    [[nodiscard]] constexpr bool is_string() const noexcept {
        return type == ObjType::STRING;
    }

    // [[nodiscard]] constexpr
    // bool is_closure() const noexcept {
    //     return type == ObjType::CLOSURE;
    // }
    //
    // [[nodiscard]] constexpr
    // bool is_function() const noexcept {
    //     return type == ObjType::FUNCTION;
    // }
    //
    // [[nodiscard]] constexpr
    // bool is_native() const noexcept {
    //     return type == ObjType::NATIVE;
    // }

    friend constexpr std::partial_ordering
    operator<=>(const Obj& lhs, const Obj& rhs) noexcept;

    friend constexpr bool operator==(const Obj& lhs, const Obj& rhs) noexcept;
};

struct ObjString : Obj {
    size_t length = 0;
    u32 hash = 0;
    bool owns_chars = false;
    gsl::owner<const char*> data_ptr = nullptr;
    // NOLINTNEXTLINE(*-c-arrays)
    __extension__ char data[];

    constexpr explicit ObjString() noexcept = delete;

    constexpr explicit ObjString(
        bool copy,
        std::string_view strv,
        u32 hsh
    ) noexcept
            : Obj(ObjType::STRING)
            , length(gsl::narrow_cast<size_t>(strv.length()))
            , hash(hsh)
            , owns_chars(copy) {
        if (owns_chars) {
            std::memcpy(&data[0], strv.data(), strv.length());
            data_ptr = &data[0];
        } else {
            data_ptr = strv.data();
        }
    }

    ObjString(const ObjString& other) = delete;
    ObjString(ObjString&& other) = delete;

    constexpr ~ObjString() noexcept = default;

    constexpr ObjString* operator=(const ObjString& rhs) noexcept = delete;
    constexpr ObjString* operator=(ObjString&& rhs) noexcept = delete;

    // implicit
    constexpr operator std::string_view() const noexcept {
        return std::string_view{data_ptr, static_cast<std::size_t>(length)};
    }

    friend constexpr std::partial_ordering
    operator<=>(const ObjString& lhs, const ObjString& rhs) noexcept;

    friend constexpr bool
    operator==(const ObjString& lhs, const ObjString& rhs) noexcept;
};

template <>
struct is_trivially_relocatable<ObjString>
        : std::__is_bitwise_relocatable<ObjString> {
    static constexpr value_type value = true;
};

template <>
struct Hash<ObjString*> {
    constexpr u32 operator()(tx::ObjString* const& obj) const noexcept {
        return obj->hash;
    }
};

ObjString* make_string(VM& tvm, bool copy, std::string_view strv) noexcept;

}  // namespace tx

template <>
struct fmt::formatter<tx::Obj> : formatter<string_view> {
    // template <typename FormatContext>
    // constexpr
    // auto format_function(
    //     const tx::ObjFunction& function,
    //     FormatContext& ctx
    // ) {
    //     if (function.name == nullptr) {
    //         return fmt::format_to(ctx.out(), "<script>");
    //     }
    //     return fmt::format_to(
    //         ctx.out(),
    //         "<fn {:s}>",
    //         function.name->content
    //     );
    // }

    template <typename FormatContext>
    constexpr auto format(const tx::Obj& obj, FormatContext& ctx) {
        switch (obj.type) {
            // case tx::ObjType::CLOSURE: {
            //     const auto& closure = obj.as<lox::ObjClosure>();
            //     return format_function(*closure.function, ctx);
            // }
            // case tx::ObjType::FUNCTION: {
            //     const auto& function = obj.as<lox::ObjFunction>();
            //     return format_function(function, ctx);
            // }
            // case tx::ObjType::NATIVE:
            //     return fmt::format_to(ctx.out(), "<native fn>");
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
