#pragma once

#include "tx/fixed_array.hxx"
#include "tx/value.hxx"

#include <optional>
#include <string_view>

namespace tx {

// "a ${b} c ${d} e" is tokenized to:
// STRING_INTERP        "a "
// INDENTIFIER          "b"
// STRING_INTERP        " c "
// INDENTIFIER          "d"
// STRING_LITERAL       " e"

struct Token {
    // clang-format off
    enum Type {
        // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
        #define TX_TOKEN(name) name,
        #include "tx/tokens.inc"
        #undef TX_TOKEN
    };
    // clang-format on

    Type type{};
    std::string_view lexeme{};
    size_t line{};
    Value value{val_none};
};

class Scanner {
    static constexpr std::size_t MAX_CHARS_IN_NUMERIC_LITERAL = 64;
    static constexpr size_t MAX_INTERP_DEPTH = 8;

    VM& parent_vm;
    const std::string_view source;
    const char* start;
    const char* current;
    size_t line{};
    FixedCapacityArray<isize, size_t, MAX_INTERP_DEPTH> str_interp_braces;

  public:
    explicit constexpr Scanner(VM& tvm, std::string_view src) noexcept
            : parent_vm(tvm)
            , source(src)
            , start(source.begin())
            , current(start)
            , line(1) {}

    [[nodiscard]] constexpr Token scan_token() noexcept;

  private:
    [[nodiscard]] constexpr bool is_at_end() const noexcept;

    constexpr char advance() noexcept;

    [[nodiscard]] constexpr char peek() const noexcept;
    [[nodiscard]] constexpr char peek_next(size_t offset = 1) const noexcept;
    [[nodiscard]] constexpr bool match(char expected) noexcept;
    [[nodiscard]] constexpr Token make_token(Token::Type type) const noexcept;

    [[nodiscard]] Token error_token(std::string_view message) const noexcept;

    constexpr void skip_whitespace() noexcept;

    [[nodiscard]] constexpr Token::Type check_keyword(
        size_t offset,
        std::string_view rest,
        Token::Type type
    ) const noexcept;

    [[nodiscard]] constexpr Token::Type identifier_type() const noexcept;
    [[nodiscard]] constexpr Token identifier() noexcept;
    [[nodiscard]] constexpr Token number() noexcept;
    [[nodiscard]] Token hex_number() noexcept;
    [[nodiscard]] Token raw_string() noexcept;
    [[nodiscard]] constexpr std::optional<u32> hex_escape(size_t digits
    ) noexcept;

    [[nodiscard]] constexpr bool
    utf8_escape(size_t digits, DynArray<char>& dst) noexcept;

    [[nodiscard]] constexpr Token string() noexcept;

    // Friends
    friend class Parser;
};

}  // namespace tx
