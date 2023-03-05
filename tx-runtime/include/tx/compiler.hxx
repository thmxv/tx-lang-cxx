#pragma once

#include "tx/chunk.hxx"
#include "tx/dyn_array.hxx"
#include "tx/scanner.hxx"
#include "tx/table.hxx"
#include "tx/type.hxx"
#include "tx/utils.hxx"

#include <gsl/gsl>

#include <string_view>
#include <memory>
#include <utility>

namespace tx {

constexpr i32 get_opcode_stack_effect(OpCode opc, size_t operand);

enum class Precedence {
    NONE,
    ASSIGNMENT,  // =
    OR,          // or
    AND,         // and
    EQUALITY,    // == !=
    COMPARISON,  // < > <= >=
    TERM,        // + -
    FACTOR,      // * /
    AS,          // as
    UNARY,       // ! -
    CALL,        // . () []
    PRIMARY,
};

struct ParseResult {
    bool is_block_expr;
    TypeSet type_set{};

    constexpr void destroy(VM& tvm) noexcept { type_set.destroy(tvm); }
};

class Parser;
using ParsePrefixFn = TypeSet (Parser::*)(bool can_assign);
using ParseInfixFn = TypeSet (Parser::*)(TypeSet lhs, bool can_assign);

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
    TypeSet type_set{};

    Local(const Token& name_, i32 dpth, bool is_constant, TypeSet&& types)
            : name(name_)
            , depth(dpth)
            , is_const(is_constant)
            , type_set(std::move(types)) {}

    constexpr void destroy(VM& tvm) noexcept;
};

struct Global {
    static constexpr bool IS_TRIVIALLY_RELOCATABLE = true;

    bool is_defined{false};
    bool is_const{true};
    TypeSet type_set{};

    friend constexpr bool operator==(const Global& lhs, const Global& rhs) {
        // FIXME: Maybe make a substructure with signature without is_defined
        return lhs.is_const == rhs.is_const;
    }

    constexpr void destroy(VM& tvm) noexcept;
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
    bool is_local;

    constexpr Upvalue(size_t idx, bool is_local_) noexcept
            : index(idx)
            , is_local(is_local_) {}
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
        for (auto& local : locals) { local.destroy(tvm); }
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

struct ParametersAndReturn {
    ParameterList parameters{};
    TypeInfoFunction type_info;

    constexpr void destroy(VM& tvm) noexcept {
        parameters.destroy(tvm);
        type_info.destroy(tvm);
    }
};

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
    void error_at_impl_begin(const Token& token) noexcept;
    void error_at_impl_end(const Token& token) noexcept;

    template <typename... Args>
    void error_at(
        const Token& token,
        fmt::format_string<Args...> fmt,
        Args&&... args
    ) noexcept {
        if (panic_mode) { return; }
        error_at_impl_begin(token);
        fmt::print(stderr, fmt, args...);
        error_at_impl_end(token);
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
        // NOLINTNEXTLINE(*-decay)
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
        assert(0 == get_byte_count_following_opcode(opc));  // NOLINT(*-decay)
        emit_instruction<0>(opc, 0);
    }

    constexpr void emit_constant(Value value) noexcept;

    constexpr void
    emit_closure(Compiler& compiler, ObjFunction& function) noexcept;

    constexpr void emit_var_length_instruction(OpCode opc, size_t idx) noexcept;

    [[nodiscard]] constexpr size_t emit_jump(OpCode instruction) noexcept;

    constexpr void emit_loop(size_t loop_start) noexcept;
    constexpr void patch_jump(size_t offset) noexcept;

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

    // FIXME: Make static or move to anonymous namespace
    [[nodiscard]] static constexpr i32
    // cppcheck-suppress functionStatic
    resolve_local(Compiler& compiler, const Token& name) noexcept;

    [[nodiscard]] constexpr size_t
    add_upvalue(Compiler& compiler, size_t index, bool is_local) noexcept;

    [[nodiscard]] constexpr std::tuple<i32, const Local*>
    resolve_upvalue(Compiler& compiler, const Token& name) noexcept;

    [[nodiscard]] i32 resolve_global(const Token& name) noexcept;

    [[nodiscard]] size_t add_global(Value identifier, Global&& sig) noexcept;

    [[nodiscard]] size_t declare_global_variable(
        const Token& name,
        bool is_const,
        TypeSet&& type_set
    ) noexcept;

    constexpr void
    add_local(const Token& name, bool is_const, TypeSet&& type_set) noexcept;

    constexpr void declare_local_variable(
        const Token& name,
        bool is_const,
        TypeSet&& type_set
    ) noexcept;

    [[nodiscard]] inline constexpr i32 declare_variable(
        const Token& name,
        bool is_const,
        TypeSet&& type_set
    ) noexcept;

    constexpr void mark_initialized(i32 global_idx) noexcept;
    constexpr void define_variable(i32 global_idx) noexcept;

    [[nodiscard]] static constexpr const ParseRule& get_rule(
        TokenType token_type
    ) noexcept;

    [[nodiscard]] constexpr TypeSet parse_prefix_only() noexcept;
    [[nodiscard]] constexpr TypeSet parse_precedence(Precedence) noexcept;
    [[nodiscard]] constexpr TypeSet parse_precedence_no_advance(Precedence
    ) noexcept;

    [[nodiscard]] constexpr std::optional<std::tuple<Token, bool>>
    parse_variable(std::string_view error_message) noexcept;

    [[nodiscard]] constexpr TypeSetArray argument_list();

    [[nodiscard]] constexpr TypeSet block_no_scope() noexcept;

    [[nodiscard]] constexpr TypeSet expression() noexcept;

    [[nodiscard]] constexpr ParseResult expression_maybe_statement(
        bool do_advance = true
    ) noexcept;

    constexpr ParametersAndReturn parameter_list_and_return_type() noexcept;

    void function_body(
        FunctionType type,
        std::string_view name,
        ParametersAndReturn&& params_ret
    ) noexcept;

    void fn_declaration() noexcept;

    [[nodiscard]] constexpr TypeInfo parse_type() noexcept;
    [[nodiscard]] constexpr TypeSet parse_type_set() noexcept;
    [[nodiscard]] constexpr TypeSetArray parse_type_set_list(
        std::string_view message
    ) noexcept;

    constexpr void var_declaration() noexcept;
    constexpr void while_statement() noexcept;
    constexpr void emit_pop_innermost_loop(bool skip_top_expression) noexcept;
    constexpr void break_statement() noexcept;
    constexpr void continue_statement() noexcept;
    constexpr void return_statement() noexcept;
    constexpr void expression_statement() noexcept;
    constexpr void synchronize() noexcept;

    [[nodiscard]] constexpr std::optional<ParseResult> statement_or_expression(
    ) noexcept;
    constexpr void statement() noexcept;

  public:
    [[nodiscard]] constexpr TypeSet grouping(bool) noexcept;
    [[nodiscard]] constexpr TypeSet literal(bool) noexcept;
    [[nodiscard]] constexpr TypeSet
    named_variable(const Token& name, bool) noexcept;
    [[nodiscard]] constexpr TypeSet variable(bool can_assign) noexcept;
    [[nodiscard]] constexpr TypeSet unary(bool) noexcept;
    [[nodiscard]] constexpr TypeSet block(bool = false) noexcept;
    [[nodiscard]] constexpr TypeSet if_expr(bool = false) noexcept;
    [[nodiscard]] constexpr TypeSet loop_expr(bool) noexcept;
    [[nodiscard]] TypeSet fn_expr(bool) noexcept;

    [[nodiscard]] constexpr TypeSet binary(TypeSet, bool) noexcept;
    [[nodiscard]] constexpr TypeSet call(TypeSet, bool) noexcept;
    [[nodiscard]] constexpr TypeSet and_(TypeSet, bool) noexcept;
    [[nodiscard]] constexpr TypeSet or_(TypeSet, bool) noexcept;
    [[nodiscard]] constexpr TypeSet as(TypeSet, bool) noexcept;

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
        [AS]              = {nullptr,        &p::as,     P::AS},
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
        [ANY_TYPE]        = {nullptr,        nullptr,    P::NONE},
        [BOOL_TYPE]       = {nullptr,        nullptr,    P::NONE},
        [CHAR_TYPE]       = {nullptr,        nullptr,    P::NONE},
        [FLOAT_TYPE]      = {nullptr,        nullptr,    P::NONE},
        [FN_TYPE]         = {nullptr,        nullptr,    P::NONE},
        [INT_TYPE]        = {nullptr,        nullptr,    P::NONE},
        [NIL_TYPE]        = {nullptr,        nullptr,    P::NONE},
        [STR_TYPE]        = {nullptr,        nullptr,    P::NONE},
        [ERROR]           = {nullptr,        nullptr,    P::NONE},
        [END_OF_FILE]     = {nullptr,        nullptr,    P::NONE},
    };
    // clang-format on

  public:
    static constexpr const ParseRule& get_rule(TokenType token_type) noexcept {
        // NOLINTNEXTLINE(*-array-index)
        return rules[to_underlying(token_type)];
    }
};

#ifdef __clang__
#pragma clang diagnostic pop
#endif

}  // namespace tx
