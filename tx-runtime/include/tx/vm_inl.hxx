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

#include <chrono>
#include <cstddef>
#include <iterator>
#include <functional>
#include <ranges>
#include <thread>

namespace tx {

inline Value core_version_string_native(VM& tvm, std::span<Value> args) {
    assert(args.empty());
    return Value{make_string(tvm, false, VERSION)};
}

inline Value core_version_major_native(VM& /*tvm*/, std::span<Value> args) {
    assert(args.empty());
    return Value{static_cast<int_t>(VERSION_MAJOR)};
}

inline Value core_version_minor_native(VM& /*tvm*/, std::span<Value> args) {
    assert(args.empty());
    return Value{static_cast<int_t>(VERSION_MINOR)};
}

inline Value core_version_patch_native(VM& /*tvm*/, std::span<Value> args) {
    assert(args.empty());
    return Value{static_cast<int_t>(VERSION_PATCH)};
}

inline Value core_version_tweak_native(VM& /*tvm*/, std::span<Value> args) {
    assert(args.empty());
    return Value{static_cast<int_t>(VERSION_TWEAK)};
}

inline Value std_clock_native(VM& /*tvm*/, std::span<Value> args) {
    assert(args.empty());
    // FIXME: dividing here does not work
    return Value{static_cast<float_t>(std::clock()) / CLOCKS_PER_SEC};
}

inline Value std_sleep_for_native(VM& /*tvm*/, std::span<Value> args) {
    assert(args.size() == 1);
    assert(args[0].is_number());
    // FIXME: use a user supplied function to support freestanding/bare-metal
    std::this_thread::sleep_for(
        std::chrono::duration<float_t>(args[0].as_float_force())
    );
    return Value{val_nil};
}

inline Value std_println_native(VM& /*tvm*/, std::span<Value> args) {
    assert(args.size() == 1);
    fmt::print("{}\n", args[0]);
    return Value{val_nil};
}

inline Value
float_has_integer_value_native(VM& /*tvm*/, std::span<Value> args) {
    assert(args.size() == 1);
    assert(args[0].is_float());
    return Value{has_integer_value(args[0].as_float())};
}

inline VM::VM(VMOptions opts, const Allocator& alloc) noexcept
        : options(opts)
        , allocator(alloc) {
    define_native("core_version_string", core_version_string_native);
    define_native("core_version_major", core_version_major_native);
    define_native("core_version_minor", core_version_minor_native);
    define_native("core_version_patch", core_version_patch_native);
    define_native("core_version_tweak", core_version_tweak_native);
    define_native("std_clock", std_clock_native);
    define_native("std_sleep_for", std_sleep_for_native);
    define_native("std_println", std_println_native);
    define_native("Float_has_integer_value", float_has_integer_value_native);
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

inline constexpr void VM::reset_stack() noexcept {
    stack.clear();
    frames.clear();
}

inline void VM::runtime_error_impl() noexcept {
    fmt::print(stderr, "\n");
    for (size_t i = frames.size() - 1; i >= 0; --i) {
        const auto& frame = frames[i];
        const auto& function = *frame.function;
        auto instruction = frame.instruction_ptr - function.chunk.code.begin()
                           - 1;
        fmt::print(
            stderr,
            "[line {:d}] in {:s}\n",
            function.chunk.get_line(static_cast<i32>(instruction)),
            function.get_display_name()
        );
    }
    reset_stack();
}

// TX_VM_CONSTEXPR
inline InterpretResult VM::interpret(std::string_view source) noexcept {
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
    assert(stack.empty());
    push(Value{make_string(*this, false, name)});
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
    runtime_error("Can only call functions.");
    return false;
}

[[nodiscard]] inline constexpr bool VM::negate_op() noexcept {
    Value result{val_none};
    if (peek(0).is_int()) {
        result = Value(-pop().as_int());
    } else if (peek(0).is_float()) {
        result = Value(-pop().as_float());
    } else {
        // TODO: make this a compile time error
        runtime_error("Operand must be a number.");
        return true;
    }
    push(result);
    return false;
}

template <template <typename> typename Op>
[[nodiscard]] constexpr bool VM::binary_op() noexcept {
    if (!peek(0).is_number() || !peek(1).is_number()) {
        // TODO: make this a compile time error
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
    disassemble_instruction(function->chunk, instruction_ptr);
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
                constexpr auto count = get_byte_count_following_opcode(CONSTANT
                );
                static_assert(count == 1);
                push(frame->read_constant<count>());
                TX_VM_BREAK();
            }
            TX_VM_CASE(CONSTANT_LONG) : {
                constexpr auto count = get_byte_count_following_opcode(
                    CONSTANT_LONG
                );
                static_assert(count == 3);
                push(frame->read_constant<count>());
                TX_VM_BREAK();
            }
            TX_VM_CASE(NIL) : {
                constexpr auto count = get_byte_count_following_opcode(NIL);
                static_assert(count == 0);
                push(Value(val_nil));
                TX_VM_BREAK();
            }
            TX_VM_CASE(TRUE) : {
                constexpr auto count = get_byte_count_following_opcode(TRUE);
                static_assert(count == 0);
                push(Value(true));
                TX_VM_BREAK();
            }
            TX_VM_CASE(FALSE) : {
                constexpr auto count = get_byte_count_following_opcode(FALSE);
                static_assert(count == 0);
                push(Value(false));
                TX_VM_BREAK();
            }
            TX_VM_CASE(POP) : {
                constexpr auto count = get_byte_count_following_opcode(POP);
                static_assert(count == 0);
                pop();
                TX_VM_BREAK();
            }
            TX_VM_CASE(GET_LOCAL) : {
                constexpr auto count = get_byte_count_following_opcode(GET_LOCAL
                );
                static_assert(count == 1);
                const auto slot = frame->read_multibyte_operand<count>();
                push(frame->slots[slot]);
                TX_VM_BREAK();
            }
            TX_VM_CASE(GET_LOCAL_LONG) : {
                constexpr auto count = get_byte_count_following_opcode(
                    GET_LOCAL_LONG
                );
                static_assert(count == 3);
                const auto slot = frame->read_multibyte_operand<count>();
                push(frame->slots[slot]);
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_LOCAL) : {
                constexpr auto count = get_byte_count_following_opcode(SET_LOCAL
                );
                static_assert(count == 1);
                const auto slot = frame->read_multibyte_operand<count>();
                frame->slots[slot] = peek(0);
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_LOCAL_LONG) : {
                constexpr auto count = get_byte_count_following_opcode(
                    SET_LOCAL_LONG
                );
                static_assert(count == 3);
                const auto slot = frame->read_multibyte_operand<count>();
                frame->slots[slot] = peek(0);
                TX_VM_BREAK();
            }
            TX_VM_CASE(GET_GLOBAL) : {
                constexpr auto count = get_byte_count_following_opcode(
                    GET_GLOBAL
                );
                static_assert(count == 1);
                auto value =
                    global_values[frame->read_multibyte_operand<count>()];
                if (value.is_none()) {
                    runtime_error("Undefined variable.");
                    return InterpretResult::RUNTIME_ERROR;
                }
                push(value);
                TX_VM_BREAK();
            }
            TX_VM_CASE(GET_GLOBAL_LONG) : {
                constexpr auto count = get_byte_count_following_opcode(
                    GET_GLOBAL_LONG
                );
                static_assert(count == 3);
                auto value =
                    global_values[frame->read_multibyte_operand<count>()];
                if (value.is_none()) {
                    runtime_error("Undefined variable.");
                    return InterpretResult::RUNTIME_ERROR;
                }
                push(value);
                TX_VM_BREAK();
            }
            TX_VM_CASE(DEFINE_GLOBAL) : {
                constexpr auto count = get_byte_count_following_opcode(
                    DEFINE_GLOBAL
                );
                static_assert(count == 1);
                global_values[frame->read_multibyte_operand<count>()] = peek(0);
                pop();
                TX_VM_BREAK();
            }
            TX_VM_CASE(DEFINE_GLOBAL_LONG) : {
                constexpr auto count = get_byte_count_following_opcode(
                    DEFINE_GLOBAL_LONG
                );
                static_assert(count == 3);
                global_values[frame->read_multibyte_operand<count>()] = peek(0);
                pop();
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_GLOBAL) : {
                constexpr auto count = get_byte_count_following_opcode(
                    SET_GLOBAL
                );
                static_assert(count == 1);
                auto index = frame->read_multibyte_operand<count>();
                if (global_values[index].is_none()) {
                    runtime_error("Undefined variable.");
                    return InterpretResult::RUNTIME_ERROR;
                }
                global_values[index] = peek(0);
                TX_VM_BREAK();
            }
            TX_VM_CASE(SET_GLOBAL_LONG) : {
                constexpr auto count = get_byte_count_following_opcode(
                    SET_GLOBAL_LONG
                );
                static_assert(count == 3);
                auto index = frame->read_multibyte_operand<count>();
                if (global_values[index].is_none()) {
                    runtime_error("Undefined variable.");
                    return InterpretResult::RUNTIME_ERROR;
                }
                global_values[index] = peek(0);
                TX_VM_BREAK();
            }
            TX_VM_CASE(EQUAL) : {
                constexpr auto count = get_byte_count_following_opcode(EQUAL);
                static_assert(count == 0);
                const Value rhs = pop();
                const Value lhs = pop();
                push(Value(lhs == rhs));
                TX_VM_BREAK();
            }
            TX_VM_CASE(NOT_EQUAL) : {
                constexpr auto count = get_byte_count_following_opcode(NOT_EQUAL
                );
                static_assert(count == 0);
                const Value rhs = pop();
                const Value lhs = pop();
                push(Value(lhs != rhs));
                TX_VM_BREAK();
            }
            TX_VM_CASE(GREATER) : {
                constexpr auto count = get_byte_count_following_opcode(GREATER);
                static_assert(count == 0);
                if (binary_op<std::greater>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(LESS) : {
                constexpr auto count = get_byte_count_following_opcode(LESS);
                static_assert(count == 0);
                if (binary_op<std::less>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(GREATER_EQUAL) : {
                constexpr auto count = get_byte_count_following_opcode(
                    GREATER_EQUAL
                );
                static_assert(count == 0);
                if (binary_op<std::greater_equal>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(LESS_EQUAL) : {
                constexpr auto count = get_byte_count_following_opcode(
                    LESS_EQUAL
                );
                static_assert(count == 0);
                if (binary_op<std::less_equal>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(ADD) : {
                constexpr auto count = get_byte_count_following_opcode(ADD);
                static_assert(count == 0);
                if (binary_op<std::plus>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(SUBSTRACT) : {
                constexpr auto count = get_byte_count_following_opcode(SUBSTRACT
                );
                static_assert(count == 0);
                if (binary_op<std::minus>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(MULTIPLY) : {
                constexpr auto count = get_byte_count_following_opcode(MULTIPLY
                );
                static_assert(count == 0);
                if (binary_op<std::multiplies>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(DIVIDE) : {
                constexpr auto count = get_byte_count_following_opcode(DIVIDE);
                static_assert(count == 0);
                if (peek(0).is_int() && peek(1).is_int()
                    && peek(0).as_int() == 0) {
                    runtime_error("Division by zero.");
                    return InterpretResult::RUNTIME_ERROR;
                }
                if (binary_op<std::divides>()) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                TX_VM_BREAK();
            }
            TX_VM_CASE(NOT) : {
                constexpr auto count = get_byte_count_following_opcode(NOT);
                static_assert(count == 0);
                push(Value(pop().is_falsey()));
                TX_VM_BREAK();
            }
            TX_VM_CASE(NEGATE) : {
                constexpr auto count = get_byte_count_following_opcode(NEGATE);
                static_assert(count == 0);
                if (negate_op()) { return InterpretResult::RUNTIME_ERROR; }
                TX_VM_BREAK();
            }
            TX_VM_CASE(JUMP) : {
                constexpr auto count = get_byte_count_following_opcode(JUMP);
                static_assert(count == 2);
                auto offset = frame->read_multibyte_operand<count>();
                frame->instruction_ptr = std::next(
                    frame->instruction_ptr,
                    offset
                );
                TX_VM_BREAK();
            }
            TX_VM_CASE(JUMP_IF_FALSE) : {
                constexpr auto count = get_byte_count_following_opcode(
                    JUMP_IF_FALSE
                );
                static_assert(count == 2);
                auto offset = frame->read_multibyte_operand<count>();
                frame->instruction_ptr = std::next(
                    frame->instruction_ptr,
                    static_cast<i64>(peek(0).is_falsey()) * offset
                );
                TX_VM_BREAK();
            }
            TX_VM_CASE(LOOP) : {
                constexpr auto count = get_byte_count_following_opcode(LOOP);
                static_assert(count == 2);
                auto offset = frame->read_multibyte_operand<count>();
                frame->instruction_ptr = std::prev(
                    frame->instruction_ptr,
                    offset
                );
                TX_VM_BREAK();
            }
            TX_VM_CASE(CALL) : {
                constexpr auto count = get_byte_count_following_opcode(CALL);
                static_assert(count == 1);
                auto arg_count = frame->read_multibyte_operand<count>();
                if (!call_value(peek(arg_count), arg_count)) {
                    return InterpretResult::RUNTIME_ERROR;
                }
                frame = &frames.back();
                TX_VM_BREAK();
            }
            TX_VM_CASE(END_SCOPE) : {
                constexpr auto count = get_byte_count_following_opcode(END_SCOPE
                );
                static_assert(count == 1);
                const auto slot_count = frame->read_multibyte_operand<count>();
                const auto result = pop();
                for (auto i = 0; i < slot_count; ++i) { pop(); }
                push(result);
                TX_VM_BREAK();
            }
            TX_VM_CASE(END_SCOPE_LONG) : {
                constexpr auto count = get_byte_count_following_opcode(
                    END_SCOPE_LONG
                );
                static_assert(count == 3);
                const auto slot_count = frame->read_multibyte_operand<count>();
                const auto result = pop();
                for (auto i = 0; i < slot_count; ++i) { pop(); }
                push(result);
                TX_VM_BREAK();
            }
            TX_VM_CASE(RETURN) : {
                constexpr auto count = get_byte_count_following_opcode(RETURN);
                static_assert(count == 0);
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
    #undef TX_VM_DISPATCH
    #undef TX_VM_CASE
    #undef TX_VM_BREAK
    // clang-format on
}

}  // namespace tx
