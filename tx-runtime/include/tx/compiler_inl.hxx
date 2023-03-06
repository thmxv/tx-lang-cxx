#pragma once

#include "tx/compiler.hxx"
//
#include "tx/chunk.hxx"
#include "tx/common.hxx"
#include "tx/debug.hxx"
#include "tx/object.hxx"
#include "tx/scanner.hxx"
#include "tx/type.hxx"
#include "tx/utils.hxx"
#include "tx/vm.hxx"
#include <fmt/format.h>
#include <gsl/util>

#include <limits>
#include <optional>
#include <ranges>
#include <functional>
#include <tuple>

namespace tx {

inline constexpr bool identifiers_equal(const Token& lhs, const Token& rhs) {
    return lhs.lexeme == rhs.lexeme;
}

// clang-format off
inline constexpr std::array opcode_stack_effect_table = {
    // NOLINTNEXTLINE(*-macro-usage)
    #define TX_OPCODE(name, length, stack_effect) stack_effect,
    #include "tx/opcodes.inc"
    #undef TX_OPCODE
};
// clang-format on

inline constexpr i32 get_opcode_stack_effect(OpCode opc, size_t operand) {
    using enum OpCode;
    if (opc == CALL || opc == END_SCOPE || opc == END_SCOPE_LONG) {
        return -gsl::narrow_cast<i32>(operand);
    }
    return gsl::at(opcode_stack_effect_table, to_underlying(opc));
}

inline constexpr void Local::destroy(VM& tvm) noexcept {
    type_set.destroy(tvm);
}

inline constexpr void Global::destroy(VM& tvm) noexcept {
    type_set.destroy(tvm);
}

inline constexpr bool is_block_expr(Token::Type token_type) noexcept {
    switch (token_type) {
        using enum Token::Type;
        case LEFT_BRACE:
        case IF:
        case LOOP: return true;
        default: return false;
    };
}

[[nodiscard]] inline ObjFunction*
// NOLINTNEXTLINE(*-easily-swappable-parameters)
Parser::compile(std::string_view file_path, std::string_view source) noexcept {
    assert(scanner == nullptr);  // NOLINT(*-decay)
    Scanner lex(parent_vm, source);
    scanner = &lex;
    module_file_path = file_path;
    Compiler comp{};
    begin_compiler(comp, FunctionType::SCRIPT, std::nullopt);
    advance();
    while (!match(Token::Type::END_OF_FILE)) { statement(); }
    emit_instruction(OpCode::NIL);
    auto& fun = end_compiler();
    comp.destroy(parent_vm);
    // TODO: Maybe do not do this at all
    if (!parent_vm.options.allow_end_compile_with_undefined_global) {
        for (const auto& [name, idx] : parent_vm.global_indices) {
            const auto& global =
                parent_vm.global_signatures[size_cast(idx.as_int())];
            if (!global.is_defined) {
                error(
                    FMT_STRING("Global variable '{}' declared but not defined."
                    ),
                    name.as_object().as<ObjString>()
                );
            }
        }
    }
    module_file_path = std::string_view();
    scanner = nullptr;
    return had_error ? nullptr : &fun;
}

inline void Parser::error_at_impl_begin(const Token& token) noexcept {
    panic_mode = true;
    had_error = true;
    fmt::print(stderr, FMT_STRING("Syntax error"));
    if (token.type == Token::Type::END_OF_FILE) {
        fmt::print(stderr, FMT_STRING(" at end: "));
    } else if (token.type == Token::Type::ERROR) {
        fmt::print(stderr, FMT_STRING(": "));
    } else {
        fmt::print(stderr, FMT_STRING(" at '{:s}': "), token.lexeme);
    }
}

// FIXME: Optionally underline other token and span more than 1 line
inline void Parser::error_at_impl_end(const Token& token) noexcept {
    assert(  // NOLINT(*-decay)
             // cppcheck-suppress mismatchingContainerExpression
        scanner->source.cbegin() <= token.lexeme.cbegin()
        // cppcheck-suppress mismatchingContainerExpression
        // cppcheck-suppress knownConditionTrueFalse ; FIXME: False positive?
        && token.lexeme.cend() <= scanner->source.cend()
    );
    fmt::print(stderr, FMT_STRING("\n"));
    size_t line_number = token.line;
    bool is_at_end = (token.type == Token::Type::END_OF_FILE)
                     || (token.type == Token::Type::ERROR
                         // cppcheck-suppress mismatchingContainerExpression
                         && token.lexeme.cend() == scanner->source.cend());
    if (is_at_end) { line_number -= 1; }
    const auto line = get_text_of_line(scanner->source, line_number);
    i64 col = 1;
    std::size_t len = 1;
    bool mark_start = true;
    // FIXME: utf8 distance not byte distance
    // cppcheck-suppress mismatchingContainerExpression
    col = std::distance(line.cbegin(), token.lexeme.cbegin());
    len = token.lexeme.size();
    auto blanks = col;
    if (col < 0 || is_at_end) {
        // cppcheck-suppress mismatchingContainerExpression
        col = std::distance(line.cbegin(), token.lexeme.cend()) - 1;
        blanks = col + 1 - static_cast<i64>(len);
        if (blanks < 0) {
            len = static_cast<std::size_t>(col) + 1;
            blanks = col + 1 - static_cast<i64>(len);
        }
        mark_start = false;
    }
    if (col < 0) { col = 0; }
    if (token.type == Token::Type::END_OF_FILE) { col -= 1; }

    fmt::print(
        stderr,
        FMT_STRING("  {:s}:{:d}:{:d}\n"),
        module_file_path.empty() ? "<unknown>" : module_file_path,
        line_number,
        col + 1
    );
    const auto line_num_digits = count_digit(token.line) + 2;
    fmt::print(
        stderr,
        FMT_STRING("{0: >{2}d} | {1:s}\n"),
        line_number,
        line,
        line_num_digits
    );
    const auto* fmt = mark_start ? "{0: >{3}} | {1: >{4}}{2:~<{5}}\n"
                                 : "{0: >{3}} | {1: >{4}}{2:~>{5}}\n";
    fmt::print(
        stderr,
        fmt::runtime(fmt),
        "",
        "",
        "^",
        line_num_digits,
        blanks,
        len
    );
}

inline constexpr void Parser::advance() noexcept {
    previous = current;
    for (;;) {
        current = scanner->scan_token();
        if constexpr (HAS_DEBUG_FEATURES) {
            if (parent_vm.options.print_tokens) { print_token(current); }
        }
        if (current.type != Token::Type::ERROR) { break; }
        error_at_current(
            FMT_STRING("{:s}"),
            current.value.as_object().as<ObjString>()
        );
    }
}

inline constexpr void
Parser::consume(Token::Type type, std::string_view message) noexcept {
    if (!match(type)) { error_at_current(FMT_STRING("{:s}"), message); }
}

[[nodiscard]] inline constexpr bool Parser::check(Token::Type type
) const noexcept {
    return current.type == type;
}

[[nodiscard]] inline constexpr bool Parser::match(Token::Type type) noexcept {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

[[nodiscard]] inline constexpr Chunk& Parser::current_chunk() const noexcept {
    return current_compiler->function->chunk;
}

[[nodiscard]] inline constexpr size_t Parser::add_constant(Value value
) noexcept {
    // if (had_error) { return -1; }
    const auto* existing = current_compiler->constant_indices.get(value);
    if (existing != nullptr) { return size_cast(existing->as_int()); }
    // TODO: error if too much constants
    parent_vm.push(value);
    auto idx = current_chunk().add_constant(parent_vm, value);
    current_compiler->constant_indices
        .set(parent_vm, value, Value{gsl::narrow_cast<int_t>(idx)});
    parent_vm.pop();
    return idx;
}

// TODO: pass long and short opcode instead of adding 1
inline constexpr void
Parser::emit_var_length_instruction(OpCode opc, size_t idx) noexcept {
    assert(idx < size_cast(1U << 24U));  // NOLINT(*-decay)
    if (idx < size_cast(1U << 8U)) {     // NOLINT(*-magic-numbers)
        assert(1 == get_byte_count_following_opcode(opc));  // NOLINT(*-decay)
        emit_instruction<1>(opc, idx);
        return;
    }
    if (idx < size_cast(1U << 24U)) {  // NOLINT(*-magic-numbers)
        opc = OpCode(to_underlying(opc) + 1);
        assert(3 == get_byte_count_following_opcode(opc));  // NOLINT(*-decay)
        emit_instruction<3>(opc, idx);
        return;
    }
    unreachable();
}

inline constexpr void Parser::emit_constant(Value value) noexcept {
    auto idx = add_constant(value);
    emit_var_length_instruction(OpCode::CONSTANT, idx);
}

inline constexpr void
Parser::emit_closure(Compiler& compiler, ObjFunction& function) noexcept {
    Expects(function.upvalue_count == compiler.upvalues.size());
    emit_var_length_instruction(
        OpCode::CLOSURE,
        add_constant(Value{&function})
    );
    for (const auto& upvalue : compiler.upvalues) {
        assert(upvalue.index >= 0);                    // NOLINT(*-decay)
        assert(upvalue.index < size_cast(1U << 24U));  // NOLINT(*-decay)
        const usize length = [](usize idx) {
            usize result = 0;
            while (idx != 0) {
                idx >>= 8U;  // NOLINT(*-magic-numbers)
                ++result;
            }
            return result == 0 ? 1 : result;
        }(static_cast<usize>(upvalue.index));
        const u8 flags = static_cast<u8>(
            static_cast<usize>(upvalue.is_local) << 7U | (length & 0b01111111U)
        );
        emit_bytes(flags);
        if (length == 1) {
            current_chunk().write_multibyte_operand<1>(
                parent_vm,
                previous.line,
                upvalue.index
            );
            continue;
        }
        if (length == 2) {
            current_chunk().write_multibyte_operand<2>(
                parent_vm,
                previous.line,
                upvalue.index
            );
            continue;
        }
        if (length == 3) {
            current_chunk().write_multibyte_operand<3>(
                parent_vm,
                previous.line,
                upvalue.index
            );
            continue;
        }
        unreachable();
    }
}

[[nodiscard]] inline constexpr size_t Parser::emit_jump(OpCode instruction
) noexcept {
    emit_instruction<2>(instruction, 0xffff);
    return current_chunk().code.size() - 2;
}

inline constexpr void Parser::emit_loop(size_t loop_start) noexcept {
    auto offset = current_chunk().code.size() - loop_start + 2 + 1;
    if (offset > std::numeric_limits<u16>::max()) {
        error(FMT_STRING("Loop body too large."));
    }
    emit_instruction<2>(OpCode::LOOP, offset);
}

inline constexpr void Parser::patch_jump(size_t offset) noexcept {
    auto jump_distance = current_chunk().code.size() - offset - 2;
    if (jump_distance > std::numeric_limits<u16>::max()) {
        error(FMT_STRING("Too much code to jump over."));
    }
    auto* ptr = &current_chunk().code[offset];
    ::tx::write_multibyte_operand<2>(ptr, jump_distance);
}

inline void Parser::begin_compiler(
    Compiler& compiler,
    FunctionType type,
    std::optional<std::string_view> name_opt
) noexcept {
    compiler.enclosing = current_compiler;
    compiler.function_type = type;
    compiler.function = allocate_object<ObjFunction>(
        parent_vm,
        compiler.locals.size(),
        module_file_path
    );
    current_compiler = &compiler;
    // Reserve first local for methods, use empty string as name to prevent use
    compiler.locals
        .emplace_back(parent_vm, Token{.lexeme = ""}, 0, true, TypeSet{});
    if (type != FunctionType::SCRIPT && name_opt.has_value()) {
        compiler.function->name = make_string(
            parent_vm,
            !parent_vm.options.allow_pointer_to_source_content,
            name_opt.value()
        );
    }
}

[[nodiscard]] inline constexpr ObjFunction& Parser::end_compiler() noexcept {
    emit_instruction(OpCode::RETURN);
    auto& fun = *current_compiler->function;
    if constexpr (HAS_DEBUG_FEATURES) {
        if (parent_vm.get_options().print_bytecode) {
            if (!had_error) {
                disassemble_chunk(current_chunk(), fun.get_display_name());
            }
        }
    }
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
        ++scope_local_count;
        current_compiler->locals.back().destroy(parent_vm);
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
        if (btc.as_opcode() == OpCode::CLOSURE
            || btc.as_opcode() == OpCode::CLOSURE_LONG) {
            const auto constant_idx = [&]() {
                if (btc.as_opcode() == OpCode::CLOSURE) {
                    return read_multibyte_operand<1>(
                        std::next(current_chunk().code.begin(), i + 1)
                    );
                }
                if (btc.as_opcode() == OpCode::CLOSURE_LONG) {
                    return read_multibyte_operand<3>(
                        std::next(current_chunk().code.begin(), i + 1)
                    );
                }
                unreachable();
            }();
            const auto& fun = current_chunk()
                                  .constants[constant_idx]
                                  .as_object()
                                  .as<ObjFunction>();
            i += 1 + 1;
            for (auto j = 0; j < fun.upvalue_count; ++j) {
                auto [is_local, index, len] = read_closure_operand(
                    std::next(current_chunk().code.begin(), i)
                );
                i += len;
            }
            continue;
        }
        if (btc.as_opcode() == OpCode::END) {
            btc = ByteCode{OpCode::JUMP};
            patch_jump(i + 1);
        }
        i += 1 + get_byte_count_following_opcode(btc.as_opcode());
    }
}

inline constexpr void Parser::end_loop() noexcept {
    patch_jumps_in_innermost_loop();
    // TODO: move to better place?
    current_compiler->innermost_loop->type_set.destroy(parent_vm);
    current_compiler->innermost_loop = current_compiler->innermost_loop
                                           ->enclosing;
}

inline constexpr TypeSet
Parser::binary(TypeSet lhs, bool /*can_assign*/) noexcept {
    auto token = previous;
    auto rule = get_rule(token.type);
    auto rhs = parse_precedence(
        static_cast<Precedence>(to_underlying(rule.precedence) + 1)
    );
    auto result = type_check_binary(parent_vm, token.type, lhs, rhs);
    if (result.is_empty()) {
        error_at(token, "Incompatible types in binary operation");
    } else {
        switch (token.type) {
            using enum Token::Type;
            case BANG_EQUAL: emit_instruction(OpCode::NOT_EQUAL); break;
            case EQUAL_EQUAL: emit_instruction(OpCode::EQUAL); break;
            case LEFT_CHEVRON: emit_instruction(OpCode::LESS); break;
            case LESS_EQUAL: emit_instruction(OpCode::LESS_EQUAL); break;
            case RIGHT_CHEVRON: emit_instruction(OpCode::GREATER); break;
            case GREATER_EQUAL: emit_instruction(OpCode::GREATER_EQUAL); break;
            case PLUS: emit_instruction(OpCode::ADD); break;
            case MINUS: emit_instruction(OpCode::SUBSTRACT); break;
            case STAR: emit_instruction(OpCode::MULTIPLY); break;
            case SLASH: emit_instruction(OpCode::DIVIDE); break;
            default: unreachable();
        }
    }
    lhs.destroy(parent_vm);
    rhs.destroy(parent_vm);
    return result;
}

[[nodiscard]] inline constexpr TypeSetArray Parser::argument_list() {
    using enum Token::Type;
    TypeSetArray result{};
    do {
        if (check(RIGHT_PAREN)) { break; }
        result.emplace_back(parent_vm, expression());
        if (result.size() == MAX_FN_PARAMETERS + 1) {
            error(
                FMT_STRING("Can't have more than {} arguments."),
                MAX_FN_PARAMETERS
            );
        }
    } while (match(COMMA));
    consume(RIGHT_PAREN, "Expect ')' after arguments.");
    return result;
}

inline constexpr TypeSet
Parser::call(TypeSet lhs, bool /*can_assign*/) noexcept {
    auto arg_types = argument_list();
    auto result = type_check_call(parent_vm, lhs, arg_types);
    if (result.is_empty()) {
        error(FMT_STRING("Incompatible types in function call."));
    }
    emit_instruction<1>(OpCode::CALL, arg_types.size());
    lhs.destroy(parent_vm);
    // FIXME: Make DynArray::destroy() call destroy on elements when required
    for (auto& type_set : arg_types) { type_set.destroy(parent_vm); }
    arg_types.destroy(parent_vm);
    return result;
}

inline constexpr TypeSet Parser::grouping(bool /*can_assign*/) noexcept {
    auto result = expression();
    consume(Token::Type::RIGHT_PAREN, "Expect ')' after expression.");
    return result;
}

inline constexpr TypeSet Parser::literal(bool /*can_assign*/) noexcept {
    switch (previous.type) {
        using enum Token::Type;
        case NIL:
            emit_instruction(OpCode::NIL);
            // FIXME: Why the cppcheck error? False positive?
            // cppcheck-suppress returnDanglingLifetime
            return {parent_vm, {TypeInfo(TypeInfo::Type::NIL)}};
        case FALSE:
            emit_instruction(OpCode::FALSE);
            // cppcheck-suppress returnDanglingLifetime
            return {parent_vm, {TypeInfo(TypeInfo::Type::BOOL)}};
        case TRUE:
            emit_instruction(OpCode::TRUE);
            // cppcheck-suppress returnDanglingLifetime
            return {parent_vm, {TypeInfo(TypeInfo::Type::BOOL)}};
        case FLOAT_LITERAL:
            emit_constant(previous.value);
            // cppcheck-suppress returnDanglingLifetime
            return {parent_vm, {TypeInfo(TypeInfo::Type::FLOAT)}};
        case INTEGER_LITERAL:
            emit_constant(previous.value);
            // cppcheck-suppress returnDanglingLifetime
            return {parent_vm, {TypeInfo(TypeInfo::Type::INT)}};
        case STRING_LITERAL:
            emit_constant(previous.value);
            // cppcheck-suppress returnDanglingLifetime
            return {parent_vm, {TypeInfo(TypeInfo::Type::STRING)}};
        default: unreachable();
    }
}

inline constexpr i32
Parser::resolve_local(Compiler& compiler, const Token& name) noexcept {
    // TODO: use ranges, enumerate+reverse
    for (auto i = compiler.locals.size() - 1; i >= 0; --i) {
        const Local& local = compiler.locals[i];
        if (identifiers_equal(name, local.name)) {
            assert(local.depth != -1);  // NOLINT(*-decay)
            // if (local.depth == -1) {
            //     error(FMT_STRING(
            //         "Can't read local variable in its own initializer."
            //     ));
            // }
            return gsl::narrow_cast<i32>(i);
        }
    }
    return -1;
}

inline constexpr size_t
Parser::add_upvalue(Compiler& compiler, size_t index, bool is_local) noexcept {
    auto upvalue_count = compiler.function->upvalue_count;
    for (size_t i = 0; i < upvalue_count; ++i) {
        const auto& upvalue = compiler.upvalues[i];
        if (upvalue.index == index && upvalue.is_local == is_local) {
            return i;
        }
    }
    if (upvalue_count == MAX_UPVALUES) {
        error(FMT_STRING("Too much closure variables in function."));
        return 0;
    }
    compiler.upvalues.emplace_back(parent_vm, index, is_local);
    return compiler.function->upvalue_count++;
}

inline constexpr std::tuple<i32, const Local*>
// NOLINTNEXTLINE(*-no-recursion)
Parser::resolve_upvalue(Compiler& compiler, const Token& name) noexcept {
    if (compiler.enclosing == nullptr) { return {-1, nullptr}; }
    if (auto local_idx = resolve_local(*compiler.enclosing, name);
        local_idx != -1) {
        auto& local = compiler.enclosing->locals[local_idx];
        local.is_captured = true;
        return {add_upvalue(compiler, local_idx, true), &local};
    }
    if (auto [upvalue_idx, var] = resolve_upvalue(*compiler.enclosing, name);
        upvalue_idx != -1) {
        return {add_upvalue(compiler, upvalue_idx, false), var};
    }
    return {-1, nullptr};
}

inline constexpr TypeSet
Parser::named_variable(const Token& name, bool can_assign) noexcept {
    OpCode get_op{};
    OpCode set_op{};
    auto is_const = true;
    const TypeSet* type_set = nullptr;
    i32 idx = resolve_local(*current_compiler, name);
    if (idx != -1) {
        get_op = OpCode::GET_LOCAL;
        set_op = OpCode::SET_LOCAL;
        const auto& local = current_compiler->locals[idx];
        is_const = local.is_const;
        type_set = &local.type_set;
    } else if (auto [uidx, var] = resolve_upvalue(*current_compiler, name);
               uidx != -1) {
        idx = uidx;
        get_op = OpCode::GET_UPVALUE;
        set_op = OpCode::SET_UPVALUE;
        is_const = var->is_const;
        type_set = &var->type_set;
    } else if (idx = resolve_global(name); idx != -1) {
        get_op = OpCode::GET_GLOBAL;
        set_op = OpCode::SET_GLOBAL;
        const auto& global = parent_vm.global_signatures[idx];
        is_const = global.is_const;
        type_set = &global.type_set;
        if (current_compiler->scope_depth == 0) {
            if (!parent_vm.global_signatures[idx].is_defined) {
                error(FMT_STRING(
                    "Use of forward declared global before definition."
                ));
            }
        }
    } else {
        error(FMT_STRING("Cannot find value with this name in current scope."));
        return TypeSet{};
    }
    if (can_assign && match(Token::Type::EQUAL)) {
        if (is_const) { error(FMT_STRING("Immutable assignment target.")); }
        auto rhs = expression();
        if (!type_check_assign(*type_set, rhs)) {
            error(FMT_STRING("Incompatible types in assignment."));
        }
        rhs.destroy(parent_vm);
        emit_var_length_instruction(set_op, idx);
    } else {
        emit_var_length_instruction(get_op, idx);
    }
    return type_set->copy(parent_vm);
}

inline constexpr TypeSet Parser::variable(bool can_assign) noexcept {
    return named_variable(previous, can_assign);
}

// FIXME: type check
inline constexpr TypeSet Parser::unary(bool /*can_assign*/) noexcept {
    auto token_type = previous.type;
    auto rhs = parse_precedence(Precedence::UNARY);
    TypeSet result{};
    switch (token_type) {
        case Token::Type::BANG:
            result.add(parent_vm, TypeInfo(TypeInfo::Type::BOOL));
            // result = type_check_not(rhs);
            emit_instruction(OpCode::NOT);
            break;
        case Token::Type::MINUS:
            emit_instruction(OpCode::NEGATE);
            result = std::move(rhs);
            break;
        default: unreachable();
    }
    // cppcheck-suppress[accessMoved]
    rhs.destroy(parent_vm);
    return result;
}

// NOLINTNEXTLINE(*-no-recursion)
inline constexpr TypeSet Parser::block_no_scope() noexcept {
    using enum Token::Type;
    // TODO: fix double move
    std::optional<TypeSet> final_expr_opt = std::nullopt;
    while (!check(RIGHT_BRACE) && !check(END_OF_FILE)) {
        auto parsed = statement_or_expression();
        if (parsed.kind != ParseResult::Kind::STATEMENT) {
            switch (current.type) {
                case RIGHT_BRACE: {
                    final_expr_opt = std::move(parsed.type_set);
                    break;
                }
                case SEMICOLON: {
                    advance();
                    emit_instruction(OpCode::POP);
                    break;
                }
                default: {
                    if (parsed.kind == ParseResult::Kind::EXPRESSION_WITH_BLOCK
                        && parsed.type_set.is_nil()) {
                        emit_instruction(OpCode::POP);
                        break;
                    }
                    error(FMT_STRING(
                        "Expect ';' or '}}' after expression inside block."
                    ));
                }
            }
        }
        // cppcheck-suppress[accessMoved]
        parsed.destroy(parent_vm);
    }
    consume(RIGHT_BRACE, "Expect '}' after block.");
    if (!final_expr_opt.has_value()) {
        emit_instruction(OpCode::NIL);
        // cppcheck-suppress[returnDanglingLifetime]
        return {parent_vm, {TypeInfo(TypeInfo::Type::NIL)}};
    }
    return std::move(final_expr_opt.value());
}

// NOLINTNEXTLINE(*-no-recursion)
inline constexpr TypeSet Parser::block(bool /*can_assign*/) noexcept {
    begin_scope();
    auto result = block_no_scope();
    end_scope();
    return result;
}

// FIXME: type check
// NOLINTNEXTLINE(*-no-recursion)
inline constexpr TypeSet Parser::if_expr(bool /*can_assign*/) noexcept {
    using enum Token::Type;
    auto cond_type = expression();
    auto then_jump = emit_jump(OpCode::JUMP_IF_FALSE);
    emit_instruction(OpCode::POP);
    consume(LEFT_BRACE, "Expect '{' before if body.");
    auto then_result = block();
    auto else_jump = emit_jump(OpCode::JUMP);
    patch_jump(then_jump);
    emit_instruction(OpCode::POP);
    TypeSet else_result;
    if (match(ELSE)) {
        switch (current.type) {
            case IF:
                advance();
                else_result = if_expr();
                break;
            case LEFT_BRACE:
                advance();
                else_result = block();
                break;
            default:
                error_at_current(FMT_STRING("Expect '{{' before else body."));
        }
    } else {
        else_result = TypeSet{};
        emit_instruction(OpCode::NIL);
    }
    cond_type.destroy(parent_vm);
    then_result.destroy(parent_vm);
    else_result.destroy(parent_vm);
    patch_jump(else_jump);
    // cppcheck-suppress[returnDanglingLifetime]
    return {parent_vm, {TypeInfo{TypeInfo::Type::NIL}}};
}

// FIXME: type check
inline constexpr TypeSet Parser::loop_expr(bool /*can_assign*/) noexcept {
    Loop loop{};
    begin_loop(loop, true);
    consume(Token::Type::LEFT_BRACE, "Expect '{' after 'loop'.");
    auto block_result = block();
    // TODO: make sure no final expression as it is meaningless
    block_result.destroy(parent_vm);
    auto loop_result = std::move(current_compiler->innermost_loop->type_set);
    emit_instruction(OpCode::POP);
    emit_loop(loop.start);
    end_loop();
    return loop_result;
}

inline TypeSet Parser::fn_expr(bool /*can_assign*/) noexcept {
    using enum Token::Type;
    consume(LEFT_PAREN, "Expect '(' after 'fn'.");
    auto params_ret = parameter_list_and_return_type();
    consume(LEFT_BRACE, "Expect '{' before function body.");
    TypeInfo ti_result(params_ret.type_info.copy(parent_vm));
    function_body(FunctionType::FUNCTION, "", std::move(params_ret));
    // cppcheck-suppress[accessMoved]
    params_ret.destroy(parent_vm);
    // cppcheck-suppress[returnDanglingLifetime]
    return {parent_vm, {std::move(ti_result)}};
}

// FIXME: type check
inline constexpr TypeSet
Parser::and_(TypeSet lhs, bool /*can_assign*/) noexcept {
    auto end_jump = emit_jump(OpCode::JUMP_IF_FALSE);
    emit_instruction(OpCode::POP);
    auto rhs = parse_precedence(Precedence::AND);
    lhs.destroy(parent_vm);
    rhs.destroy(parent_vm);
    patch_jump(end_jump);
    return {};
}

// FIXME: type check
inline constexpr TypeSet
Parser::or_(TypeSet lhs, bool /*can_assign*/) noexcept {
    auto else_jump = emit_jump(OpCode::JUMP_IF_FALSE);
    auto end_jump = emit_jump(OpCode::JUMP);
    patch_jump(else_jump);
    emit_instruction(OpCode::POP);
    auto rhs = parse_precedence(Precedence::OR);
    lhs.destroy(parent_vm);
    rhs.destroy(parent_vm);
    patch_jump(end_jump);
    return {};
}

[[nodiscard]] inline constexpr TypeSet
Parser::as(TypeSet lhs, bool /*can_assign*/) noexcept {
    // FIXME: Emit opcode that will test and possibly emit a runtime error
    lhs.destroy(parent_vm);
    return parse_type_set();
}

inline constexpr const ParseRule& Parser::get_rule(Token::Type token_type
) noexcept {
    return ParseRules::get_rule(token_type);
}

inline constexpr void Parser::help_cut_short() noexcept {
    if (previous_statement_was_cut_short) {
        fmt::print(
            FMT_STRING("Help: parentheses are required to parse this as an "
                       "expression.\n")
        );
        previous_statement_was_cut_short = false;
    }
}

inline constexpr TypeSet Parser::parse_prefix_only() noexcept {
    const ParsePrefixFn prefix_rule = get_rule(previous.type).prefix;
    if (prefix_rule == nullptr) {
        error(FMT_STRING("Expect expression."));
        help_cut_short();
        return {};
    }
    // return std::invoke(prefix_rule, *this, true);
    auto type_set = std::invoke(prefix_rule, *this, true);
    previous_statement_was_cut_short = current.type == Token::Type::EQUAL
                                       || Precedence::ASSIGNMENT
                                              <= get_rule(current.type)
                                                     .precedence;
    return type_set;
}

inline constexpr TypeSet Parser::parse_precedence(Precedence precedence
) noexcept {
    advance();
    return parse_precedence_no_advance(precedence);
}

inline constexpr TypeSet Parser::parse_precedence_no_advance(
    Precedence precedence
) noexcept {
    const ParsePrefixFn prefix_rule = get_rule(previous.type).prefix;
    if (prefix_rule == nullptr) {
        error(FMT_STRING("Expect expression."));
        help_cut_short();
        return {};
    }
    const bool can_assign = precedence <= Precedence::ASSIGNMENT;
    auto result = std::invoke(prefix_rule, *this, can_assign);
    previous_statement_was_cut_short = false;
    while (precedence <= get_rule(current.type).precedence) {
        advance();
        const ParseInfixFn infix_rule = get_rule(previous.type).infix;
        result = std::invoke(infix_rule, *this, std::move(result), can_assign);
    }
    if (can_assign && match(Token::Type::EQUAL)) {
        error(FMT_STRING("Invalid assignment target."));
    }
    return result;
}

inline i32 Parser::resolve_global(const Token& name) noexcept {
    auto identifier = Value{make_string(
        parent_vm,
        !parent_vm.get_options().allow_pointer_to_source_content,
        name.lexeme
    )};
    auto* index = parent_vm.global_indices.get(identifier);
    if (index != nullptr) { return gsl::narrow_cast<i32>(index->as_int()); }
    return -1;
}

inline size_t Parser::add_global(Value identifier, Global&& sig) noexcept {
    parent_vm.push(identifier);
    auto result = parent_vm
                      .add_global(identifier, std::move(sig), Value{val_none});
    parent_vm.pop();
    return result;
}

inline size_t Parser::declare_global_variable(
    const Token& name,
    bool is_const,
    TypeSet&& type_set
) noexcept {
    assert(current_compiler->scope_depth == 0);  // NOLINT(*-decay)
    Global sig{.is_const = is_const, .type_set = std::move(type_set)};
    auto identifier = Value{make_string(
        parent_vm,
        !parent_vm.get_options().allow_pointer_to_source_content,
        name.lexeme
    )};
    const auto* idx_ptr = parent_vm.global_indices.get(identifier);
    if (idx_ptr != nullptr) {
        const auto idx = size_cast(idx_ptr->as_int());
        const auto& global = parent_vm.global_signatures[idx];
        if (sig == global) {
            if (!global.is_defined) {
                // definition of a (forward) declared but not yet defined
                // var
            } else if (!parent_vm.options.allow_global_redefinition) {
                error_at(name, FMT_STRING("Redefinition of global variable."));
            }
        } else {
            error_at(name, FMT_STRING("Redeclaration of global variable."));
        }
    }
    if (idx_ptr == nullptr) { return add_global(identifier, std::move(sig)); }
    sig.destroy(parent_vm);
    return size_cast(idx_ptr->as_int());
}

inline constexpr void Parser::add_local(
    const Token& name,
    bool is_const,
    TypeSet&& type_set
) noexcept {
    if (current_compiler->locals.size() == MAX_LOCALS) {
        error(FMT_STRING("Too many local variables in function."));
        return;
    }
    current_compiler->locals
        .emplace_back(parent_vm, name, -1, is_const, std::move(type_set));
}

inline constexpr void Parser::declare_local_variable(
    const Token& name,
    bool is_const,
    TypeSet&& type_set
) noexcept {
    assert(current_compiler->scope_depth > 0);  // NOLINT(*-decay)
    // TODO: use ranges
    for (auto i = current_compiler->locals.size() - 1; i >= 0; --i) {
        const auto& local = current_compiler->locals[i];
        if (local.depth != -1 && local.depth < current_compiler->scope_depth) {
            break;
        }
        if (identifiers_equal(name, local.name)) {
            error_at(
                name,
                FMT_STRING("Already a variable with this name in this scope.")
            );
        }
    }
    add_local(name, is_const, std::move(type_set));
}

inline constexpr i32 Parser::declare_variable(
    const Token& name,
    bool is_const,
    TypeSet&& type_set
) noexcept {
    if (current_compiler->scope_depth > 0) {
        declare_local_variable(name, is_const, std::move(type_set));
        return -1;
    }
    return gsl::narrow_cast<i32>(
        declare_global_variable(name, is_const, std::move(type_set))
    );
}

inline constexpr std::optional<std::tuple<Token, bool>> Parser::parse_variable(
    std::string_view error_message
) noexcept {
    auto is_const = previous.type != Token::Type::VAR;
    if (!match(Token::Type::IDENTIFIER)) {
        error_at_current(FMT_STRING("{:s}"), error_message);
        return std::nullopt;
    }
    return std::make_tuple(previous, is_const);
}

inline constexpr void Parser::mark_initialized(i32 global_idx) noexcept {
    if (current_compiler->scope_depth == 0) {
        assert(global_idx >= 0);  // NOLINT(*-decay)
        parent_vm.global_signatures[global_idx].is_defined = true;
        return;
    }
    current_compiler->locals.back().depth = current_compiler->scope_depth;
}

inline constexpr void Parser::define_variable(i32 global_idx) noexcept {
    mark_initialized(global_idx);
    if (current_compiler->scope_depth == 0) {
        assert(global_idx >= 0);  // NOLINT(*-decay)
        emit_var_length_instruction(OpCode::DEFINE_GLOBAL, global_idx);
    }
}

inline constexpr TypeSet Parser::expression() noexcept {
    return parse_precedence(Precedence::ASSIGNMENT);
}

inline constexpr ParametersAndReturn Parser::parameter_list_and_return_type(
) noexcept {
    using enum Token::Type;
    ParametersAndReturn result;
    do {
        if (check(RIGHT_PAREN)) { break; }
        if (result.parameters.size() == MAX_FN_PARAMETERS) {
            error_at_current(
                FMT_STRING("Can't have more than {} parameters."),
                MAX_FN_PARAMETERS
            );
        }
        // TODO: need in, out or inout (in by default)
        // what about out for immutable types -> error?
        auto is_const = previous.type != OUT;
        consume(IDENTIFIER, "Expect parameter name.");
        const auto name = previous;
        consume(COLON, "Expect ':' after parameter name.");
        result.parameters.emplace_back(parent_vm, name, is_const);
        result.type_info.parameter_types.emplace_back(
            parent_vm,
            parse_type_set()
        );
    } while (match(COMMA));
    consume(RIGHT_PAREN, "Expect ')' after parameters.");
    if (match(MINUS)) {
        consume(RIGHT_CHEVRON, "Expect '>' after '-'.");
        result.type_info.return_type = parse_type_set();
    } else {
        result.type_info.return_type = {
            parent_vm,
            {TypeInfo{TypeInfo::Type::NIL}}};
    }
    return result;
}

// NOLINTNEXTLINE(*-no-recursion)
inline void Parser::function_body(
    FunctionType type,
    std::string_view name,
    ParametersAndReturn&& params_ret
) noexcept {
    Compiler compiler;
    begin_compiler(compiler, type, name);
    begin_scope();
    current_compiler->function->arity = params_ret.parameters.size();
    assert(  // NOLINT(*-decay)
        params_ret.parameters.size()
        == params_ret.type_info.parameter_types.size()
    );
    for (size_t i = 0; i < params_ret.parameters.size(); ++i) {
        declare_local_variable(
            params_ret.parameters[i].name,
            params_ret.parameters[i].is_const,
            std::move(params_ret.type_info.parameter_types[i])
        );
        define_variable(-1);
    }
    auto block_result = block_no_scope();
    if (!type_check_assign(params_ret.type_info.return_type, block_result)) {
        // TODO: Handle block without final expression
        // TODO: More information
        error(FMT_STRING("Incompatible return type."));
    }
    block_result.destroy(parent_vm);
    // end_scope(); // NOTE: Not necessary
    auto& function = end_compiler();
    emit_closure(compiler, function);
    compiler.destroy(parent_vm);
    params_ret.destroy(parent_vm);
}

// NOLINTNEXTLINE(*-no-recursion)
inline void Parser::fn_declaration() noexcept {
    using enum Token::Type;
    auto var_opt = parse_variable("Expect function name.");
    if (!var_opt.has_value()) { return; }
    auto [name, is_const] = var_opt.value();
    consume(LEFT_PAREN, "Expect '(' after function name.");
    auto params_ret = parameter_list_and_return_type();
    auto global_idx = declare_variable(
        name,
        is_const,
        {parent_vm, {TypeInfo{params_ret.type_info.copy(parent_vm)}}}
    );
    if (match(LEFT_BRACE)) {
        mark_initialized(global_idx);
        function_body(
            FunctionType::FUNCTION,
            name.lexeme,
            std::move(params_ret)
        );
        define_variable(global_idx);
        // NOTE: Do not permit trailing ; after inline fn definition (for now)
        // (void)match(SEMICOLON);
    } else {
        consume(SEMICOLON, "Expect '{' or ';' after function declaration.");
        params_ret.destroy(parent_vm);
    }
}

// NOLINTNEXTLINE(*-no-recursion)
inline constexpr TypeInfo Parser::parse_type() noexcept {
    switch (current.type) {
        using enum Token::Type;
        case ANY_TYPE: advance(); return TypeInfo{TypeInfo::Type::ANY};
        case NIL_TYPE: advance(); return TypeInfo{TypeInfo::Type::NIL};
        case BOOL_TYPE: advance(); return TypeInfo{TypeInfo::Type::BOOL};
        case INT_TYPE: advance(); return TypeInfo{TypeInfo::Type::INT};
        case FLOAT_TYPE: advance(); return TypeInfo{TypeInfo::Type::FLOAT};
        case CHAR_TYPE: advance(); return TypeInfo{TypeInfo::Type::CHAR};
        case STR_TYPE: advance(); return TypeInfo{TypeInfo::Type::STRING};
        case FN_TYPE: {
            // FIXME: move to parse_function_type
            advance();
            consume(LEFT_CHEVRON, "Expect '<' after Fn.");
            auto param_types = parse_type_set_list(
                "Expect second '<' before parameter type list"
            );
            consume(COMMA, "Expect ',' after parameters types.");
            auto return_type = parse_type_set();
            consume(RIGHT_CHEVRON, "Expect '>' after function return type.");
            return TypeInfo{
                TypeInfoFunction{
                                 std::move(param_types),
                                 std::move(return_type)}
            };
        }
        default: error_at_current(FMT_STRING("Expect type name")); return {};
    }
}

// NOLINTNEXTLINE(*-no-recursion)
inline constexpr TypeSet Parser::parse_type_set() noexcept {
    TypeSet result;
    do { result.add(parent_vm, parse_type()); } while (match(Token::Type::OR));
    return result;
}

// NOLINTNEXTLINE(*-no-recursion)
inline constexpr TypeSetArray Parser::parse_type_set_list(
    std::string_view message
) noexcept {
    using enum Token::Type;
    consume(LEFT_CHEVRON, message);
    TypeSetArray result;
    do {
        if (check(RIGHT_CHEVRON)) { break; }
        result.push_back(parent_vm, parse_type_set());
    } while (match(COMMA));
    consume(RIGHT_CHEVRON, "Expect '>' after type list.");
    return result;
}

inline constexpr void Parser::var_declaration() noexcept {
    using enum Token::Type;
    auto var_opt = parse_variable("Expect variable name.");
    if (!var_opt.has_value()) { return; }
    auto [name, is_const] = var_opt.value();
    std::optional<TypeSet> decl_type;
    std::optional<TypeSet> expr_type;
    if (match(COLON)) { decl_type = parse_type_set(); }
    if (match(EQUAL)) {
        expr_type = expression();
    } else {
        if (!decl_type.has_value()) {
            error(FMT_STRING("Expect ':' or '=' after variable name."));
        }
        if (current_compiler->scope_depth > 0) {
            error(FMT_STRING(
                // TODO: print var name
                "Local variable should be initialized in declaration."
            ));
        }
    }
    if (decl_type.has_value() && expr_type.has_value()) {
        if (!type_check_assign(decl_type.value(), expr_type.value())) {
            // TODO: More information
            error(FMT_STRING("Incompatible type in variable declaration."));
        }
    }
    if (!had_error) [[likely]] {
        i32 global_idx = -1;
        if (decl_type.has_value()) {
            global_idx = declare_variable(
                name,
                is_const,
                std::move(decl_type.value())
            );
        } else {
            assert(expr_type.has_value());  // NOLINT(*-decay)
            global_idx = declare_variable(
                name,
                is_const,
                std::move(expr_type.value())
            );
        }
        if (expr_type.has_value()) { define_variable(global_idx); }
    }
    consume(SEMICOLON, "Expect ';' after variable declaration.");
    if (decl_type.has_value()) { decl_type->destroy(parent_vm); }
    if (expr_type.has_value()) { expr_type->destroy(parent_vm); }
}

// TODO: typecheck
// NOLINTNEXTLINE(*-no-recursion)
inline constexpr void Parser::while_statement() noexcept {
    using enum Token::Type;
    Loop loop{};
    begin_loop(loop);
    auto cond_type = expression();
    auto exit_jump = emit_jump(OpCode::JUMP_IF_FALSE);
    emit_instruction(OpCode::POP);
    consume(LEFT_BRACE, "Expect '{' before while body.");
    auto block_result = block();
    emit_instruction(OpCode::POP);
    emit_loop(loop.start);
    patch_jump(exit_jump);
    emit_instruction(OpCode::POP);
    end_loop();
    (void)match(SEMICOLON);
    cond_type.destroy(parent_vm);
    block_result.destroy(parent_vm);
}

inline constexpr void Parser::emit_pop_innermost_loop(bool skip_top_expression
) noexcept {
    size_t loop_local_count = 0;
    for (size_t i = current_compiler->locals.size() - 1;
         i >= 0
         && current_compiler->locals[i].depth
                > current_compiler->innermost_loop->scope_depth;
         --i) {
        if (!skip_top_expression) { emit_instruction(OpCode::POP); }
        ++loop_local_count;
    }
    if (skip_top_expression && loop_local_count > 0) {
        emit_var_length_instruction(OpCode::END_SCOPE, loop_local_count);
    }
}

// TODO: typecheck
inline constexpr void Parser::break_statement() noexcept {
    using enum Token::Type;
    if (current_compiler->innermost_loop == nullptr) {
        error(FMT_STRING("Can't use 'break' outside of a loop."));
    }
    auto& loop = *current_compiler->innermost_loop;
    if (loop.is_loop_expr) {
        if (match(SEMICOLON)) {
            emit_instruction(OpCode::NIL);
            loop.type_set.add(parent_vm, TypeInfo{TypeInfo::Type::NIL});
        } else {
            auto type_set = expression();
            loop.type_set.move_all_from(parent_vm, std::move(type_set));
            // cppcheck-suppress[accessMoved]
            type_set.destroy(parent_vm);
            consume(
                SEMICOLON,
                "Expect ';' after expression in 'break' statement."
            );
        }
    } else {
        consume(SEMICOLON, "Expect ';' after 'break'.");
    }
    emit_pop_innermost_loop(loop.is_loop_expr);
    // emit jump with placeholder opcode that will need patching later
    (void)emit_jump(OpCode::END);
}

inline constexpr void Parser::continue_statement() noexcept {
    using enum Token::Type;
    if (current_compiler->innermost_loop == nullptr) {
        error(FMT_STRING("Can't use 'continue' outside of a loop."));
    }
    consume(SEMICOLON, "Expect ; after 'continue'.");
    emit_pop_innermost_loop(false);
    emit_loop(current_compiler->innermost_loop->start);
}

// TODO: typecheck
inline constexpr void Parser::return_statement() noexcept {
    using enum Token::Type;
    if (current_compiler->function_type == FunctionType::SCRIPT) {
        error(FMT_STRING("Can't return from top-level code."));
    }
    if (match(SEMICOLON)) {
        emit_instruction(OpCode::NIL);
    } else {
        auto expr_type = expression();
        consume(SEMICOLON, "Expect ';' after return value.");
        expr_type.destroy(parent_vm);
    }
    emit_instruction(OpCode::RETURN);
}

inline constexpr void Parser::synchronize() noexcept {
    using enum Token::Type;
    panic_mode = false;
    while (current.type != END_OF_FILE) {
        if (previous.type == SEMICOLON) { return; }
        switch (current.type) {
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

inline constexpr ParseResult Parser::expression_maybe_statement() noexcept {
    if (is_block_expr(previous.type)) {
        return {
            .kind = ParseResult::Kind::EXPRESSION_WITH_BLOCK,
            .type_set = parse_prefix_only()};
    }
    return {
        .kind = ParseResult::Kind::EXPRESSION_WITHOUT_BLOCK,
        .type_set = parse_precedence_no_advance(Precedence::ASSIGNMENT)};
}

// NOLINTNEXTLINE(*-no-recursion)
inline constexpr ParseResult Parser::statement_or_expression() noexcept {
    using enum Token::Type;
    // NOTE: Do not allow useless ';' (for now)
    // if (match(SEMICOLON)) { return; }
    if (match(FN)) {
        if (check(IDENTIFIER)) {
            fn_declaration();
            return {.kind = ParseResult::Kind::STATEMENT};
        }
        if (!check(LEFT_PAREN)) {
            error(FMT_STRING("Expect function name or '(' after 'fn'."));
        }
        return expression_maybe_statement();
    }
    if (match(VAR) || match(LET)) {
        var_declaration();
        return {.kind = ParseResult::Kind::STATEMENT};
    }
    if (match(RETURN)) {
        return_statement();
        return {.kind = ParseResult::Kind::STATEMENT};
    }
    if (match(WHILE)) {
        while_statement();
        return {.kind = ParseResult::Kind::STATEMENT};
    }
    if (match(BREAK)) {
        break_statement();
        return {.kind = ParseResult::Kind::STATEMENT};
    }
    if (match(CONTINUE)) {
        continue_statement();
        return {.kind = ParseResult::Kind::STATEMENT};
    }
    advance();
    return expression_maybe_statement();
}

inline constexpr void Parser::statement() noexcept {
    using enum Token::Type;
    auto parsed = statement_or_expression();
    if (parsed.kind != ParseResult::Kind::STATEMENT) {
        emit_instruction(OpCode::POP);
        if (parsed.kind == ParseResult::Kind::EXPRESSION_WITH_BLOCK
            && parsed.type_set.is_nil()) {
            (void)match(SEMICOLON);
        } else {
            if (!match(SEMICOLON)) {
                error_at_current(FMT_STRING("Expect ';' after expression."));
                help_cut_short();
            }
        }
    }
    parsed.destroy(parent_vm);
    if (panic_mode) { synchronize(); }
}

}  // namespace tx
