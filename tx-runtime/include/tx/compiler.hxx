#pragma once

#include "tx/fixed_array.hxx"
#include "tx/scanner.hxx"
#include "tx/vm.hxx"

#include <gsl/gsl>

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

struct Local {
    Token name{};
    i32 depth{0};
    bool is_const{true};

    Local(Token name_, i32 dpth, bool is_constant)
            : name(name_)
            , depth(dpth)
            , is_const(is_constant) {}
};

struct Loop {
    size_t start;
    size_t scope_depth;
    Loop* enclosing;
};

struct Compiler {
    using LocalArray = FixedCapacityArray<Local, size_t, 256>;

    LocalArray locals;
    i32 scope_depth{0};
    Loop* innermost_loop = nullptr;
};

class Parser {
    VM& parent_vm;
    Scanner scanner;
    Token current{};
    Token previous{};
    bool had_error{false};
    bool panic_mode{false};
    Compiler* current_compiler{nullptr};
    // TODO: move to Compiler? go in pair whith chunk
    ValueMap constant_indices{};
    Chunk& chunk_;

  public:
    constexpr Parser(VM& tvm, std::string_view source, Chunk& chunk) noexcept
            : parent_vm(tvm)
            , scanner(parent_vm, source)
            , chunk_(chunk) {}

    Parser(const Parser&) = delete;
    Parser(Parser&&) = delete;

    constexpr ~Parser() { constant_indices.destroy(parent_vm); }

    Parser& operator=(const Parser&) = delete;
    Parser& operator=(Parser&&) = delete;

    constexpr bool compile() noexcept;

  private:
    constexpr void error_at(Token& token, std::string_view message) noexcept;
    constexpr void error(std::string_view message) noexcept;
    constexpr void error_at_current(std::string_view message) noexcept;

    constexpr void advance() noexcept;
    constexpr void consume(TokenType type, std::string_view message) noexcept;

    [[nodiscard]] constexpr bool check(TokenType type) const noexcept;
    [[nodiscard]] constexpr bool match(TokenType type) noexcept;

    [[nodiscard]] constexpr Chunk& current_chunk() const noexcept;
    [[nodiscard]] constexpr size_t add_constant(Value value) noexcept;

    template <typename... Ts>
        requires((std::is_nothrow_constructible_v<ByteCode, Ts>) && ...)
    constexpr void emit_bytes(Ts... bytes) noexcept;

    constexpr void emit_constant(Value value) noexcept;
    constexpr void emit_return() noexcept;
    constexpr void emit_var_length_instruction(OpCode opc, size_t idx) noexcept;
    [[nodiscard]] constexpr size_t emit_jump(OpCode instruction) noexcept;
    constexpr void emit_loop(size_t loop_start) noexcept;
    constexpr void patch_jump(i32 offset) noexcept;
    constexpr void begin_compiler(Compiler& compiler) noexcept;
    constexpr void end_compiler() noexcept;
    constexpr void begin_scope() noexcept;
    constexpr void end_scope() noexcept;
    constexpr void begin_loop(Loop& loop);
    constexpr void end_loop();

  public:
    constexpr void binary(bool) noexcept;
    constexpr void grouping(bool) noexcept;
    constexpr void literal(bool) noexcept;
    constexpr void named_variable(const Token& name, bool) noexcept;
    constexpr void variable(bool) noexcept;
    constexpr void unary(bool) noexcept;
    constexpr void block(bool) noexcept;
    constexpr void if_expr(bool) noexcept;
    constexpr void and_(bool) noexcept;
    constexpr void or_(bool) noexcept;

  private:
    [[nodiscard]] static constexpr const ParseRule& get_rule(
        TokenType token_type
    ) noexcept;

    constexpr void parse_precedence(Precedence) noexcept;

    [[nodiscard]] constexpr i32
    resolve_local(Compiler& compiler, const Token& name) noexcept;

    [[nodiscard]] constexpr size_t identifier_global_index(const Token& name
    ) noexcept;

    constexpr void add_local(Token name, bool is_const) noexcept;
    constexpr void declare_variable(bool is_const) noexcept;

    [[nodiscard]] constexpr size_t parse_variable(const char* error_message
    ) noexcept;

    constexpr void mark_initialized() noexcept;
    constexpr void define_variable(size_t global) noexcept;

    constexpr void expression() noexcept;
    constexpr void var_declaration() noexcept;
    constexpr void while_statement() noexcept;
    constexpr void continue_statement() noexcept;
    constexpr void expression_statement() noexcept;
    constexpr void synchronize() noexcept;
    constexpr void statement() noexcept;
    [[nodiscard]] constexpr bool statement_no_expression() noexcept;

    // friend class ParseRules;
};

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc99-designator"
#endif

class ParseRules {
    using enum TokenType;
    using P = Precedence;
    using p = Parser;

    // clang-format off
    // NOLINTNEXTLINE(*-avoid-c-arrays)
    __extension__ static constexpr ParseRule rules[] = {
        [LEFT_PAREN]      = {&p::grouping, nullptr,    P::NONE},
        [RIGHT_PAREN]     = {nullptr,      nullptr,    P::NONE},
        [LEFT_BRACE]      = {&p::block,    nullptr,    P::NONE},
        [RIGHT_BRACE]     = {nullptr,      nullptr,    P::NONE},
        [LEFT_BRACKET]    = {nullptr,      nullptr,    P::NONE},
        [RIGHT_BRACKET]   = {nullptr,      nullptr,    P::NONE},
        [COLON]           = {nullptr,      nullptr,    P::NONE},
        [COMMA]           = {nullptr,      nullptr,    P::NONE},
        [DOT]             = {nullptr,      nullptr,    P::NONE},
        [MINUS]           = {&p::unary,    &p::binary, P::TERM},
        [PIPE]            = {nullptr,      nullptr,    P::NONE},
        [PLUS]            = {nullptr,      &p::binary, P::TERM},
        [SEMICOLON]       = {nullptr,      nullptr,    P::NONE},
        [SLASH]           = {nullptr,      &p::binary, P::FACTOR},
        [STAR]            = {nullptr,      &p::binary, P::FACTOR},
        [BANG]            = {&p::unary,    nullptr,    P::NONE},
        [BANG_EQUAL]      = {nullptr,      &p::binary, P::EQUALITY},
        [EQUAL]           = {nullptr,      nullptr,    P::NONE},
        [EQUAL_EQUAL]     = {nullptr,      &p::binary, P::EQUALITY},
        [LEFT_CHEVRON]    = {nullptr,      &p::binary, P::COMPARISON},
        [LESS_EQUAL]      = {nullptr,      &p::binary, P::COMPARISON},
        [RIGHT_CHEVRON]   = {nullptr,      &p::binary, P::COMPARISON},
        [GREATER_EQUAL]   = {nullptr,      &p::binary, P::COMPARISON},
        [IDENTIFIER]      = {&p::variable, nullptr,    P::NONE},
        [INTEGER_LITERAL] = {&p::literal,  nullptr,    P::NONE},
        [FLOAT_LITERAL]   = {&p::literal,  nullptr,    P::NONE},
        [STRING_LITERAL]  = {&p::literal,  nullptr,    P::NONE},
        [STRING_INTERP]   = {nullptr,      nullptr,    P::NONE},
        [AND]             = {nullptr,      &p::and_,   P::AND},
        [AS]              = {nullptr,      nullptr,    P::NONE},
        [ASYNC]           = {nullptr,      nullptr,    P::NONE},
        [AWAIT]           = {nullptr,      nullptr,    P::NONE},
        [BREAK]           = {nullptr,      nullptr,    P::NONE},
        [CONTINUE]        = {nullptr,      nullptr,    P::NONE},
        [ELSE]            = {nullptr,      nullptr,    P::NONE},
        [FALSE]           = {&p::literal,  nullptr,    P::NONE},
        [FOR]             = {nullptr,      nullptr,    P::NONE},
        [FN]              = {nullptr,      nullptr,    P::NONE},
        [IF]              = {&p::if_expr,  nullptr,    P::NONE},
        [IN]              = {nullptr,      nullptr,    P::NONE},
        [INOUT]           = {nullptr,      nullptr,    P::NONE},
        [IMPORT]          = {nullptr,      nullptr,    P::NONE},
        [IS]              = {nullptr,      nullptr,    P::NONE},
        [LET]             = {nullptr,      nullptr,    P::NONE},
        [LOOP]            = {nullptr,      nullptr,    P::NONE},
        [NIL]             = {&p::literal,  nullptr,    P::NONE},
        [MATCH]           = {nullptr,      nullptr,    P::NONE},
        [OR]              = {nullptr,      &p::or_,    P::OR},
        [OUT]             = {nullptr,      nullptr,    P::NONE},
        [RETURN]          = {nullptr,      nullptr,    P::NONE},
        [SELF]            = {nullptr,      nullptr,    P::NONE},
        [STRUCT]          = {nullptr,      nullptr,    P::NONE},
        [SUPER]           = {nullptr,      nullptr,    P::NONE},
        [TRUE]            = {&p::literal,  nullptr,    P::NONE},
        [VAR]             = {nullptr,      nullptr,    P::NONE},
        [WHILE]           = {nullptr,      nullptr,    P::NONE},
        [ANY]             = {nullptr,      nullptr,    P::NONE},
        [BOOL]            = {nullptr,      nullptr,    P::NONE},
        [CHAR]            = {nullptr,      nullptr,    P::NONE},
        [FLOAT]           = {nullptr,      nullptr,    P::NONE},
        [INT]             = {nullptr,      nullptr,    P::NONE},
        [NIL_TYPE]        = {nullptr,      nullptr,    P::NONE},
        [STR]             = {nullptr,      nullptr,    P::NONE},
        [ERROR]           = {nullptr,      nullptr,    P::NONE},
        [END_OF_FILE]     = {nullptr,      nullptr,    P::NONE},
    };
    // clang-format on

  public:
    static constexpr const ParseRule& get_rule(TokenType token_type) noexcept {
        return gsl::at(rules, to_underlying(token_type));
    }
};

#ifdef __clang__
#pragma clang diagnostic pop
#endif

}  // namespace tx
