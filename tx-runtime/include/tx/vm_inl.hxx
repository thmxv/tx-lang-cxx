#pragma once

#include "tx/vm.hxx"
//
#include "tx/chunk.hxx"
#include "tx/common.hxx"
#include "tx/compiler.hxx"
#include "tx/debug.hxx"
#include "tx/object.hxx"
#include "tx/utils.hxx"

#include <fmt/format.h>

#include <cstddef>
#include <iterator>
#include <functional>

namespace tx {

constexpr VM::~VM() noexcept {
    global_indices.destroy(*this);
    global_values.destroy(*this);
    strings.destroy(*this);
    free_objects(*this, objects);
}

inline constexpr ByteCode VM::read_byte() noexcept {
    auto result = *instruction_ptr;
    instruction_ptr = std::next(instruction_ptr);
    return result;
}

inline constexpr size_t VM::read_multibyte_index(bool is_long) noexcept {
    const auto [constant_idx, new_ptr] = ::tx::read_multibyte_index(
        instruction_ptr,
        is_long
    );
    instruction_ptr = new_ptr;
    return constant_idx;
}

[[nodiscard]] inline constexpr u16 VM::read_short() noexcept {
    instruction_ptr = std::next(instruction_ptr, 2);
    return static_cast<u16>(
        (instruction_ptr[-2].value << 8) | instruction_ptr[-1].value
    );
}

inline constexpr Value VM::read_constant(bool is_long) noexcept {
    const auto constant_idx = read_multibyte_index(is_long);
    return chunk_ptr->constants[constant_idx];
}

inline constexpr void VM::reset_stack() noexcept { stack.clear(); }

inline void VM::runtime_error_impl() noexcept {
    fmt::print(stderr, "\n");
    // for (const auto& frame : frames | std::views::reverse) {
    //     const auto& function = *frame.closure->function;
    //     auto instruction = frame.instruction_pointer
    //                        - function.chunk.code.data() - 1;
    auto instruction = instruction_ptr - chunk_ptr->code.data() - 1;
    fmt::print(
        stderr,
        "[line {:d}] in {:s}\n",
        chunk_ptr->get_line(static_cast<i32>(instruction)),
        "script"
    );
    // }
    reset_stack();
}

inline TX_VM_CONSTEXPR InterpretResult VM::interpret(std::string_view source
) noexcept {
    if constexpr (HAS_DEBUG_FEATURES) {
        if (options.print_tokens) { print_tokens(*this, source); }
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

[[nodiscard]] inline constexpr bool VM::negate_op() noexcept {
    Value result{val_none};
    if (peek(0).is_int()) {
        result = Value(-pop().as_int());
    } else if (peek(0).is_float()) {
        result = Value(-pop().as_float());
    } else {
        runtime_error("Operand must be a number.");
        return true;
    }
    push(result);
    return false;
}

template <template <typename> typename Op>
[[nodiscard]] constexpr bool VM::binary_op() noexcept {
    if (!peek(0).is_number() || !peek(1).is_number()) {
        runtime_error("Operands must be numbers.");
        return true;
    }
    const auto rhs = pop();
    const auto lhs = pop();
    Value result{val_none};
    if (lhs.is_int() && rhs.is_int()) {
        Op<int_t> bop;
        result = Value(bop(lhs.as_int(), rhs.as_int()));
    } else {
        float_t left = lhs.as_float_force();
        float_t rght = rhs.as_float_force();
        Op<float_t> bop;
        result = Value(bop(left, rght));
    }
    push(result);
    return false;
}

inline void VM::print_stack() const noexcept {
    fmt::print(FMT_STRING("          "));
    for (const auto& slot : stack) { fmt::print(FMT_STRING("[ {} ]"), slot); }
    fmt::print(FMT_STRING("\n"));
}

inline void VM::print_instruction() const noexcept {
    disassemble_instruction(
        *chunk_ptr,
        static_cast<size_t>(
            std::distance(chunk_ptr->code.data(), instruction_ptr)
        )
    );
}

inline constexpr void VM::debug_trace() const noexcept {
    if constexpr (HAS_DEBUG_FEATURES) {
        if (options.trace_execution) {
            print_stack();
            print_instruction();
        }
    }
}

inline TX_VM_CONSTEXPR InterpretResult VM::run(const Chunk& chunk) noexcept {
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
        // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
        #define TX_VM_CASE(name) case name
        // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
        #define TX_VM_BREAK() break
    #endif
    // clang-format on
    chunk_ptr = &chunk;
    instruction_ptr = chunk_ptr->code.data();
    for (;;) {
        // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
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
            TX_VM_CASE(NIL) : {
                push(Value(val_nil));
                TX_VM_BREAK();
            }
            TX_VM_CASE(TRUE) : {
                push(Value(true));
                TX_VM_BREAK();
            }
            TX_VM_CASE(FALSE) : {
                push(Value(false));
                TX_VM_BREAK();
            }
            TX_VM_CASE(POP) : {
                pop();
                TX_VM_BREAK();
            }
            TX_VM_CASE(GET_LOCAL) : {
                const auto slot = read_multibyte_index(false);
                push(stack[slot]);
                TX_VM_BREAK();
            }
            TX_VM_CASE(GET_LOCAL_LONG) : {
                const auto slot = read_multibyte_index(true);
                push(stack[slot]);
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_LOCAL) : {
                const auto slot = read_multibyte_index(false);
                stack[slot] = peek(0);
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_LOCAL_LONG) : {
                const auto slot = read_multibyte_index(true);
                stack[slot] = peek(0);
                TX_VM_BREAK();
            }
            TX_VM_CASE(GET_GLOBAL) : {
                auto value = global_values[read_multibyte_index(false)];
                if (value.is_none()) {
                    runtime_error("Undefined variable.");
                    return InterpretResult::RUNTIME_ERROR;
                }
                push(value);
                TX_VM_BREAK();
            }
            TX_VM_CASE(GET_GLOBAL_LONG) : {
                auto value = global_values[read_multibyte_index(true)];
                if (value.is_none()) {
                    runtime_error("Undefined variable.");
                    return InterpretResult::RUNTIME_ERROR;
                }
                push(value);
                TX_VM_BREAK();
            }
            TX_VM_CASE(DEFINE_GLOBAL) : {
                global_values[read_multibyte_index(false)] = peek(0);
                pop();
                TX_VM_BREAK();
            }
            TX_VM_CASE(DEFINE_GLOBAL_LONG) : {
                global_values[read_multibyte_index(true)] = peek(0);
                pop();
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_GLOBAL) : {
                auto index = read_multibyte_index(false);
                if (global_values[index].is_none()) {
                    runtime_error("Undefined variable.");
                    return InterpretResult::RUNTIME_ERROR;
                }
                global_values[index] = peek(0);
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_GLOBAL_LONG) : {
                auto index = read_multibyte_index(true);
                if (global_values[index].is_none()) {
                    runtime_error("Undefined variable.");
                    return InterpretResult::RUNTIME_ERROR;
                }
                global_values[index] = peek(0);
                TX_VM_BREAK();
            }
            TX_VM_CASE(EQUAL) : {
                const Value rhs = pop();
                const Value lhs = pop();
                push(Value(lhs == rhs));
                TX_VM_BREAK();
            }
            TX_VM_CASE(NOT_EQUAL) : {
                const Value rhs = pop();
                const Value lhs = pop();
                push(Value(lhs != rhs));
                TX_VM_BREAK();
            }
            TX_VM_CASE(GREATER) : {
                if (binary_op<std::greater>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(LESS) : {
                if (binary_op<std::less>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(GREATER_EQUAL) : {
                if (binary_op<std::greater_equal>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(LESS_EQUAL) : {
                if (binary_op<std::less_equal>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
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
            TX_VM_CASE(NOT) : {
                push(Value(pop().is_falsey()));
                TX_VM_BREAK();
            }
            TX_VM_CASE(NEGATE) : {
                if (negate_op()) { return InterpretResult::RUNTIME_ERROR; }
                TX_VM_BREAK();
            }
            TX_VM_CASE(JUMP) : {
                auto offset = read_short();
                instruction_ptr = std::next(instruction_ptr, offset);
                TX_VM_BREAK();
            }
            TX_VM_CASE(JUMP_IF_FALSE) : {
                auto offset = read_short();
                if (peek(0).is_falsey()) { instruction_ptr += offset; }
                TX_VM_BREAK();
            }
            TX_VM_CASE(END_SCOPE) : {
                const auto slot_count = read_multibyte_index(false);
                const auto result = pop();
                for (auto i = 0; i < slot_count; ++i) { pop(); }
                push(result);
                TX_VM_BREAK();
            }
            TX_VM_CASE(RETURN) : { return InterpretResult::OK; }
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
