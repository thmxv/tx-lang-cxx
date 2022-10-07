#pragma once

#include "tx/chunk.hxx"
#include "tx/common.hxx"
#include "tx/debug.hxx"
#include "tx/vm.hxx"

#include <fmt/format.h>

#include <cstddef>
#include <iterator>
#include <functional>

namespace tx {

inline constexpr ByteCode VM::read_byte() noexcept {
    return *instruction_ptr++;
}

inline constexpr Value VM::read_constant(bool is_long) noexcept {
    const auto [constant_idx, new_ptr] = read_constant_index(
        instruction_ptr,
        is_long
    );
    instruction_ptr = new_ptr;
    return chunk_ptr->constants[constant_idx];
}

constexpr inline InterpretResult VM::interpret(const Chunk& chunk) noexcept {
    chunk_ptr = &chunk;
    instruction_ptr = chunk_ptr->code.data();
    return run();
}

template<typename T, template<typename> typename Op>
[[nodiscard]] constexpr
bool VM::binary_op() noexcept {
    // if (!peek(0).is_number() || !peek(1).is_number()) {
    //     runtime_error("Operands must be numbers.");
    //     return true;
    // }
    const double rhs = pop(); //.as_number();
    const double lhs = pop(); //.as_number();
    const Op<double> op;
    push(Value{T{op(lhs, rhs)}});
    return false;
}

constexpr inline InterpretResult VM::run() noexcept {
    while (true) {
        if constexpr (HAS_DEBUG_FEATURES) {
            if (trace_execution) {
                fmt::print(FMT_STRING("          "));
                for (const auto& slot : stack) {
                    fmt::print(FMT_STRING("[ {} ]"), slot);
                }
                fmt::print(FMT_STRING("\n"));
                disassemble_instruction(
                    *chunk_ptr,
                    static_cast<size_t>(
                        std::distance(chunk_ptr->code.data(), instruction_ptr)
                    )
                );
            }
        }
        const OpCode instruction = read_byte().as_opcode();
        switch (instruction) {
            case OpCode::CONSTANT: {
                push(read_constant(false));
                break;
            }
            case OpCode::CONSTANT_LONG: {
                push(read_constant(true));
                break;
            }
            case OpCode::ADD: {
                if (binary_op<double, std::plus>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                break;
            }
            case OpCode::SUBSTRACT: {
                if (binary_op<double, std::minus>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                break;
            }
            case OpCode::MULTIPLY: {
                if (binary_op<double, std::multiplies>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                break;
            }
            case OpCode::DIVIDE: {
                if (binary_op<double, std::divides>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                break;
            }
            case OpCode::NEGATE: {
                push(-pop());
                break;
            }
            case OpCode::RETURN: {
                fmt::print(FMT_STRING("{}\n"), pop());
                return InterpretResult::OK;
            }
        }
    }
}

}  // namespace tx
