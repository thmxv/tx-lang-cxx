#pragma once

#include "chunk.hxx"

namespace tx {

void disassemble_chunk(const Chunk& chunk, std::string_view name) noexcept;

size_t disassemble_instruction(const Chunk& chunk, size_t offset) noexcept;

void print_tokens(std::string_view source) noexcept;

}  // namespace tx
