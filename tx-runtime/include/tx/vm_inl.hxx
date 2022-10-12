#pragma once

#include "tx/chunk.hxx"
#include "tx/common.hxx"
#include "tx/compiler.hxx"
#include "tx/debug.hxx"
#include "tx/utils.hxx"
#include "tx/vm.hxx"

#include <fmt/format.h>

#include <cstddef>
#include <iterator>
#include <functional>

namespace tx {

constexpr ByteCode VM::read_byte() noexcept { return *instruction_ptr++; }

constexpr Value VM::read_constant(bool is_long) noexcept {
    const auto [constant_idx, new_ptr] = read_constant_index(
        instruction_ptr,
        is_long
    );
    instruction_ptr = new_ptr;
    return chunk_ptr->constants[constant_idx];
}

TX_VM_CONSTEXPR InterpretResult VM::interpret(std::string_view source
) noexcept {
    if constexpr (HAS_DEBUG_FEATURES) {
        if (options.print_tokens) { print_tokens(source); }
    }
    Chunk chunk;
    Parser current_parser(*this, source, chunk);
    parser = &current_parser;
    if (!parser->compile()) {
        chunk.destroy(*this);
        return InterpretResult::COMPILE_ERROR;
    }
    auto result = run(chunk);
    chunk.destroy(*this);
    parser = nullptr;
    return result;
}

[[nodiscard]] constexpr bool VM::negate_op() noexcept {
    const auto operand = pop();
    Value result;
    if (operand.is_int()) {
        result = Value(-operand.as_int());
    } else if (operand.is_float()) {
        result = Value(-operand.as_float());
    } else {
        return true;
    }
    push(result);
    return false;
}

template <template <typename> typename Op>
[[nodiscard]] constexpr bool VM::binary_op() noexcept {
    // if (!peek(0).is_float() || !peek(1).is_float()) {
    //     runtime_error("Operands must be numbers.");
    //     return true;
    // }
    const auto rhs = pop();
    const auto lhs = pop();
    Value result;
    if (lhs.is_int() && rhs.is_int()) {
        Op<int_t> bop;
        result = Value(bop(lhs.as_int(), rhs.as_int()));
    } else {
        float_t left = lhs.is_int() ? static_cast<float_t>(lhs.as_int())
                                    : lhs.as_float();
        float_t rght = rhs.is_int() ? static_cast<float_t>(rhs.as_int())
                                    : rhs.as_float();
        Op<float_t> bop;
        result = Value(bop(left, rght));
    }
    push(result);
    return false;
}

void VM::print_stack() const noexcept {
    fmt::print(FMT_STRING("          "));
    for (const auto& slot : stack) { fmt::print(FMT_STRING("[ {} ]"), slot); }
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

TX_VM_CONSTEXPR InterpretResult VM::run(const Chunk& chunk) noexcept {
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
            TX_VM_CASE(CONSTANT) : {
                push(read_constant(false));
                TX_VM_BREAK();
            }
            TX_VM_CASE(CONSTANT_LONG) : {
                push(read_constant(true));
                TX_VM_BREAK();
            }
            TX_VM_CASE(ADD) : {
                if (binary_op<std::plus>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(SUBSTRACT) : {
                if (binary_op<std::minus>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(MULTIPLY) : {
                if (binary_op<std::multiplies>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(DIVIDE) : {
                if (binary_op<std::divides>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(NEGATE) : {
                if (negate_op()) { return InterpretResult::RUNTIME_ERROR; }
                TX_VM_BREAK();
            }
            TX_VM_CASE(RETURN) : {
                fmt::print(FMT_STRING("{}\n"), pop());
                return InterpretResult::OK;
            }
            TX_VM_CASE(END) : { unreachable(); }
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
