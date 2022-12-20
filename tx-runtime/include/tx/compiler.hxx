#pragma once

#include "tx/chunk.hxx"
#include "tx/fixed_array.hxx"
#include "tx/scanner.hxx"
#include "tx/table.hxx"

#include "tx/utils.hxx"
#include <gsl/gsl>

#include <string_view>
#include <utility>

namespace tx {

inline constexpr i32 get_opcode_stack_effect(OpCode opc, size_t operand);

struct TypeInfo {
    // TODO
};

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

struct ParseResult {
    TokenType token_type;
    TypeInfo type_info;

    [[nodiscard]] constexpr bool is_block_expr() const noexcept;
};

class Parser;
using ParsePrefixFn = TypeInfo (Parser::*)(bool can_assign);
using ParseInfixFn = TypeInfo (Parser::*)(TypeInfo lhs, bool can_assign);

struct ParseRule {
    ParsePrefixFn prefix;
    ParseInfixFn infix;
    Precedence precedence;
};

struct Local {
    static constexpr bool IS_TRIVIALLY_RELOCATABLE = true;

    Token name{};
    i32 depth{0};
    bool is_captured{false};
    bool is_const{true};

    Local(const Token& name_, i32 dpth, bool is_constant)
            : name(name_)
            , depth(dpth)
            , is_const(is_constant) {}
};

struct Global {
    static constexpr bool IS_TRIVIALLY_RELOCATABLE = true;

    bool is_defined{false};
    bool is_const{true};

    friend constexpr bool operator==(const Global& lhs, const Global& rhs) {
        // FIXME: Maybe make a substructure with signature without is_defined
        return lhs.is_const == rhs.is_const;
    }
};

struct Loop {
    size_t start;
    size_t scope_depth;
    bool is_loop_expr;
    Loop* enclosing;
};

struct Upvalue {
    static constexpr bool IS_TRIVIALLY_RELOCATABLE = true;

    size_t index;
    bool is_local :1;
    bool is_const :1;

    constexpr Upvalue(size_t idx, bool is_local_, bool is_const_) noexcept
            : index(idx)
            , is_local(is_local_)
            , is_const(is_const_) {}
};

enum struct FunctionType {
    FUNCTION,
    SCRIPT
};

struct ObjFunction;

struct Compiler {
    using LocalArray = DynArray<Local>;
    using UpvalueArray = DynArray<Upvalue>;

    Compiler* enclosing{nullptr};
    ObjFunction* function{nullptr};
    FunctionType function_type;
    ValueMap constant_indices{};
    LocalArray locals;
    UpvalueArray upvalues;
    i32 scope_depth{0};
    i32 num_slots{0};
    Loop* innermost_loop = nullptr;

    constexpr void destroy(VM& tvm) noexcept {
        constant_indices.destroy(tvm);
        locals.destroy(tvm);
        upvalues.destroy(tvm);
    }
};

struct Parameter {
    static constexpr bool IS_TRIVIALLY_RELOCATABLE = true;

    Token name;
    bool is_const;

    constexpr explicit Parameter(const Token& name_, bool is_const_) noexcept
            : name(name_)
            , is_const(is_const_) {}
};

using ParameterList = DynArray<Parameter>;

class Parser {
    VM& parent_vm;
    Scanner* scanner{nullptr};
    Token current{};
    Token previous{};
    bool had_error{false};
    bool panic_mode{false};
    Compiler* current_compiler{nullptr};
    std::string_view module_file_path;

  public:
    constexpr explicit Parser(VM& tvm) noexcept : parent_vm(tvm) {}

    [[nodiscard]] ObjFunction*
    compile(std::string_view file_path, std::string_view source) noexcept;

  private:
    void error_at_impl1(const Token& token) noexcept;
    void error_at_impl2(const Token& token) noexcept;

    template <typename... Args>
    void error_at(
        const Token& token,
        fmt::format_string<Args...> fmt,
        Args&&... args
    ) noexcept {
        if (panic_mode) { return; }
        error_at_impl1(token);
        fmt::print(stderr, fmt, args...);
        error_at_impl2(token);
    }

    template <typename... Args>
    void error(fmt::format_string<Args...> fmt, Args&&... args) noexcept {
        error_at(previous, fmt, args...);
    }

    template <typename... Args>
    void
    error_at_current(fmt::format_string<Args...> fmt, Args&&... args) noexcept {
        error_at(current, fmt, args...);
    }

    constexpr void advance() noexcept;
    constexpr void consume(TokenType type, std::string_view message) noexcept;

    [[nodiscard]] constexpr bool check(TokenType type) const noexcept;
    [[nodiscard]] constexpr bool match(TokenType type) noexcept;

    [[nodiscard]] constexpr Chunk& current_chunk() const noexcept;

    [[nodiscard]] constexpr size_t add_constant(Value value) noexcept;

    template <typename... Ts>
        requires((std::is_nothrow_constructible_v<ByteCode, Ts>) && ...)
    constexpr void emit_bytes(Ts... bytes) noexcept {
        current_chunk()
            .write_bytes(parent_vm, previous.line, std::forward<Ts>(bytes)...);
    }

    template <u32 N>
    inline constexpr void
    emit_instruction(OpCode opc, size_t operand) noexcept {
        assert(opc == OpCode::END || N == get_byte_count_following_opcode(opc));
        current_chunk()
            .write_instruction<N>(parent_vm, previous.line, opc, operand);
        current_compiler->num_slots += get_opcode_stack_effect(opc, operand);
        if (current_compiler->num_slots
            > current_compiler->function->max_slots) {
            current_compiler->function->max_slots = current_compiler->num_slots;
        }
    }

    inline constexpr void emit_instruction(OpCode opc) noexcept {
        assert(0 == get_byte_count_following_opcode(opc));
        emit_instruction<0>(opc, 0);
    }

    constexpr void emit_constant(Value value) noexcept;

    constexpr void
    emit_closure(Compiler& compiler, ObjFunction& function) noexcept;

    constexpr void emit_var_length_instruction(OpCode opc, size_t idx) noexcept;

    [[nodiscard]] constexpr size_t emit_jump(OpCode instruction) noexcept;

    constexpr void emit_loop(size_t loop_start) noexcept;
    constexpr void patch_jump(i32 offset) noexcept;

    void begin_compiler(
        Compiler& compiler,
        FunctionType type,
        std::optional<std::string_view> name_opt
    ) noexcept;

    [[nodiscard]] constexpr ObjFunction& end_compiler() noexcept;

    constexpr void begin_scope() noexcept;
    constexpr void patch_jumps_in_innermost_loop() noexcept;
    constexpr void end_scope() noexcept;
    constexpr void begin_loop(Loop& loop, bool is_loop_expr = false) noexcept;
    constexpr void end_loop() noexcept;

    [[nodiscard]] constexpr i32
    resolve_local(Compiler& compiler, const Token& name) noexcept;

    [[nodiscard]] constexpr size_t add_upvalue(
        Compiler& compiler,
        size_t index,
        bool is_local,
        bool is_const
    ) noexcept;

    [[nodiscard]] constexpr i32
    resolve_upvalue(Compiler& compiler, const Token& name) noexcept;

    [[nodiscard]] i32 resolve_global(const Token& name) noexcept;

    [[nodiscard]] size_t add_global(Value identifier, Global sig) noexcept;

    [[nodiscard]] size_t declare_global_variable(bool is_const) noexcept;
    constexpr void add_local(const Token& name, bool is_const) noexcept;
    constexpr void
    declare_local_variable(const Token& name, bool is_const) noexcept;
    constexpr void mark_initialized(i32 global_idx) noexcept;
    constexpr void define_variable(i32 global_idx) noexcept;

    [[nodiscard]] static constexpr const ParseRule& get_rule(
        TokenType token_type
    ) noexcept;

    constexpr ParseResult
    parse_precedence(Precedence, bool do_advance = true) noexcept;

    [[nodiscard]] constexpr i32 parse_variable(std::string_view error_message
    ) noexcept;

    [[nodiscard]] constexpr u8 argument_list();

    constexpr TypeInfo block_no_scope() noexcept;
    constexpr ParseResult expression(bool do_advance = true) noexcept;

    constexpr ParameterList parameter_list() noexcept;

    void function_body(
        FunctionType type,
        std::string_view name,
        const ParameterList& params
    ) noexcept;

    void fn_declaration() noexcept;

    constexpr void var_declaration() noexcept;
    constexpr void while_statement() noexcept;
    constexpr void emit_pop_innermost_loop(bool skip_top_expression) noexcept;
    constexpr void break_statement() noexcept;
    constexpr void continue_statement() noexcept;
    constexpr void return_statement() noexcept;
    constexpr void expression_statement() noexcept;
    constexpr void synchronize() noexcept;

    constexpr std::optional<ParseResult> statement_or_expression() noexcept;
    constexpr void statement() noexcept;

  public:
    constexpr TypeInfo grouping(bool) noexcept;
    constexpr TypeInfo literal(bool) noexcept;
    constexpr TypeInfo named_variable(const Token& name, bool) noexcept;
    constexpr TypeInfo variable(bool can_assign) noexcept;
    constexpr TypeInfo unary(bool) noexcept;
    constexpr TypeInfo block(bool = false) noexcept;
    constexpr TypeInfo if_expr(bool = false) noexcept;
    constexpr TypeInfo loop_expr(bool) noexcept;
    TypeInfo fn_expr(bool) noexcept;

    constexpr TypeInfo binary(TypeInfo, bool) noexcept;
    constexpr TypeInfo call(TypeInfo, bool) noexcept;
    constexpr TypeInfo and_(TypeInfo, bool) noexcept;
    constexpr TypeInfo or_(TypeInfo, bool) noexcept;

    // Friends
    friend constexpr void mark_compiler_roots(VM& tvm) noexcept;
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
        [LEFT_PAREN]      = {&p::grouping,   &p::call,   P::CALL},
        [RIGHT_PAREN]     = {nullptr,        nullptr,    P::NONE},
        [LEFT_BRACE]      = {&p::block,      nullptr,    P::NONE},
        [RIGHT_BRACE]     = {nullptr,        nullptr,    P::NONE},
        [LEFT_BRACKET]    = {nullptr,        nullptr,    P::NONE},
        [RIGHT_BRACKET]   = {nullptr,        nullptr,    P::NONE},
        [COLON]           = {nullptr,        nullptr,    P::NONE},
        [COMMA]           = {nullptr,        nullptr,    P::NONE},
        [DOT]             = {nullptr,        nullptr,    P::NONE},
        [MINUS]           = {&p::unary,      &p::binary, P::TERM},
        [PIPE]            = {nullptr,        nullptr,    P::NONE},
        [PLUS]            = {nullptr,        &p::binary, P::TERM},
        [SEMICOLON]       = {nullptr,        nullptr,    P::NONE},
        [SLASH]           = {nullptr,        &p::binary, P::FACTOR},
        [STAR]            = {nullptr,        &p::binary, P::FACTOR},
        [BANG]            = {&p::unary,      nullptr,    P::NONE},
        [BANG_EQUAL]      = {nullptr,        &p::binary, P::EQUALITY},
        [EQUAL]           = {nullptr,        nullptr,    P::NONE},
        [EQUAL_EQUAL]     = {nullptr,        &p::binary, P::EQUALITY},
        [LEFT_CHEVRON]    = {nullptr,        &p::binary, P::COMPARISON},
        [LESS_EQUAL]      = {nullptr,        &p::binary, P::COMPARISON},
        [RIGHT_CHEVRON]   = {nullptr,        &p::binary, P::COMPARISON},
        [GREATER_EQUAL]   = {nullptr,        &p::binary, P::COMPARISON},
        [IDENTIFIER]      = {&p::variable,   nullptr,    P::NONE},
        [INTEGER_LITERAL] = {&p::literal,    nullptr,    P::NONE},
        [FLOAT_LITERAL]   = {&p::literal,    nullptr,    P::NONE},
        [STRING_LITERAL]  = {&p::literal,    nullptr,    P::NONE},
        [STRING_INTERP]   = {nullptr,        nullptr,    P::NONE},
        [AND]             = {nullptr,        &p::and_,   P::AND},
        [AS]              = {nullptr,        nullptr,    P::NONE},
        [ASYNC]           = {nullptr,        nullptr,    P::NONE},
        [AWAIT]           = {nullptr,        nullptr,    P::NONE},
        [BREAK]           = {nullptr,        nullptr,    P::NONE},
        [CONTINUE]        = {nullptr,        nullptr,    P::NONE},
        [ELSE]            = {nullptr,        nullptr,    P::NONE},
        [FALSE]           = {&p::literal,    nullptr,    P::NONE},
        [FOR]             = {nullptr,        nullptr,    P::NONE},
        [FN]              = {&p::fn_expr,    nullptr,    P::NONE},
        [IF]              = {&p::if_expr,    nullptr,    P::NONE},
        [IN]              = {nullptr,        nullptr,    P::NONE},
        [INOUT]           = {nullptr,        nullptr,    P::NONE},
        [IMPORT]          = {nullptr,        nullptr,    P::NONE},
        [IS]              = {nullptr,        nullptr,    P::NONE},
        [LET]             = {nullptr,        nullptr,    P::NONE},
        [LOOP]            = {&p::loop_expr,  nullptr,    P::NONE},
        [NIL]             = {&p::literal,    nullptr,    P::NONE},
        [MATCH]           = {nullptr,        nullptr,    P::NONE},
        [OR]              = {nullptr,        &p::or_,    P::OR},
        [OUT]             = {nullptr,        nullptr,    P::NONE},
        [RETURN]          = {nullptr,        nullptr,    P::NONE},
        [SELF]            = {nullptr,        nullptr,    P::NONE},
        [STRUCT]          = {nullptr,        nullptr,    P::NONE},
        [SUPER]           = {nullptr,        nullptr,    P::NONE},
        [TRUE]            = {&p::literal,    nullptr,    P::NONE},
        [VAR]             = {nullptr,        nullptr,    P::NONE},
        [WHILE]           = {nullptr,        nullptr,    P::NONE},
        [ANY]             = {nullptr,        nullptr,    P::NONE},
        [BOOL]            = {nullptr,        nullptr,    P::NONE},
        [CHAR]            = {nullptr,        nullptr,    P::NONE},
        [FLOAT]           = {nullptr,        nullptr,    P::NONE},
        [INT]             = {nullptr,        nullptr,    P::NONE},
        [NIL_TYPE]        = {nullptr,        nullptr,    P::NONE},
        [STR]             = {nullptr,        nullptr,    P::NONE},
        [ERROR]           = {nullptr,        nullptr,    P::NONE},
        [END_OF_FILE]     = {nullptr,        nullptr,    P::NONE},
    };
    // clang-format on

  public:
    static constexpr const ParseRule& get_rule(TokenType token_type) noexcept {
        return rules[to_underlying(token_type)];
    }
};

#ifdef __clang__
#pragma clang diagnostic pop
#endif

}  // namespace tx
