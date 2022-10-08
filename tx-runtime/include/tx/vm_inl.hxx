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

TX_VM_CONSTEXPR inline InterpretResult VM::interpret(
    const Chunk& chunk
) noexcept {
    return run(chunk);
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

void VM::print_stack() const noexcept {
    fmt::print(FMT_STRING("          "));
    for (const auto& slot : stack) {
        fmt::print(FMT_STRING("[ {} ]"), slot);
    }
    fmt::print(FMT_STRING("\n"));
}

void VM::print_instruction() const noexcept {
    disassemble_instruction(
        *chunk_ptr,
        static_cast<size_t>(
            std::distance(chunk_ptr->code.data(), instruction_ptr)
        )
    );
}

constexpr void VM::debug_trace() const noexcept {
    if constexpr (HAS_DEBUG_FEATURES) {
        if (options.trace_execution) {
            print_stack();
            print_instruction();
        }
    }
}

TX_VM_CONSTEXPR inline InterpretResult VM::run(const Chunk& chunk) noexcept {
    // clang-format off
    #ifdef TX_ENABLE_COMPUTED_GOTO
        __extension__
        static void* dispatch_table[] = {
            #define TX_OPCODE(name, _) &&L_opcode_##name,
            #include "tx/opcodes.inc"
            #undef TX_OPCODE
        };
        #define TX_VM_DISPATCH TX_VM_BREAK();
        #define TX_VM_CASE(name) L_opcode_##name
        #define TX_VM_BREAK() \
            do { \
                debug_trace(); \
                instruction = read_byte().as_opcode(); \
                (__extension__( \
                    {goto *dispatch_table[to_underlying(instruction)];} \
                )); \
            } while (false)
    #else
        #define TX_VM_DISPATCH  \
            debug_trace(); \
            instruction = read_byte().as_opcode(); \
            switch (instruction)
        #define TX_VM_CASE(name) case name
        #define TX_VM_BREAK() break
    #endif
    // clang-format on
    chunk_ptr = &chunk;
    instruction_ptr = chunk_ptr->code.data();
    for (;;) {
        OpCode instruction; 
        TX_VM_DISPATCH {
            using enum OpCode;
            TX_VM_CASE(CONSTANT): {
                push(read_constant(false));
                TX_VM_BREAK();
            }
            TX_VM_CASE(CONSTANT_LONG): {
                push(read_constant(true));
                TX_VM_BREAK();
            }
            TX_VM_CASE(ADD): {
                if (binary_op<double, std::plus>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(SUBSTRACT): {
                if (binary_op<double, std::minus>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(MULTIPLY): {
                if (binary_op<double, std::multiplies>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(DIVIDE): {
                if (binary_op<double, std::divides>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(NEGATE): {
                push(-pop());
                TX_VM_BREAK();
            }
            TX_VM_CASE(RETURN): {
                fmt::print(FMT_STRING("{}\n"), pop());
                return InterpretResult::OK;
            }
            TX_VM_CASE(END): {
                unreachable();
            }
        }
    }
    unreachable();
    return InterpretResult::RUNTIME_ERROR;
    // clang-format off
    #undef TX_VM_LOOP
    #undef TX_VM_CASE
    #undef TX_VM_CONTINUE
    // clang-format on
}

}  // namespace tx
