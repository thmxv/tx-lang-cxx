#pragma once

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

[[nodiscard]] inline size_t
constant_instruction(std::string_view name, const Chunk& chunk, size_t offset) {
    const u8 constant_idx = chunk.code[offset + 1].as_u8();
    fmt::print(
        "{:16s} {:4d} '{}'\n",
        name,
        constant_idx,
        chunk.constants[constant_idx]
    );
    return offset + 2;
}

[[nodiscard]] inline size_t long_constant_instruction(
    std::string_view name,
    const Chunk& chunk,
    size_t offset
) {
    const auto constant_idx =
        static_cast<u32>(chunk.code[offset + 1].as_u8())
        | (static_cast<u32>(chunk.code[offset + 2].as_u8()) << 8U)
        | (static_cast<u32>(chunk.code[offset + 3].as_u8()) << 16U);
    fmt::print(
        "{:16s} {:4d} '{}'\n",
        name,
        constant_idx,
        chunk.constants[static_cast<size_t>(constant_idx)]
    );
    return offset + 4;
}

[[nodiscard]] inline size_t
simple_instruction(std::string_view name, size_t offset) noexcept {
    fmt::print(FMT_STRING("{:s}\n"), name);
    return offset + 1;
}

[[nodiscard]] inline size_t
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
            return constant_instruction("CONSTANT", chunk, offset);
        case OpCode::CONSTANT_LONG:
            return long_constant_instruction("CONSTANT", chunk, offset);
        case OpCode::RETURN: return simple_instruction("RETURN", offset);
    }
    fmt::print(
        FMT_STRING("Unknown opcode {:d}\n"),
        static_cast<u8>(instruction)
    );
    return offset + 1;
}

}  // namespace tx
