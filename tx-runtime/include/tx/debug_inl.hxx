#pragma once

#include "tx/chunk.hxx"
#include "tx/debug.hxx"
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

[[nodiscard]] inline size_t
simple_instruction(std::string_view name, size_t offset) noexcept {
    fmt::print(FMT_STRING("{:8s}\n"), name);
    return offset + 1;
}

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
    switch (instruction) {
        using enum OpCode;
        case CONSTANT:
            return constant_instruction("CONSTANT", chunk, offset, false);
        case CONSTANT_LONG:
            return constant_instruction("CONSTANT", chunk, offset, true);
        case ADD: return simple_instruction("ADD", offset);
        case SUBSTRACT: return simple_instruction("SUBSTRACT", offset);
        case MULTIPLY: return simple_instruction("MULTIPLY", offset);
        case DIVIDE: return simple_instruction("DIVIDE", offset);
        case NEGATE: return simple_instruction("NEGATE", offset);
        case RETURN: return simple_instruction("RETURN", offset);
        case END: return simple_instruction("END", offset);
    }
    unreachable();
}

inline void print_tokens(std::string_view source) noexcept {
    Scanner scanner(source);
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
            FMT_STRING("{:2d} '{:s}' {}\n"),
            to_underlying(token.type),
            token.lexeme,
            token.value
        );
        if (token.type == TokenType::END_OF_FILE) { break; }
    }
}

}  // namespace tx
