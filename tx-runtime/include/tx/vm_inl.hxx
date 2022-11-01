#pragma once

#include "tx/vm.hxx"
//
#include "tx/chunk.hxx"
#include "tx/common.hxx"
#include "tx/compiler.hxx"
#include "tx/debug.hxx"
#include "tx/formatting.hxx"
#include "tx/object.hxx"
#include "tx/utils.hxx"
#include "tx/value.hxx"

#include <fmt/format.h>

#include <cstddef>
#include <iterator>
#include <functional>
#include <ranges>

namespace tx {

inline Value std_cpu_clock_native(VM& /*tvm*/, std::span<Value> /*args*/) {
    // TODO: only differences in clock are meaningful do not devide here
    return Value{static_cast<float_t>(std::clock()) / CLOCKS_PER_SEC};
}

// wall clock?
inline Value std_now_native(VM& tvm, std::span<Value> /*args*/) {
    // XXX
    return Value{make_string(tvm, true, "Not implemented!")};
}

inline Value std_print_native(VM& /*tvm*/, std::span<Value> args) {
    fmt::print("{}", args[0]);
    return Value{val_nil};
}

inline VM::VM(VMOptions opts, const Allocator& alloc) noexcept
        : options(opts)
        , allocator(alloc) {
    define_native("std_print", std_print_native);
    define_native("std_cpu_clock", std_cpu_clock_native);
    define_native("std_now", std_now_native);
}

inline constexpr VM::~VM() noexcept {
    global_indices.destroy(*this);
    global_values.destroy(*this);
    strings.destroy(*this);
    free_objects(*this, objects);
}

[[nodiscard]] inline constexpr ByteCode CallFrame::read_byte() noexcept {
    auto result = *instruction_ptr;
    instruction_ptr = std::next(instruction_ptr);
    return result;
}

[[nodiscard]] inline constexpr size_t CallFrame::read_multibyte_index(
    bool is_long
) noexcept {
    const auto [constant_idx, new_ptr] = ::tx::read_multibyte_index(
        instruction_ptr,
        is_long
    );
    instruction_ptr = new_ptr;
    return constant_idx;
}

[[nodiscard]] inline constexpr u16 CallFrame::read_short() noexcept {
    instruction_ptr = std::next(instruction_ptr, 2);
    return static_cast<u16>(
        // NOLINTNEXTLINE(*-magic-numbers, *-union-access)
        static_cast<u16>(std::prev(instruction_ptr, 2)->value << 8U)
        // NOLINTNEXTLINE(*-union-access)
        | static_cast<u16>(std::prev(instruction_ptr, 1)->value)
    );
}

[[nodiscard]] inline constexpr Value CallFrame::read_constant(bool is_long
) noexcept {
    const auto constant_idx = read_multibyte_index(is_long);
    return function->chunk.constants[static_cast<size_t>(constant_idx)];
}

inline constexpr void VM::reset_stack() noexcept {
    stack.clear();
    frames.clear();
}

inline void VM::runtime_error_impl() noexcept {
    fmt::print(stderr, "\n");
    // for (const auto& frame : frames | std::views::reverse) {
    for (size_t i = frames.size() - 1; i >= 0; --i) {
        const auto& frame = frames[i];
        const auto& function = *frame.function;
        auto instruction = frame.instruction_ptr - function.chunk.code.begin()
                           - 1;
        fmt::print(
            stderr,
            "[line {:d}] in {:s}\n",
            function.chunk.get_line(static_cast<i32>(instruction)),
            function.get_name()
        );
    }
    reset_stack();
}

// TX_VM_CONSTEXPR
inline InterpretResult VM::interpret(std::string_view source) noexcept {
    // TODO: print tokens as they are consumed
    if constexpr (HAS_DEBUG_FEATURES) {
        if (options.print_tokens) { print_tokens(*this, source); }
    }
    Parser current_parser(*this, source);
    parser = &current_parser;
    ObjFunction* function = parser->compile();
    parser = nullptr;
    if (function == nullptr) { return InterpretResult::COMPILE_ERROR; }
    push(Value{function});
    (void)call(*function, 0);
    return run();
}

inline constexpr size_t VM::define_global(Value name, Value val) noexcept {
    // TODO: maybe do the GC push/pop dance here
    auto new_index = global_values.size();
    global_values.push_back(*this, val);
    global_indices.set(*this, name, Value{static_cast<int_t>(new_index)});
    return new_index;
}

inline void VM::define_native(std::string_view name, NativeFn fun) noexcept {
    push(Value{make_string(*this, true, name)});
    push(Value{allocate_object<ObjNative>(*this, fun)});
    define_global(stack[0], stack[1]);
    pop();
    pop();
}

[[nodiscard]] inline constexpr bool
VM::call(ObjFunction& fun, size_t arg_c) noexcept {
    // TODO: make this a compile time error
    if (arg_c != fun.arity) {
        runtime_error(
            "Expected {:d} arguments but got {:d}.",
            fun.arity,
            arg_c
        );
        return false;
    }
    if (frames.size() == FRAMES_MAX) {
        runtime_error("Stack overflow.");
        return false;
    }
    frames.push_back(CallFrame{
        .function = &fun,
        .instruction_ptr = fun.chunk.code.begin(),
        .slots = std::prev(stack.end(), arg_c + 1)});
    return true;
}

[[nodiscard]] inline constexpr bool
VM::call_value(Value callee, size_t arg_c) noexcept {
    if (callee.is_object()) {
        auto& obj = callee.as_object();
        switch (obj.type) {
            using enum ObjType;
            case FUNCTION: return call(obj.as<ObjFunction>(), arg_c);
            case NATIVE: {
                auto& native = obj.as<ObjNative>();
                auto result = std::invoke(
                    native.function,
                    *this,
                    std::span<Value>(
                        std::prev(stack.end(), arg_c),
                        static_cast<std::size_t>(arg_c)
                    )
                );
                stack.erase(std::prev(stack.cend(), arg_c + 1), stack.cend());
                push(result);
                return true;
            }
            default: break;
        }
    }
    // TODO: make this a compile time error
    runtime_error("Can't only call functions and classes.");
    return false;
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

inline void CallFrame::print_instruction() const noexcept {
    disassemble_instruction(
        function->chunk,
        static_cast<size_t>(
            std::distance(function->chunk.code.cbegin(), instruction_ptr)
        )
    );
}

inline constexpr void VM::debug_trace() const noexcept {
    if constexpr (HAS_DEBUG_FEATURES) {
        if (options.trace_execution) {
            print_stack();
            frames.back().print_instruction();
        }
    }
}

[[gnu::flatten]] inline TX_VM_CONSTEXPR InterpretResult VM::run() noexcept {
    // clang-format off
    #ifdef TX_ENABLE_COMPUTED_GOTO
        __extension__
        static void* dispatch_table[] = {
            #define TX_OPCODE(name, length, _) &&L_opcode_##name,
            #include "tx/opcodes.inc"
            #undef TX_OPCODE
        };
        #define TX_VM_DISPATCH TX_VM_BREAK();
        #define TX_VM_CASE(name) L_opcode_##name
        #define TX_VM_BREAK() \
            do { \
                debug_trace(); \
                instruction = frame->read_byte().as_opcode(); \
                (__extension__( \
                    {goto *dispatch_table[to_underlying(instruction)];} \
                )); \
            } while (false)
    #else
        #define TX_VM_DISPATCH  \
            debug_trace(); \
            instruction = frame->read_byte().as_opcode(); \
            switch (instruction)
        // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
        #define TX_VM_CASE(name) case name
        // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
        #define TX_VM_BREAK() break
    #endif
    // clang-format on

    // Try to force the compiler to store it in a register
    CallFrame* frame = &frames.back();
    for (;;) {
        // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
        OpCode instruction;
        TX_VM_DISPATCH {
            using enum OpCode;
            TX_VM_CASE(CONSTANT) : {
                push(frame->read_constant(false));
                TX_VM_BREAK();
            }
            TX_VM_CASE(CONSTANT_LONG) : {
                push(frame->read_constant(true));
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
                const auto slot = frame->read_multibyte_index(false);
                push(frame->slots[slot]);
                TX_VM_BREAK();
            }
            TX_VM_CASE(GET_LOCAL_LONG) : {
                const auto slot = frame->read_multibyte_index(true);
                push(frame->slots[slot]);
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_LOCAL) : {
                const auto slot = frame->read_multibyte_index(false);
                frame->slots[slot] = peek(0);
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_LOCAL_LONG) : {
                const auto slot = frame->read_multibyte_index(true);
                frame->slots[slot] = peek(0);
                TX_VM_BREAK();
            }
            TX_VM_CASE(GET_GLOBAL) : {
                auto value = global_values[frame->read_multibyte_index(false)];
                if (value.is_none()) {
                    runtime_error("Undefined variable.");
                    return InterpretResult::RUNTIME_ERROR;
                }
                push(value);
                TX_VM_BREAK();
            }
            TX_VM_CASE(GET_GLOBAL_LONG) : {
                auto value = global_values[frame->read_multibyte_index(true)];
                if (value.is_none()) {
                    runtime_error("Undefined variable.");
                    return InterpretResult::RUNTIME_ERROR;
                }
                push(value);
                TX_VM_BREAK();
            }
            TX_VM_CASE(DEFINE_GLOBAL) : {
                global_values[frame->read_multibyte_index(false)] = peek(0);
                pop();
                TX_VM_BREAK();
            }
            TX_VM_CASE(DEFINE_GLOBAL_LONG) : {
                global_values[frame->read_multibyte_index(true)] = peek(0);
                pop();
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_GLOBAL) : {
                auto index = frame->read_multibyte_index(false);
                if (global_values[index].is_none()) {
                    runtime_error("Undefined variable.");
                    return InterpretResult::RUNTIME_ERROR;
                }
                global_values[index] = peek(0);
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_GLOBAL_LONG) : {
                auto index = frame->read_multibyte_index(true);
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
                auto offset = frame->read_short();
                frame->instruction_ptr = std::next(
                    frame->instruction_ptr,
                    offset
                );
                TX_VM_BREAK();
            }
            TX_VM_CASE(JUMP_IF_FALSE) : {
                auto offset = frame->read_short();
                frame->instruction_ptr = std::next(
                    frame->instruction_ptr,
                    static_cast<i64>(peek(0).is_falsey()) * offset
                );
                TX_VM_BREAK();
            }
            TX_VM_CASE(LOOP) : {
                auto offset = frame->read_short();
                frame->instruction_ptr = std::prev(
                    frame->instruction_ptr,
                    offset
                );
                TX_VM_BREAK();
            }
            TX_VM_CASE(CALL) : {
                auto arg_count = frame->read_multibyte_index(false);
                if (!call_value(peek(arg_count), arg_count)) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                frame = &frames.back();
                TX_VM_BREAK();
            }
            TX_VM_CASE(END_SCOPE) : {
                const auto slot_count = frame->read_multibyte_index(false);
                const auto result = pop();
                for (auto i = 0; i < slot_count; ++i) { pop(); }
                push(result);
                TX_VM_BREAK();
            }
            TX_VM_CASE(RETURN) : {
                const auto result = pop();
                frames.pop_back();
                if (frames.empty()) {
                    pop();
                    return InterpretResult::OK;
                }
                stack.erase(frame->slots, stack.end());
                push(result);
                frame = &frames.back();
                TX_VM_BREAK();
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
