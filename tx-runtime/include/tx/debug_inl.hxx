#pragma once

#include "tx/chunk.hxx"
#include "tx/debug.hxx"
#include "tx/formatting.hxx"
#include "tx/scanner.hxx"
#include "tx/value.hxx"

#include <fmt/core.h>
#include <fmt/format.h>

namespace tx {

inline void
disassemble_chunk(const Chunk& chunk, std::string_view name) noexcept {
    fmt::print(FMT_STRING("== {:s} ==\n"), name);
    for (size_t offset = 0; offset < chunk.code.size();) {
        offset = disassemble_instruction(chunk, offset);
    }
}

[[nodiscard]] inline size_t constant_instruction(
    std::string_view name,
    const Chunk& chunk,
    size_t offset,
    bool is_long
) noexcept {
    const auto [constant_idx, new_ptr] = read_constant_index(
        &chunk.code[offset + 1],
        is_long
    );
    fmt::print(
        "{:8s} {:4d} '{}'\n",
        name,
        constant_idx,
        chunk.constants[constant_idx]
    );
    return static_cast<size_t>(std::distance(chunk.code.data(), new_ptr));
}

[[nodiscard]] inline size_t var_length_instruction(
    std::string_view name,
    const Chunk& chunk,
    size_t offset,
    bool is_long
) noexcept {
    const auto [constant_idx, new_ptr] = read_constant_index(
        &chunk.code[offset + 1],
        is_long
    );
    fmt::print("{:8s} {:4d}\n", name, constant_idx);
    return static_cast<size_t>(std::distance(chunk.code.data(), new_ptr));
}

[[nodiscard]] inline size_t
simple_instruction(std::string_view name, size_t offset) noexcept {
    fmt::print(FMT_STRING("{:8s}\n"), name);
    return offset + 1;
}

// clang-format off
inline constexpr std::array opcode_name_table = {
    // NOLINTNEXTLINE(*-macro-usage)
    #define TX_OPCODE(name, _) #name,
    #include "tx/opcodes.inc"
    #undef TX_OPCODE
};
// clang-format on

inline size_t
disassemble_instruction(const Chunk& chunk, size_t offset) noexcept {
    fmt::print(FMT_STRING("{:04d} "), offset);
    const auto line = chunk.get_line(offset);
    if (offset > 0 && line == chunk.get_line(offset - 1)) {
        fmt::print(FMT_STRING("   | "));
    } else {
        fmt::print(FMT_STRING("{:4d} "), line);
    }
    const OpCode instruction = chunk.code[offset].as_opcode();
    const auto* name = opcode_name_table[to_underlying(instruction)];
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
        case END: return simple_instruction(name, offset);
        case CONSTANT:
        case GET_GLOBAL:
        case SET_GLOBAL:
        case DEFINE_GLOBAL:
            return constant_instruction(name, chunk, offset, false);
        case CONSTANT_LONG:
        case GET_GLOBAL_LONG:
        case SET_GLOBAL_LONG:
        case DEFINE_GLOBAL_LONG:
            return constant_instruction(name, chunk, offset, true);
        case GET_LOCAL:
        case SET_LOCAL:
            return var_length_instruction(name, chunk, offset, false);
        case GET_LOCAL_LONG:
        case SET_LOCAL_LONG:
            return var_length_instruction(name, chunk, offset, true);
    }
    unreachable();
}

// clang-format off
inline constexpr std::array token_name_table = {
    // NOLINTNEXTLINE(*-macro-usage)
    #define TX_TOKEN(name) #name,
    #include "tx/tokens.inc"
    #undef TX_TOKEN
};
// clang-format on

inline void print_tokens(VM& tvm, std::string_view source) noexcept {
    Scanner scanner(tvm, source);
    int line = -1;
    for (;;) {
        Token token = scanner.scan_token();
        if (token.line != line) {
            fmt::print(FMT_STRING("{:4d} "), token.line);
            line = token.line;
        } else {
            fmt::print("   | ");
        }
        fmt::print(
            FMT_STRING("{:16s} '{:s}' {}\n"),
            std::string_view(token_name_table[to_underlying(token.type)]),
            token.lexeme,
            token.value
        );
        if (token.type == TokenType::END_OF_FILE) { break; }
    }
}

}  // namespace tx
