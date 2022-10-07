#pragma once

#include "chunk.hxx"
#include "debug.hxx"
#include "value.hxx"

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
        case OpCode::CONSTANT:
            return constant_instruction("CONSTANT", chunk, offset, false);
        case OpCode::CONSTANT_LONG:
            return constant_instruction("CONSTANT", chunk, offset, true);
        case OpCode::ADD: return simple_instruction("ADD", offset);
        case OpCode::SUBSTRACT: return simple_instruction("SUBSTRACT", offset);
        case OpCode::MULTIPLY: return simple_instruction("MULTIPLY", offset);
        case OpCode::DIVIDE: return simple_instruction("DIVIDE", offset);
        case OpCode::NEGATE: return simple_instruction("NEGATE", offset);
        case OpCode::RETURN: return simple_instruction("RETURN", offset);
    }
    fmt::print(
        FMT_STRING("Unknown opcode {:d}\n"),
        static_cast<u8>(instruction)
    );
    return offset + 1;
}

}  // namespace tx
