#pragma once

#include "tx/scanner.hxx"
#include "tx/vm.hxx"

#include <string_view>

namespace tx {

enum class Precedence {
    NONE,
    ASSIGNMENT,  // =
    OR,          // or
    AND,         // and
    EQUALITY,    // == !=
    COMPARISON,  // < > <= >=
    TERM,        // + -
    FACTOR,      // * /
    UNARY,       // ! -
    CALL,        // . () []
    PRIMARY,
};

class Parser;
using ParseFn = void (Parser::*)(bool);

struct ParseRule {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
};

struct Compiler {};

class Parser {
    VM& tvm;
    Scanner scanner;
    Token current{};
    Token previous{};
    bool had_error = false;
    bool panic_mode = false;
    Chunk& chunk_;

  public:
    constexpr Parser(VM& tvm_, std::string_view source, Chunk& chunk) noexcept
            : tvm(tvm_)
            , scanner(source)
            , chunk_(chunk) {}

    constexpr bool compile() noexcept;

  private:
    constexpr void error_at(Token& token, std::string_view message) noexcept;
    constexpr void error(std::string_view message) noexcept;
    constexpr void error_at_current(std::string_view message) noexcept;

    constexpr void advance() noexcept;
    constexpr void consume(TokenType type, std::string_view message) noexcept;

    [[nodiscard]] constexpr Chunk& current_chunk() const noexcept;

    template <typename... Ts>
        requires((std::is_nothrow_constructible_v<ByteCode, Ts>) && ...)
    constexpr void emit_bytes(Ts... bytes) noexcept;

    constexpr void emit_return() noexcept;
    constexpr void emit_constant(Value value) noexcept;

  public:
    constexpr void end_compiler() noexcept;
    constexpr void binary(bool) noexcept;
    constexpr void grouping(bool) noexcept;
    // constexpr void number(bool) noexcept;
    constexpr void literal(bool) noexcept;
    constexpr void unary(bool) noexcept;
    constexpr void parse_precedence(Precedence) noexcept;
    static constexpr const ParseRule& get_rule(TokenType token_type) noexcept;
    constexpr void expression() noexcept;

    // friend class ParseRules;
};

class ParseRules {
    // clang-format off
    __extension__ static constexpr ParseRule rules[] = {
        [to_underlying(TokenType::LEFT_PAREN)]              = {&Parser::grouping, nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::RIGHT_PAREN)]             = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::LEFT_BRACE)]              = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::RIGHT_BRACE)]             = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::LEFT_BRACKET)]            = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::RIGHT_BRACKET)]           = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::COLON)]                   = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::COMMA)]                   = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::DOT)]                     = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::MINUS)]                   = {&Parser::unary,    &Parser::binary, Precedence::TERM      },
        [to_underlying(TokenType::PIPE)]                    = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::PLUS)]                    = {nullptr,           &Parser::binary, Precedence::TERM      },
        [to_underlying(TokenType::SEMICOLON)]               = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::SLASH)]                   = {nullptr,           &Parser::binary, Precedence::FACTOR    },
        [to_underlying(TokenType::STAR)]                    = {nullptr,           &Parser::binary, Precedence::FACTOR    },
        [to_underlying(TokenType::BANG)]                    = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::BANG_EQUAL)]              = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::EQUAL)]                   = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::EQUAL_EQUAL)]             = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::LEFT_CHEVRON)]            = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::LESS_EQUAL)]              = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::RIGHT_CHEVRON)]           = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::GREATER_EQUAL)]           = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::IDENTIFIER)]              = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::INTEGER_LITERAL)]         = {&Parser::literal,   nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::FLOAT_LITERAL)]           = {&Parser::literal,   nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::STRING_LITERAL)]          = {&Parser::literal,  nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::STRING_INTERPOLATION)]    = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::AND)]                     = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::AS)]                      = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::ASYNC)]                   = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::AWAIT)]                   = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::BREAK)]                   = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::CONTINUE)]                = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::ELSE)]                    = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::FALSE)]                   = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::FOR)]                     = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::FN)]                      = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::IF)]                      = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::IMPORT)]                  = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::IS)]                      = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::LET)]                     = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::LOOP)]                    = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::NIL)]                     = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::MATCH)]                   = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::OR)]                      = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::RETURN)]                  = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::SELF)]                    = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::STRUCT)]                  = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::SUPER)]                   = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::TRUE)]                    = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::VAR)]                     = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::WHILE)]                   = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::ANY)]                     = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::BOOL)]                    = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::CHAR)]                    = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::FLOAT)]                   = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::INT)]                     = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::NIL_TYPE)]                = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::STR)]                     = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::ERROR)]                   = {nullptr,           nullptr,         Precedence::NONE      },
        [to_underlying(TokenType::END_OF_FILE)]             = {nullptr,           nullptr,         Precedence::NONE      },
    };
    // clang-format on

  public:
    static constexpr const ParseRule& get_rule(TokenType token_type) noexcept {
        return rules[to_underlying(token_type)];
    }
};

}  // namespace tx
