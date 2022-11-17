#pragma once

#include "tx/compiler.hxx"
//
#include "tx/chunk.hxx"
#include "tx/common.hxx"
#include "tx/debug.hxx"
#include "tx/object.hxx"
#include "tx/scanner.hxx"
#include "tx/utils.hxx"

#include <limits>
#include <ranges>
#include <functional>

namespace tx {

static inline constexpr bool
identifiers_equal(const Token& lhs, const Token& rhs) {
    return lhs.lexeme == rhs.lexeme;
}

inline constexpr bool ParseResult::is_block_expr() const noexcept {
    switch (token_type) {
        case LEFT_BRACE:
        case IF:
        case LOOP: return true;
        default: return false;
    };
}

[[nodiscard]] inline ObjFunction* Parser::compile() noexcept {
    Compiler comp{};
    begin_compiler(comp, FunctionType::SCRIPT, nullptr);
    advance();
    while (!match(TokenType::END_OF_FILE)) { statement(); }
    auto* fun = end_compiler();
    return had_error ? nullptr : fun;
}

inline constexpr void
Parser::error_at(Token& token, std::string_view message) noexcept {
    if (panic_mode) { return; }
    panic_mode = true;
    fmt::print(stderr, "[line {:d}] Error", token.line);
    if (token.type == TokenType::END_OF_FILE) {
        fmt::print(stderr, " at end");
    } else if (token.type == TokenType::ERROR) {
    } else {
        fmt::print(stderr, " at '{:s}'", token.lexeme);
    }
    fmt::print(stderr, ": {:s}\n", message);
    had_error = true;
}

inline constexpr void Parser::error(std::string_view message) noexcept {
    error_at(previous, message);
}

inline constexpr void Parser::error_at_current(std::string_view message
) noexcept {
    error_at(current, message);
}

inline constexpr void Parser::advance() noexcept {
    previous = current;
    for (;;) {
        current = scanner.scan_token();
        if constexpr (HAS_DEBUG_FEATURES) {
            if (parent_vm.options.print_tokens) { print_token(current); }
        }
        if (current.type != TokenType::ERROR) { break; }
        error_at_current(current.lexeme.cbegin());
    }
}

inline constexpr void
Parser::consume(TokenType type, std::string_view message) noexcept {
    if (current.type == type) {
        advance();
        return;
    }
    error_at_current(message);
}

[[nodiscard]] inline constexpr bool Parser::check(TokenType type
) const noexcept {
    return current.type == type;
}

[[nodiscard]] inline constexpr bool Parser::match(TokenType type) noexcept {
    if (!check(type)) { return false; }
    advance();
    return true;
}

[[nodiscard]] inline constexpr Chunk& Parser::current_chunk() const noexcept {
    return current_compiler->function->chunk;
}

[[nodiscard]] inline constexpr size_t Parser::add_constant(Value value
) noexcept {
    // if (had_error) { return -1; }
    const auto* existing = current_compiler->constant_indices.get(value);
    if (existing != nullptr) { return static_cast<size_t>(existing->as_int()); }
    // TODO: error if too much constants
    auto idx = current_chunk().add_constant(parent_vm, value);
    current_compiler->constant_indices
        .set(parent_vm, value, Value{static_cast<int_t>(idx)});
    return idx;
}

template <typename... Ts>
    requires((std::is_nothrow_constructible_v<ByteCode, Ts>) && ...)
inline constexpr void Parser::emit_bytes(Ts... bytes) noexcept {
    current_chunk().write(parent_vm, previous.line, bytes...);
}

inline constexpr void Parser::emit_return() noexcept {
    emit_bytes(OpCode::NIL, OpCode::RETURN);
}

inline constexpr void
Parser::emit_var_length_instruction(OpCode opc, size_t idx) noexcept {
    current_chunk().write_constant(parent_vm, previous.line, opc, idx);
}

inline constexpr void Parser::emit_constant(Value value) noexcept {
    auto idx = add_constant(value);
    emit_var_length_instruction(OpCode::CONSTANT, idx);
}

[[nodiscard]] inline constexpr size_t Parser::emit_jump(OpCode instruction
) noexcept {
    emit_bytes(instruction, static_cast<u8>(0xff), static_cast<u8>(0xff));
    return current_chunk().code.size() - 2;
}

inline constexpr void Parser::emit_loop(size_t loop_start) noexcept {
    emit_bytes(OpCode::LOOP);
    auto offset = current_chunk().code.size() - loop_start + 2;
    if (offset > std::numeric_limits<u16>::max()) {
        error("Loop body too large.");
    }
    const u16 offset_16 = static_cast<u16>(offset);
    emit_bytes(
        static_cast<u8>((offset_16 >> 8U) & 0xffU),
        static_cast<u8>(offset_16 & 0xffU)
    );
}

inline constexpr void Parser::patch_jump(i32 offset) noexcept {
    auto jump_distance = current_chunk().code.size() - offset - 2;
    if (jump_distance > std::numeric_limits<u16>::max()) {
        error("Too much code to jump over.");
    }
    const u16 jump_16 = static_cast<u16>(jump_distance);
    current_chunk().code[offset].value = static_cast<u8>(
        (jump_16 >> 8U) & 0xffU
    );
    current_chunk().code[offset + 1].value = static_cast<u8>(jump_16 & 0xffU);
}

inline void Parser::begin_compiler(
    Compiler& compiler,
    FunctionType type,
    Token* name
) noexcept {
    compiler.enclosing = current_compiler;
    compiler.function_type = type;
    compiler.function = allocate_object<ObjFunction>(parent_vm);
    current_compiler = &compiler;
    if (type != FunctionType::SCRIPT && name != nullptr) {
        current_compiler->function->name = make_string(
            parent_vm,
            !parent_vm.options.allow_pointer_to_source_content,
            name->lexeme
        );
    }
    // Reserve first local for methods, use empty string as name to prevent use
    current_compiler->locals.push_back(Local(Token{.lexeme = ""}, 0, true));
}

[[nodiscard]] inline constexpr ObjFunction* Parser::end_compiler() noexcept {
    emit_return();
    auto* fun = current_compiler->function;
    if constexpr (HAS_DEBUG_FEATURES) {
        if (parent_vm.get_options().print_bytecode) {
            if (!had_error) {
                disassemble_chunk(current_chunk(), fun->get_name());
            }
        }
    }
    current_compiler->constant_indices.destroy(parent_vm);
    current_compiler = current_compiler->enclosing;
    return fun;
}

inline constexpr void Parser::begin_scope() noexcept {
    current_compiler->scope_depth++;
}

inline constexpr void Parser::end_scope() noexcept {
    size_t scope_local_count = 0;
    current_compiler->scope_depth--;
    while (!current_compiler->locals.empty()
           && current_compiler->locals.back().depth
                  > current_compiler->scope_depth) {
        // emit_bytes(OpCode::POP);
        ++scope_local_count;
        current_compiler->locals.pop_back();
    }
    if (scope_local_count > 0) {
        emit_var_length_instruction(OpCode::END_SCOPE, scope_local_count);
    }
}

inline constexpr void
Parser::begin_loop(Loop& loop, bool is_loop_expr) noexcept {
    loop.enclosing = current_compiler->innermost_loop;
    loop.start = current_chunk().code.size();
    loop.is_loop_expr = is_loop_expr;
    loop.scope_depth = current_compiler->scope_depth;
    current_compiler->innermost_loop = &loop;
}

inline constexpr void Parser::patch_jumps_in_innermost_loop() noexcept {
    for (size_t i = current_compiler->innermost_loop->start;
         i < current_chunk().code.size();) {
        ByteCode& btc = current_chunk().code[i];
        if (btc.as_opcode() == OpCode::END) {
            btc = ByteCode{OpCode::JUMP};
            patch_jump(i + 1);
            // i += 3;
            // continue;
        }
        i += 1 + get_byte_count_following_opcode(btc.as_opcode());
    }
}

inline constexpr void Parser::end_loop() noexcept {
    patch_jumps_in_innermost_loop();
    current_compiler->innermost_loop = current_compiler->innermost_loop
                                           ->enclosing;
}

inline constexpr TypeInfo
Parser::binary(TypeInfo lhs, bool /*can_assign*/) noexcept {
    auto token_type = previous.type;
    auto rule = get_rule(token_type);
    auto rhs = parse_precedence(
        static_cast<Precedence>(to_underlying(rule.precedence) + 1)
    );
    (void)lhs;
    (void)rhs;
    switch (token_type) {
        using enum TokenType;
        case BANG_EQUAL: emit_bytes(OpCode::NOT_EQUAL); break;
        case EQUAL_EQUAL: emit_bytes(OpCode::EQUAL); break;
        case LEFT_CHEVRON: emit_bytes(OpCode::LESS); break;
        case LESS_EQUAL: emit_bytes(OpCode::LESS_EQUAL); break;
        case RIGHT_CHEVRON: emit_bytes(OpCode::GREATER); break;
        case GREATER_EQUAL: emit_bytes(OpCode::GREATER_EQUAL); break;
        case PLUS: emit_bytes(OpCode::ADD); break;
        case MINUS: emit_bytes(OpCode::SUBSTRACT); break;
        case STAR: emit_bytes(OpCode::MULTIPLY); break;
        case SLASH: emit_bytes(OpCode::DIVIDE); break;
        default: unreachable();
    }
    return TypeInfo{};
}

[[nodiscard]] inline constexpr u8 Parser::argument_list() {
    u8 arg_count = 0;
    do {
        if (check(RIGHT_PAREN)) { break; }
        expression();
        if (arg_count == 255) { error("Can't have more than 255 arguments."); }
        ++arg_count;
    } while (match(COMMA));
    consume(RIGHT_PAREN, "Expect ')' after arguments.");
    return arg_count;
}

inline constexpr TypeInfo
Parser::call(TypeInfo lhs, bool /*can_assign*/) noexcept {
    auto arg_count = argument_list();
    emit_bytes(OpCode::CALL, arg_count);
    (void)lhs;
    return TypeInfo{};
}

inline constexpr TypeInfo Parser::grouping(bool /*can_assign*/) noexcept {
    auto result = expression();
    (void)result;
    consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
    return TypeInfo{};
}

inline constexpr TypeInfo Parser::literal(bool /*can_assign*/) noexcept {
    switch (previous.type) {
        case NIL: emit_bytes(OpCode::NIL); break;
        case FALSE: emit_bytes(OpCode::FALSE); break;
        case TRUE: emit_bytes(OpCode::TRUE); break;
        case FLOAT_LITERAL:
        case INTEGER_LITERAL:
        case STRING_LITERAL: emit_constant(previous.value); break;
        default: unreachable();
    }
    return TypeInfo{};
}

inline constexpr i32
Parser::resolve_local(Compiler& compiler, const Token& name) noexcept {
    // TODO: use ranges, enumerate+reverse
    // for (const auto& local : compiler.locals | std::views::reverse) {
    for (i32 i = compiler.locals.size() - 1; i >= 0; --i) {
        const Local& local = compiler.locals[i];
        if (identifiers_equal(name, local.name)) {
            if (local.depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

inline constexpr TypeInfo
Parser::named_variable(const Token& name, bool can_assign) noexcept {
    OpCode get_op{};
    OpCode set_op{};
    auto is_const = true;
    auto idx = resolve_local(*current_compiler, name);
    if (idx != -1) {
        get_op = OpCode::GET_LOCAL;
        set_op = OpCode::SET_LOCAL;
        is_const = current_compiler->locals[idx].is_const;
    } else {
        idx = identifier_global_index(name);
        get_op = OpCode::GET_GLOBAL;
        set_op = OpCode::SET_GLOBAL;
        is_const = false;
    }
    if (can_assign && match(EQUAL)) {
        auto rhs = expression();
        if (is_const) { error("Immutable assignment target."); }
        (void)rhs;
        emit_var_length_instruction(set_op, idx);
    } else {
        emit_var_length_instruction(get_op, idx);
    }
    return TypeInfo{};
}

inline constexpr TypeInfo Parser::variable(bool can_assign) noexcept {
    return named_variable(previous, can_assign);
}

inline constexpr TypeInfo Parser::unary(bool /*can_assign*/) noexcept {
    auto token_type = previous.type;
    auto rhs = parse_precedence(Precedence::UNARY);
    switch (token_type) {
        case TokenType::BANG: emit_bytes(OpCode::NOT); break;
        case TokenType::MINUS: emit_bytes(OpCode::NEGATE); break;
        default: unreachable();
    }
    (void)rhs;
    return TypeInfo{};
}

inline constexpr TypeInfo Parser::block(bool /*can_assign*/) noexcept {
    begin_scope();
    bool has_final_expression = false;
    while (!check(RIGHT_BRACE) && !check(END_OF_FILE)) {
        const auto last = statement_or_expression();
        if (last != StatementType::STATEMENT) {
            switch (current.type) {
                case RIGHT_BRACE: has_final_expression = true; break;
                case SEMICOLON:
                    advance();
                    emit_bytes(OpCode::POP);
                    continue;
                default:
                    if (last == StatementType::EXPRESSION_WITH_BLOCK) {
                        emit_bytes(OpCode::POP);
                        continue;
                    }
                    error("Expect ';' or '}' after expression inside block.");
            }
        }
    }
    consume(RIGHT_BRACE, "Expect '}' after block.");
    if (!has_final_expression) { emit_bytes(OpCode::NIL); }
    end_scope();
    return TypeInfo{};
}

inline constexpr TypeInfo Parser::if_expr(bool can_assign) noexcept {
    consume(LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(RIGHT_PAREN, "Expect ')' after condition.");
    auto then_jump = emit_jump(OpCode::JUMP_IF_FALSE);
    emit_bytes(OpCode::POP);
    consume(LEFT_BRACE, "Expect '{' before body.");
    // TODO: should we realy pass can_assign? or save and restore
    auto then_result = block(can_assign);
    (void)then_result;
    auto else_jump = emit_jump(OpCode::JUMP);
    patch_jump(then_jump);
    emit_bytes(OpCode::POP);
    if (match(ELSE)) {
        consume(LEFT_BRACE, "Expect '{' before body.");
        // TODO: should we realy pass can_assign?
        auto else_result = block(can_assign);
        (void)else_result;
    } else {
        emit_bytes(OpCode::NIL);
    }
    patch_jump(else_jump);
    return TypeInfo{};
}

inline constexpr TypeInfo Parser::loop_expr(bool can_assign) noexcept {
    Loop loop{};
    begin_loop(loop, true);
    consume(LEFT_BRACE, "Expect '{' after 'loop'.");
    // TODO: should we realy pass can_assign? or save and restore
    auto result = block(can_assign);
    (void)result;  // TODO: make sure no final expression as it is meaningless
    emit_bytes(OpCode::POP);
    emit_loop(loop.start);
    end_loop();
    return TypeInfo{};
}

inline TypeInfo Parser::fn_expr(bool /*can_assign*/) noexcept {
    function(FunctionType::FUNCTION, nullptr);
    return TypeInfo{};
}

inline constexpr TypeInfo
Parser::and_(TypeInfo lhs, bool /*can_assign*/) noexcept {
    auto end_jump = emit_jump(OpCode::JUMP_IF_FALSE);
    emit_bytes(OpCode::POP);
    auto rhs = parse_precedence(Precedence::AND);
    (void)lhs;
    (void)rhs;
    patch_jump(end_jump);
    return TypeInfo{};
}

inline constexpr TypeInfo
Parser::or_(TypeInfo lhs, bool /*can_assign*/) noexcept {
    auto else_jump = emit_jump(OpCode::JUMP_IF_FALSE);
    auto end_jump = emit_jump(OpCode::JUMP);
    patch_jump(else_jump);
    emit_bytes(OpCode::POP);
    auto rhs = parse_precedence(Precedence::OR);
    (void)lhs;
    (void)rhs;
    patch_jump(end_jump);
    return TypeInfo{};
}

inline constexpr const ParseRule& Parser::get_rule(TokenType token_type
) noexcept {
    return ParseRules::get_rule(token_type);
}

inline constexpr ParseResult Parser::parse_precedence(Precedence precedence
) noexcept {
    advance();
    auto token_type = previous.type;
    const ParsePrefixFn prefix_rule = get_rule(previous.type).prefix;
    if (prefix_rule == nullptr) {
        error("Expect expression.");
        return ParseResult{.token_type = ERROR, .type_info = {}};
    }
    const bool can_assign = precedence <= Precedence::ASSIGNMENT;
    auto result = std::invoke(prefix_rule, *this, can_assign);
    while (precedence <= get_rule(current.type).precedence) {
        advance();
        token_type = previous.type;
        const ParseInfixFn infix_rule = get_rule(previous.type).infix;
        result = std::invoke(infix_rule, *this, result, can_assign);
    }
    if (can_assign && match(EQUAL)) {
        error("Invalid assignment target.");
    }
    return ParseResult{.token_type = token_type, .type_info = result};
}

inline size_t Parser::identifier_global_index(const Token& name) noexcept {
    auto identifier = Value{make_string(
        parent_vm,
        !parent_vm.get_options().allow_pointer_to_source_content,
        name.lexeme
    )};
    auto* index = parent_vm.global_indices.get(identifier);
    if (index != nullptr) { return static_cast<size_t>(index->as_int()); }
    return parent_vm.define_global(identifier, Value{val_none});
}

inline constexpr void Parser::add_local(Token name, bool is_const) noexcept {
    if (current_compiler->locals.size() == 256) {
        error("Too many local variables in function.");
        return;
    }
    current_compiler->locals.emplace_back(name, -1, is_const);
}

inline constexpr void Parser::declare_variable(bool is_const) noexcept {
    if (current_compiler->scope_depth == 0) { return; }
    const auto& name = previous;
    // TODO: use ranges
    // for (const auto& local : current_compiler->locals | std::views::reverse)
    // {
    for (auto i = current_compiler->locals.size() - 1; i >= 0; --i) {
        const auto& local = current_compiler->locals[i];
        if (local.depth != -1 && local.depth < current_compiler->scope_depth) {
            break;
        }
        if (identifiers_equal(name, local.name)) {
            error("Already a variable with this name in this scope.");
        }
    }
    add_local(name, is_const);
}

inline constexpr size_t Parser::parse_variable(const char* error_message
) noexcept {
    auto is_const = previous.type != VAR;
    consume(TokenType::IDENTIFIER, error_message);
    // TODO: rework code for following function that handle locals and if test
    // that follows to exit early.
    declare_variable(is_const);
    if (current_compiler->scope_depth > 0) { return 0; }
    return identifier_global_index(previous);
}

inline constexpr void Parser::mark_initialized() noexcept {
    if (current_compiler->scope_depth == 0) { return; }
    current_compiler->locals.back().depth = current_compiler->scope_depth;
}

inline constexpr void Parser::define_variable(size_t global) noexcept {
    if (current_compiler->scope_depth > 0) {
        mark_initialized();
        return;
    }
    emit_var_length_instruction(OpCode::DEFINE_GLOBAL, global);
}

inline constexpr ParseResult Parser::expression() noexcept {
    return parse_precedence(Precedence::ASSIGNMENT);
}

inline void Parser::function(FunctionType type, Token* name) noexcept {
    Compiler compiler;
    begin_compiler(compiler, type, name);
    begin_scope();
    consume(LEFT_PAREN, "Expect '(' after function name.");
    do {
        if (check(RIGHT_PAREN)) { break; }
        ++current_compiler->function->arity;
        if (current_compiler->function->arity > 255) {
            error_at_current("Can't have more than 255 parameters.");
        }
        // TODO: need in out or inout
        auto constant = parse_variable("Expect parameter name.");
        define_variable(constant);
    } while (match(COMMA));
    consume(RIGHT_PAREN, "Expect ')' after parameters.");
    consume(LEFT_BRACE, "Expect '{' before function body.");
    // TODO: make sure we can pass true here
    auto block_result = block(true);
    (void)block_result;
    // end_scope(); // TODO: not necessary?
    auto* function = end_compiler();
    emit_var_length_instruction(
        OpCode::CONSTANT,
        add_constant(Value{function})
    );
}

inline void Parser::fn_declaration() noexcept {
    auto global_idx = parse_variable("Expect funtion name.");
    mark_initialized();
    function(FunctionType::FUNCTION, &previous);
    define_variable(global_idx);
}

inline constexpr void Parser::var_declaration() noexcept {
    auto global_idx = parse_variable("Expect variable name.");
    if (match(TokenType::EQUAL)) {
        expression();
    } else {
        if (current_compiler->scope_depth > 0) {
            error("Local variable should be initialized in declaration.");
        }
        // TODO: allow forward declaration  for globals but mark as
        // uninitialized and not nil
        emit_bytes(OpCode::NIL);
    }
    consume(TokenType::SEMICOLON, "Expect ';' after variable declaration.");
    define_variable(global_idx);
}

inline constexpr void Parser::while_statement() noexcept {
    Loop loop{};
    begin_loop(loop);
    consume(LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(RIGHT_PAREN, "Expect ')' after condition.");
    auto exit_jump = emit_jump(OpCode::JUMP_IF_FALSE);
    emit_bytes(OpCode::POP);
    consume(LEFT_BRACE, "Expect '{' after 'while (...)'.");
    // TODO: make sure it is ok to pass true for can_assign. I beleive the
    // passed value is irelevant.
    const auto block_result = block(true);
    (void)block_result;
    emit_bytes(OpCode::POP);
    emit_loop(loop.start);
    patch_jump(exit_jump);
    emit_bytes(OpCode::POP);
    end_loop();
}

inline constexpr void Parser::break_statement() noexcept {
    if (current_compiler->innermost_loop == nullptr) {
        error("Can't use 'break' outside of a loop.");
    }
    const bool is_loop_expr = current_compiler->innermost_loop->is_loop_expr;
    if (is_loop_expr) {
        if (match(SEMICOLON)) {
            emit_bytes(OpCode::NIL);
        } else {
            expression();
            consume(SEMICOLON, "Expect ; after break return expression.");
        }
    } else {
        consume(SEMICOLON, "Expect ; after 'break'.");
    }
    // TODO: extract to emit_pop_innermost_loop_locals()
    size_t loop_local_count = 0;
    for (size_t i = current_compiler->locals.size() - 1;
         i >= 0
         && current_compiler->locals[i].depth
                > current_compiler->innermost_loop->scope_depth;
         --i) {
        if (!is_loop_expr) { emit_bytes(OpCode::POP); }
        ++loop_local_count;
    }
    if (is_loop_expr && loop_local_count > 0) {
        emit_var_length_instruction(OpCode::END_SCOPE, loop_local_count);
    }
    // jump with placeholder opcode that will need patching later
    (void)emit_jump(OpCode::END);
}

inline constexpr void Parser::continue_statement() noexcept {
    if (current_compiler->innermost_loop == nullptr) {
        error("Can't use 'continue' outside of a loop.");
    }
    consume(SEMICOLON, "Expect ; after 'continue'.");
    // TODO: make reusable in discard_locals()
    for (size_t i = current_compiler->locals.size() - 1;
         i >= 0
         && current_compiler->locals[i].depth
                > current_compiler->innermost_loop->scope_depth;
         --i) {
        emit_bytes(OpCode::POP);
    }
    emit_loop(current_compiler->innermost_loop->start);
}

// inline constexpr void Parser::expression_statement() noexcept {
//     const bool is_block_expr = check_block_expression();
//     expression();
//     if (is_block_expr) {
//         (void)match(SEMICOLON);
//     } else {
//         consume(TokenType::SEMICOLON, "Expect ';' after expression.");
//     }
//     emit_bytes(OpCode::POP);
// }

inline constexpr void Parser::return_statement() noexcept {
    if (current_compiler->function_type == FunctionType::SCRIPT) {
        error("Can't return from top-level code.");
    }
    if (match(SEMICOLON)) {
        emit_return();
    } else {
        expression();
        consume(SEMICOLON, "Expect ';' after return value.");
        emit_bytes(OpCode::RETURN);
    }
}

inline constexpr void Parser::synchronize() noexcept {
    panic_mode = false;
    while (current.type != TokenType::END_OF_FILE) {
        if (previous.type == TokenType::SEMICOLON) { return; }
        switch (current.type) {
            using enum TokenType;
            case STRUCT:
            case FN:
            case LET:
            case VAR:
            case IF:
            case MATCH:
            case LOOP:
            case WHILE:
            case FOR:
            case RETURN:
            case IMPORT: return;
            default:;  // Do nothing.
        }
        advance();
    }
}

inline constexpr StatementType Parser::statement_or_expression() noexcept {
    using enum StatementType;
    if (match(SEMICOLON)) { return STATEMENT; }
    if (match(FN)) {
        if (check(IDENTIFIER)) {
            fn_declaration();
            return STATEMENT;
        }
        fn_expr(true);
        return EXPRESSION_WITHOUT_BLOCK;
    }
    if (match(VAR) || match(LET)) {
        var_declaration();
        return STATEMENT;
    }
    if (match(RETURN)) {
        return_statement();
        return STATEMENT;
    }
    if (match(WHILE)) {
        while_statement();
        return STATEMENT;
    }
    if (match(BREAK)) {
        break_statement();
        return STATEMENT;
    }
    if (match(CONTINUE)) {
        continue_statement();
        return STATEMENT;
    }
    auto result = expression();
    return result.is_block_expr() ? EXPRESSION_WITH_BLOCK
                                  : EXPRESSION_WITHOUT_BLOCK;
}

inline constexpr void Parser::statement() noexcept {
    auto parsed = statement_or_expression();
    switch (parsed) {
        using enum StatementType;
        case STATEMENT: break;
        case EXPRESSION_WITHOUT_BLOCK:
            consume(TokenType::SEMICOLON, "Expect ';' after expression.");
            emit_bytes(OpCode::POP);
            break;
        case EXPRESSION_WITH_BLOCK:
            (void)match(TokenType::SEMICOLON);
            emit_bytes(OpCode::POP);
            break;
    }
    if (panic_mode) { synchronize(); }
}

}  // namespace tx
