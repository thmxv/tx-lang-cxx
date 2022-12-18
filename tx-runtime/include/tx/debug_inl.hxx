#pragma once

#include "tx/common.hxx"
#include "tx/debug.hxx"
//
#include "tx/chunk.hxx"
#include "tx/formatting.hxx"
#include "tx/object.hxx"
#include "tx/scanner.hxx"
#include "tx/value.hxx"

#include <fmt/core.h>
#include <fmt/format.h>

namespace tx {

// clang-format off
inline constexpr std::array opcode_name_table = {
    // NOLINTNEXTLINE(*-macro-usage)
    #define TX_OPCODE(name, length, _) #name,
    #include "tx/opcodes.inc"
    #undef TX_OPCODE
};

inline constexpr std::array token_name_table = {
    // NOLINTNEXTLINE(*-macro-usage)
    #define TX_TOKEN(name) #name,
    #include "tx/tokens.inc"
    #undef TX_TOKEN
};
// clang-format on

inline const char* get_opcode_name(OpCode instruction) {
    return gsl::at(opcode_name_table, to_underlying(instruction));
}

inline const char* get_token_name(TokenType tok) {
    return gsl::at(token_name_table, to_underlying(tok));
}

inline void
disassemble_chunk(const Chunk& chunk, std::string_view name) noexcept {
    fmt::print(FMT_STRING("=={:=^40s}==\n"), name);
    for (const auto* ptr = chunk.code.cbegin(); ptr < chunk.code.cend();) {
        ptr = disassemble_instruction(chunk, ptr);
    }
}

template <u32 N>
[[nodiscard]] inline const ByteCode*
constant_instruction(const ByteCode* ptr, const Chunk& chunk) noexcept {
    const OpCode instruction = ptr->as_opcode();
    const auto* name = get_opcode_name(instruction);
    assert(N == get_byte_count_following_opcode(instruction));
    const auto constant_idx = read_multibyte_operand<N>(std::next(ptr));
    fmt::print(
        FMT_STRING("{:18s} {:4d} '{}'\n"),
        name,
        constant_idx,
        chunk.constants[size_cast(constant_idx)]
    );
    return std::next(ptr, 1 + N);
}

template <u32 N>
[[nodiscard]] inline const ByteCode*
closure_instruction(const ByteCode* ptr, const Chunk& chunk) noexcept {
    const OpCode instruction = ptr->as_opcode();
    std::advance(ptr, 1);
    const auto* name = get_opcode_name(instruction);
    assert(N == get_byte_count_following_opcode(instruction));
    const auto constant_idx = read_multibyte_operand<N>(ptr);
    std::advance(ptr, N);
    fmt::print(
        FMT_STRING("{:18s} {:4d} '{}'\n"),
        name,
        constant_idx,
        chunk.constants[size_cast(constant_idx)]
    );
    const auto& function =
        chunk.constants[constant_idx].as_object().template as<ObjFunction>();
    for (size_t i = 0; i < function.upvalue_count; ++i) {
        auto [is_local, index, len] = read_closure_operand(ptr);
        fmt::print(
            FMT_STRING("{:04d}      |                       {:s} {:d}\n"),
            size_cast(std::distance(chunk.code.cbegin(), ptr)),
            is_local ? "local" : "upvalue",
            index
        );
        std::advance(ptr, len);
    }
    return ptr;
}

template <u32 N>
[[nodiscard]] inline const ByteCode* var_length_instruction(const ByteCode* ptr
) noexcept {
    const OpCode instruction = ptr->as_opcode();
    const auto* name = get_opcode_name(instruction);
    assert(N == get_byte_count_following_opcode(instruction));
    const auto constant_idx = read_multibyte_operand<N>(std::next(ptr));
    fmt::print(FMT_STRING("{:18s} {:4d}\n"), name, constant_idx);
    return std::next(ptr, 1 + N);
}

[[nodiscard]] inline const ByteCode*
jump_instruction(const ByteCode* ptr, size_t sign, size_t offset) {
    assert(sign == -1 || sign == 1);
    const OpCode instruction = ptr->as_opcode();
    const auto* name = get_opcode_name(instruction);
    assert(2 == get_byte_count_following_opcode(instruction));
    const auto jump = static_cast<u16>(read_multibyte_operand<2>(std::next(ptr))
    );
    fmt::print(
        FMT_STRING("{:18s} {:4d} -> {:d}\n"),
        name,
        offset,
        offset + 1 + 2 + sign * jump
    );
    return std::next(ptr, 1 + 2);
}

[[nodiscard]] inline const ByteCode* simple_instruction(const ByteCode* ptr
) noexcept {
    const OpCode instruction = ptr->as_opcode();
    const auto* name = get_opcode_name(instruction);
    assert(0 == get_byte_count_following_opcode(instruction));
    fmt::print(FMT_STRING("{:18s}\n"), name);
    return std::next(ptr);
}

inline const ByteCode*
disassemble_instruction(const Chunk& chunk, const ByteCode* ptr) noexcept {
    const auto offset = size_cast(std::distance(chunk.code.cbegin(), ptr));
    fmt::print(FMT_STRING("{:04d} "), offset);
    const auto line = chunk.get_line(offset);
    if (offset > 0 && line == chunk.get_line(offset - 1)) {
        fmt::print(FMT_STRING("   | "));
    } else {
        fmt::print(FMT_STRING("{:4d} "), line);
    }
    const OpCode instruction = ptr->as_opcode();
    switch (instruction) {
        using enum OpCode;
        case NIL:
        case TRUE:
        case FALSE:
        case POP:
        case EQUAL:
        case NOT_EQUAL:
        case GREATER:
        case LESS:
        case GREATER_EQUAL:
        case LESS_EQUAL:
        case ADD:
        case SUBSTRACT:
        case MULTIPLY:
        case DIVIDE:
        case NOT:
        case NEGATE:
        case RETURN:
        case END: return simple_instruction(ptr);
        case CONSTANT: return constant_instruction<1>(ptr, chunk);
        case CONSTANT_LONG: return constant_instruction<3>(ptr, chunk);
        case GET_GLOBAL:
        case SET_GLOBAL:
        case DEFINE_GLOBAL:
        case GET_LOCAL:
        case SET_LOCAL:
        case GET_UPVALUE:
        case SET_UPVALUE:
        case CALL:
        case END_SCOPE: return var_length_instruction<1>(ptr);
        case GET_GLOBAL_LONG:
        case SET_GLOBAL_LONG:
        case DEFINE_GLOBAL_LONG:
        case GET_LOCAL_LONG:
        case SET_LOCAL_LONG:
        case GET_UPVALUE_LONG:
        case SET_UPVALUE_LONG:
        case END_SCOPE_LONG: return var_length_instruction<3>(ptr);
        case JUMP:
        case JUMP_IF_FALSE: return jump_instruction(ptr, 1, offset);
        case LOOP: return jump_instruction(ptr, -1, offset);
        case CLOSURE: return closure_instruction<1>(ptr, chunk);
        case CLOSURE_LONG: return closure_instruction<3>(ptr, chunk);
    }
    unreachable();
}

inline void print_token(const Token& token) noexcept {
    static int line = -1;
    if (token.line != line) {
        fmt::print(FMT_STRING("{:4d} "), token.line);
        line = token.line;
    } else {
        fmt::print(FMT_STRING("   | "));
    }
    // TODO: trim, remove newlines and other perturbing chars, ...
    fmt::print(
        FMT_STRING("{:16s} '{:s}' "),
        std::string_view(get_token_name(token.type)),
        token.lexeme
    );
    if (!token.value.is_none()) { fmt::print(FMT_STRING("{}"), token.value); }
    fmt::print(FMT_STRING("\n"));
}

inline void print_tokens(VM& tvm, std::string_view source) noexcept {
    Scanner scanner(tvm, source);
    for (;;) {
        const Token token = scanner.scan_token();
        print_token(token);
        if (token.type == TokenType::END_OF_FILE) { break; }
    }
}

}  // namespace tx
