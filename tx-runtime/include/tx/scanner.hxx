#pragma once

#include "tx/fixed_array.hxx"
#include "tx/value.hxx"

#include <optional>
#include <string_view>

namespace tx {

enum class TokenType {
    // single char tokens
    LEFT_PAREN,
    RIGHT_PAREN,
    LEFT_BRACE,
    RIGHT_BRACE,
    LEFT_BRACKET,
    RIGHT_BRACKET,
    COLON,
    COMMA,
    DOT,
    MINUS,
    PIPE,
    PLUS,
    SEMICOLON,
    SLASH,
    STAR,
    // one or two char tokens
    BANG,
    BANG_EQUAL,
    EQUAL,
    EQUAL_EQUAL,
    LEFT_CHEVRON,
    LESS_EQUAL,
    RIGHT_CHEVRON,
    GREATER_EQUAL,
    // literals
    IDENTIFIER,
    INTEGER_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    // "a {b} c {d} e" is tokenized to:
    // STRING_INTERPOLATION "a "
    // INDENTIFIER          "b"
    // STRING_INTERPOLATION " c "
    // INDENTIFIER          "d"
    // STRING_LITERAL       " e"
    STRING_INTERPOLATION,
    // keywords
    AND,
    AS,
    ASYNC,
    AWAIT,
    BREAK,
    CONTINUE,
    ELSE,
    FALSE,
    FOR,
    FN,
    IF,
    IMPORT,
    IS,
    LET,
    LOOP,
    NIL,
    MATCH,
    OR,
    RETURN,
    SELF,
    STRUCT,
    SUPER,
    TRUE,
    VAR,
    WHILE,
    // built-in types
    ANY,
    BOOL,
    CHAR,
    FLOAT,
    INT,
    NIL_TYPE,
    STR,

    ERROR,
    END_OF_FILE,
};

struct Token {
    TokenType type;
    std::string_view lexeme;
    size_t line;
    Value value{};
};

class Scanner {
    static constexpr std::size_t MAX_CHARS_IN_NUMERIC_LITERAL = 64;
    static constexpr size_t MAX_INTERP_DEPTH = 8;

    const std::string_view source;
    const char* start;
    const char* current;
    size_t line{};
    FixedCapacityArray<isize, size_t, MAX_INTERP_DEPTH> str_interp_braces;

  public:
    explicit constexpr Scanner(std::string_view src) noexcept
            : source(src)
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
    [[nodiscard]] constexpr Token make_token(TokenType type) const noexcept;

    [[nodiscard]] constexpr Token error_token(std::string_view message
    ) const noexcept;

    constexpr void skip_whitespace() noexcept;

    [[nodiscard]] constexpr TokenType check_keyword(
        size_t offset,
        std::string_view rest,
        TokenType type
    ) const noexcept;

    [[nodiscard]] constexpr TokenType identifier_type() const noexcept;
    [[nodiscard]] constexpr Token identifier() noexcept;
    [[nodiscard]] constexpr Token number() noexcept;
    [[nodiscard]] constexpr Token hex_number() noexcept;
    [[nodiscard]] constexpr Token raw_string() noexcept;
    [[nodiscard]] constexpr std::optional<i32> hex_escape(size_t digits
    ) noexcept;
    [[nodiscard]] constexpr Token string() noexcept;
};

}  // namespace tx
