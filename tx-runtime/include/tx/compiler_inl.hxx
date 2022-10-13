#pragma once

#include "tx/common.hxx"
#include "tx/compiler.hxx"
#include "tx/debug.hxx"
#include "tx/scanner.hxx"
#include "tx/utils.hxx"

#include <functional>

namespace tx {

inline constexpr bool Parser::compile() noexcept {
    advance();
    expression();
    consume(TokenType::END_OF_FILE, "Expect end of expression.");
    end_compiler();
    return !had_error;
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

inline constexpr void Parser::error_at_current(std::string_view message) noexcept {
    error_at(current, message);
}

inline constexpr void Parser::advance() noexcept {
    previous = current;
    for (;;) {
        current = scanner.scan_token();
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

[[nodiscard]] inline constexpr Chunk& Parser::current_chunk() const noexcept {
    return chunk_;
}

template <typename... Ts>
    requires((std::is_nothrow_constructible_v<ByteCode, Ts>) && ...)
inline constexpr void Parser::emit_bytes(Ts... bytes) noexcept {
    current_chunk().write(tvm, previous.line, bytes...);
}

inline constexpr void Parser::emit_return() noexcept {
    // emit_byte(OpCode::NIL);
    emit_bytes(OpCode::RETURN);
}

inline constexpr void Parser::emit_constant(Value value) noexcept {
    current_chunk().write_constant(tvm, previous.line, value);
}

// [[nodiscard]]
inline constexpr void Parser::end_compiler() noexcept {
    emit_return();
    if constexpr (HAS_DEBUG_FEATURES) {
        if (tvm.get_options().print_bytecode) {
            if (!had_error) {
                // disassemble_chunk(current_chunk(), function->get_name());
                disassemble_chunk(current_chunk(), "code");
            }
        }
    }
    // return function;
}

inline constexpr void Parser::binary(bool /*can_assign*/) noexcept {
    auto token_type = previous.type;
    auto rule = get_rule(token_type);
    parse_precedence(static_cast<Precedence>(to_underlying(rule.precedence) + 1)
    );
    switch (token_type) {
        using enum TokenType;
        case PLUS: emit_bytes(OpCode::ADD); break;
        case MINUS: emit_bytes(OpCode::SUBSTRACT); break;
        case STAR: emit_bytes(OpCode::MULTIPLY); break;
        case SLASH: emit_bytes(OpCode::DIVIDE); break;
        default: unreachable();
    }
}

inline constexpr void Parser::grouping(bool /*can_assign*/) noexcept {
    expression();
    consume(TokenType::RIGHT_PAREN, "Expect ')' after expression.");
}

// constexpr void Parser::number(bool  /*can_assign*/) noexcept {
inline constexpr void Parser::literal(bool  /*can_assign*/) noexcept {
    const Value value = previous.value;
    emit_constant(value);
}

inline constexpr void Parser::unary(bool /*can_assign*/) noexcept {
    auto token_type = previous.type;
    parse_precedence(Precedence::UNARY);
    switch (token_type) {
        case TokenType::MINUS: emit_bytes(OpCode::NEGATE); break;
        default: unreachable();
    }
}

inline constexpr void Parser::parse_precedence(Precedence precedence) noexcept {
    advance();
    const ParseFn prefix_rule = get_rule(previous.type).prefix;
    if (prefix_rule == nullptr) {
        error("Except expression.");
        return;
    }
    const bool can_assign = precedence <= Precedence::ASSIGNMENT;
    std::invoke(prefix_rule, *this, can_assign);
    while (precedence <= get_rule(current.type).precedence) {
        advance();
        const ParseFn infix_rule = get_rule(previous.type).infix;
        std::invoke(infix_rule, *this, can_assign);
    }
}

inline constexpr const ParseRule& Parser::get_rule(TokenType token_type) noexcept {
    return ParseRules::get_rule(token_type);
}

inline constexpr void Parser::expression() noexcept {
    parse_precedence(Precedence::ASSIGNMENT);
}

// void Compiler::emit_loop(index_t loop_start) noexcept {
//     emit_byte(OpCode::LOOP);
//     const auto offset = static_cast<isize>(current_chunk().code.size())
//                         - loop_start + 2;
//     if (offset > std::numeric_limits<u16>::max()) {
//         parser.error("Loop body too large.");
//     }
//     emit_byte(static_cast<u8>((offset >> 8) & 0xff));
//     emit_byte(static_cast<u8>(offset & 0xff));
// }
//
// [[nodiscard]] i32 Compiler::emit_jump(OpCode instruction) noexcept {
//     emit_byte(instruction);
//     emit_byte(static_cast<u8>(0xff));
//     emit_byte(static_cast<u8>(0xff));
//     return current_chunk().code.size() - 2;
// }

// void Compiler::emit_constant(Value value) noexcept {
//     emit_bytes(OpCode::CONSTANT, make_constant(value));
// }
//
// void Compiler::patch_jump(i32 offset) noexcept {
//     // -2 to adjust for bytecode for the jump offset itself.
//     const auto jump = current_chunk().code.size() - offset - 2;
//     if (jump > std::numeric_limits<u16>::max()) {
//         parser.error("Too much code to jump over.");
//     }
//     current_chunk().code[offset].value_index = static_cast<u8>(
//         (jump >> 8) & 0xff
//     );
//     current_chunk().code[offset + 1].value_index = static_cast<u8>(jump &
//     0xff);
// }
//
// [[nodiscard]] u8 Compiler::make_constant(Value value) noexcept {
//     const auto index = current_chunk().add_constant(value);
//     if (index > std::numeric_limits<u8>::max()) {
//         parser.error("Too many constants in one chunk.");
//         return 0;
//     }
//     return static_cast<u8>(index);
// }
//
// constexpr void Compiler::begin_scope() noexcept { scope_depth++; }
//
// void Compiler::end_scope() noexcept {
//     scope_depth--;
//     while (!locals.empty() && locals.back().depth > scope_depth) {
//         if (locals.back().is_captured) {
//             emit_byte(OpCode::CLOSE_UPVALUE);
//         } else {
//             emit_byte(OpCode::POP);
//         }
//         locals.pop_back();
//     }
// }
//
// void Compiler::add_local(Token name) noexcept {
//     if (locals.size() == UINT8_COUNT) {
//         parser.error("Too many local variables in function.");
//         return;
//     }
//     locals.emplace_back(name, -1, false);
// }
//
// void Compiler::declare_variable(Token name) noexcept {
//     if (scope_depth == 0) { return; }
//     for (const auto& local : locals | std::views::reverse) {
//         if (local.depth != -1 && local.depth < scope_depth) { break; }
//         if (identifiers_equal(name, local.name)) {
//             parser.error("Already a variable with this name in this scope.");
//         }
//     }
//     add_local(name);
// }
//
// void Compiler::mark_initialized() noexcept {
//     if (scope_depth == 0) { return; }
//     locals.back().depth = scope_depth;
// }
//
// void Compiler::define_variable(u8 global) noexcept {
//     if (scope_depth > 0) {
//         mark_initialized();
//         return;
//     }
//     emit_bytes(OpCode::DEFINE_GLOBAL, global);
// }
//
// [[nodiscard]] u8 Compiler::identifier_constant(Token& name) noexcept {
//     return make_constant(Value{copy_string(parser.vm, name.lexeme)});
// }
//
// [[nodiscard]] isize Compiler::resolve_local(const Token& name) noexcept {
//     for (i32 i = locals.size() - 1; i >= 0; --i) {
//         const Local& local = locals[i];
//         if (identifiers_equal(name, local.name)) {
//             if (local.depth == -1) {
//                 parser.error("Can't read local variable in its own
//                 initializer."
//                 );
//             }
//             return i;
//         }
//     }
//     return -1;
// }
//
// [[nodiscard]] constexpr isize
// Compiler::add_upvalue(u8 index, bool is_local) noexcept {
//     const auto upvalue_count = function->upvalue_count;
//     for (auto i = 0; i < upvalue_count; ++i) {
//         const auto& upvalue = upvalues[static_cast<std::size_t>(i)];
//         if (upvalue.index == index && upvalue.is_local == is_local) {
//             return i;
//         }
//     }
//     if (upvalue_count == UINT8_COUNT) {
//         parser.error("Too many closure variables in function.");
//         return 0;
//     }
//     upvalues[static_cast<std::size_t>(upvalue_count)].is_local = is_local;
//     upvalues[static_cast<std::size_t>(upvalue_count)].index = index;
//     return function->upvalue_count++;
// }
//
// [[nodiscard]] constexpr isize Compiler::resolve_upvalue(const Token& name
// ) noexcept {
//     if (enclosing == nullptr) { return -1; }
//     const auto local = enclosing->resolve_local(name);
//     if (local != -1) {
//         const auto u8_local = static_cast<u8>(local);
//         enclosing->locals[u8_local].is_captured = true;
//         return add_upvalue(u8_local, true);
//     }
//     const auto upvalue = enclosing->resolve_upvalue(name);
//     if (upvalue != -1) {
//         return add_upvalue(static_cast<std::uint8_t>(upvalue), false);
//     }
//     return -1;
// }

}  // namespace tx
