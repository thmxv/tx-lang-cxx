#pragma once

#include "chunk.hxx"

namespace tx {

const char* get_opcode_name(OpCode instruction);

void disassemble_chunk(const Chunk& chunk, std::string_view name) noexcept;

const ByteCode*
disassemble_instruction(const Chunk& chunk, const ByteCode* ptr) noexcept;

struct Token;
void print_token(const Token& token) noexcept;
void print_tokens(VM& tvm, std::string_view source) noexcept;

}  // namespace tx
